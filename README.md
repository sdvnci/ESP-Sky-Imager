# SkyDeVision Sky Imaging Station
[![Compile](https://github.com/sudoDeVinci/ESP-Sky-Imager/actions/workflows/compile.yml/badge.svg?branch=revamp)](https://github.com/sudoDeVinci/ESP-Sky-Imager/actions/workflows/compile.yml)
![Doxygen](/images/doxygen_badge.svg)

The ESP32S3-based low-cost sky imaging station.

![Prototype](/images/station_prototype_modern.jpg)

Wirelessly sent environmental readings and sky images enable cloud separation and weather prediction.

![Cloud detection](/images/cloud_detection_result01.png)

# Features

## WLAN Configurability

Multiple WLAN access points' details can be stored in the `networks.json` configurations file. This is a JSON file with the template:

```json
{
    "networks": [
        {
            "SSID": "Your network name",
            "PASS": "Your network password"
        },
        {
            "SSID": "Another network name",
            "PASS": "Another network password"
        }
    ]
}
```

During setup, the station scans the area for access points, and will attempt to connect to access points with details matching those listed, starting from the top. You can see this in `wifiSetup` in [comm.cpp](/comm.cpp).

## Value Caching To reduce cost and traffic

The system implements multi-level caching to optimize performance and reduce network usage:

- NTP time synchronization data is cached for 6 hours
- QNH (atmospheric pressure) values from external APIs are cached for 2 hours
- When offline, readings and images are stored locally in structured log files
- All cached data is automatically synchronized when connectivity is restored

## File system agnostic usage

The station is designed to work with different storage options:

- Primarily uses SD_MMC (SD card) for storage when available
- Falls back to internal LittleFS if SD card is not detected
- Maintains consistent file operations across both file systems
- Automatically creates and manages required directories and files

## Graceful Failure

The system includes robust error handling mechanisms:

- Monitors all sensors and connectivity components
- Continues operation with limited functionality when components fail
- Implements timeouts for external communications
- Logs errors and retries operations with exponential backoff
- Automatically recovers from most error conditions without manual intervention

## Offline/Online modes

The station seamlessly adapts to changing network conditions:

- In online mode: transmits data in real-time to configured server endpoints
- In offline mode: stores all readings and images locally on the SD card
- Automatically syncs cached data when connectivity is restored
- Maintains accurate timekeeping during extended offline periods
- Optimizes power usage based on network availability

## Power Management

- Implements deep sleep between readings to conserve battery
- Configurable sleep durations with different day/night schedules
- Wakes only during daylight hours for meaningful sky images
- Dynamically adjusts power states based on sensor requirements

## *COMING SOON*: Configurations Portal
- Web-based configuration interface
- Real-time sensor monitoring dashboard
- Remote firmware updates
- More advanced scheduling options

## Hardware Requirements

- Compatible ESP32-S3 with OV5640 camera
- SHT31 temperature and humidity sensor (I2C address 0x44)
- BMP390 barometric pressure sensor (I2C address 0x77)
- SD card module (recommended 16GB+)
- Optional SSD1306 display for status information

## Sensor Wiring

The SHT31 and BMP390 share I2C bus 0:

| Signal | GPIO |
|---|---|
| SDA | 41 |
| SCL | 42 |

The pins are set in `sensorpins` in [SensorContainer.hpp](/SensorContainer.hpp). The camera's SCCB bus is separate (see [camera_pins.h](/camera_pins.h)).

The sensors are driven by [ESP-Environment-Drivers](https://github.com/sdvnci/ESP-Environment-Drivers), which serialises I2C traffic per bus so the sensors can be read safely from more than one FreeRTOS task. Each reading averages 20 samples per sensor with outliers removed. Temperature comes from the SHT31, falling back to the BMP390 if the SHT31 is down. Pressure is sent in hPa, and altitude is computed from the server's QNH.

## Building

Install the dependencies, then build for an ESP32-S3 board on the [arduino-esp32](https://github.com/espressif/arduino-esp32) core:

- [ArduinoJson](https://arduinojson.org/) from the Library Manager.
- [ESP-Environment-Drivers](https://github.com/sdvnci/ESP-Environment-Drivers) v1.0.0. Download the `v1.0.0` release as a ZIP and use *Sketch → Include Library → Add .ZIP Library*, or run:

```bash
arduino-cli lib install --git-url https://github.com/sdvnci/ESP-Environment-Drivers.git#v1.0.0
```

CI installs the same pinned version (see [compile.yml](/.github/workflows/compile.yml)).

## Setup Instructions

1. Connect hardware components according to the pin definitions in [camera_pins.h](/camera_pins.h) and the sensor wiring above
2. Create a `networks.json` file with your WiFi credentials
3. Flash the firmware to your ESP32-S3 device
4. Place the device in a weather-protected enclosure with the camera facing the sky
5. The system will automatically connect, calibrate, and begin operation