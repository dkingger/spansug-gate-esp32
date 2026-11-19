#pragma once

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>

class MachineState;
class RebootScheduler;

class WebServerManager {
public:
  WebServerManager();

  void begin(MachineState& machineState, float& currentAmps,
             RebootScheduler& rebootScheduler);
  void maintain();
  float loadBaseline() const;

private:
  AsyncWebServer server_;
  Preferences preferences_;
  MachineState* machineState_ = nullptr;
  RebootScheduler* rebootScheduler_ = nullptr;
  float* currentAmps_ = nullptr;
  bool started_ = false;
  bool restartPending_ = false;
  unsigned long restartAt_ = 0;
};