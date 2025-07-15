/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <stdio.h>
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
#elif USE_DUMMY_SENSOR
LOG_MODULE_DECLARE(dummy_ble, CONFIG_LOG_DEFAULT_LEVEL);
/* BLE definitions */
#define DEVICE_NAME "nRF52840_DUMMY"
#endif

#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Eddystone frame types */
#define EDDYSTONE_TLM_FRAME_TYPE    0x20

/* Eddystone-TLM frame structure (fixed size) */
struct eddystone_tlm_frame {
	uint8_t frame_type;      /* 0x20 for TLM */
	uint8_t version;         /* TLM version (0x00) */
	uint16_t battery_voltage; /* Battery voltage in mV (big endian) */
	uint16_t temperature;    /* Temperature in 8.8 fixed point (big endian) */
	uint32_t adv_count;      /* Advertisement count (big endian) */
	uint32_t uptime;         /* Uptime in 0.1s increments (big endian) */
} __packed;

/* Temperature and humidity data */
static float current_temperature = 0.0f;
static float current_humidity = 0.0f;
static uint32_t advertisement_count = 0;
static bool advertising_started = false;

/* Extended advertising set for long-range BLE */
static struct bt_le_ext_adv *adv_set;

/* Eddystone-TLM advertisement data buffer */
static uint8_t tlm_data[14];  /* TLM frame is exactly 14 bytes */
static uint8_t tlm_data_len = sizeof(struct eddystone_tlm_frame);

/* Function to encode sensor data into Eddystone-TLM format */
static int encode_sensor_tlm(float temperature, float humidity)
{
	struct eddystone_tlm_frame *tlm = (struct eddystone_tlm_frame *)tlm_data;
	
	/* Increment advertisement count */
	advertisement_count++;
	
	/* Fill TLM frame */
	tlm->frame_type = EDDYSTONE_TLM_FRAME_TYPE;
	tlm->version = 0x00;  /* TLM version 0 */
	
	/* Battery voltage - we'll encode humidity here (0-100% as 0-3300mV) */
	tlm->battery_voltage = sys_cpu_to_be16((uint16_t)(humidity * 33));
	
	/* Temperature in 8.8 fixed point format (big endian) */
	/* Convert to 8.8: multiply by 256 and store as 16-bit big endian */
	int16_t temp_8_8 = (int16_t)(temperature * 256);
	tlm->temperature = sys_cpu_to_be16((uint16_t)temp_8_8);
	
	/* Advertisement count (big endian) */
	tlm->adv_count = sys_cpu_to_be32(advertisement_count);
	
	/* Uptime in 0.1s increments (big endian) */
	uint32_t uptime_deciseconds = k_uptime_get_32() / 100;  /* Convert ms to 0.1s */
	tlm->uptime = sys_cpu_to_be32(uptime_deciseconds);
	
	LOG_INF("TLM: T=%.2f°C, H=%.2f%%, Count=%u, Uptime=%u.%us", 
		(double)temperature, (double)humidity, 
		advertisement_count, uptime_deciseconds/10, uptime_deciseconds%10);
	LOG_INF("Encoded TLM data length: %d", tlm_data_len);
	
	return 0;
}

/* Create extended advertising set for coded PHY */
static int create_advertising_coded(void)
{
	int err;
	
	/* Configure extended advertising parameters for Coded PHY */
	struct bt_le_adv_param param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CODED,  /* Extended advertising with Coded PHY */
		BT_GAP_ADV_FAST_INT_MIN_2,  /* Min advertising interval */
		BT_GAP_ADV_FAST_INT_MAX_2,  /* Max advertising interval */
		NULL);  /* No peer address */

	LOG_INF("Creating extended advertising set for long-range BLE...");

	/* Create the extended advertising set */
	err = bt_le_ext_adv_create(&param, NULL, &adv_set);
	if (err) {
		LOG_ERR("Failed to create advertising set (err %d)", err);
		return err;
	}

	LOG_INF("Created advertising set successfully");

	return 0;
}

/* Start extended advertising */
static void start_advertising_coded(void)
{
	int err;

	err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		LOG_ERR("Failed to start advertising set (err %d)", err);
		return;
	}

	advertising_started = true;
	LOG_INF("Advertiser set started with Coded PHY for long range");
}

/* BLE advertisement data for Eddystone-TLM */
static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe), /* Eddystone Service UUID */
	BT_DATA(BT_DATA_SVC_DATA16, tlm_data, 0), /* Will be updated with actual data */
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN), /* Device name */
};

/* Initialize BLE */
static void bt_ready(int err)
{
	LOG_INF("bt_ready callback called with err=%d", err);
	
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized successfully");
	
	/* Initial TLM encoding with default values */
	LOG_INF("Encoding initial TLM with T=%.2f, H=%.2f", 
		(double)current_temperature, (double)current_humidity);
		
	if (encode_sensor_tlm(current_temperature, current_humidity) < 0) {
		LOG_ERR("Failed to encode initial TLM");
		return;
	}

	LOG_INF("TLM encoded successfully, length=%d", tlm_data_len);

	/* Update advertisement data with encoded TLM */
	ad[2].data = tlm_data;
	ad[2].data_len = tlm_data_len;

	/* Create advertising set */
	err = create_advertising_coded();
	if (err) {
		LOG_ERR("Advertising failed to create (err %d)", err);
		return;
	}

	LOG_INF("Setting advertising data for extended advertising...");

	/* Set the advertising data for this set */
	err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Failed to set advertising data (err %d)", err);
		return;
	}

	/* Start advertising */
	start_advertising_coded();
}


int ble_init(void)
{
	int err;

	LOG_INF("Starting BLE initialization...");

	/* Initialize BLE stack */
	err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("BLE initialization failed (err %d)", err);
		return err;
	}
	
	LOG_INF("BLE enable called successfully, waiting for bt_ready callback...");
	return 0;  /* Success */
}

void ble_update_sensor_values(float temperature, float humidity)
{
	int err;
	
	/* Update current values */
	current_temperature = temperature;
	current_humidity = humidity;
	
	LOG_INF("Updating Eddystone TLM with T=%.2f°C, H=%.2f%%", 
		(double)temperature, (double)humidity);

	/* Encode new sensor data into TLM */
	if (encode_sensor_tlm(temperature, humidity) < 0) {
		LOG_ERR("Failed to encode sensor TLM");
		return;
	}

	/* Only update advertising if already started */
	if (!advertising_started) {
		LOG_INF("Advertising not started yet, will be handled by bt_ready callback");
		return;
	}

	/* For ongoing updates, we need to stop and restart advertising to update the data */
	LOG_INF("Updating extended advertising with new TLM data (count=%u)", advertisement_count);
	
	/* Stop current extended advertising */
	err = bt_le_ext_adv_stop(adv_set);
	if (err && err != -EALREADY) {  /* -EALREADY means already stopped */
		LOG_ERR("Failed to stop extended advertising (err %d)", err);
		return;
	}

	/* Update advertisement data with new TLM */
	ad[2].data = tlm_data;
	ad[2].data_len = tlm_data_len;

	/* Set the updated advertising data */
	err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Failed to set updated advertising data (err %d)", err);
		return;
	}

	/* Restart extended advertising with updated data */
	err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		LOG_ERR("Failed to restart extended advertising (err %d)", err);
		advertising_started = false;
		return;
	}
	
	LOG_INF("Long-range Eddystone-TLM advertising updated with new sensor data");
}
