#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

static const int SERVO_PIN = 3;

// WS2812B (1 pixel) på GPIO2
static const int PIXEL_PIN = 2;
static const int PIXEL_COUNT = 1;
Adafruit_NeoPixel pixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

static int OPEN_DEG  = 11;
static int CLOSE_DEG = 105;

Servo gateServo;

enum GateState { STATE_UNKNOWN, STATE_OPEN, STATE_CLOSED };
GateState state = STATE_UNKNOWN;

void setLedForState(GateState s) {
  if (s == STATE_OPEN) {
    pixel.setPixelColor(0, pixel.Color(0, 100, 0));   // grøn
  } else if (s == STATE_CLOSED) {
    pixel.setPixelColor(0, pixel.Color(100, 0, 0));   // rød
  } else {
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));     // sluk
  }
  pixel.show();
}

void moveTo(int deg) {
  deg = constrain(deg, 0, 180);
  gateServo.write(deg);
  Serial.print("Servo -> ");
  Serial.println(deg);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // LED init
  pixel.begin();
  pixel.setBrightness(40);   // justér efter smag (0-255)
  setLedForState(STATE_UNKNOWN);

  // Servo init
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);

  Serial.println("\nServo kalibrering + WS2812 status");
  Serial.println("Tast:");
  Serial.println("  o = open  (LED grøn)");
  Serial.println("  c = close (LED rød)");
  Serial.println("  + / - = juster OPEN_DEG");
  Serial.println("  ] / [ = juster CLOSE_DEG");
  Serial.println("  p = print værdier");
  Serial.println();

  // Start i lukket position (valgfrit)
  moveTo(CLOSE_DEG);
  state = STATE_CLOSED;
  setLedForState(state);
}

void loop() {
  if (!Serial.available()) return;

  char ch = (char)Serial.read();

  if (ch == 'o') {
    moveTo(OPEN_DEG);
    state = STATE_OPEN;
    setLedForState(state);
  } else if (ch == 'c') {
    moveTo(CLOSE_DEG);
    state = STATE_CLOSED;
    setLedForState(state);
  } else if (ch == '+') {
    OPEN_DEG++;
    Serial.print("OPEN_DEG="); Serial.println(OPEN_DEG);
  } else if (ch == '-') {
    OPEN_DEG--;
    Serial.print("OPEN_DEG="); Serial.println(OPEN_DEG);
  } else if (ch == ']') {
    CLOSE_DEG++;
    Serial.print("CLOSE_DEG="); Serial.println(CLOSE_DEG);
  } else if (ch == '[') {
    CLOSE_DEG--;
    Serial.print("CLOSE_DEG="); Serial.println(CLOSE_DEG);
  } else if (ch == 'p') {
    Serial.print("OPEN_DEG="); Serial.println(OPEN_DEG);
    Serial.print("CLOSE_DEG="); Serial.println(CLOSE_DEG);
  }
}
