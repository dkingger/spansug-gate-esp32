#include "web_server.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"
#include "machine_state.h"
#include "reboot_scheduler.h"

namespace {
const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Current Sensor</title>
  <style>
    :root { color-scheme: light; font-family: Georgia, serif; }
    body { margin: 0; min-height: 100vh; background: #e9eef2; color: #17212b; }
    main { max-width: 34rem; margin: 0 auto; padding: 2rem 1rem; }
    section { background: #fff; border: 1px solid #cbd5dc; padding: 1.5rem; }
    h1 { margin-top: 0; font-size: 1.8rem; }
    .reading { font: 3.5rem Georgia, serif; color: #006d77; margin: 1.5rem 0; }
    .status { display: inline-block; padding: .5rem 1rem; color: #fff; background: #64748b; }
    .status.on { background: #0f766e; }
    label { display: block; margin: 1.5rem 0 .4rem; }
    input, button { box-sizing: border-box; font: inherit; padding: .7rem; }
    input { width: 100%; border: 1px solid #9aa8b2; }
    button { margin-top: .75rem; border: 0; color: #fff; background: #006d77; cursor: pointer; }
    button.restart { background: #b42318; }
    #message { min-height: 1.4rem; margin-top: 1rem; }
    small { color: #52616b; }
  </style>
</head>
<body>
  <main>
    <section>
      <h1>ZMCT103C Current Sensor</h1>
      <small>Live current reading</small>
      <div class="reading"><span id="current">0.000</span> A</div>
      <div id="status" class="status">OFF</div>
      <label for="baseline">Current baseline (A)</label>
      <input id="baseline" type="number" min="0" max="5" step="0.001">
      <button type="button" onclick="saveBaseline()">Save baseline</button>
      <button class="restart" type="button" onclick="restartDevice()">Restart device</button>
      <div id="message" role="status"></div>
      <small>Machine turns on at baseline + 50 mA and off at baseline - 50 mA.</small>
      <p><small>Last reboot: <span id="lastReboot">Waiting for time sync...</span></small></p>
      <p><small>Next scheduled reboot: <span id="nextReboot">Waiting for time sync...</span></small></p>
      <p><small>Firmware <span id="version">...</span></small></p>
    </section>
  </main>
  <script>
    async function updateValues() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        document.getElementById('current').textContent = data.current.toFixed(3);
        document.getElementById('baseline').value = data.baseline.toFixed(3);
        document.getElementById('lastReboot').textContent = data.lastReboot;
        document.getElementById('nextReboot').textContent = data.nextReboot;
        document.getElementById('version').textContent = data.version;
        const status = document.getElementById('status');
        status.textContent = data.machineOn ? 'ON' : 'OFF';
        status.className = 'status ' + (data.machineOn ? 'on' : '');
      } catch (error) {
        document.getElementById('message').textContent = 'Unable to reach sensor';
      }
    }
    async function saveBaseline() {
      const value = document.getElementById('baseline').value;
      const response = await fetch('/setbaseline?value=' + encodeURIComponent(value));
      document.getElementById('message').textContent = response.ok
        ? 'Baseline saved'
        : await response.text();
      updateValues();
    }
    async function restartDevice() {
      if (!confirm('Restart the ESP32 now?')) return;
      document.getElementById('message').textContent = 'Restarting device...';
      await fetch('/restart', { method: 'POST' });
    }
    updateValues();
    setInterval(updateValues, 1000);
  </script>
</body>
</html>
)rawliteral";
}

WebServerManager::WebServerManager() : server_(80) {}

namespace {
String formatTimestamp(time_t timestamp) {
  if (timestamp < 1700000000) {
    return "Waiting for time sync...";
  }

  struct tm localTime;
  localtime_r(&timestamp, &localTime);
  char formatted[24];
  strftime(formatted, sizeof(formatted), "%Y-%m-%d %H:%M:%S", &localTime);
  return String(formatted);
}
}

void WebServerManager::maintain() {
  if (restartPending_ && millis() >= restartAt_) {
    ESP.restart();
  }
}

float WebServerManager::loadBaseline() const {
  Preferences preferences;
  if (!preferences.begin("current-sensor", true)) {
    return DEFAULT_BASELINE_CURRENT;
  }

  const float baseline = preferences.getFloat("baseline", DEFAULT_BASELINE_CURRENT);
  preferences.end();
  return isfinite(baseline) ? constrain(baseline, 0.0f, MAX_BASELINE_CURRENT)
                            : DEFAULT_BASELINE_CURRENT;
}

void WebServerManager::begin(MachineState& machineState, float& currentAmps,
                             RebootScheduler& rebootScheduler) {
  if (started_) {
    return;
  }

  machineState_ = &machineState;
  currentAmps_ = &currentAmps;
  rebootScheduler_ = &rebootScheduler;

  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server_.on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json = "{\"current\":" + String(*currentAmps_, 3)
        + ",\"baseline\":" + String(machineState_->baseline(), 3)
        + ",\"machineOn\":" + String(machineState_->isOn() ? "true" : "false")
        + ",\"lastReboot\":\"" + formatTimestamp(rebootScheduler_->lastReboot()) + "\""
        + ",\"nextReboot\":\"" + formatTimestamp(rebootScheduler_->nextReboot()) + "\""
        + ",\"version\":\"" + FIRMWARE_VERSION + "\"}";
    request->send(200, "application/json", json);
  });

  server_.on("/setbaseline", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Missing value parameter");
      return;
    }

    const float baseline = request->getParam("value")->value().toFloat();
    if (!isfinite(baseline) || baseline < 0.0f || baseline > MAX_BASELINE_CURRENT) {
      request->send(400, "text/plain", "Baseline must be between 0 and 5 A");
      return;
    }

    machineState_->setBaseline(baseline);
    preferences_.begin("current-sensor", false);
    preferences_.putFloat("baseline", baseline);
    preferences_.end();
    request->send(200, "text/plain", "OK");
  });

  server_.on("/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
    restartPending_ = true;
    restartAt_ = millis() + 250;
    request->send(200, "text/plain", "Restarting");
  });

  server_.begin();
  started_ = true;
  Serial.println("Web server started");
}