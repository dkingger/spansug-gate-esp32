# Node-RED – kørende opsætning

`spansug-live.json` er arbejdsfilen baseret på en kopi af flowfilen fra `spansug-backend_spind`, modtaget 19. november 2026. Originalen indeholder 213 noder og 14 faner. Arbejdsfilen har yderligere fire noder til rondelsliberens panellys.

Originalens SHA-256: `ce5abc93ee201a675b95cd2adb24ae6f0bdd6b1e1d897723f2b33ab2fc625074`

Aktivt projekt på Pi'en: `Spaansug`. Flowfil: `/home/pi/.node-red/projects/Spaansug/flows.json`. Stien er fundet i projektkonfigurationen; opstartsloggen var ikke længere tilgængelig.

Eksporten erstatter de tidligere separate flowfiler som projektets udgangspunkt. De oprindelige noder er bevaret, herunder de manuelle relæknapper og simulatoren. Rondelsliberens OPEN/WAIT/CLOSE-forgreninger er suppleret med panelstatus. Knappernes topic `spansug/relay/cmd` har ingen modtager i eksporten. Der er ingen særskilt opstartssekvens, som sender CLOSE til alle spjæld.

Tidligere lokale flows, herunder `System Initialization`, ligger i [arkivet](../../archive/backend-before-live-sync/node-red/). De importeres ikke automatisk.

Ingen credentials-fil følger med. En ny installation kan kræve dashboard- og GPIO-noder samt separat konfiguration af credentials.

## Rondelsliberens panellys

OPEN og WAIT publicerer `open`, CLOSE publicerer `closed` på `spansug/gate/rondelsliber/state` (QoS 1, ikke retained). Det eksisterende LED-flow bruger dette til GPIO 19. Status viser den ønskede tilstand, uden fysisk spjældfeedback.

Originaleksporten ligger uændret i `archive/live-snapshot-2026-11-19/` fra repositoryets rod.

En afgrænset import ligger i `backend/node-red-imports/rondelsliber-panel-led.json`. Importér via Node-RED-editoren og vælg at erstatte/opdatere eksisterende noder med samme ID; opret ikke en ekstra kopi af fanen. Kontrollér importoversigten: kun Rondelsliber Gate ændres, med fire nye noder og tre ekstra forbindelser. Brokerkonfigurationen medfølger som eksisterende reference. Deploy først ved en passende pause i driften, da genstart af fanens noder kan påvirke en igangværende efterløbstimer. Der er ikke deployet fra denne computer.

### Deploymentstatus

Brugeren har bekræftet deployment på `spansug-backend_spind` den 19. november 2026 via Modified Nodes. Fire noder og seks forbindelser blev tilføjet uden ekstra fane eller broker. Node-RED fortsatte med samme proces-ID, og der blev ikke rapporteret fejl i kontrolperioden. Backup på Pi’en: `/home/pi/.node-red/backups/Spaansug-flows-before-panel-led-20261119-205721.json`. Fysisk test af OPEN → WAIT → CLOSE afventer tilstedeværelse i værkstedet.
