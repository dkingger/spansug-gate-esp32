#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --------- USER CONFIG ----------
static const char* WIFI_SSID = "Fablab";
static const char* WIFI_PASS = "11223344";

static const char* MQTT_HOST = "spansug-backend.local";
static const uint16_t MQTT_PORT = 1883;

static const char* GATE_ID = "stor_cnc";  // SKIFT FOR HVER MASKINE!

// GPIO pin til at læse CNC signal (pull down)
static const int SIGNAL_PIN = 5;  // GPIO 5 er sikker på ESP32-C3

// Debounce og timing
static const uint32_t DEBOUNCE_MS = 100;
static const uint32_t RECONNECT_INTERVAL_MS = 5000;
// -------------------------------

String topic_active = String("spansug/gate/") + GATE_ID + "/machine_active";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool lastSignalState = false;
uint32_t lastDebounceTime = 0;
uint32_t lastReconnectAttempt = 0;

void wifiConnect() {
  Serial.print("WiFi forbinder til ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK, IP: ");
  Serial.println(WiFi.localIP());
}

bool mqttConnect() {
  if (mqtt.connected()) return true;
  
  uint32_t now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS) {
    return false;
  }
  lastReconnectAttempt = now;

  Serial.print("MQTT forbinder til ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  String clientId = String("spansug-sensor-") + GATE_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("MQTT forbundet!");
    return true;
  } else {
    Serial.print("MQTT fejl, rc=");
    Serial.println(mqtt.state());
    return false;
  }
}

void publishMachineState(bool active) {
  Serial.println("\n=== PUBLISH START ===");
  Serial.print("MQTT forbundet: ");
  Serial.println(mqtt.connected() ? "JA" : "NEJ");
  
  if (!mqtt.connected()) {
    Serial.println("Ingen MQTT forbindelse, forsøger at forbinde...");
    if (!mqttConnect()) {
      Serial.println("MQTT reconnect FEJLEDE!");
      return;
    }
    Serial.println("MQTT reconnect OK");
  }

  const char* payload = active ? "1" : "0";
  
  Serial.print("Topic: ");
  Serial.println(topic_active);
  Serial.print("Payload: ");
  Serial.println(payload);
  Serial.print("Retained: true");
  Serial.println();
  
  bool result = mqtt.publish(topic_active.c_str(), payload, true);
  
  Serial.print("Publish resultat: ");
  Serial.println(result ? "SUCCESS" : "FAILED");
  
  if (result) {
    Serial.print("Published: ");
    Serial.print(topic_active);
    Serial.print(" = ");
    Serial.println(payload);
  } else {
    Serial.println("MQTT publish FEJL!");
    Serial.print("MQTT state: ");
    Serial.println(mqtt.state());
  }
  Serial.println("=== PUBLISH SLUT ===\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Vent på serial forbindelse (max 3 sek)
  for(int i = 0; i < 30 && !Serial; i++) {
    delay(100);
  }
  
  Serial.println("\n\n=== SPANSUG REMOTE SENSOR START ===");
  Serial.print("Gate ID: ");
  Serial.println(GATE_ID);
  Serial.print("Signal Pin: ");
  Serial.println(SIGNAL_PIN);

  // Setup GPIO med pull-down
  pinMode(SIGNAL_PIN, INPUT_PULLDOWN);  // Pull down aktiveret
  Serial.println("Pin mode: INPUT_PULLDOWN aktiveret");
  
  // WiFi
  wifiConnect();
  
  // MQTT
  mqttConnect();
  
  // Send initial state
  lastSignalState = digitalRead(SIGNAL_PIN);
  publishMachineState(lastSignalState);
  
  Serial.println("=== SENSOR KLAR ===\n");
}

void loop() {
  // Tjek WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi tabt, genopretter...");
    wifiConnect();
  }

  // Tjek MQTT
  if (!mqtt.connected()) {
    mqttConnect();
  }
  mqtt.loop();

  // Læs signal med debounce
  bool currentSignal = digitalRead(SIGNAL_PIN);
  
  // DEBUG: Print signal hver 5. sekund
  static uint32_t lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 5000) {
    lastDebugPrint = millis();
    Serial.print("Pin ");
    Serial.print(SIGNAL_PIN);
    Serial.print(" = ");
    Serial.println(currentSignal ? "HIGH" : "LOW");
  }
  
  if (currentSignal != lastSignalState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    // Signal er stabilt, tjek om det har ændret sig
    if (currentSignal != lastSignalState) {
      Serial.println("\n>>> PIN ÆNDRING DETEKTERET <<<");
      Serial.print("Gammel værdi: ");
      Serial.println(lastSignalState ? "HIGH" : "LOW");
      Serial.print("Ny værdi: ");
      Serial.println(currentSignal ? "HIGH" : "LOW");
      
      lastSignalState = currentSignal;
      
      Serial.print("Signal ændret: ");
      Serial.println(currentSignal ? "HIGH (Aktiv)" : "LOW (Inaktiv)");
      
      Serial.print("Sender MQTT... MQTT connected: ");
      Serial.println(mqtt.connected() ? "JA" : "NEJ");
      
      publishMachineState(currentSignal);
    }
  }

  delay(10);  // Small delay
}
