/*
 * Copyright (c) 2024 Your Company
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

/* Initialize BLE stack and start Eddystone advertising */
int ble_init(void);

/* Update sensor values and restart Eddystone advertising with new data */
void ble_update_sensor_values(float temperature, float humidity);

#endif /* BLE_SERVICE_H_ */
