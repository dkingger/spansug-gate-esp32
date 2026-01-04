# Spånsug – Gate Controller (ESP32-C3 + MQTT)

Dette repository indeholder **den aktive MQTT-baserede firmware** til styring af motoriserede spjæld (gates) i spånsugssystemet i **FabLab Spinderihallerne (Vejle)**.

> 🔧 **Kalibrering**
> Kalibreringskode (servo + WS2812 uden MQTT) ligger i mappen **`src/Calibration/`** og har sin egen `README.md`.

---

## Overordnet arkitektur

```
[ Maskine 1 ]     [ Maskine 2 ]     [ Maskine 3 ]
    │                 │                 │
    ▼                 ▼                 ▼
[ ESP32-gate1 ]   [ ESP32-gate2 ]   [ ESP32-gate3 ]
    │                 │                 │
    └─────────────────┼─────────────────┘
                      │
                    MQTT
                      │
                      ▼
          [ Raspberry Pi Node-RED ]
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
     Automatiske   Manuel      Relay Manager
      Gates (3)    Gate (1)    (KRITISK - 1)
          │           │           │
          └───────────┼───────────┘
                      │
                      ▼
             [ GPIO 12 Relay ]
            (Spånsuger motor)

Raspberry Pi:
- Mosquitto (MQTT broker)
- Node-RED (5 flows: 3 auto gates + 1 manual + 1 relay manager)
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
├── src/                # MQTT-firmware
├── src/Calibration/    # Kalibreringskode (egen README)
├── images/             # Diagrammer og fotos
├── node-red/           # Node-RED flow (egen README)
├── platformio.ini      # Indeholder begge environments (MQTT og Calibration)
└── README.md           # (denne fil)
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

### 5 Flows arbetar sammen

**Automatiske gates (med servo-kontrol):**
- `spansug-gate-rundsav_auto.json` – Rundsav
- `spansug-gate-stor_cnc.json` – Stor CNC
- `spansug-gate-lille_cnc.json` – Lille CNC

**Manuel gate (kun sensor):**
- `spansug-gate-rundsav_manual.json` – Rundsav manuel

**KRITISK – Central relay koordinering:**
- `spansug-relay-manager.json` – Sikrer at kun ét relæ styrer spånsuger-motoren

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

**Med central Relay Manager (RIGTIGT ✓):**
1. `stor_cnc` åbner → Manager tæller 1 gate åbent → relæ ON
2. `rundsav_auto` åbner → Manager tæller 2 gates åbne → relæ forbliver ON
3. `stor_cnc` lukker → Manager tæller 1 gate åbent → relæ forbliver ON ✓
4. `rundsav_auto` lukker → Manager tæller 0 gates åbne → relæ OFF
5. Spånsuger stopper først når alle arbejder er færdige ✓

---

## Status

* [x] MQTT-baseret gate-styring
* [x] WS2812 status-LED
* [x] Blinkende LED i ventefase (`WAIT`)
* [x] Node-RED efterløbstid
* [ ] CT clamp / strømsensor
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

