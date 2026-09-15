#include "reboot_scheduler.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {
const time_t MIN_VALID_EPOCH = 1700000000;

bool getLocalTimeInfo(struct tm& localTime) {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_EPOCH) {
    return false;
  }

  localtime_r(&now, &localTime);
  return true;
}

uint32_t dayKey(const struct tm& localTime) {
  return static_cast<uint32_t>(localTime.tm_year * 400 + localTime.tm_yday);
}
}

void RebootScheduler::begin() {
  Preferences preferences;
  if (preferences.begin("system", true)) {
    lastReboot_ = preferences.getULong64("last-reboot", 0);
    lastScheduledDay_ = preferences.getUInt("scheduled-day", 0);
    preferences.end();
  }
}

void RebootScheduler::maintain() {
  struct tm localTime;
  if (!getLocalTimeInfo(localTime)) {
    return;
  }

  const time_t now = time(nullptr);
  if (!bootRecorded_) {
    lastReboot_ = now;
    bootRecorded_ = true;

    Preferences preferences;
    if (preferences.begin("system", false)) {
      preferences.putULong64("last-reboot", static_cast<uint64_t>(lastReboot_));
      preferences.end();
    }
    Serial.println("System boot time recorded");
  }

  if (localTime.tm_wday != 0 || localTime.tm_hour < WEEKLY_REBOOT_HOUR) {
    return;
  }

  const uint32_t today = dayKey(localTime);
  if (lastScheduledDay_ == today) {
    return;
  }

  lastScheduledDay_ = today;
  Preferences preferences;
  if (preferences.begin("system", false)) {
    preferences.putUInt("scheduled-day", lastScheduledDay_);
    preferences.end();
  }

  Serial.println("Weekly Sunday reboot scheduled now");
  delay(100);
  ESP.restart();
}

time_t RebootScheduler::lastReboot() const {
  return lastReboot_;
}

time_t RebootScheduler::nextReboot() const {
  struct tm localTime;
  if (!getLocalTimeInfo(localTime)) {
    return 0;
  }

  const time_t now = time(nullptr);
  int daysUntilSunday = (7 - localTime.tm_wday) % 7;
  if (daysUntilSunday == 0 && localTime.tm_hour >= WEEKLY_REBOOT_HOUR) {
    daysUntilSunday = 7;
  }

  localTime.tm_mday += daysUntilSunday;
  localTime.tm_hour = WEEKLY_REBOOT_HOUR;
  localTime.tm_min = 0;
  localTime.tm_sec = 0;
  localTime.tm_isdst = -1;

  const time_t scheduled = mktime(&localTime);
  return scheduled > now ? scheduled : 0;
}