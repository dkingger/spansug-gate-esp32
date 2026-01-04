# Spånsug – Gate Controller (ESP32-C3 + MQTT)

Dette repository indeholder **den aktive MQTT-baserede firmware** til styring af motoriserede spjæld (gates) i spånsugssystemet i **FabLab Spinderihallerne (Vejle)**.

> 🔧 **Kalibrering**
> Kalibreringskode (servo + WS2812 uden MQTT) ligger i mappen **`src/Calibration/`** og har sin egen `README.md`.

---

## Overordnet arkitektur

```
[ Maskine ]
    │
    ▼
[ ESP32-C3 gate ]  --MQTT-->  [ Raspberry Pi ]
    ▲                               │
    └-----------MQTT cmd------------┘

Raspberry Pi:
- Mosquitto (MQTT broker)
- Node-RED (logik + efterløbstid)
```

**Princip:**

* ESP32 er en simpel hardware-node (servo, LED, input)
* Al beslutningslogik ligger i Node-RED
* ESP32 viser kun status (LED) og udfører kommandoer

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

Node-RED kører på Raspberry Pi og fungerer som **central styringslogik**.

### Flow-princip

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

### Variabel

* `flow.lukke_delay` – efterløbstid i **sekunder**

---

## Status

* [x] MQTT-baseret gate-styring
* [x] WS2812 status-LED
* [x] Blinkende LED i ventefase (`WAIT`)
* [x] Node-RED efterløbstid
* [ ] CT clamp / strømsensor
* [ ] Generisk gate-konfiguration
* [ ] Master-logik for spånsuger

---

## Noter

* Projektet er designet til **værkstedsbrug**
* Robusthed prioriteres over kompleksitet
* ESP32 holdes bevidst enkel
* Al beslutningslogik samles centralt i Node-RED

---

FabLab Spinderihallerne · Vejle

