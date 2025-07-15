# nRF52840 Temperature and Humidity Sensor with BLE

This project demonstrates how to use an SHT31 or SHT41 temperature and humidity sensor with an nRF52840 Dongle, making the sensor data available over Bluetooth Low Energy (BLE).

## Features

- Support for both SHT31 and SHT41 sensors
- BLE Environmental Sensing Service (ESS) with temperature and humidity characteristics
- LED status indicator
- Configurable via build options
  
## BLE Services

The application provides the following BLE services:

- Environmental Sensing Service (0x181A)
  - Temperature Characteristic (0x2A6E)
  - Humidity Characteristic (0x2A6F)
- Device Information Service

## Connection

You can connect to the device using a BLE scanner app on your smartphone or other BLE central devices. The device will appear as "nRF52840_SHT31" or "nRF52840_SHT41" depending on the build configuration.

## License

SPDX-License-Identifier: Apache-2.0
