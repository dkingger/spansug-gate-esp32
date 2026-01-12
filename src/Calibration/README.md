# Spånsug – ESP32-C3 Gate Controller (Kalibrering)

Dette projekt er en **kalibrerings- og test-firmware** til et motorstyret spjæld
(brugt i spånsug-systemet i FabLab Spinderihallerne).

Firmwaren kører på en **ESP32-C3 SuperMini** og bruges til at:

- Kalibrere **servo-positioner** (OPEN / CLOSE)
- Give **visuel status** via en WS2812B LED
- Forberede hardware inden MQTT / backend-logik tilkobles

> ⚠️ Denne version indeholder **ingen MQTT**  
> Den er udelukkende til **mekanisk og elektrisk kalibrering**.

> ⚙️ **VIGTIG OPSÆTNING**
> Før upload skal du opdatere **WiFi og netværksindstillinger** i `CalibrateServo.cpp`:
> - `ssid` - Dit WiFi netværksnavn
> - `password` - Dit WiFi password
> - `staticIP` - Fast IP-adresse (standard: 192.168.87.18)
> - `gateway` - Gateway adresse (standard: 192.168.87.1)
> - `subnet` - Subnet mask (standard: 255.255.255.0)

---

## Hardware

- ESP32-C3 SuperMini
- Servo (MG996R / MG966R el.lign.)
- 1× WS2812B (NeoPixel)
- Ekstern 5–6 V strømforsyning til servo
- Fælles GND mellem ESP32 og servo/LED

### Pinout (default)
| Funktion | GPIO |
|-------|------|
| Servo signal | GPIO 3 |
| WS2812B data | GPIO 2 |

*(Kan ændres i koden)*

## Tilslutning (test-setup)

Nedenstående diagram viser test-opstillingen med:
- ESP32-C3 SuperMini  
- Servo til spjæld  
- WS2812B status-LED  

![Test setup – servo og WS2812B](/images/TestsSetupConnections.png)

![Webinterface – kalibrering](/images/webinterface.png)

### Webinterface (kalibrering)

- Juster `OPEN_DEG` og `CLOSE_DEG` direkte i felterne eller med +/- knapperne.
- Vælg LED-farve for ÅBEN og LUKKET via farvevælgerne; farven opdateres straks på LED’en.
- Tryk **“Gem til NVS”** for at gemme både vinkler og farver permanent i ESP32’ens flash (NVS).

---

## LED-status

| Tilstand | LED |
|--------|-----|
| Spjæld åben | 🟢 Grøn |
| Spjæld lukket | 🔴 Rød |
| Ukendt / init | Slukket |

---

## NVS (Non-Volatile Storage)

Kalibrerede værdier gemmes automatisk i ESP32'ens **NVS** (flash-hukommelse) og bevares ved genstart:

- `OPEN_DEG` – åben position i grader (0–180)
- `CLOSE_DEG` – lukket position i grader (0–180)
- `last_state` – sidste kendte tilstand (åben/lukket)

Værdierne gemmes automatisk når de ændres via webinterfacet, og indlæses automatisk ved opstart.

---

## Vigtige noter

- Brug altid **fælles GND**
- Hvis servoen “brummer” i endestop → justér 2–5 grader tilbage
- mosquitto_sub -h 127.0.0.1 -t "spansug/gate/rondelsliber/#" -v (For at se hvad ESP32 sender)
- mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "OPEN" (Kommando til manuel åbning)
- mosquitto_pub -h 127.0.0.1 -t "spansug/gate/rondelsliber/cmd" -m "CLOSE" (Kommando til manuel lukning)


---

## Projektstatus

- [x] Servo-kalibrering
- [x] Visuel status (WS2812B)
- [ ] MQTT (OPEN / CLOSE kommandoer)
- [ ] Machine_active input (strømsensor / afbryder)
- [ ] Endelig gate-controller firmware

---

## Næste skridt

Denne kode bruges som grundlag for næste version, hvor ESP32’en:

- Sender `machine_active` via MQTT
- Modtager `OPEN` / `CLOSE` kommandoer fra backend
- Indgår i et samlet spånsug-styringssystem

---

FabLab Spinderihallerne  
Vejle




