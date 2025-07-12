/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAIN_H_
#define MAIN_H_

/* Sensor type configuration */
/* Set only one of these to 1, the other to 0 */
#define USE_SHT31 1
#define USE_SHT41 0

/* Common definitions regardless of sensor type */
#define SLEEP_TIME_MS   2000

/* Error code definitions */
#define ERROR_NONE                     0
#define ERROR_LED_INIT                 1  /* LED device not ready */
#define ERROR_LED_CONFIG               2  /* Failed to configure LED */
#define ERROR_SENSOR_NOT_READY         3  /* Temperature/humidity sensor not ready */
#define ERROR_SENSOR_FETCH_FAILED      4  /* Failed to fetch sensor data */
#define ERROR_SENSOR_TEMP_READ_FAILED  5  /* Failed to read temperature data */
#define ERROR_SENSOR_HUM_READ_FAILED   6  /* Failed to read humidity data */
#define ERROR_BLE_INIT_FAILED          7  /* BLE initialization failed */

/* Error indication parameters */
#define ERROR_BLINK_FAST_MS           200  /* Fast blink - 200ms on/off */
#define ERROR_BLINK_SLOW_MS           500  /* Slow blink - 500ms on/off */
#define ERROR_BLINK_PAUSE_MS         1500  /* Pause between error code sequences */
#define ERROR_BLINK_REPEAT           1000  /* Number of times to repeat error pattern */

#endif /* MAIN_H_ */
