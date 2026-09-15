#pragma once

#include <PubSubClient.h>

class MqttManager {
public:
  explicit MqttManager(PubSubClient& client);

  void begin();
  bool maintain();
  bool publishCurrent(float currentAmps);
  bool publishHeartbeat();
  bool publishMachineState(bool isOn);

private:
  PubSubClient& client_;
  unsigned long lastAttempt_ = 0;
};