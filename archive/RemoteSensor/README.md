# Remote Sensor til Spånsug Gate System

## Formål
Remote sensor er en enkel ESP32 C3 SuperMini der monteres på CNC-maskiner eller andre værktøjer som ikke har direkte adgang til spånsug-spjældet. Den lytter til et signal fra maskinen og sender MQTT beskeder til backend-systemet.

## Hardware
- **ESP32 C3 SuperMini**
- **Strømforsyning**: 5V via USB eller VIN pin
- **Signal input**: Pull-down signal fra maskine (3.3V = aktiv, 0V = inaktiv)

## Tilslutning
```
CNC/Maskine Signal → GPIO 4 (eller den pin du vælger i koden)
CNC/Maskine GND    → ESP32 GND
Strøm              → ESP32 USB eller VIN+GND
```

⚠️ **VIGTIGT**: Sørg for at signalet er 3.3V safe! ESP32 er ikke 5V tolerant.

## Konfiguration

Åbn [remote_sensor.cpp](remote_sensor.cpp) og tilpas følgende parametre:

```cpp
// WiFi credentials
static const char* WIFI_SSID = "DIT_WIFI_SSID";
static const char* WIFI_PASS = "DIT_WIFI_PASSWORD";

// MQTT server
static const char* MQTT_HOST = "spansug-backend.local";

// Gate/maskine ID (VIGTIGT: Skal matche den gate der skal styres!)
static const char* GATE_ID = "stor_cnc";  // <-- SKIFT TIL DIN MASKINE

// GPIO pin til signal fra maskine
static const int SIGNAL_PIN = 4;  // <-- TILPAS HVIS NØDVENDIGT
```

### Vigtige parametre:
- **GATE_ID**: Skal matche præcis det gate ID som spjældet bruger (f.eks. "stor_cnc", "lille_cnc", "rundsav_auto")
- **SIGNAL_PIN**: GPIO pin der læser maskinens signal (standard GPIO 4)
- **DEBOUNCE_MS**: Tid i millisekunder før signal accepteres (standard 100ms)

## Upload firmware

```bash
# Fra projekt-roden:
pio run -e remote_sensor -t upload

# Med Serial Monitor:
pio run -e remote_sensor -t upload && pio device monitor -e remote_sensor
```

## Funktionsmåde

1. **Opstart**: ESP32 forbinder til WiFi og MQTT broker
2. **Signal læsning**: 
   - Signal HIGH (3.3V) = Maskine AKTIV → Sender `1` til MQTT topic
   - Signal LOW (0V) = Maskine INAKTIV → Sender `0` til MQTT topic
3. **MQTT topic**: `spansug/gate/<GATE_ID>/machine_active`
4. **Automatisk recovery**: Hvis WiFi eller MQTT forbindelse tabes, genoprettes den automatisk

### MQTT Beskeder
Remote sensoren sender til samme topic som gate-systemet lytter til:

```
Topic: spansug/gate/stor_cnc/machine_active
Payload: "1" (maskine aktiv) eller "0" (maskine inaktiv)
Retained: Ja
```

## Serial Monitor Output

Ved opstart:
```
=== SPANSUG REMOTE SENSOR START ===
Gate ID: stor_cnc
Signal Pin: 4
WiFi forbinder til Fablab
...
WiFi OK, IP: 192.168.87.123
MQTT forbinder til spansug-backend.local:1883
MQTT forbundet!
=== SENSOR KLAR ===
```

Ved signal ændring:
```
Signal ændret: HIGH (Aktiv)
Published: spansug/gate/stor_cnc/machine_active = 1
```

## Fejlfinding

### ESP32 forbinder ikke til WiFi
- Tjek SSID og password er korrekt
- Tjek WiFi signal styrke på monteringsstedet
- Se Serial Monitor for fejlbeskeder

### MQTT fejl
- Tjek at `spansug-backend.local` kan findes på netværket
- Ping backend fra en computer: `ping spansug-backend.local`
- Tjek at MQTT broker (Mosquitto) kører på backend serveren
- Se Serial Monitor for rc= fejlkode

### Signal læses ikke korrekt
- Tjek SIGNAL_PIN er korrekt i koden
- Tjek signalet er 3.3V (IKKE 5V!)
- Tjek GND er forbundet mellem maskine og ESP32
- Øg DEBOUNCE_MS hvis signalet er ustabilt

### Serial Monitor viser ingenting
- Tjek baud rate er 115200
- Tryk RESET knap på ESP32 efter upload
- Tjek USB kabel er et data-kabel (ikke kun opladning)

## Fordele ved Remote Sensor

✅ **Nem installation**: Kan monteres direkte på maskinen uden kabler til spjældet  
✅ **Pålidelig**: Automatisk reconnect ved netværksfejl  
✅ **Simpel**: Ingen buttons, servo eller LED - kun signal læsning  
✅ **Fleksibel**: Kan tilpasses forskellige GPIO pins og maskiner  
✅ **Debug venlig**: Serial Monitor viser al aktivitet  

## Bemærkninger

- Remote sensor kræver stabil WiFi forbindelse til maskinens placering
- Test altid signalniveauet (max 3.3V) før tilslutning
- Brug samme GATE_ID som den gate der skal styres
- MQTT beskeder er "retained" så status bevares ved genstart
