# Spånsug – Gate Controller (ESP32‑C3) + Node‑RED backend

Dette repository dokumenterer og indeholder kode til et spjæld (gate) i spånsugssystemet i **FabLab Spinderihallerne (Vejle)**.

Systemet er opdelt i:

* **ESP32‑C3 ved hver maskine**: styrer servo (spjæld) + WS2812B status‑LED og taler MQTT.
* **Raspberry Pi**: kører **Mosquitto (MQTT broker)** + **Node‑RED**, som håndterer logik og efterløbstid.

---

## Arkitektur (overblik)

```
[ Maskine ]
    │
    ▼
[ ESP32‑C3 gate ]  --MQTT-->  [ Raspberry Pi (Mosquitto + Node‑RED) ]
    ▲                               │
    └----------MQTT cmd-------------┘
```
![NodeRedFlow](/images/flow.png)

**Princip:** ESP32 er “muskel + I/O”. Node‑RED er “hjernen”.

---

## Hardware

* ESP32‑C3 SuperMini
* Servo: MG996R / MG966R (eller tilsvarende)
* 1× WS2812B (NeoPixel)
* (Midlertidigt) afbryder til test af `machine_active`
* Ekstern 5–6 V strømforsyning til servo

### Pinout (default i koden)

| Funktion        | GPIO |
| --------------- | ---: |
| Servo signal    |    3 |
| WS2812B data    |    2 |
| Afbryder (test) |    4 |

> **Vigtigt:** Servo må **ikke** forsynes fra ESP32. Brug ekstern 5–6 V og **fælles GND**.

---

## Tilslutning (test-setup)

Billedet ligger i repo’et:

![Test setup – servo og WS2812B](/images/TestsSetupConnections.png)

---

## LED-status (WS2812B)

| Tilstand      | LED     |
| ------------- | ------- |
| Spjæld åben   | 🟢 Grøn |
| Spjæld lukket | 🔴 Rød  |
| Init/ukendt   | Slukket |

---

## MQTT topics (eksempel: rondelsliber)

**Fra ESP32 → backend**

* Topic:

  ```
  spansug/gate/rondelsliber/machine_active
  ```
* Payload:

  * `1` = maskinen kører
  * `0` = maskinen stoppet

**Fra backend → ESP32**

* Topic:

  ```
  spansug/gate/rondelsliber/cmd
  ```
* Payload:

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

## Node‑RED flow (backend-logik)

Node‑RED-flowet styrer, hvornår der sendes `OPEN`/`CLOSE` til ESP32.

### Regler

1. Når `machine_active = 1`

* Send `OPEN` **med det samme**
* Annullér evt. planlagt lukning

2. Når `machine_active = 0`

* Vent **N sekunder** (efterløbstid)
* Send `CLOSE`

3. Hvis maskinen starter igen inden N sekunder

* Lukning annulleres (spjæld forbliver åbent)

### Variabel

Flowet bruger:

* `flow.lukke_delay` (sekunder)

---

## Import/Export af Node‑RED flow

### Export (gem flow som fil)

Node‑RED → menu (≡) → **Export** → vælg **Current flow** (eller Selected nodes) → **Download** (JSON).

### Import

Node‑RED → menu (≡) → **Import** → vælg JSON‑fil → **Import** → **Deploy**.

---

## Raspberry Pi / Mosquitto (LAN)

MQTT broker lytter på LAN:

* IP (eksempel): `192.168.87.133`
* Port: `1883`

Tjek på Pi:

```bash
sudo ss -lntp | grep 1883
```

---

## Status

* [x] Servo‑kalibrering
* [x] WS2812B status
* [x] MQTT topics defineret
* [x] Node‑RED efterløbslogik
* [ ] CT clamp / strømsensor som `machine_active`
* [ ] Fælles/generisk flow for alle gates
* [ ] Master-logik for spånsuger (hvis ønsket)

---

FabLab Spinderihallerne · Vejle
