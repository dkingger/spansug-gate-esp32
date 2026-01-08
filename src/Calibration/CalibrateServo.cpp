#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>

// WiFi credentials
const char* ssid = "<DIT SSID>";
const char* password = "<DIN KODE>";

// Servo konfiguration
static const int SERVO_PIN = 3;
static int OPEN_DEG  = 11;
static int CLOSE_DEG = 105;
Servo gateServo;

// WS2812B (1 pixel) paa GPIO2
static const int PIXEL_PIN = 2;
static const int PIXEL_COUNT = 1;
Adafruit_NeoPixel pixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// LED farver (brugervalgte, gemmes i NVS)
static uint8_t OPEN_R = 0, OPEN_G = 180, OPEN_B = 0;    // grøn
static uint8_t CLOSE_R = 180, CLOSE_G = 0, CLOSE_B = 0; // rød

// Tilstand
enum GateState { STATE_UNKNOWN, STATE_OPEN, STATE_CLOSED };
GateState state = STATE_UNKNOWN;

// Webserver
AsyncWebServer server(80);

// Persistent storage (NVS)
Preferences prefs;
static const char* NVS_NS    = "servo";
static const char* KEY_OPEN  = "open_deg";
static const char* KEY_CLOSE = "close_deg";
static const char* KEY_STATE = "last_state"; // 0=unknown,1=open,2=closed
static const char* KEY_OC_R  = "open_r";
static const char* KEY_OC_G  = "open_g";
static const char* KEY_OC_B  = "open_b";
static const char* KEY_CC_R  = "close_r";
static const char* KEY_CC_G  = "close_g";
static const char* KEY_CC_B  = "close_b";

// Serial log buffer (sidste 50 linjer)
String logBuffer = "";
const int MAX_LOG_LINES = 50;

void addLog(String msg) {
  // Log only to internal buffer (no serial output)
  logBuffer += msg + "\n";
  // Begraens buffer stoerrelse
  int lineCount = 0;
  for (int i = logBuffer.length() - 1; i >= 0; i--) {
    if (logBuffer[i] == '\n') lineCount++;
    if (lineCount > MAX_LOG_LINES) {
      logBuffer = logBuffer.substring(i + 1);
      break;
    }
  }
}

static int stateToInt(GateState s) {
  if (s == STATE_OPEN) return 1;
  if (s == STATE_CLOSED) return 2;
  return 0;
}

static GateState intToState(int v) {
  if (v == 1) return STATE_OPEN;
  if (v == 2) return STATE_CLOSED;
  return STATE_UNKNOWN;
}

void saveNvsAll() {
  prefs.putInt(KEY_OPEN,  OPEN_DEG);
  prefs.putInt(KEY_CLOSE, CLOSE_DEG);
  prefs.putInt(KEY_STATE, stateToInt(state));
  prefs.putUInt(KEY_OC_R, OPEN_R);
  prefs.putUInt(KEY_OC_G, OPEN_G);
  prefs.putUInt(KEY_OC_B, OPEN_B);
  prefs.putUInt(KEY_CC_R, CLOSE_R);
  prefs.putUInt(KEY_CC_G, CLOSE_G);
  prefs.putUInt(KEY_CC_B, CLOSE_B);
  addLog("NVS save: open=" + String(OPEN_DEG) +
         " close=" + String(CLOSE_DEG) +
    " state=" + String(stateToInt(state)) +
    " openRGB=" + String(OPEN_R) + "," + String(OPEN_G) + "," + String(OPEN_B) +
    " closeRGB=" + String(CLOSE_R) + "," + String(CLOSE_G) + "," + String(CLOSE_B));
}

void loadNvsAll() {
  OPEN_DEG  = prefs.getInt(KEY_OPEN,  OPEN_DEG);
  CLOSE_DEG = prefs.getInt(KEY_CLOSE, CLOSE_DEG);
  state     = intToState(prefs.getInt(KEY_STATE, stateToInt(state)));

   // farver (default grøn/rød)
  OPEN_R  = prefs.getUInt(KEY_OC_R, OPEN_R);
  OPEN_G  = prefs.getUInt(KEY_OC_G, OPEN_G);
  OPEN_B  = prefs.getUInt(KEY_OC_B, OPEN_B);
  CLOSE_R = prefs.getUInt(KEY_CC_R, CLOSE_R);
  CLOSE_G = prefs.getUInt(KEY_CC_G, CLOSE_G);
  CLOSE_B = prefs.getUInt(KEY_CC_B, CLOSE_B);

  // Sanity clamp
  OPEN_DEG  = constrain(OPEN_DEG, 0, 180);
  CLOSE_DEG = constrain(CLOSE_DEG, 0, 180);

  addLog("NVS load: open=" + String(OPEN_DEG) +
         " close=" + String(CLOSE_DEG) +
    " state=" + String(stateToInt(state)) +
    " openRGB=" + String(OPEN_R) + "," + String(OPEN_G) + "," + String(OPEN_B) +
    " closeRGB=" + String(CLOSE_R) + "," + String(CLOSE_G) + "," + String(CLOSE_B));
}

