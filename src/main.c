/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include "conf.h"
#include "ble_service.h"

#if USE_SHT31
LOG_MODULE_REGISTER(sht31_ble, CONFIG_LOG_DEFAULT_LEVEL);
#elif USE_SHT41
LOG_MODULE_REGISTER(sht41_ble, CONFIG_LOG_DEFAULT_LEVEL);
#elif USE_DUMMY_SENSOR
LOG_MODULE_REGISTER(dummy_ble, CONFIG_LOG_DEFAULT_LEVEL);
#endif

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if USE_SHT31
/* The devicetree node identifier for the SHT31 sensor */
#define TEMP_HUM_SENSOR_NODE DT_INST(0, sensirion_sht3xd)
#define SENSOR_NAME "SHT31"
#elif USE_SHT41
/* The devicetree node identifier for the SHT41 sensor */
#define TEMP_HUM_SENSOR_NODE DT_INST(0, sensirion_sht4x)
#define SENSOR_NAME "SHT41"
#elif USE_DUMMY_SENSOR
#define SENSOR_NAME "DUMMY"
#endif

/* Forward declarations */
static void indicate_error(const struct gpio_dt_spec *led_dev, uint8_t error_code);
#if USE_DUMMY_SENSOR
static void process_dummy_sensor_sample(void);
#else
static void process_sensor_sample(const struct device *dev);
#endif

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void process_sensor_sample(const struct device *dev)
{
	struct sensor_value temp, hum;
	float temperature, humidity;
	static bool error_reported = false;
	
	if (sensor_sample_fetch(dev) < 0) {
		printf("%s sensor sample fetch failed\n", SENSOR_NAME);
		if (!error_reported) {
			indicate_error(&led, ERROR_SENSOR_FETCH_FAILED);
			error_reported = true;
		}
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
		printf("Cannot read temperature data from %s\n", SENSOR_NAME);
		if (!error_reported) {
			indicate_error(&led, ERROR_SENSOR_TEMP_READ_FAILED);
			error_reported = true;
		}
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum) < 0) {
		printf("Cannot read humidity data from %s\n", SENSOR_NAME);
		if (!error_reported) {
			indicate_error(&led, ERROR_SENSOR_HUM_READ_FAILED);
			error_reported = true;
		}
		return;
	}
	
	/* If we've made it here, reset the error_reported flag */
	error_reported = false;

	/* Convert sensor values to floating point */
	temperature = sensor_value_to_double(&temp);
	humidity = sensor_value_to_double(&hum);
	/* Display temperature and humidity values */
	printf("%s Measurement:\n", SENSOR_NAME);
	printf("  Temperature: %.2f °C\n", (double)temperature);
	printf("  Humidity: %.2f %%\n", (double)humidity);

	/* Update BLE characteristics and notify connected clients */
	ble_update_sensor_values(temperature, humidity);
}



int main(void)
{
	int ret;
	bool led_state = true;
	
#if !USE_DUMMY_SENSOR
#if USE_SHT31
	const struct device *sensor_dev = DEVICE_DT_GET_ANY(sensirion_sht3xd);
#elif USE_SHT41
	const struct device *sensor_dev = DEVICE_DT_GET_ANY(sensirion_sht4x);
#endif
#endif

	/* Initialize LED first, as we'll need it to indicate errors */
	if (!gpio_is_ready_dt(&led)) {
		printf("Error: LED device is not ready\n");
		/* Can't indicate this error as LED isn't ready */
		return -1;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printf("Error: Failed to configure LED pin\n");
		/* Can't indicate this error properly, but try a simple blink */
		gpio_pin_set_dt(&led, 1);
		k_msleep(100);
		gpio_pin_set_dt(&led, 0);
		return -1;
	}

	/* Initialize temperature/humidity sensor */
#if USE_DUMMY_SENSOR
	printf("%s sensor mode enabled (no physical sensor required)\n", SENSOR_NAME);
#else
	if (!device_is_ready(sensor_dev)) {
		printf("Error: %s device not found or not ready\n", SENSOR_NAME);
		indicate_error(&led, ERROR_SENSOR_NOT_READY);
		return -1;
	}
	printf("%s sensor is ready\n", SENSOR_NAME);
#endif
	printf("Sampling every %d ms\n", SLEEP_TIME_MS);
	
	/* Initialize Bluetooth */
	ret = ble_init();
	if (ret != 0) {
		printf("BLE initialization failed with error: %d\n", ret);
		indicate_error(&led, ERROR_BLE_INIT_FAILED);
		return -1;
	}

	while (1) {
		/* Toggle LED to indicate the system is running */
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return -1;
		}

		led_state = !led_state;
		
		/* Sample and display sensor data */
#if USE_DUMMY_SENSOR
		process_dummy_sensor_sample();
#else
		process_sensor_sample(sensor_dev);
#endif
		
		/* Sleep for the specified time */
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}

