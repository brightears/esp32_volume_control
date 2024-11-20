# ESP32 Noise Sensor

An ESP32-based noise sensing system that automatically adjusts music volume based on ambient noise levels using the Soundtrack Your Brand API.

## Features

- Always-accessible web configuration interface
- No password required for device access
- Automatic volume adjustment based on ambient noise
- Real-time sound level monitoring
- Configurable sensitivity
- Persistent settings storage
- Dual WiFi mode (AP + Station)

## Hardware Requirements

- ESP32 Development Board
- Sound Sensor Module (Analog)
- Power Supply
- Connection to Pin 36 for sound sensor

## Software Setup

1. Install PlatformIO
2. Clone this repository
3. Connect ESP32 to your computer
4. Run these commands:
   ```bash
   # Build project
   pio run
   
   # Upload filesystem
   pio run -t uploadfs
   
   # Upload firmware
   pio run -t upload
   ```

## Initial Configuration

1. Connect to the "Noise_Sensor" WiFi network
2. Open web browser and navigate to http://192.168.4.1
3. Configure:
   - Your WiFi credentials
   - Soundtrack Your Brand API key
   - Sensitivity level

## Usage

The device will:
1. Create an open WiFi network named "Noise_Sensor"
2. Allow configuration through web interface
3. Connect to configured WiFi network
4. Monitor ambient noise levels
5. Adjust music volume automatically

## File Structure

```
ESP32_Volume_Control/
├── src/                    # Source files
│   ├── main.cpp           # Main application code
│   ├── api_client.cpp     # API client implementation
│   ├── sound_sensor.cpp   # Sound sensor handling
│   ├── wifi_manager.cpp   # WiFi management
│   └── captive_portal.cpp # Captive portal implementation
├── include/               # Header files
├── data/                  # Web interface files
│   ├── index.html
│   ├── styles.css
│   └── script.js
├── platformio.ini         # Project configuration
└── partitions_custom.csv  # Partition table
```

## Troubleshooting

1. If device not accessible:
   - Ensure you're connected to "Noise_Sensor" network
   - Try accessing 192.168.4.1 directly

2. If volume not adjusting:
   - Verify API key is correct
   - Check WiFi connection status
   - Adjust sensitivity settings

3. If settings not saving:
   - Ensure proper power supply
   - Check for error messages in web interface

## License

MIT License