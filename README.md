# SkyDeVision Sky Imaging Station
[![Compile](https://github.com/sdvnci/ESP-Sky-Imager/actions/workflows/compile.yml/badge.svg?branch=revamp)](https://github.com/sdvnci/ESP-Sky-Imager/actions/workflows/compile.yml)
[![Unit Tests](https://github.com/sdvnci/ESP-Sky-Imager/actions/workflows/testing.yml/badge.svg?branch=revamp)](https://github.com/sdvnci/ESP-Sky-Imager/actions/workflows/testing.yml)
![Doxygen](/images/doxygen_badge.svg)

The ESP32S3-based low-cost sky imaging station.

![Prototype](/images/station_prototype_modern.jpg)

Wirelessly sent environmental readings and sky images enable cloud separation and weather prediction.

![Cloud detection](/images/cloud_detection_result01.png)

# Features

## Configuration Files

The station reads its settings from the root of the SD card (or LittleFS if there's no card).

`networks.json` lists the WLAN access points it may join. At boot it scans for networks and connects to the first match, in the order listed:

```json
{
    "networks": [
        { "ssid": "Your network name", "pass": "Your network password" },
        { "ssid": "Another network name", "pass": "Another network password" }
    ]
}
```

`server.json` holds the server's base URL, including the scheme and port. Requests go to `<host>/api/...`:

```json
{ "host": "http://192.168.1.10:5000", "certificate": "" }
```

`certificate` is read but not used yet, so use plain HTTP for now.

Schedule, timezone and cache lifetimes are set in [StationConfig.hpp](/StationConfig.hpp).

## Wake Cycle

The station spends almost all its time in deep sleep. Each wake:

1. If the clock is set and it's outside operating hours (06:00–21:00 local by default), it goes straight back to sleep until the morning, without powering up WiFi or the sensors.
2. Connects to WiFi and syncs the clock over NTP when it has never been set or the last sync is over 6 hours old. The RTC keeps time through deep sleep in between.
3. Uses the cached QNH if it's under 2 hours old, otherwise fetches a new one from the server, falling back to the last known value when offline.
4. Takes a reading and sends it, along with the sensor statuses.
5. If the reading couldn't be delivered, appends it to the offline log. If it was delivered, uploads the backlog of logged readings, up to 50 per wake.
6. Sleeps for 20 minutes, or until morning.

Timestamps are UTC. The local timezone (`AST4`, Saint Lucia) is only used for the schedule.

## Offline Log

Undelivered readings go to `readings.jsonl`, one JSON reading per line. Appending doesn't read the log into memory, so it can grow without risking the heap.

- Readings are uploaded oldest first, with the time they were taken, not the time they were sent.
- A reading leaves the log only once the server accepts it, or definitively refuses it (400, 409 or 422). Any other failure, such as a timeout, a 5xx or a 404 from a wrong URL, keeps it and the rest of the backlog for the next wake.
- The log is rewritten via a temporary file. If power is lost mid-rewrite, the next wake finishes the swap, so nothing is lost or sent twice.
- A reading taken before the clock has ever been set is discarded, since it can't be placed in time.

## Cache

`cache.json` keeps the last QNH and the time of the last NTP sync, so the station doesn't hit the network on every wake. Times are stored as Unix seconds.

## File System Agnostic Usage

- Uses the SD card (SD_MMC) when one is present.
- Falls back to the internal LittleFS flash otherwise.

## Graceful Failure

- Each sensor is initialised independently, and its status is reported to the server. A failed SHT31 falls back to the BMP390 for temperature.
- Every network wait has a timeout: WiFi connection, NTP sync, and HTTP connect and read.
- Without WiFi or a server, readings are kept in the offline log.

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

## Testing

[`tests/`](/tests) runs on a PC with [EpoxyDuino](https://github.com/bxparks/EpoxyDuino) and [AUnit](https://github.com/bxparks/AUnit). It covers the offline log, the cache, the schedule, reading serialisation, and the sensor path, using an in-memory stand-in for the ESP32 file system ([`tests/stubs/FS.h`](/tests/stubs/FS.h)) and the drivers' simulated I2C bus. The firmware build ignores the folder, since Arduino only compiles the sketch root and `src/`.

```bash
mkdir -p ../deps && cd ../deps
git clone --depth 1 --branch v1.6.0 https://github.com/bxparks/EpoxyDuino.git
git clone --depth 1 --branch v1.7.1 https://github.com/bxparks/AUnit.git
git clone --depth 1 --branch v7.4.2 https://github.com/bblanchon/ArduinoJson.git
git clone --depth 1 --branch v1.0.0 https://github.com/sdvnci/ESP-Environment-Drivers.git
cd ../ESP-Sky-Imager/tests
make EPOXY_DUINO_DIR=../../deps/EpoxyDuino AUNIT_DIR=../../deps/AUnit \
     ARDUINOJSON_DIR=../../deps/ArduinoJson DRIVERS_DIR=../../deps/ESP-Environment-Drivers
./tests.out
```

## Setup Instructions

1. Connect hardware components according to the pin definitions in [camera_pins.h](/camera_pins.h) and the sensor wiring above
2. Create a `networks.json` file with your WiFi credentials
3. Flash the firmware to your ESP32-S3 device
4. Place the device in a weather-protected enclosure with the camera facing the sky
5. The system will automatically connect, calibrate, and begin operation