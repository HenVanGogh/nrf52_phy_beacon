import asyncio
import struct
from bleak import BleakScanner

# --- Configuration ---
TARGET_DEVICE_NAME = "nRF52840_DUMMY"
SCAN_DURATION = 30  # Scan for 30 seconds
EDDYSTONE_SERVICE_UUID = "0000feaa-0000-1000-8000-00805f9b34fb"  # Eddystone Service UUID

def decode_eddystone_tlm(service_data):
    """Decode Eddystone-TLM frame and extract sensor data."""
    try:
        print(f"   🔍 Decoding TLM data: {service_data.hex()} (length: {len(service_data)})")
        
        if len(service_data) < 12:
            print(f"   ❌ Data too short: {len(service_data)} bytes, need at least 12")
            return None
            
        # Our TLM frame structure (without frame type byte):
        # Version(1) + Battery(2) + Temperature(2) + AdvCount(4) + Uptime(4) = 13 bytes
        # But we might have 12 bytes if frame type is missing
        
        if len(service_data) == 12:
            # Missing frame type, TLM data starts with version
            # Based on nRF code: version(1) + humidity_encoded(2) + temp_8_8(2) + adv_count(4) + uptime(4)
            # But we only have 12 bytes, so maybe uptime is truncated to 3 bytes
            
            # From nRF logs: data should be version(1) + battery/humidity(2) + temp(2) + count(4) + uptime(4)
            # Data: 063615e3000002b2000035c7
            # Expected: T=23.43°C, H=53.92%, Count=695, Uptime=1386.7s
            
            # Let's decode manually
            version = service_data[0]  # 0x06 = 6 ✓
            
            # Humidity encoded as battery voltage (big endian): humidity * 33
            battery_voltage = struct.unpack('>H', service_data[1:3])[0]  # 0x3615 = 13845
            # This should be 53.92 * 33 = 1779, but we get 13845
            
            # Temperature in 8.8 fixed point (big endian)
            temperature_raw = struct.unpack('>H', service_data[3:5])[0]  # 0x15e3 = 5603
            
            # Advertisement count (big endian, 4 bytes)
            adv_count = struct.unpack('>I', service_data[5:9])[0]  # Should be 695
            
            # Uptime (remaining 3 bytes, pad to 4)
            uptime_bytes = service_data[9:12] + b'\x00'  # Pad to 4 bytes
            uptime = struct.unpack('>I', uptime_bytes)[0]
            
            frame_type = 0x20  # Assume TLM
            
            print(f"   Manual decode attempt:")
            print(f"   Version: {version}")
            print(f"   Battery raw: {battery_voltage} (expected ~{53.92*33:.0f})")
            print(f"   Temp raw: {temperature_raw}")
            print(f"   Count: {adv_count} (expected 695)")
            print(f"   Uptime: {uptime}")
            
        elif len(service_data) >= 14:
            # Has frame type (need 14 bytes for full TLM: 1+1+2+2+4+4)
            frame_type, version, battery_voltage, temperature_raw, adv_count, uptime = struct.unpack('>BBHHII', service_data[:14])
        else:
            print(f"   ❌ Unexpected data length: {len(service_data)}")
            return None
            
        print(f"   Frame type: 0x{frame_type:02x}, Version: {version}")
        print(f"   Raw battery: {battery_voltage}, Raw temp: {temperature_raw}")
        print(f"   Adv count: {adv_count}, Uptime: {uptime}")
            
        # Decode temperature from 8.8 fixed point to Celsius
        temperature = struct.unpack('>h', struct.pack('>H', temperature_raw))[0] / 256.0
        
        # Decode humidity from battery voltage field (our custom encoding)
        humidity = battery_voltage / 33.0
        
        # Convert uptime from deciseconds to seconds
        uptime_seconds = uptime / 10.0
        
        result = {
            'frame_type': frame_type,
            'version': version,
            'temperature': temperature,
            'humidity': humidity,
            'adv_count': adv_count,
            'uptime_seconds': uptime_seconds
        }
        
        print(f"   ✅ Decoded: T={temperature:.2f}°C, H={humidity:.2f}%")
        return result
        
    except Exception as e:
        print(f"   ❌ Error decoding TLM frame: {e}")
        print(f"   Raw data: {service_data.hex()}")
        return None

