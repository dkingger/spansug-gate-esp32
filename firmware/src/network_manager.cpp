#include "network_manager.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "secrets.h"

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  lastAttempt_ = millis();
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void NetworkManager::maintain() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeConfigured_) {
      setenv("TZ", TIME_ZONE, 1);
      tzset();
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      timeConfigured_ = true;
      Serial.println("NTP time synchronization started");
    }
    if (!mdnsStarted_ && MDNS.begin("currentsensor")) {
      mdnsStarted_ = true;
      Serial.println("mDNS started: currentsensor.local");
    }
    return;
  }

  const unsigned long now = millis();
  if (now - lastAttempt_ < WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastAttempt_ = now;
  mdnsStarted_ = false;
  timeConfigured_ = false;
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool NetworkManager::connected() const {
  return WiFi.status() == WL_CONNECTED;
}