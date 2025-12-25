# Spånsug – Gate Controller (ESP32-C3 + MQTT)

Dette repository indeholder **den aktive MQTT-baserede firmware** til styring af motoriserede spjæld (gates) i spånsugssystemet i **FabLab Spinderihallerne (Vejle)**.

> 🔧 **Kalibrering:**
> Kalibreringskode (servo + WS2812 uden MQTT) ligger i mappen **`Calibration/`** og har sin egen `README.md`.

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

---

## Mappestruktur

```
/
├── src/                # Aktiv ESP32 MQTT-firmware
├── Calibration/        # Kalibreringskode (egen README)
├── images/             # Diagrammer og fotos
├── node-red            # NodeRed flow (egen README)
├── platformio.ini
└── README.md           # (denne fil)
```

---

## Hardware (ESP32 gate)

* ESP32-C3 SuperMini
* Servo (MG996R / MG966R eller tilsvarende)
* 1× WS2812B (NeoPixel)
* (Midlertidigt) afbryder eller senere CT clamp
* Ekstern 5–6 V strømforsyning til servo

### Pinout (default)

| Funktion                       | GPIO |
| ------------------------------ | ---: |
| Servo signal                   |    3 |
| WS2812B data                   |    2 |
| Machine active (test/afbryder) |    4 |

> ⚠️ Servo må **aldrig** forsynes fra ESP32. Brug ekstern 5–6 V og fælles GND.

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

* `OPEN`
* `CLOSE`

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

**Luk spjæld manuelt:**

```bash
mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "CLOSE"
```

---

## Node-RED backend (kort)

Node-RED kører på Raspberry Pi og fungerer som **styringslogik**.

### Flow-princip

1. `machine_active = 1`

   * Send `OPEN` med det samme
   * Annullér evt. planlagt lukning

2. `machine_active = 0`

   * Vent **N sekunder** (efterløbstid)
   * Send `CLOSE`

3. Starter maskinen igen inden N sekunder

   * Lukning annulleres

### Variabel

* `flow.lukke_delay` – efterløbstid i **sekunder**

---

## Status

* [x] MQTT-baseret gate-styring
* [x] WS2812 status-LED
* [x] Node-RED efterløbstid
* [ ] CT clamp / strømsensor
* [ ] Generisk gate-konfiguration
* [ ] Master-logik for spånsuger

---

## Noter

* Projektet er designet til **værkstedsbrug**
* Robusthed prioriteres over kompleksitet
* ESP32 holdes bevidst enkel
* Al logik samles centralt i Node-RED

---

FabLab Spinderihallerne · Vejle


