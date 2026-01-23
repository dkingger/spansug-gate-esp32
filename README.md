# Spånsug – Gate Controller (ESP32-C3 + MQTT)

Dette repository indeholder **den aktive MQTT-baserede firmware** til styring af motoriserede spjæld (gates) i spånsugssystemet i **FabLab Spinderihallerne (Vejle)**.

> ⚙️ **VIGTIG OPSÆTNING (MQTT firmware)**
> Før upload skal du opdatere **WiFi credentials** i `src/main.cpp`:
> - `WIFI_SSID` - Dit WiFi netværksnavn
> - `WIFI_PASS` - Dit WiFi password
> - `MQTT_HOST` - MQTT broker adresse (standard: `spansug-backend.local`)
> - `GATE_ID` - Gate identifikator (f.eks. `rundsav_auto`)

> 🔧 **VIGTIG OPSÆTNING (Kalibrering)**
> Kalibreringskode (servo + WS2812 uden MQTT) ligger i mappen **`src/Calibration/`** og har sin egen `README.md`.
> Før upload skal du opdatere i `src/Calibration/CalibrateServo.cpp`:
> - `ssid` - Dit WiFi netværksnavn
> - `password` - Dit WiFi password
> - `staticIP`, `gateway`, `subnet` - Netværksindstillinger (standard: 192.168.87.18)

---

## Overordnet arkitektur

```
[ Maskine 1 ]     [ Maskine 2 ]     [ Maskine 3 ]     [ Maskine 4 ]
    │                 │                 │                 │
    ▼                 ▼                 ▼                 ▼
[ ESP32-gate1 ]   [ ESP32-gate2 ]   [ ESP32-gate3 ]   [ ESP32-gate4 ]
    │                 │                 │                 │
    └─────────────────┼─────────────────┼─────────────────┘
                      │
                    MQTT
                      │
                      ▼
          [ Raspberry Pi Node-RED ]
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
     Automatiske   Manuel      Relay Manager
      Gates (5)    Gate (1)    (KRITISK - 1)
          │           │           │
          └───────────┼───────────┘
                      │
                      ▼
             [ GPIO 18 Relay ]
            (Spånsuger motor)

Raspberry Pi:
- Mosquitto (MQTT broker)
- Node-RED (7 flows: 5 auto gates + 1 manual + 1 relay manager)
```

**Princip:**

* ESP32 er en simpel hardware-node (servo, LED, input)
* Al beslutningslogik ligger i Node-RED
* ESP32 viser kun status (LED) og udfører kommandoer
* **KRITISK:** En central Relay Manager koordinerer spånsuger-motoren
  - Kun ét relæ styrer spånsuger - kan ikke kontrolleres individuelt fra hver gate
  - Relay Manager tæller åbne gates: ON hvis ≥1 gate åbent, OFF hvis alle lukket
  - Forhindrer at spånsuger slukkes mens andre gates stadig er åbne

---

## Mappestruktur

```
/
├── src/                  # MQTT-firmware (main gate controller)
├── src/Calibration/      # Kalibreringskode (egen README)
├── src/CurrentSensor/    # ZMCT103C strømmåler til automatisk gate-triggering
├── src/RemoteSensor/     # Remote sensor implementationer
├── images/               # Diagrammer og fotos
├── node-red/             # Node-RED flow (egen README)
├── web/                  # Web dashboard filer
├── deploy-to-pi.ps1      # Deploy script (Windows → Raspberry Pi)
├── install-pi.sh         # Installations script (kør på Raspberry Pi)
├── debug-backend.sh      # Diagnosticerings script til backend
├── platformio.ini        # Indeholder begge environments (MQTT og Calibration)
└── README.md             # (denne fil)
```

---

## Hardware (ESP32 gate)

* ESP32-C3 SuperMini
* Servo (MG996R / MG966R eller tilsvarende)
* 1× WS2812B (NeoPixel)
* Strømsensor m. relæ udgang
* Kondensator til at klare peeks fra Servo motoren

![Test setup – servo og WS2812B](/images/SetupConnections.png)

### Pinout (default)

| Funktion                       | GPIO |
| ------------------------------ | ---: |
| Servo signal                   |    3 |
| WS2812B data                   |    2 |
| Machine active                 |    4 |

> ⚠️ Servo **fælles GND** med LED og relæ

---

## LED-status (WS2812B)

LED’en på hver gate giver visuel feedback direkte i værkstedet:

| Tilstand             | Kommando | LED-adfærd       |
| -------------------- | -------- | ---------------- |
| Åben                 | `OPEN`   | Grøn (fast)      |
| Efterløb             | `WAIT`   | Grøn (blinkende) |
| Lukket               | `CLOSE`  | Rød (fast)       |

Efterløb er perioden fra maskinen stopper, til spjældet faktisk lukkes.

