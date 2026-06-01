# DomoC — Nodi Arduino

Tutti i firmware del sistema usano **Arduino framework** con **painlessMesh**.

## Struttura

```
nodes_arduino/
├── README.md              # questo file
│
├── (libreria condivisa — installare in Arduino/libraries/domoc/)
│   ../../arduino_lib/domoc/
│   ├── domoc_protocol.h     Protocollo binario: struct, enum, CRC8
│   ├── domoc_descriptor.h   NodeDescriptor per auto-descrizione HMI
│   ├── DomocMesh.h/cpp      Wrapper painlessMesh (registrazione, heartbeat, CRC)
│   ├── DomocLed.h           Helper WS2812B (stati → colori, lampeggio)
│   └── DomocMotor.h         Driver H-bridge relay (STEP, GREY_WATER)
│
├── master/       ROOT mesh — registry nodi, KEY_ON, forwarding HMI
├── step/         Gradino motorizzato + SHT31 (temperatura/umidità esterna)
├── grey_water/   Valvola acque grigie + telecamera portellone + batteria servizio
├── fresh_water/  Elettrovalvola acque chiare (relay NC)
├── thermo_bunk/  Termostato letto a castello (DS18B20 + relay valvola aria)
├── thermo_loft/  Termostato mansarda (identico a thermo_bunk)
├── thermo_kitchen/ Termostato cucina (identico a thermo_bunk)
├── hmi/          Controller HMI — stub (LVGL Fase 3)
├── rear/         Telecamera retromarcia + tensione batteria servizio — HTTP (NO mesh)
└── cam_ext/      Telecamere esterne + motion detection — HTTP MJPEG (NO mesh)
```

## Installazione libreria domoc

**Arduino IDE**: Copia `Code/arduino_lib/domoc/` in `~/Arduino/libraries/domoc/`

**arduino-cli**:
```bash
cp -r Code/arduino_lib/domoc ~/Arduino/libraries/
```

## Librerie esterne richieste

Installabili via Library Manager o arduino-cli:

| Libreria | Usata da |
|---|---|
| `painlessMesh` | tutti i nodi (tranne rear_cam, cam_ext) |
| `FastLED` | tutti i nodi con WS2812B |
| `Adafruit SHT31 library` | step |
| `Adafruit BusIO` | step (dipendenza SHT31) |
| `DallasTemperature` | thermo_bunk, thermo_loft, thermo_kitchen |
| `OneWire` | thermo_* (dipendenza DallasTemperature) |

## Board settings

| Nodo | Board | Note |
|---|---|---|
| master | ESP32C3 Dev Module | |
| step, grey_water, fresh_water | ESP32S3 Dev Module | Partition: Huge APP |
| thermo_bunk/loft/kitchen | ESP32C3 Dev Module | |
| hmi | ESP32S3 Dev Module | PSRAM: OPI PSRAM |
| rear, cam_ext | AI Thinker ESP32-CAM | |

## Protocollo mesh

Il protocollo binario è invariato rispetto alla versione ESP-IDF:
- Header 4 byte: `msg_type | src_id | dst_id | seq_num`
- Payload max 200 byte
- CRC8 (CRC-8/MAXIM) appeso in coda

painlessMesh trasporta `String`, quindi i frame binari vengono codificati in
esadecimale (`DomocMesh` gestisce la conversione in modo trasparente).
