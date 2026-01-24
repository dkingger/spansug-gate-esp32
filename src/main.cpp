#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "secrets.h"

// --------- USER CONFIG ----------

static const char* MQTT_HOST = "spansug-backend.local";  // Debian server
static const uint16_t MQTT_PORT = 1883;

static const char* GATE_ID = "rundsav_auto";

// Topics
static String topic_cmd    = String("spansug/gate/") + GATE_ID + "/cmd";
static String topic_state  = String("spansug/gate/") + GATE_ID + "/state";
static String topic_active = String("spansug/gate/") + GATE_ID + "/machine_active";

// Pins
static const int SERVO_PIN  = 3;
static const int PIXEL_PIN  = 2;
static const int SWITCH_PIN = 4; // midlertidig afbryder (tilpas hvis du vil)

// Servo kalibrering
static int OPEN_DEG  = 11;
static int CLOSE_DEG = 105;
// -------------------------------

// NVS persistent storage
Preferences prefs;
static const char* NVS_NS    = "servo";
static const char* KEY_OPEN  = "open_deg";
static const char* KEY_CLOSE = "close_deg";
static const char* KEY_STATE = "last_state"; // 0=unknown,1=open,2=wait,3=closed
static const char* KEY_OC_R  = "open_r";
static const char* KEY_OC_G  = "open_g";
static const char* KEY_OC_B  = "open_b";
static const char* KEY_CC_R  = "close_r";
static const char* KEY_CC_G  = "close_g";
static const char* KEY_CC_B  = "close_b";

// LED colors (set via calibration, read-only here)
static uint8_t OPEN_R = 0, OPEN_G = 180, OPEN_B = 0;
static uint8_t CLOSE_R = 180, CLOSE_G = 0, CLOSE_B = 0;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Servo gateServo;
Adafruit_NeoPixel pixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Tilføjet WAIT-state
enum GateState {
  STATE_UNKNOWN,
  STATE_OPEN,
  STATE_WAIT,    // venter/efterløb -> LED blinker grønt
  STATE_CLOSED
};

GateState state = STATE_UNKNOWN;

// Blink-konfiguration
static const uint32_t BLINK_INTERVAL_MS = 500; // justér hvis du vil
uint32_t lastBlinkMs = 0;
bool blinkOn = false;

// --- NVS helpers ---
static int stateToInt(GateState s) {
  if (s == STATE_OPEN) return 1;
  if (s == STATE_WAIT) return 2;
  if (s == STATE_CLOSED) return 3;
  return 0;
}

static GateState intToState(int v) {
  if (v == 1) return STATE_OPEN;
  if (v == 2) return STATE_WAIT;
  if (v == 3) return STATE_CLOSED;
  return STATE_UNKNOWN;
}

void saveState() {
  // Gem kun tilstand, IKKE servo-grader (de sættes kun via kalibrering)
  prefs.putInt(KEY_STATE, stateToInt(state));
  Serial.println("NVS saved state=" + String(stateToInt(state)));
}

void loadNvsAll() {
  // Indlæs servo-grader (sat via kalibrering) og sidste tilstand
  OPEN_DEG  = prefs.getInt(KEY_OPEN,  OPEN_DEG);
  CLOSE_DEG = prefs.getInt(KEY_CLOSE, CLOSE_DEG);
  state     = intToState(prefs.getInt(KEY_STATE, stateToInt(state)));

  // Indlæs LED-farver (sat via kalibrering)
  OPEN_R  = prefs.getUInt(KEY_OC_R, OPEN_R);
  OPEN_G  = prefs.getUInt(KEY_OC_G, OPEN_G);
  OPEN_B  = prefs.getUInt(KEY_OC_B, OPEN_B);
  CLOSE_R = prefs.getUInt(KEY_CC_R, CLOSE_R);
  CLOSE_G = prefs.getUInt(KEY_CC_G, CLOSE_G);
  CLOSE_B = prefs.getUInt(KEY_CC_B, CLOSE_B);

  // Sanity clamp
  OPEN_DEG  = constrain(OPEN_DEG, 0, 180);
  CLOSE_DEG = constrain(CLOSE_DEG, 0, 180);

  Serial.println("NVS loaded: open=" + String(OPEN_DEG) + 
                 " close=" + String(CLOSE_DEG) + 
                 " state=" + String(stateToInt(state)));
}

// --- LED helpers ---
void ledOff() {
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();
}

void ledGreen() {
  pixel.setPixelColor(0, pixel.Color(OPEN_R, OPEN_G, OPEN_B));
  pixel.show();
}

void ledRed() {
  pixel.setPixelColor(0, pixel.Color(255, 0, 0));
  pixel.show();
}

