#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

// Firmware version
const char* FIRMWARE_VERSION = "v1.2.3"; // Enhanced OTA stability (WiFi sleep disabled, explicit port, server.end())

// WiFi credentials
const char* ssid = "newdahl";
const char* password = "12345678";
const char* ota_password = "fablabvejle!";

// MQTT Broker settings
const char* mqtt_broker = "spansug-backend.local";
const int mqtt_port = 1883;
const char* mqtt_topic_status = "spansug/gate/rondelsliber/machine_active";
const char* mqtt_topic_current = "spansug/gate/rondelsliber/current";
const char* mqtt_topic_heartbeat = "spansug/gate/rondelsliber/heartbeat";

// Pin definitions
#define CURRENT_SENSOR_PIN 4  // Analog pin for ZMCT103C Signal Out

// ZMCT103C Current Transformer Module specifications
// - Current Ratio: 5A:5mA (Transformer Turns Ratio 1000:1)
// - Rated Primary Current: 5A at 50/60Hz
// - Module includes onboard sampling resistor and precision op-amp circuit
// - Onboard potentiometer for amplification adjustment
// - Module requires 5V power supply (VCC)
// - Connect both Ground pins (G) together to MCU ground
//
// Voltage Divider Protection (5V → 3.3V):
// - R1 = 10kΩ (between sensor output and ESP32 GPIO)
// - R2 = 20kΩ (between ESP32 GPIO and GND)
// - Voltage division ratio: Vout = Vin × (R2/(R1+R2)) = Vin × 0.667
const float VOLTAGE_REF = 3.3;
const int ADC_RESOLUTION = 4096;
const int SAMPLES = 1000;  // Number of samples for RMS calculation
const float MAX_CURRENT = 5.0;  // Maximum measurable current (5A)
const float VOLTAGE_DIVIDER_RATIO = 0.667;  // R2/(R1+R2) = 20k/(10k+20k)
const float VOLTAGE_COMPENSATION = 1.0 / VOLTAGE_DIVIDER_RATIO;  // 1.5x to compensate
float calibrationFactor = 4.35;  // Calibrated with 780W drill (3.39A expected)

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);
Preferences preferences;

unsigned long lastReadTime = 0;
const unsigned long readInterval = 300; // Read every 300ms for faster response
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 5000; // Publish status every 5 seconds

// Reduce SAMPLES and delay for faster sampling - prevents loop blocking
#define SAMPLES 500  // Reduce from 1000 to 500 for faster sampling (still accurate)

// Machine state detection
float baselineCurrent = 0.1; // Default baseline in Amperes
bool machineIsOn = false;
float hysteresis = 0.1; // Prevent flickering (100mA)
int zeroOffset = 2048; // ADC zero point (will be calibrated)

// Debounce logic to prevent rapid on/off switching
unsigned long stateChangeTime = 0;
bool pendingState = false;
bool pendingStateValue = false;
const unsigned long DEBOUNCE_DELAY = 2000; // 2 seconds - state must be stable

void setupOTA() {
  ArduinoOTA.setHostname("currentsensor");
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.setPort(3232);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("OTA update started: " + type);
    // Disconnect MQTT and stop web server to allow clean OTA
    client.disconnect();
    server.end();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA update finished - rebooting");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready: currentsensor.local");
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Disable WiFi sleep for OTA stability
    WiFi.setSleep(false);
    
    // Start mDNS
    if (MDNS.begin("currentsensor")) {
      Serial.println("mDNS started: currentsensor.local");
      MDNS.addService("arduino", "tcp", 3232); // Advertise OTA service
    } else {
      Serial.println("Error setting up mDNS!");
    }

    setupOTA();
  } else {
    Serial.println("\nWiFi connection FAILED!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
  }
}

unsigned long lastMQTTAttempt = 0;
const unsigned long mqttRetryInterval = 5000; // Try reconnect every 5 seconds

void reconnectMQTT() {
  // Non-blocking reconnect - only try once per call
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastMQTTAttempt >= mqttRetryInterval) {
      lastMQTTAttempt = now;
      Serial.println("Attempting MQTT connection...");
      String clientId = "ESP32Current-" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {
        Serial.println("MQTT connected");
        String status = machineIsOn ? "1" : "0";
        client.publish(mqtt_topic_status, status.c_str(), true);
      } else {
        Serial.print("Failed, rc=");
        Serial.println(client.state());
      }
    }
  }
}

