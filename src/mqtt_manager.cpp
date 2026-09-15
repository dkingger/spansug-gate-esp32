#include "mqtt_manager.h"

#include <Arduino.h>

#include "config.h"

MqttManager::MqttManager(PubSubClient& client) : client_(client) {}

void MqttManager::begin() {
  client_.setServer(MQTT_BROKER, MQTT_PORT);
}

bool MqttManager::maintain() {
  if (client_.connected()) {
    client_.loop();
    return false;
  }

  const unsigned long now = millis();
  if (now - lastAttempt_ < MQTT_RETRY_INTERVAL_MS) {
    return false;
  }

  lastAttempt_ = now;
  String clientId = "ESP32Current-" + String(random(0xffff), HEX);
  Serial.println("Attempting MQTT connection...");
  if (!client_.connect(clientId.c_str())) {
    Serial.print("MQTT failed, rc=");
    Serial.println(client_.state());
    return false;
  }

  Serial.println("MQTT connected");
  return true;
}

bool MqttManager::publishCurrent(float currentAmps) {
  char payload[16];
  snprintf(payload, sizeof(payload), "%.3f", currentAmps);
  return client_.publish(MQTT_TOPIC_CURRENT, payload, true);
}

bool MqttManager::publishHeartbeat() {
  return client_.publish(MQTT_TOPIC_HEARTBEAT, "1", true);
}

bool MqttManager::publishMachineState(bool isOn) {
  return client_.publish(MQTT_TOPIC_STATUS, isOn ? "1" : "0", true);
}