void setLedForState(GateState s) {
  if (s == STATE_OPEN) {
    pixel.setPixelColor(0, pixel.Color(OPEN_R, OPEN_G, OPEN_B));
    addLog("LED: AABEN (brugerfarve)");
  } else if (s == STATE_CLOSED) {
    pixel.setPixelColor(0, pixel.Color(CLOSE_R, CLOSE_G, CLOSE_B));
    addLog("LED: LUKKET (brugerfarve)");
  } else {
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));     // sluk
    addLog("LED: SLUKKET (UKENDT)");
  }
  pixel.show();
}

void moveTo(int deg) {
  deg = constrain(deg, 0, 180);
  gateServo.write(deg);
  addLog("Servo -> " + String(deg) + " grader");
}

void setupWiFi() {
  addLog("\nWiFi: Forbinder til " + String(ssid) + "...");
  WiFi.mode(WIFI_STA);

  // Netværksnavn/hostname
  WiFi.setHostname("Servo Tester");

  // Configur static IP
  IPAddress staticIP(192, 168, 87, 18);
  IPAddress gateway(192, 168, 87, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns1(192, 168, 87, 1);

  WiFi.config(staticIP, gateway, subnet, dns1);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("WiFi: Forbundet!");
    addLog("IP: " + WiFi.localIP().toString());
  } else {
    addLog("WiFi: Forbindelsesfejl!");
  }
}