float readCurrent() {
  // Read samples and calculate AC RMS current
  long sumSquared = 0;
  int minVal = 4095;
  int maxVal = 0;
  long sumValues = 0;
  
  // Take samples
  for (int i = 0; i < SAMPLES; i++) {
    int val = analogRead(CURRENT_SENSOR_PIN);
    sumValues += val;
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
  }
  
  // Calculate average (DC offset/zero point)
  int avgValue = sumValues / SAMPLES;
  
  // If signal is very stable, it's likely just noise
  int peakToPeak = maxVal - minVal;
  if (peakToPeak < 10) {
    return 0.0; // No meaningful AC signal
  }
  
  // Now calculate RMS with corrected zero
  sumSquared = 0;
  for (int i = 0; i < SAMPLES; i++) {
    int val = analogRead(CURRENT_SENSOR_PIN);
    int centered = val - avgValue;
    sumSquared += (long)centered * centered;
    // Removed delayMicroseconds - was blocking MQTT keep-alive
  }
  
  float rms = sqrt((float)sumSquared / SAMPLES);
  
  // Convert to voltage (ADC reading to actual voltage)
  float voltage = (rms / (float)ADC_RESOLUTION) * VOLTAGE_REF;
  
  // Compensate for voltage divider
  voltage = voltage * VOLTAGE_COMPENSATION;
  
  // Convert to current (empirical - adjust calibrationFactor)
  float current = voltage * calibrationFactor;
  
  return current;
}

