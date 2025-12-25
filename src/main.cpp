#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

// --------- USER CONFIG ----------
static const char* WIFI_SSID = "newdahl";
static const char* WIFI_PASS = "12345678";

static const char* MQTT_HOST = "192.168.87.133";  // Raspberry Pi
static const uint16_t MQTT_PORT = 1883;

static const char* GATE_ID = "rondelsliber";

// Topics
static String topic_cmd    = String("spansug/gate/") + GATE_ID + "/cmd";
static String topic_active = String("spansug/gate/") + GATE_ID + "/machine_active";

// Pins
static const int SERVO_PIN  = 3;
static const int PIXEL_PIN  = 2;
static const int SWITCH_PIN = 4; // midlertidig afbryder (tilpas hvis du vil)

// Servo kalibrering
static int OPEN_DEG  = 11;
static int CLOSE_DEG = 105;
// -------------------------------

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

// --- LED helpers ---
void ledOff() {
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();
}

void ledGreen() {
  pixel.setPixelColor(0, pixel.Color(0, 255, 0));
  pixel.show();
}

void ledRed() {
  pixel.setPixelColor(0, pixel.Color(255, 0, 0));
  pixel.show();
}

void setLedForState(GateState s) {
  // Bemærk: WAIT håndteres i updateBlink(), så her sætter vi bare en starttilstand.
  if (s == STATE_OPEN) {
    ledGreen();
  } else if (s == STATE_CLOSED) {
    ledRed();
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
void handleCmd(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "OPEN") {
    moveTo(OPEN_DEG);
    state = STATE_OPEN;
    setLedForState(state);
  } else if (cmd == "WAIT") {
    // Venter/efterløb: ingen servo-bevægelse, kun blink
    state = STATE_WAIT;
    resetBlink();
  } else if (cmd == "CLOSE") {
    moveTo(CLOSE_DEG);
    state = STATE_CLOSED;
    setLedForState(state);
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
  WiFi.begin(WIFI_SSID, WIFI_PASS);

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
  delay(300);

  // LED
  pixel.begin();
  pixel.setBrightness(40);
  setLedForState(STATE_UNKNOWN);

  // Servo
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);

  // Start i lukket
  moveTo(CLOSE_DEG);
  state = STATE_CLOSED;
  setLedForState(state);

  // Switch (midlertidig)
  pinMode(SWITCH_PIN, INPUT_PULLUP); // afbryder til GND når "ON"
  lastSwitch = digitalRead(SWITCH_PIN);

  wifiConnect();
  mqttConnect();

  // publish initial state
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