/*
 * Function to indicate error codes via LED blink patterns
 * Error code is indicated by number of fast blinks:
 * - LED will blink quickly N times, where N is the error code
 * - Then pause for ERROR_BLINK_PAUSE_MS
 * - Repeat the sequence ERROR_BLINK_REPEAT times
 */
static void indicate_error(const struct gpio_dt_spec *led_dev, uint8_t error_code)
{
	int ret;
	
	if (error_code == ERROR_NONE) {
		return; /* No error to indicate */
	}
	
	printf("Error occurred: Code %d\n", error_code);
	
	/* Repeat the error pattern multiple times */
	for (int repeat = 0; repeat < ERROR_BLINK_REPEAT; repeat++) {
		/* First, ensure LED is off */
		gpio_pin_set_dt(led_dev, 0);
		k_msleep(ERROR_BLINK_PAUSE_MS);
		
		/* Blink the LED according to error code */
		for (int i = 0; i < error_code; i++) {
			/* LED on */
			ret = gpio_pin_set_dt(led_dev, 1);
			if (ret < 0) {
				/* If LED control fails, we can't indicate errors */
				return;
			}
			k_msleep(ERROR_BLINK_FAST_MS);
			
			/* LED off */
			gpio_pin_set_dt(led_dev, 0);
			k_msleep(ERROR_BLINK_FAST_MS);
		}
		
		/* Pause between repeats */
		k_msleep(ERROR_BLINK_PAUSE_MS);
	}
}

#if USE_DUMMY_SENSOR
void process_dummy_sensor_sample(void)
{
	static float base_temperature = 22.5f;  /* Base temperature in Celsius */
	static float base_humidity = 45.0f;     /* Base humidity in percent */
	static float temp_offset = 0.0f;
	static float hum_offset = 0.0f;
	static int sample_count = 0;
	
	float temperature, humidity;
	
	/* Generate realistic varying sensor data */
	sample_count++;
	
	/* Create slow temperature variation (sine wave with period of ~60 samples) */
	temp_offset = 3.0f * sin((double)sample_count * 0.1);
	
	/* Create slow humidity variation (cosine wave with period of ~80 samples) */
	hum_offset = 15.0f * cos((double)sample_count * 0.08);
	
	/* Add some random noise */
	temp_offset += ((float)(k_uptime_get_32() % 100) - 50.0f) / 100.0f;  /* ±0.5°C noise */
	hum_offset += ((float)(k_uptime_get_32() % 200) - 100.0f) / 50.0f;   /* ±2% noise */
	
	temperature = base_temperature + temp_offset;
	humidity = base_humidity + hum_offset;
	
	/* Clamp humidity to realistic range */
	if (humidity < 0.0f) humidity = 0.0f;
	if (humidity > 100.0f) humidity = 100.0f;
	
	/* Display temperature and humidity values */
	printf("%s Measurement (Sample #%d):\n", SENSOR_NAME, sample_count);
	printf("  Temperature: %.2f °C\n", (double)temperature);
	printf("  Humidity: %.2f %%\n", (double)humidity);

	/* Update BLE Eddystone broadcast with new sensor data */
	ble_update_sensor_values(temperature, humidity);
}
#endif