void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Current Sensor Calibration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 100 100'><defs><linearGradient id='lg' x1='0%' y1='0%' x2='100%' y2='100%'><stop offset='0%' style='stop-color:%23ff6600;stop-opacity:1'/><stop offset='50%' style='stop-color:%23ecff00;stop-opacity:1'/><stop offset='100%' style='stop-color:%23ffb200;stop-opacity:1'/></linearGradient></defs><path d='M50 8 L32 48 L48 48 L35 98 L75 38 L55 38 Z' fill='url(%23lg)' stroke='%23ff6600' stroke-width='1.2' stroke-linejoin='round'/></svg>">
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
    .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { color: #333; }
    .value { font-size: 48px; color: #007bff; margin: 20px 0; }
    .status { font-size: 24px; margin: 10px 0; padding: 10px; border-radius: 5px; }
    .status.on { background: #28a745; color: white; }
    .status.off { background: #6c757d; color: white; }
    input { padding: 10px; font-size: 18px; width: 150px; margin: 10px; }
    button { padding: 12px 30px; font-size: 18px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
    button:hover { background: #0056b3; }
    .info { margin: 10px 0; color: #666; }
    .version { margin-top: 30px; padding-top: 20px; border-top: 1px solid #ddd; color: #999; font-size: 12px; }
  </style>
  <script>
    function updateValues() {
      fetch('/data').then(r => r.json()).then(data => {
        document.getElementById('current').innerText = data.current.toFixed(3);
        document.getElementById('baseline').innerText = data.baseline.toFixed(3);
        document.getElementById('version').innerText = data.version;
        let statusEl = document.getElementById('status');
        statusEl.innerText = data.machineOn ? 'ON' : 'OFF';
        statusEl.className = 'status ' + (data.machineOn ? 'on' : 'off');
      });
    }
    function setBaseline() {
      let value = document.getElementById('baselineInput').value;
      fetch('/setbaseline?value=' + value).then(() => {
        alert('Baseline updated to ' + value + 'A');
        updateValues();
      });
    }
    setInterval(updateValues, 1000);
    window.onload = updateValues;
  </script>
</head>
<body>
  <div class="container">
    <h1>ZMCT103C Current Sensor</h1>
    <div class="info">Current Reading:</div>
    <div class="value"><span id="current">0.000</span> A</div>
    <div class="status off" id="status">OFF</div>
    <hr>
    <h2>Calibration</h2>
    <div class="info">Current Baseline: <strong><span id="baseline">0.000</span> A</strong></div>
    <input type="number" id="baselineInput" step="0.01" placeholder="Baseline (A)">
    <br>
    <button onclick="setBaseline()">Set Baseline</button>
    <div class="info" style="margin-top: 20px; font-size: 14px;">
      Machine turns ON when current exceeds baseline + 50mA
    </div>
    <div class="version">
      Firmware: <span id="version">...</span>
    </div>
  </div>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });

  // API endpoint for current data
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    float current = readCurrent();
    String json = "{";
    json += "\"current\":" + String(current, 3) + ",";
    json += "\"baseline\":" + String(baselineCurrent, 3) + ",";
    json += "\"machineOn\":" + String(machineIsOn ? "true" : "false") + ",";
    json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  // API endpoint to set baseline
  server.on("/setbaseline", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      baselineCurrent = request->getParam("value")->value().toFloat();
      preferences.begin("current-sensor", false);
      preferences.putFloat("baseline", baselineCurrent);
      preferences.end();
      Serial.print("Baseline set to: ");
      Serial.println(baselineCurrent, 3);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing value parameter");
    }
  });

  server.begin();
  Serial.println("Web server started");
  Serial.print("Access at: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Or at: http://currentsensor.local");
}

void setup() {
  Serial.begin(115200);
  
  Serial.println("\n=================================");
  Serial.print("CurrentSensor Firmware: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("=================================\n");
  
  // Load saved baseline from preferences
  preferences.begin("current-sensor", true);
  baselineCurrent = preferences.getFloat("baseline", 0.1);
  preferences.end();
  
  Serial.print("Loaded baseline: ");
  Serial.print(baselineCurrent, 3);
  Serial.println(" A");
  
  // Configure ADC for better accuracy
  analogReadResolution(12); // 12-bit resolution for ESP32
  analogSetAttenuation(ADC_11db); // Full range 0-3.3V
  
  Serial.println("ZMCT103C Current Sensor Setup Complete");
  Serial.println("Module Info:");
  Serial.println("- Current Ratio: 5A:5mA (1000:1)");
  Serial.println("- Max Current: 5A");
  Serial.println("- Voltage Divider: R1=10kOhm, R2=20kOhm (5V->3.3V protection)");
  Serial.println("- Adjust onboard potentiometer for calibration");
  
  // Setup WiFi
  setupWiFi();
  
  // Setup Web Server
  setupWebServer();
  
  // Setup MQTT
  client.setServer(mqtt_broker, mqtt_port);
  
  // Setup current sensor pin
  pinMode(CURRENT_SENSOR_PIN, INPUT);
  
  Serial.println("Current Sensor Setup Complete");
}

void loop() {
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime = currentTime;
    
    float current = readCurrent();
    
    Serial.print("Current: ");
    Serial.print(current, 3);
    Serial.print(" A | Baseline: ");
    Serial.print(baselineCurrent, 3);
    Serial.print(" A | Machine: ");
    
    // Detect state change with hysteresis
    bool newState;
    if (machineIsOn) {
      // Machine is ON - turn OFF only if current drops below baseline
      newState = current > baselineCurrent;
    } else {
      // Machine is OFF - turn ON only if current exceeds baseline + hysteresis
      newState = current > (baselineCurrent + hysteresis);
    }
    
    // Debounce logic: state must be stable for DEBOUNCE_DELAY ms
    if (newState != machineIsOn) {
      if (!pendingState || pendingStateValue != newState) {
        // New state detected - start debounce timer
        pendingState = true;
        pendingStateValue = newState;
        stateChangeTime = currentTime;
        Serial.print(newState ? "ON" : "OFF");
        Serial.println(" [PENDING]");
      } else if (currentTime - stateChangeTime >= DEBOUNCE_DELAY) {
        // State has been stable for debounce period - confirm change
        machineIsOn = newState;
        pendingState = false;
        
        String status = machineIsOn ? "1" : "0";
        Serial.print(machineIsOn ? "ON" : "OFF");
        Serial.println(" [STATE CHANGED - CONFIRMED]");
        
        // Publish state change to MQTT
        client.publish(mqtt_topic_status, status.c_str(), true);
        
        // Also publish current reading
        String payload = String(current, 3);
        client.publish(mqtt_topic_current, payload.c_str());
        
        lastPublishTime = currentTime; // Reset publish timer
      } else {
        // Still waiting for debounce
        Serial.print(newState ? "ON" : "OFF");
        Serial.print(" [DEBOUNCING ");
        Serial.print((DEBOUNCE_DELAY - (currentTime - stateChangeTime)) / 1000.0, 1);
        Serial.println("s]");
      }
    } else {
      // State matches current - reset pending
      if (pendingState) {
        Serial.print(machineIsOn ? "ON" : "OFF");
        Serial.println(" [DEBOUNCE CANCELLED]");
        pendingState = false;
      } else {
        Serial.println(machineIsOn ? "ON" : "OFF");
      }
    }
    
    // Periodic heartbeat - send on separate topic to not trigger flows
    if (currentTime - lastPublishTime >= publishInterval) {
      // Send heartbeat on dedicated topic (for dashboard connectivity check)
      client.publish(mqtt_topic_heartbeat, "alive");
      
      lastPublishTime = currentTime;
      Serial.print(" [HEARTBEAT]");
    }
  }
}
