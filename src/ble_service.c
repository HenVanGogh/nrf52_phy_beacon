/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
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

/* BLE advertisement data for Eddystone-TLM */
static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe), /* Eddystone Service UUID */
	BT_DATA(BT_DATA_SVC_DATA16, tlm_data, 0), /* Will be updated with actual data */
};

/* Scan response data with device name */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
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

	LOG_INF("Starting Eddystone-TLM advertising with %d array elements...", ARRAY_SIZE(ad));

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	advertising_started = true;
	LOG_INF("Eddystone-TLM advertising started successfully!");
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

	/* Only restart advertising if not started yet */
	if (!advertising_started) {
		
		LOG_INF("Stopping and restarting advertising (count=%u)", advertisement_count);
		
		/* Stop current advertising */
		err = bt_le_adv_stop();
		if (err && err != -EALREADY) {  /* -EALREADY means already stopped */
			LOG_ERR("Failed to stop advertising (err %d)", err);
		}

		/* Update advertisement data with new TLM */
		ad[2].data = tlm_data;
		ad[2].data_len = tlm_data_len;

		/* Restart advertising with updated data */
		err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		if (err) {
			LOG_ERR("Failed to restart advertising (err %d)", err);
			advertising_started = false;
			return;
		}
		
		advertising_started = true;
		LOG_INF("Eddystone-TLM advertising started with initial sensor data");
	} else {
		/* For ongoing updates, we need to stop and restart advertising to update the data */
		LOG_INF("Updating advertising with new TLM data (count=%u)", advertisement_count);
		
		/* Stop current advertising */
		err = bt_le_adv_stop();
		if (err && err != -EALREADY) {  /* -EALREADY means already stopped */
			LOG_ERR("Failed to stop advertising (err %d)", err);
			return;
		}

		/* Update advertisement data with new TLM */
		ad[2].data = tlm_data;
		ad[2].data_len = tlm_data_len;

		/* Restart advertising with updated data */
		err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		if (err) {
			LOG_ERR("Failed to restart advertising (err %d)", err);
			advertising_started = false;
			return;
		}
		
		LOG_INF("Eddystone-TLM advertising updated with new sensor data");
	}
}
