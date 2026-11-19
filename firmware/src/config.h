#pragma once

#include <stdint.h>

// Current transformer and ADC calibration.
static constexpr float VOLTAGE_REF = 3.3f;
static constexpr int ADC_RESOLUTION = 4096;
static constexpr int CURRENT_SENSOR_PIN = 4;
static constexpr int SAMPLES = 1000;
static constexpr float VOLTAGE_COMPENSATION = 1.0f / 0.667f;
static constexpr float CALIBRATION_FACTOR = 4.35f;
static constexpr float DEFAULT_BASELINE_CURRENT = 0.1f;
static constexpr float STATE_HYSTERESIS = 0.05f;
static constexpr float MAX_BASELINE_CURRENT = 5.0f;
static constexpr char FIRMWARE_VERSION[] = "1.0.0";
static constexpr char TIME_ZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// MQTT Broker settings
static constexpr char MQTT_BROKER[] = "spansug-backend.local";
static constexpr uint16_t MQTT_PORT = 1883;
static constexpr char MQTT_TOPIC_STATUS[] = "spansug/gate/rondelsliber/machine_active";
static constexpr char MQTT_TOPIC_CURRENT[] = "spansug/gate/rondelsliber/current";
static constexpr char MQTT_TOPIC_HEARTBEAT[] = "spansug/gate/rondelsliber/heartbeat";


// Machine state detection
static constexpr uint32_t STATE_DEBOUNCE_MS = 2000;
static constexpr uint32_t CURRENT_INTERVAL_MS = 1000;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
static constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;
static constexpr int WEEKLY_REBOOT_HOUR = 3;