---

## MQTT-konfiguration

### Topics (eksempel: `rondelsliber`)

**ESP32 → backend**

```
spansug/gate/rondelsliber/machine_active
```

Payload:

* `1` = maskinen kører
* `0` = maskinen stoppet

**Backend → ESP32**

```
spansug/gate/rondelsliber/cmd
```

Payload:

* `OPEN`  – åbn spjæld
* `WAIT`  – ventefase / efterløb (ingen servo-bevægelse)
* `CLOSE` – luk spjæld

---

## Manuel MQTT-test (køres på Raspberry Pi)

**Se al trafik for en gate:**

```bash
mosquitto_sub -h 127.0.0.1 -t "spansug/gate/rondelsliber/#" -v
```

**Åbn spjæld manuelt:**

```bash
mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "OPEN"
```

**Sæt gate i ventefase (blink):**

```bash
mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "WAIT"
```

**Luk spjæld manuelt:**

```bash
mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "CLOSE"
```

---

## Node-RED backend (kort)

Node-RED kører på Raspberry Pi og fungerer som **central styringslogik** med 5 integrerede flows.

### 7 Flows arbetar sammen

**Automatiske gates (med servo-kontrol):**
- `spansug-gate-rundsav_auto.json` – Rundsav Auto
- `spansug-gate-stor_cnc.json` – Stor CNC
- `spansug-gate-lille_cnc.json` – Lille CNC
- `spansug-rondelsliber-flow.json` – Rondelsliber
- `spansug-gate-baandsliber.json` – Båndsliberen

**Manuel gate (kun sensor):**
- `spansug-gate-rundsav_manual.json` – Rundsav Manual

**KRITISK – Central relay koordinering:**
- `spansug-relay-manager.json` – Sikrer at kun ét relæ styrer spånsuger-motoren (GPIO 18)

### Flow-princip (automatiske gates)

1. **`machine_active = 1`**

   * Send `OPEN` med det samme
   * Nulstil evt. aktiv lukke-timer

2. **`machine_active = 0`**

   * Send `WAIT` med det samme
   * Start efterløbstimer

3. **Efter N sekunder**

   * Send `CLOSE`

4. **Hvis maskinen starter igen inden N sekunder**

   * Send `OPEN`
   * Lukning annulleres

### Relay Manager Flow (KRITISK!)

**Problem:** Hvis hver gate havde sit eget relæ-output, kunne relæen slukkes af en gate selvom andre stadig var åbne.

**Løsning:** Central Relay Manager tæller alle åbne gates:
- Relæ = ON hvis ≥1 gate åbent
- Relæ = OFF kun hvis ALLE gates lukket

**Input:** Abonnerer på status fra alle 4 gates
**Output:** Styrer GPIO 12 (spånsuger-relæ)

### Variabler

Hver gate bruger **flow-variabler** for efterløbstid og status:

```
flow.[gatename]_delay = 30  (sekunder)
flow.[gatename]_status = "LUKKET"
```

Relay Manager bruger flow context til at tælle åbne gates.

---

## Relay Manager – Hvorfor det er vigtigt

**Scenarie uden central manager (GALT ❌):**
1. `stor_cnc` åbner → relæ ON
2. `rundsav_auto` åbner → relæ forbliver ON
3. `stor_cnc` lukker → sendes kommando til relæ OFF
4. **Problem:** Relæ slukkes selv om `rundsav_auto` stadig er åbent!
5. Spånsuger stopper mens der stadig arbejdes ❌
 (GPIO 18)
2. `rundsav_auto` åbner → Manager tæller 2 gates åbne → relæ forbliver ON
3. `stor_cnc` lukker → Manager tæller 1 gate åbent → relæ forbliver ON ✓
4. `rundsav_auto` lukker → Manager tæller 0 gates åbne → relæ OFF
5. Spånsuger stopper først når alle arbejder er færdige ✓

Relay Manager publicerer også vacuum status til MQTT (`spansug/vacuum/active`) for brug i dashboards. forbliver ON ✓
4. `rundsav_auto` lukker → Manager tæller 0 gates åbne → relæ OFF
5. Spånsuger stopper først når alle arbejder er færdige ✓

---

## Current Sensor - Automatisk Gate-triggering

For maskiner uden elektronisk tænd/sluk-signal (f.eks. rondelsliber) kan vi bruge **strømmåling** til at detektere når maskinen kører.

### Princip

```
Strømførende ledning → ZMCT103C sensor → ESP32-C3 → MQTT → Node-RED → Gate OPEN/CLOSE
```

1. **ZMCT103C** måler AC strøm gennem maskines netledning
2. **ESP32-C3** beregner RMS og sammenligner med baseline
3. Når strøm > baseline → Send `"1"` til MQTT topic
4. **Node-RED** modtager og åbner tilhørende gate
5. Når strøm < baseline → Send `"0"` og gate lukkes efter efterløb

