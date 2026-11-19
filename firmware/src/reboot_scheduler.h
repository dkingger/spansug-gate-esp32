#pragma once

#include <time.h>

class RebootScheduler {
public:
  void begin();
  void maintain();
  time_t lastReboot() const;
  time_t nextReboot() const;

private:
  time_t lastReboot_ = 0;
  uint32_t lastScheduledDay_ = 0;
  bool bootRecorded_ = false;
};