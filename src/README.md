# Spansug Gate Current Sensor

ESP32-C3 firmware for measuring machine current with a ZMCT103C current transformer, detecting whether the machine is active, and publishing telemetry over MQTT.

## Features

- AC RMS current measurement on ADC GPIO 4
- Debounced machine state detection
- Configurable baseline current with 50 mA hysteresis
- Baseline stored in ESP32 flash using `Preferences`
- Non-blocking Wi-Fi and MQTT reconnects
- Automatic reboot every Sunday at 03:00 local time
- MQTT status, current, and heartbeat telemetry
- Calibration web interface available through mDNS

## Hardware

- ESP32-C3 DevKitM-1
- ZMCT103C current transformer module
- The sensor module is powered from 5 V
- Sensor output is connected to GPIO 4 through the configured voltage divider
- Connect the sensor and ESP32 grounds together

The ADC configuration assumes a 12-bit ADC and a 3.3 V reference. The calibration values are in `src/config.h`.

## Configuration

Copy or edit `src/secrets.h` with the Wi-Fi credentials for the target network:

```cpp
#pragma once

#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
```

Configure the MQTT broker and topics in `src/config.h`:

- Broker: `spansug-backend.local`
- Port: `1883`
- Status: `spansug/gate/rondelsliber/machine_active`
- Current: `spansug/gate/rondelsliber/current`
- Heartbeat: `spansug/gate/rondelsliber/heartbeat`

Do not commit real Wi-Fi credentials or other secrets.

## Build and Upload

Install PlatformIO, connect the ESP32-C3, and run:

```sh
pio run --environment esp32-c3-devkitm-1
pio run --target upload --environment esp32-c3-devkitm-1 --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

The serial monitor runs at `115200` baud. The upload port depends on the connected board.

## Web Calibration Interface

After the board connects to Wi-Fi, open either:

- `http://currentsensor.local/`
- `http://<esp32-ip-address>/`

The page displays the cached current reading and machine state. Set the baseline current in amperes and save it. The value is stored in flash and restored after reboot.

The machine state thresholds are:

- ON: baseline + 50 mA
- OFF: baseline - 50 mA
- State must remain stable for 2 seconds

The web API is also available directly:

```text
GET /data
GET /setbaseline?value=0.100
POST /restart
```

The restart endpoint reboots the ESP32 after sending its response. The web page exposes it as the `Restart device` button.

Web requests use the latest reading from the main loop; they do not trigger an additional blocking ADC measurement.

## Automatic Reboot

The ESP32 synchronizes its clock with NTP after connecting to Wi-Fi and uses Danish local time (`CET-1CEST,M3.5.0/2,M10.5.0/3`), including daylight-saving changes. It automatically reboots once every Sunday at 03:00.

The last synchronized boot time is stored in flash and shown on the web page as `Last reboot`. The page also shows the next scheduled reboot. If the device has not synchronized time yet, the page displays `Waiting for time sync...`.

## Source Layout

- `src/main.cpp` coordinates timed sensor, network, MQTT, and web-server work
- `src/current_sensor.*` configures the ADC and calculates RMS current
- `src/machine_state.*` applies baseline, hysteresis, and debounce logic
- `src/network_manager.*` manages Wi-Fi and mDNS
- `src/mqtt_manager.*` manages MQTT connection and publishing
- `src/web_server.*` serves the calibration UI and persistence API
- `src/config.h` contains hardware, timing, calibration, and MQTT settings
- `src/secrets.h` contains local Wi-Fi credentials

## Validation

Build the target with:

```sh
pio run --environment esp32-c3-devkitm-1
```
