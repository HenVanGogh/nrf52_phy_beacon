# Testing Your Eddystone BLE Application

## Quick Test Steps:

### 1. Mobile App Testing (Easiest)
1. Install "nRF Connect for Mobile" on your phone
2. Open the app and go to Scanner tab
3. Look for device named "nRF52840_DUMMY"
4. You should see Eddystone-URL service data
5. The URL should contain sensor data like: `http://example.com/sensor?t=XX.XX&h=YY.YY`

### 2. What to Look For:
- **Device Name**: nRF52840_DUMMY
- **Service UUID**: 0xFEAA (Eddystone service)
- **Service Data**: Should contain URL with temperature and humidity
- **Advertisement Type**: Non-connectable
- **Updates**: URL should change every 2 seconds with new sensor values

### 3. Troubleshooting:
- Make sure Bluetooth is enabled on your test device
- The nRF52840 should be within range (typically 10-30 meters)
- Look for the LED blinking - this indicates the device is running
- Check console output for any error messages

### 4. Console Output to Expect:
```
DUMMY sensor mode enabled (no physical sensor required)
Sampling every 2000 ms
Bluetooth initialized
Eddystone advertising started
DUMMY Measurement (Sample #1):
  Temperature: 22.15 °C
  Humidity: 47.33 %
Updating Eddystone URL with T=22.15°C, H=47.33%
Eddystone advertising restarted with new sensor data
```

### 5. Advanced Testing with Nordic Command Line Tools:
If you have nRF Command Line Tools installed:
```bash
# Scan for BLE devices
nrfjprog --com

# Or use Nordic's BLE sniffer tools
```

## Expected Behavior:
- Device advertises every 2 seconds with updated sensor data
- Temperature varies around 22.5°C ±3.5°C
- Humidity varies around 45% ±15%
- LED blinks to show system is alive
- No connections needed - it's broadcast-only