void setupWebServer() {
  // Hent HTML fra SPIFFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/index.html")) {
      request->send(SPIFFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html not found");
    }
  });

  // Status endpoint
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String stateStr = (state == STATE_OPEN) ? "AABEN" : (state == STATE_CLOSED) ? "LUKKET" : "UKENDT";

    // Parse log til array
    String logJson = "[";
    int lineCount = 0;
    int pos = 0;
    while (pos < (int)logBuffer.length() && lineCount < 30) {
      int newlinePos = logBuffer.indexOf('\n', pos);
      if (newlinePos == -1) newlinePos = logBuffer.length();
      String line = logBuffer.substring(pos, newlinePos);
      if (line.length() > 0) {
        if (lineCount > 0) logJson += ",";
        logJson += "\"" + line + "\"";
        lineCount++;
      }
      pos = newlinePos + 1;
    }
    logJson += "]";

    char openHex[8];
    snprintf(openHex, sizeof(openHex), "%02X%02X%02X", OPEN_R, OPEN_G, OPEN_B);
    char closeHex[8];
    snprintf(closeHex, sizeof(closeHex), "%02X%02X%02X", CLOSE_R, CLOSE_G, CLOSE_B);

    String json = "{\"state\":\"" + stateStr +
                  "\",\"open_deg\":" + String(OPEN_DEG) +
                  ",\"close_deg\":" + String(CLOSE_DEG) +
                  ",\"open_color\":\"#" + String(openHex) + "\"" +
                  ",\"close_color\":\"#" + String(closeHex) + "\"" +
                  ",\"log\":" + logJson + "}";
    request->send(200, "application/json", json);
  });

  // Command endpoint
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool changed = false;

    if (request->hasParam("c")) {
      String cmd = request->getParam("c")->value();
      char ch = cmd[0];

      if (ch == 'o') {
        moveTo(OPEN_DEG);
        state = STATE_OPEN;
        setLedForState(state);
        changed = true;
      } else if (ch == 'c') {
        moveTo(CLOSE_DEG);
        state = STATE_CLOSED;
        setLedForState(state);
        changed = true;
      } else if (ch == '+') {
        OPEN_DEG = constrain(OPEN_DEG + 1, 0, 180);
        addLog("OPEN_DEG=" + String(OPEN_DEG));
        changed = true;
      } else if (ch == '-') {
        OPEN_DEG = constrain(OPEN_DEG - 1, 0, 180);
        addLog("OPEN_DEG=" + String(OPEN_DEG));
        changed = true;
      } else if (ch == ']') {
        CLOSE_DEG = constrain(CLOSE_DEG + 1, 0, 180);
        addLog("CLOSE_DEG=" + String(CLOSE_DEG));
        changed = true;
      } else if (ch == '[') {
        CLOSE_DEG = constrain(CLOSE_DEG - 1, 0, 180);
        addLog("CLOSE_DEG=" + String(CLOSE_DEG));
        changed = true;
      }
    }

    String stateStr = (state == STATE_OPEN) ? "AABEN" : (state == STATE_CLOSED) ? "LUKKET" : "UKENDT";
    String json = "{\"state\":\"" + stateStr + "\",\"open_deg\":" + String(OPEN_DEG) + ",\"close_deg\":" + String(CLOSE_DEG) + "}";
    request->send(200, "application/json", json);
  });

  // Set endpoint for direct degree values
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool changed = false;

    if (request->hasParam("open")) {
      String v = request->getParam("open")->value();
      int val = v.toInt();
      int nv = constrain(val, 0, 180);
      if (nv != OPEN_DEG) {
        OPEN_DEG = nv;
        addLog("OPEN_DEG=" + String(OPEN_DEG));
        changed = true;
      }
    }
    if (request->hasParam("close")) {
      String v = request->getParam("close")->value();
      int val = v.toInt();
      int nv = constrain(val, 0, 180);
      if (nv != CLOSE_DEG) {
        CLOSE_DEG = nv;
        addLog("CLOSE_DEG=" + String(CLOSE_DEG));
        changed = true;
      }
    }

    if (request->hasParam("open_color")) {
      String v = request->getParam("open_color")->value();
      if (v.startsWith("#")) v.remove(0,1);
      if (v.length() == 6) {
        long rgb = strtol(v.c_str(), nullptr, 16);
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = rgb & 0xFF;
        if (r != OPEN_R || g != OPEN_G || b != OPEN_B) {
          OPEN_R = r; OPEN_G = g; OPEN_B = b;
          if (state == STATE_OPEN) setLedForState(state);
          addLog("OPEN_COLOR=#" + v);
          changed = true;
        }
      }
    }

    if (request->hasParam("close_color")) {
      String v = request->getParam("close_color")->value();
      if (v.startsWith("#")) v.remove(0,1);
      if (v.length() == 6) {
        long rgb = strtol(v.c_str(), nullptr, 16);
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = rgb & 0xFF;
        if (r != CLOSE_R || g != CLOSE_G || b != CLOSE_B) {
          CLOSE_R = r; CLOSE_G = g; CLOSE_B = b;
          if (state == STATE_CLOSED) setLedForState(state);
          addLog("CLOSE_COLOR=#" + v);
          changed = true;
        }
      }
    }

    String stateStr = (state == STATE_OPEN) ? "AABEN" : (state == STATE_CLOSED) ? "LUKKET" : "UKENDT";
    char openHex[8];
    snprintf(openHex, sizeof(openHex), "%02X%02X%02X", OPEN_R, OPEN_G, OPEN_B);
    char closeHex[8];
    snprintf(closeHex, sizeof(closeHex), "%02X%02X%02X", CLOSE_R, CLOSE_G, CLOSE_B);

    String json = "{\"state\":\"" + stateStr + "\",\"open_deg\":" + String(OPEN_DEG) +
                  ",\"close_deg\":" + String(CLOSE_DEG) +
                  ",\"open_color\":\"#" + String(openHex) + "\"" +
                  ",\"close_color\":\"#" + String(closeHex) + "\"" + "}";
    request->send(200, "application/json", json);
  });

  // Gem manuelt til NVS
  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request) {
    saveNvsAll();
    String stateStr = (state == STATE_OPEN) ? "AABEN" : (state == STATE_CLOSED) ? "LUKKET" : "UKENDT";
    String json = "{\"ok\":true,\"state\":\"" + stateStr + "\",\"open_deg\":" + String(OPEN_DEG) + ",\"close_deg\":" + String(CLOSE_DEG) + "}";
    request->send(200, "application/json", json);
    addLog("NVS: gemt (open=" + String(OPEN_DEG) + ", close=" + String(CLOSE_DEG) + ")");
  });

  // Clear log endpoint
  server.on("/clearlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    logBuffer = "";
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.begin();
  addLog("Webserver startet paa port 80");
}

void setup() {
  // Serial output removed (web logging used instead)
  delay(300);

  // NVS init + load persisted values (survives reboot/brownout)
  prefs.begin(NVS_NS, false);
  loadNvsAll();

  // LED init
  pixel.begin();
  pixel.setBrightness(40);
  setLedForState(STATE_UNKNOWN);

  // Servo init
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);

  // SPIFFS init
  if (!SPIFFS.begin(true)) {
    addLog("SPIFFS mount failed");
  } else {
    addLog("SPIFFS mounted");
  }

  addLog("\n========== BOOT ==========");
  addLog("Servo & LED WebControl");

  // WiFi setup
  setupWiFi();

  // Web server setup
  setupWebServer();

  // Genskab sidste kendte state efter reboot
  if (state == STATE_OPEN) {
    moveTo(OPEN_DEG);
    setLedForState(state);
  } else if (state == STATE_CLOSED) {
    moveTo(CLOSE_DEG);
    setLedForState(state);
  } else {
    // fallback: start lukket (samme adfærd som før, bare uden at overskrive state)
    moveTo(CLOSE_DEG);
    state = STATE_CLOSED;
    setLedForState(state);
    saveNvsAll();
  }

  addLog("Setup faerdig!");
}

void loop() {
  // Webserveren koerer asynkront, saa loop() kan vaere tom
  delay(100);
}
