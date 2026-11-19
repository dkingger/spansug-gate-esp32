# Systemets opbygning

Strømsensor → ESP32-C3 → MQTT → Node-RED på Raspberry Pi → spjæld og spånsugermotor.

Den aktive firmware måler maskinstrøm og sender maskinstatus, strøm og heartbeat. Node-RED håndterer åbning, efterløb og lukning. En central Relay Manager koordinerer motoren på tværs af spjæld. Dashboardet viser status.

MQTT-broker og topics indstilles i `firmware/src/config.h`. Wi-Fi-oplysninger ligger lokalt i `firmware/src/secrets.h`, som ignoreres af Git.
