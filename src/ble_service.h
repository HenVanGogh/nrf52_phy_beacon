/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* Initialize BLE stack and start advertising */
int ble_init(void);

/* Update sensor values and notify subscribers */
void ble_update_sensor_values(float temperature, float humidity);

#endif /* BLE_SERVICE_H_ */
