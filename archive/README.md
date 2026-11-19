# Tidligere firmware

Disse filer lå tidligere under `src/old_code/`. De er bevaret til reference og er ikke med i det aktive firmwarebuild. Placeringen siger ikke noget om, hvilken firmware der stadig kører på værkstedets enheder.

- `Calibration/`: servokalibrering.
- `RemoteSensor/`: ekstern maskinsensor.
- `CurrentSensor/`: tidligere samlet strømsensorfirmware.
- `platformio.ini.legacy`: den oprindelige buildopsætning med historiske miljøer. Stierne passer til den tidligere struktur og skal tilpasses før brug.

Der blev ikke fundet en separat servospjæld-firmware blandt disse kildefiler. Den gamle hovedvejledning omtaler den; se [historisk dokumentation](../docs/legacy-README.md).
