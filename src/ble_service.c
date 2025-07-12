/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble_service.h"
#include "conf.h"

#if USE_SHT31
LOG_MODULE_DECLARE(sht31_ble, CONFIG_LOG_DEFAULT_LEVEL);
/* BLE definitions */
#define DEVICE_NAME "nRF52840_SHT31"
#elif USE_SHT41
LOG_MODULE_DECLARE(sht41_ble, CONFIG_LOG_DEFAULT_LEVEL);
/* BLE definitions */
#define DEVICE_NAME "nRF52840_SHT41"
#endif

#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Temperature and humidity data */
static uint16_t temperature_value;   /* Temperature in 0.01 degrees Celsius */
static uint16_t humidity_value;      /* Humidity in 0.01 percent */

static struct bt_conn *current_conn;

/* Temperature characteristic read callback */
static ssize_t read_temperature(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	const uint16_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

/* Humidity characteristic read callback */
static ssize_t read_humidity(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	const uint16_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

/* Environmental Sensing Service Definition */
BT_GATT_SERVICE_DEFINE(ess_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
	
	/* Temperature Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_temperature, NULL, &temperature_value),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	
	/* Humidity Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_humidity, NULL, &humidity_value),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	LOG_INF("Connected");
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

/* BLE advertisement data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x1a, 0x18), /* Environmental Sensing Service */
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Initialize BLE */
static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising started");
}

int ble_init(void)
{
	int err;

	/* Initialize BLE stack */
	err = bt_enable(bt_ready);
	if (err) {
		printk("BLE initialization failed (err %d)\n", err);
		return err;
	}

	/* Register connection callbacks */
	bt_conn_cb_register(&conn_callbacks);
	
	return 0;  /* Success */
}

void ble_update_sensor_values(float temperature, float humidity)
{
	/* Update BLE characteristics (temperature in hundredths of a degree Celsius) */
	temperature_value = (uint16_t)(temperature * 100);
	humidity_value = (uint16_t)(humidity * 100);

	/* Notify connected clients about the new values */
	if (current_conn) {
		/* Notify temperature characteristic (index 2 in the ESS service) */
		bt_gatt_notify(NULL, &ess_svc.attrs[2], &temperature_value, sizeof(temperature_value));
		
		/* Notify humidity characteristic (index 5 in the ESS service) */
		bt_gatt_notify(NULL, &ess_svc.attrs[5], &humidity_value, sizeof(humidity_value));
	}
}