### Hardware Setup
- **ESP32-C3 Super Mini** med WiFi + MQTT
- **ZMCT103C Current Transformer** (5A max, 1000:1 ratio)
- **Voltage divider** (10kΩ / 20kΩ) for GPIO beskyttelse
- **Web interface** for baseline kalibrering (http://currentsensor.local)

### Integration
Systemet sender samme MQTT payload som remote sensors:
```
Topic: spansug/gate/rondelsliber/machine_active
Payload: "1" (ON) eller "0" (OFF)
```

Se detaljeret dokumentation i [`src/CurrentSensor/README.md`](src/CurrentSensor/README.md)

---

## Raspberry Pi Backend Setup

Systemet kræver en Raspberry Pi med følgende services:
- **Mosquitto** (MQTT broker)
- **Node-RED** (styrelogik)
- **Apache/Nginx** (web server til dashboard)

### Quick Start: Opsætning af ny Raspberry Pi

#### 1. Forbered Raspberry Pi
1. Flash Raspberry Pi OS (Lite eller Desktop) med Raspberry Pi Imager
2. Aktiver SSH under avancerede indstillinger
3. Sæt brugernavn og password (f.eks. `pi` / `raspberry`)
4. Indsæt SD-kort og boot Pi'en
5. Find Pi'ens IP-adresse (tjek din router eller brug `arp -a`)

#### 2. Deploy filer fra Windows
Opdater IP-adresse, brugernavn og password i `deploy-to-pi.ps1`, derefter kør:
```powershell
.\deploy-to-pi.ps1
```

Dette kopierer:
- Node-RED flows
- Web dashboard filer
- Installations script

#### 3. Installer backend på Raspberry Pi
SSH til Pi'en:
```bash
ssh pi@<IP-ADRESSE>
```

Kør installations scriptet:
```bash
cd ~/spansug-backend
bash install-pi.sh
```

Scriptet installerer og konfigurerer automatisk:
- ✅ Mosquitto MQTT broker (port 1883)
- ✅ Node-RED med nødvendige nodes (port 1880)
- ✅ Apache web server (port 80)
- ✅ Hostname: `spansug-backend.local`
- ✅ Avahi daemon til .local DNS

#### 4. Importer Node-RED flows
1. Åbn Node-RED editor: `http://<IP-ADRESSE>:1880`
2. Klik på menu (☰) → Import
3. Vælg "select a file to import"
4. Importer alle `.json` filer fra `~/spansug-backend/node-red/`:
   - `spansug-gate-rundsav_auto.json`
   - `spansug-gate-stor_cnc.json`
   - `spansug-gate-lille_cnc.json`
   - `spansug-gate-rundsav_manual.json`
   - `spansug-relay-manager.json`
   - `spansug-rondelsliber-flow.json`
   - `spansug-gate-baandsliber.json`
5. Klik **Deploy** (øverst til højre)

#### 5. Verificer installation
Kør diagnosticerings script:
```bash
~/spansug-backend/debug-backend.sh
```

Tjek at alle services kører:
- Mosquitto: `sudo systemctl status mosquitto`
- Node-RED: `sudo systemctl status nodered`
- Apache: `sudo systemctl status apache2`

#### 6. Test systemet
- **Node-RED editor**: `http://<IP-ADRESSE>:1880`
- **Dashboard**: `http://<IP-ADRESSE>/dashboard.html`
- **MQTT broker**: `<IP-ADRESSE>:1883`
- **Hostname**: `http://spansug-backend.local:1880`

Test MQTT kommunikation:
```bash
# Subscribe til alle topics
mosquitto_sub -h 127.0.0.1 -t "spansug/#" -v

# Test kommando (i anden terminal)
mosquitto_pub -h 127.0.0.1 -t "spansug/gate/test/cmd" -m "OPEN"
```

### Opdatering af eksisterende backend
Hvis du skal opdatere en kørende backend:
```powershell
# Fra Windows
.\deploy-to-pi.ps1

# På Raspberry Pi - genimporter opdaterede flows i Node-RED editor
```

---

## Status

* [x] MQTT-baseret gate-styring
* [x] WS2812 status-LED
* [x] Blinkende LED i ventefase (`WAIT`)
* [x] Node-RED efterløbstid
* [x] CT clamp / strømsensor (ZMCT103C implementation)
* [x] Generisk gate-konfiguration
* [x] Master-logik for spånsuger

---

## Noter

* Projektet er designet til **værkstedsbrug**
* Robusthed prioriteres over kompleksitet
* ESP32 holdes bevidst enkel
* Al beslutningslogik samles centralt i Node-RED

---

FabLab Spinderihallerne · Vejle

