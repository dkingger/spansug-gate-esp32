# Spånsug – FabLab Spinderihallerne

Automatisk spånsug til værkstedets maskiner. ESP32-enheder sender maskinstatus via MQTT til en Raspberry Pi, hvor Node-RED styrer spjæld og spånsugermotor.

## Find rundt

| Mappe | Indhold |
| --- | --- |
| [firmware](firmware/README.md) | Aktuel ESP32-C3-strømsensor til rondelsliberen |
| [backend](backend/README.md) | Raspberry Pi, Node-RED, dashboard og scripts |
| [panel/skilte](panel/README.md) | Skilte til panelet i værkstedet og tilhørende fontpakke |
| [docs](docs/README.md) | Systembeskrivelse, hardware og billeder |
| [archive](archive/README.md) | Tidligere firmware og oprindelig buildopsætning |

## Arbejd med ESP32

Åbn `firmware/` som projekt i PlatformIO, eller åbn `kode-workspace.code-workspace`, der viser både overblikket og firmwareprojektet.

```sh
cp firmware/src/secrets.h.example firmware/src/secrets.h
# Udfyld lokale Wi-Fi-oplysninger i secrets.h.
pio run --project-dir firmware -e currentsensor
```

Se firmwarevejledningen for USB-upload og kalibrering. De arkiverede programmer indgår ikke i dette build.
