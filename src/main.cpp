#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "current_sensor.h"
#include "machine_state.h"
#include "mqtt_manager.h"
#include "network_manager.h"
#include "reboot_scheduler.h"
#include "web_server.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
CurrentSensor currentSensor(CURRENT_SENSOR_PIN);
MachineState machineState;
NetworkManager network;
RebootScheduler rebootScheduler;
MqttManager mqtt(mqttClient);
WebServerManager webServer;

unsigned long lastCurrentRead = 0;
unsigned long lastHeartbeat = 0;
float currentAmps = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting current sensor...");

  randomSeed(micros());
  currentSensor.begin();
  machineState.begin(webServer.loadBaseline());
  rebootScheduler.begin();
  network.begin();
  mqtt.begin();
  webServer.begin(machineState, currentAmps, rebootScheduler);
}

void loop() {
  const unsigned long now = millis();
  webServer.maintain();
  network.maintain();
  rebootScheduler.maintain();

  if (!network.connected()) {
    delay(10);
    return;
  }

  mqtt.maintain();
  if (!mqttClient.connected()) {
    delay(10);
    return;
  }

  if (now - lastCurrentRead >= CURRENT_INTERVAL_MS) {
    lastCurrentRead = now;
    currentAmps = currentSensor.readAmps();
    mqtt.publishCurrent(currentAmps);
    if (machineState.update(currentAmps, now)) {
      mqtt.publishMachineState(machineState.isOn());
    }
  }

  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = now;
    mqtt.publishHeartbeat();
  }
}