# Raspberry Pi-backend

- `node-red/`: MQTT-styrelogik, efterløb, spjæld, motorrelæ og status.
- `web/`: dashboard og startside.
- `scripts/`: installation, deployment, import og fejlsøgning.

## Kørende installation

På `spansug-backend_spind` peger Node-REDs konfiguration på projektet `Spaansug` i `/home/pi/.node-red/projects/Spaansug/`. Apache serverer `/var/www/html/`. De lokale HTML-filer matcher Pi’ens filer via SHA-256. Live-floweksporten er nu udgangspunktet i `node-red/spansug-live.json`.

`~/spansug-backend/` findes ikke på den undersøgte Pi; det er deployscriptets mellemmappe, som oprettes ved deployment. Scripts bruger stadig det historiske hostname `spansug-backend.local`; kontrollér destinationen før brug.

## Deployment fra computeren

Fra repositoryets rod:

```sh
bash backend/scripts/deploy-to-pi.sh
```

På Windows: `powershell -ExecutionPolicy Bypass -File backend/scripts/deploy-to-pi.ps1`. Tilføj `-QuickUpdate` for automatisk flowimport. Scripts bruger `spansug-backend.local` og kopierer til `~/spansug-backend/` på Pi'en. Flowimport målretter projektet `Spaansug` og tager backup før skrivning.

Ved første installation køres `bash install-pi.sh` på Pi'en fra `~/spansug-backend/`. Flowimport kan køres med `bash import-nodered-flows.sh`, når scriptet er kopieret dertil. Linux-deployscriptet kopierer installations- og debugscriptet; kopiér importscriptet separat, hvis det skal bruges på Pi'en.

Dashboard: `http://spansug-backend.local/`. Node-RED: `http://spansug-backend.local:1880`.

Se [Node-RED-dokumentationen](node-red/README.md). Deployment og installation ændrer den tilsluttede Pi; oprydningen i repositoryet udfører ikke disse handlinger.
