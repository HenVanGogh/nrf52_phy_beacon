.. zephyr:code-sample:: nrf52840-sht31
   :name: nRF52840 SHT31 Sensor Sample
   :relevant-api: gpio_interface sensor_interface i2c_interface

   Read temperature and humidity data from an SHT31 sensor using I2C.

Overview
********

This sample reads temperature and humidity data from an SHT31 sensor connected
to the Adafruit nRF52840 board via I2C. The LED will blink to indicate the
system is running, and sensor data is printed to the console at regular intervals.

The source code shows how to:

#. Get a pin specification from the :ref:`devicetree <dt-guide>` as a
   :c:struct:`gpio_dt_spec` for the LED
#. Configure the GPIO pin as an output
#. Initialize and use the SHT31 sensor driver
#. Read temperature and humidity values and print them to the console

.. _blinky-sample-requirements:

Requirements
************

Your board must:

#. Be an Adafruit nRF52840 board or compatible
#. Have an LED connected via a GPIO pin (these are called "User LEDs" on many of
   Zephyr's :ref:`boards`).
#. Have the LED configured using the ``led0`` devicetree alias.
#. Have an SHT31 temperature/humidity sensor connected to the I2C0 bus at address 0x44

Building and Running
********************

Build and flash the application as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/sensor/sht31
   :board: adafruit_nrf52840
   :goals: build flash
   :compact:

After flashing, the LED starts to blink and the SHT31 sensor readings 
(temperature and humidity) are printed on the console every 2 seconds. 
If a runtime error occurs or the sensor cannot be initialized, appropriate 
error messages will be printed to the console.

Hardware Connections
*****************

Connect the SHT31 sensor to the Adafruit nRF52840 board as follows:

* SHT31 VCC -> 3.3V
* SHT31 GND -> GND
* SHT31 SCL -> SCL (I2C0 SCL pin on the nRF52840)
* SHT31 SDA -> SDA (I2C0 SDA pin on the nRF52840)

For the Adafruit nRF52840 Feather, the I2C pins are:
* SCL: Pin 13 (P0.14)
* SDA: Pin 11 (P0.16)

Troubleshooting
**************

If the sensor is not detected, check the following:

1. Verify that the SHT31 is properly connected to the correct I2C pins
2. Verify that the SHT31 has power (3.3V)
3. Confirm that the I2C address is correct (0x44 or 0x45, depending on the ADDR pin state)
4. Check if pullup resistors are needed on the I2C lines

The default I2C address used in this sample is 0x44 (ADDR pin connected to GND). 
If your SHT31 uses address 0x45 (ADDR pin connected to VCC), you need to modify the board 
overlay file accordingly.