void setLedForState(GateState s) {
  // Bemærk: WAIT håndteres i updateBlink(), så her sætter vi bare en starttilstand.
  if (s == STATE_OPEN) {
    pixel.setPixelColor(0, pixel.Color(OPEN_R, OPEN_G, OPEN_B));
    pixel.show();
  } else if (s == STATE_CLOSED) {
    pixel.setPixelColor(0, pixel.Color(CLOSE_R, CLOSE_G, CLOSE_B));
    pixel.show();
  } else if (s == STATE_WAIT) {
    // start med slukket (blink starter i loop)
    ledOff();
  } else {
    ledOff();
  }
}

void resetBlink() {
  lastBlinkMs = millis();
  blinkOn = false;
  ledOff();
}

void updateBlink() {
  if (state != STATE_WAIT) return;

  const uint32_t now = millis();
  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blinkOn = !blinkOn;

    if (blinkOn) ledGreen();
    else ledOff();
  }
}

// --- Servo ---
void moveTo(int deg) {
  deg = constrain(deg, 0, 180);
  gateServo.write(deg);
  Serial.print("Servo -> ");
  Serial.println(deg);
}

// --- Command handling ---
void publishState() {
  const char* stateStr = "unknown";
  if (state == STATE_OPEN) stateStr = "open";
  else if (state == STATE_CLOSED) stateStr = "closed";
  else if (state == STATE_WAIT) stateStr = "wait";
  
  mqtt.publish(topic_state.c_str(), stateStr, true); // retained
  Serial.print("Published state: ");
  Serial.println(stateStr);
}

void handleCmd(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "OPEN") {
    moveTo(OPEN_DEG);
    state = STATE_OPEN;
    setLedForState(state);
    saveState();
    publishState();
  } else if (cmd == "WAIT") {
    // Venter/efterløb: ingen servo-bevægelse, kun blink
    state = STATE_WAIT;
    resetBlink();
    saveState();
    publishState();
  } else if (cmd == "CLOSE") {
    moveTo(CLOSE_DEG);
    state = STATE_CLOSED;
    setLedForState(state);
    saveState();
    publishState();
  } else {
    Serial.print("Ukendt cmd: ");
    Serial.println(cmd);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT in [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (String(topic) == topic_cmd) {
    handleCmd(msg);
  }
}

// --- WiFi/MQTT ---
void wifiConnect() {
  Serial.print("WiFi forbinder til ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK, IP: ");
  Serial.println(WiFi.localIP());
}

void mqttConnect() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  while (!mqtt.connected()) {
    String clientId = String("spansug-") + GATE_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("MQTT forbinder som ");
    Serial.println(clientId);

    if (mqtt.connect(clientId.c_str())) {
      Serial.println("MQTT OK");
      mqtt.subscribe(topic_cmd.c_str());
      Serial.print("Sub: ");
      Serial.println(topic_cmd);
    } else {
      Serial.print("MQTT fejl, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
}

int lastSwitch = -1;

void publishMachineActive(int v) {
  // v = 1/0
  const String payload = String(v);
  mqtt.publish(topic_active.c_str(), payload.c_str(), true /*retained*/);
  Serial.print("Pub ");
  Serial.print(topic_active);
  Serial.print(" = ");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Vent på serial forbindelse (max 3 sek)
  for(int i = 0; i < 30 && !Serial; i++) {
    delay(100);
  }
  
  Serial.println("\n\n=== SPANSUG GATE START ===");
  Serial.print("Gate ID: ");
  Serial.println(GATE_ID);

  // Init NVS
  prefs.begin(NVS_NS, false); // read-write mode
  loadNvsAll(); // Indlæs gemte værdier

  // LED
  pixel.begin();
  pixel.setBrightness(40);
  setLedForState(state); // Vis indlæst tilstand

  // Servo
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);

  // Gendan sidste position baseret på tilstand
  if (state == STATE_OPEN) {
    moveTo(OPEN_DEG);
  } else if (state == STATE_CLOSED) {
    moveTo(CLOSE_DEG);
  } else {
    // Default til lukket hvis ukendt
    moveTo(CLOSE_DEG);
    state = STATE_CLOSED;
    setLedForState(state);
  }

  // Switch (midlertidig)
  pinMode(SWITCH_PIN, INPUT_PULLUP); // afbryder til GND når "ON"
  lastSwitch = digitalRead(SWITCH_PIN);

  wifiConnect();
  mqttConnect();

  // publish initial state
  publishState();
  publishMachineActive(lastSwitch == LOW ? 1 : 0);

  Serial.println("Klar. MQTT cmd: OPEN / WAIT / CLOSE + switch -> machine_active");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) wifiConnect();
  if (!mqtt.connected()) mqttConnect();
  mqtt.loop();

  // Blink LED hvis vi er i WAIT
  updateBlink();

  // Switch -> machine_active
  const int s = digitalRead(SWITCH_PIN);
  if (s != lastSwitch) {
    lastSwitch = s;
    const int active = (s == LOW) ? 1 : 0;
    publishMachineActive(active);
  }

  delay(20);
}
