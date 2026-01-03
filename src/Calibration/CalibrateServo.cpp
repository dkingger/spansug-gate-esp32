#include <Arduino.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>

// WiFi credentials
const char* ssid = "newdahl";
const char* password = "12345678";

// Servo konfiguration
static const int SERVO_PIN = 3;
static int OPEN_DEG  = 11;
static int CLOSE_DEG = 105;
Servo gateServo;

// WS2812B (1 pixel) paa GPIO2
static const int PIXEL_PIN = 2;
static const int PIXEL_COUNT = 1;
Adafruit_NeoPixel pixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

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
  addLog("NVS save: open=" + String(OPEN_DEG) +
         " close=" + String(CLOSE_DEG) +
         " state=" + String(stateToInt(state)));
}

void loadNvsAll() {
  OPEN_DEG  = prefs.getInt(KEY_OPEN,  OPEN_DEG);
  CLOSE_DEG = prefs.getInt(KEY_CLOSE, CLOSE_DEG);
  state     = intToState(prefs.getInt(KEY_STATE, stateToInt(state)));

  // Sanity clamp
  OPEN_DEG  = constrain(OPEN_DEG, 0, 180);
  CLOSE_DEG = constrain(CLOSE_DEG, 0, 180);

  addLog("NVS load: open=" + String(OPEN_DEG) +
         " close=" + String(CLOSE_DEG) +
         " state=" + String(stateToInt(state)));
}

void setLedForState(GateState s) {
  if (s == STATE_OPEN) {
    pixel.setPixelColor(0, pixel.Color(0, 100, 0));   // groen
    addLog("LED: GROEN (AABEN)");
  } else if (s == STATE_CLOSED) {
    pixel.setPixelColor(0, pixel.Color(100, 0, 0));   // rod
    addLog("LED: ROD (LUKKET)");
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

    String json = "{\"state\":\"" + stateStr + "\",\"open_deg\":" + String(OPEN_DEG) + ",\"close_deg\":" + String(CLOSE_DEG) + ",\"log\":" + logJson + "}";
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

    if (changed) saveNvsAll();

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

    if (changed) saveNvsAll();

    String stateStr = (state == STATE_OPEN) ? "AABEN" : (state == STATE_CLOSED) ? "LUKKET" : "UKENDT";
    String json = "{\"state\":\"" + stateStr + "\",\"open_deg\":" + String(OPEN_DEG) + ",\"close_deg\":" + String(CLOSE_DEG) + "}";
    request->send(200, "application/json", json);
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