async def scan_for_eddystone_tlm(target_name: str, duration: int):
    """Scans for Eddystone-TLM beacons and decodes sensor data."""
    print(f"Scanning for Eddystone-TLM beacons from '{target_name}' for {duration} seconds...")
    print("Press Ctrl+C to stop scanning early.\n")
    
    found_devices = {}
    last_seen = {}  # Track last time we saw each device to reduce spam
    
    def detection_callback(device, advertisement_data):
        """Callback function called for each advertisement packet."""
        try:
            device_name = device.name if device.name else "Unknown"
            
            # Debug: Print all devices we see with our target name
            if target_name.lower() in device_name.lower():
                
                # Rate limiting: only process each device every 2 seconds
                import time
                now = time.time()
                if device.address in last_seen and (now - last_seen[device.address]) < 2.0:
                    return
                last_seen[device.address] = now
                
                print(f"🔍 Found target device: {device_name} ({device.address})")
                print(f"   Service data: {advertisement_data.service_data}")
                print(f"   Service UUIDs: {advertisement_data.service_uuids}")
                
                # Look for Eddystone service data
                service_data = advertisement_data.service_data
                
                # Check for Eddystone service UUID (can be 16-bit 0xFEAA or full UUID)
                eddystone_data = None
                if service_data:
                    print(f"   Available service data keys: {list(service_data.keys())}")
                    # Try both 16-bit and full UUID formats
                    for uuid, data in service_data.items():
                        print(f"   Checking UUID: {uuid} -> {data.hex() if data else 'None'}")
                        # Check for Eddystone service UUID or the custom UUID we see
                        if "feaa" in str(uuid).lower() or "0020" in str(uuid).lower():
                            eddystone_data = data
                            print(f"   ✅ Found potential TLM data: {data.hex()}")
                            break
                
                if eddystone_data:
                    tlm_data = decode_eddystone_tlm(eddystone_data)
                    if tlm_data:
                        # Store or update device info
                        found_devices[device.address] = {
                            'name': device_name,
                            'address': device.address,
                            'rssi': advertisement_data.rssi,
                            'tlm_data': tlm_data
                        }
                        
                        print(f"📡 Device: {device_name} ({device.address})")
                        print(f"   RSSI: {advertisement_data.rssi} dBm")
                        print(f"   🌡️  Temperature: {tlm_data['temperature']:.2f} °C")
                        print(f"   💧 Humidity: {tlm_data['humidity']:.2f} %")
                        print(f"   📊 Advertisement Count: {tlm_data['adv_count']}")
                        print(f"   ⏱️  Uptime: {tlm_data['uptime_seconds']:.1f} seconds")
                        print(f"   🔋 Frame Version: {tlm_data['version']}")
                        print("-" * 50)
                    else:
                        print(f"   ❌ Failed to decode TLM data from: {eddystone_data.hex()}")
                else:
                    print(f"   ❌ No Eddystone service data found")
                print()
                    
        except Exception as e:
            print(f"Error processing advertisement: {e}")
    
    try:
        # Start scanning with callback using new API
        scanner = BleakScanner(detection_callback=detection_callback)
        
        await scanner.start()
        await asyncio.sleep(duration)
        await scanner.stop()
        
        print(f"\n--- Scan Complete ---")
        if found_devices:
            print(f"Found {len(found_devices)} Eddystone-TLM device(s):")
            for addr, info in found_devices.items():
                tlm = info['tlm_data']
                print(f"  {info['name']} ({addr}): T={tlm['temperature']:.2f}°C, H={tlm['humidity']:.2f}%")
        else:
            print("No Eddystone-TLM devices found.")
            print("Make sure:")
            print("  1. The nRF52840 device is powered on and running")
            print("  2. The device name matches the target name")
            print("  3. Bluetooth is enabled on this system")
        
        return found_devices
        
    except KeyboardInterrupt:
        print("\n🛑 Scan interrupted by user")
        await scanner.stop()
        return found_devices
    except Exception as e:
        print(f"An error occurred during scanning: {e}")
        print("Ensure Bluetooth is enabled and you have necessary permissions.")
        return {}

async def continuous_monitoring(target_name: str):
    """Continuously monitor for Eddystone-TLM beacons."""
    print(f"🔄 Starting continuous monitoring for '{target_name}'...")
    print("Press Ctrl+C to stop.\n")
    
    last_values = {}
    
    def detection_callback(device, advertisement_data):
        try:
            device_name = device.name if device.name else "Unknown"
            
            if target_name.lower() not in device_name.lower():
                return
                
            service_data = advertisement_data.service_data
            eddystone_data = None
            
            if service_data:
                for uuid, data in service_data.items():
                    if "feaa" in str(uuid).lower() or "0020" in str(uuid).lower():
                        eddystone_data = data
                        break
            
            if eddystone_data:
                tlm_data = decode_eddystone_tlm(eddystone_data)
                if tlm_data:
                    # Check if values changed significantly
                    key = device.address
                    if key not in last_values or \
                       abs(last_values[key].get('temperature', 0) - tlm_data['temperature']) > 0.1 or \
                       abs(last_values[key].get('humidity', 0) - tlm_data['humidity']) > 1.0:
                        
                        last_values[key] = tlm_data
                        
                        import datetime
                        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
                        print(f"[{timestamp}] {device_name}: T={tlm_data['temperature']:.2f}°C, H={tlm_data['humidity']:.2f}%, Count={tlm_data['adv_count']}")
                        
        except Exception as e:
            print(f"Error in continuous monitoring: {e}")
    
    try:
        scanner = BleakScanner(detection_callback=detection_callback)
        
        await scanner.start()
        # Run indefinitely until Ctrl+C
        while True:
            await asyncio.sleep(1)
            
    except KeyboardInterrupt:
        print("\n🛑 Monitoring stopped by user")
        await scanner.stop()
    except Exception as e:
        print(f"Error in continuous monitoring: {e}")
        await scanner.stop()

async def main():
    """Main function with options for different scanning modes."""
    print("🔍 Eddystone-TLM Scanner for nRF52840 Sensor")
    print("=" * 50)
    
    while True:
        print("\nSelect scanning mode:")
        print("1. Single scan (30 seconds)")
        print("2. Continuous monitoring")
        print("3. Exit")
        
        try:
            choice = input("\nEnter choice (1-3): ").strip()
            
            if choice == "1":
                await scan_for_eddystone_tlm(TARGET_DEVICE_NAME, SCAN_DURATION)
            elif choice == "2":
                await continuous_monitoring(TARGET_DEVICE_NAME)
            elif choice == "3":
                print("👋 Goodbye!")
                break
            else:
                print("❌ Invalid choice. Please enter 1, 2, or 3.")
                
        except KeyboardInterrupt:
            print("\n👋 Goodbye!")
            break
        except Exception as e:
            print(f"❌ Error: {e}")

if __name__ == "__main__":
    asyncio.run(main())
