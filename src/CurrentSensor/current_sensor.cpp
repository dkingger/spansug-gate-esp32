#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

// WiFi credentials
const char* ssid = "Fablab";
const char* password = "11223344";

// MQTT Broker settings
const char* mqtt_broker = "spansug-backend.local";
const int mqtt_port = 1883;
const char* mqtt_topic_status = "spansug/gate/rondelsliber/machine_active";
const char* mqtt_topic_current = "spansug/gate/rondelsliber/current";

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
const unsigned long readInterval = 1000; // Read every second

// Machine state detection
float baselineCurrent = 0.1; // Default baseline in Amperes
bool machineIsOn = false;
float hysteresis = 0.05; // Prevent flickering (50mA)
int zeroOffset = 2048; // ADC zero point (will be calibrated)

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
    
    // Start mDNS
    if (MDNS.begin("currentsensor")) {
      Serial.println("mDNS started: currentsensor.local");
    } else {
      Serial.println("Error setting up mDNS!");
    }
  } else {
    Serial.println("\nWiFi connection FAILED!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "ESP32Current-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      client.publish(mqtt_topic_status, "online");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
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
    delayMicroseconds(100);
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
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }
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
  </style>
  <script>
    function updateValues() {
      fetch('/data').then(r => r.json()).then(data => {
        document.getElementById('current').innerText = data.current.toFixed(3);
        document.getElementById('baseline').innerText = data.baseline.toFixed(3);
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
    json += "\"machineOn\":" + String(machineIsOn ? "true" : "false");
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
    
    // If state changed, send MQTT message
    if (newState != machineIsOn) {
      machineIsOn = newState;
      String status = machineIsOn ? "1" : "0";
      Serial.print(machineIsOn ? "ON" : "OFF");
      Serial.println(" [STATE CHANGED]");
      
      // Publish state change to MQTT
      client.publish(mqtt_topic_status, status.c_str());
      
      // Also publish current reading
      String payload = String(current, 3);
      client.publish(mqtt_topic_current, payload.c_str());
    } else {
      Serial.println(machineIsOn ? "ON" : "OFF");
    }
  }
}
