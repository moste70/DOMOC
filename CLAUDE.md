# DomoC — Contesto di progetto per Claude Code

## Cos'è DomoC

Sistema domotico embedded per il controllo completo di un camper. Ogni funzionalità
fisica (gradino, valvole, termostati, luci, batterie) è gestita da un nodo ESP32
dedicato e autonomo; tutti i nodi comunicano tramite una **rete ESP-Mesh Wi-Fi**.

**Principio architetturale fondamentale**: la logica di sicurezza vive nei nodi, non
nel coordinatore. Se il MASTER/ROOT o l'HMI sono offline, ogni nodo continua ad operare
correttamente in autonomia.

---

## Nodi del sistema

| ID | Nome | MCU | Funzione |
| --- | --- | --- | --- |
| 0x0001 | **MASTER (ROOT)** | ESP32-C3 | Root mesh fisso, registry nodi, NVS, routing OTA, monitoring segnali di sistema. Nessuna logica applicativa. |
| 0x0002 | **STEP** | ESP32-S3-MINI-1 | Gradino motorizzato bidirezionale (H-bridge relay), finecorsa, chiusura automatica su KEY_ON, sensore SHT31 temperatura/umidità esterna |
| 0x0003 | **GREY_WATER** | ESP32-S3-MINI-1 | Valvola acque grigie (H-bridge relay) + telecamera portellone |
| 0x0004 | **FRESH_WATER** | ESP32-S3-MINI-1 | Elettrovalvola acque chiare NC (relay singolo) |
| 0x0005 | **THERMO_BUNK** | ESP32-C3 | Termostato letto a castello, valvola aria calda |
| 0x0006 | **THERMO_LOFT** | ESP32-C3 | Termostato mansarda, valvola aria calda |
| 0x0007 | **THERMO_KITCHEN** | ESP32-C3 | Termostato cucina, valvola aria calda |
| 0x0008 | **REAR_CAM** | ESP32-CAM | Telecamera retromarcia stream MJPEG via HTTP (fuori mesh dati video) |
| 0x0009 | **FRONT_DOOR** | ESP32-S3-MINI-1 | Porta ingresso motorizzata + finecorsa |
| 0x000A | **CAM_EXT** | ESP32-CAM | Telecamere esterne + motion detection, stream MJPEG |
| 0x000B | **HMI** | ESP32-S3R8 | Controller portatile Waveshare Knob: display touch rotondo 1.8" 360×360, encoder, batteria LiPo 800mAh |
| 0x000F | — | — | ID riservato HMI |

> Il nodo ROOT è documentato come **MASTER** nei file più recenti (`Document/nodes/master.md`).
> THERMO_BUNK, THERMO_LOFT, THERMO_KITCHEN e HMI non montano il PCB universale v3.0.
> REAR_CAM e CAM_EXT trasmettono video via stream HTTP diretto, non tramite la mesh.

---

## Hardware — PCB Universale v3.0

Ogni nodo funzione (eccetto HMI, THERMO e CAM) monta lo stesso PCB, sempre completamente
popolato con **ESP32-S3-MINI-1**. Il ruolo di ogni IO è assegnato da `node_config.json`.

**Risorse fisiche per scheda:**

| Risorsa | Qtà | GPIO |
| --- | --- | --- |
| H-bridge relay (K1-K3 per HB1, K4-K6 per HB2) | 2 | HB1: GPIO11/12/13 — HB2: GPIO14/15/16 |
| Relay SPDT generale (K7, K8) | 2 | GPIO17 (REL1), GPIO18 (REL2) |
| Optoisolatore PC817-A (12V isolato) | 2 | GPIO3 (OPT1), GPIO4 (OPT2) |
| Partitore ADC (0–16.5V→0–3.3V) | 2 | GPIO1 (ADC_DIV1), GPIO2 (ADC_DIV2) |
| Bus 1-Wire (DS18B20) | 1 | GPIO10 |
| Bus I2C (pull-up 4.7kΩ fissi) | 1 | GPIO8 (SDA), GPIO9 (SCL) |
| LED RGB WS2812B-2020 | 1 | GPIO21 |
| UART debug | 1 | GPIO43 (TX) / GPIO44 (RX) |
| GPIO liberi (espansione) | 3 | GPIO5 (EXP1), GPIO6 (EXP2), GPIO7 (EXP3) |

**H-bridge relay — logica controllo:**

Ogni H-bridge usa 3 relay SPDT pilotati da NPN (BC547B): K_DIR_A e K_DIR_B per la direzione,
K_ENABLE in serie sul rail 12V_SW. Firmware: K_ENABLE=OFF prima di ogni cambio di direzione.

| K_DIR_A | K_DIR_B | K_ENABLE | Risultato |
| --- | --- | --- | --- |
| OFF | OFF | OFF | Motore scollegato (standby sicuro) |
| ON | OFF | ON | MOT_A=+12V, MOT_B=GND → **APRI** |
| OFF | ON | ON | MOT_A=GND, MOT_B=+12V → **CHIUDI** |
| ON | ON | ON | ⚠ Cortocircuito — **vietato in firmware** |

**Mappa GPIO completa ESP32-S3-MINI-1:**

| GPIO | Segnale | Funzione |
| --- | --- | --- |
| GPIO1 | ADC_DIV1 | Partitore tensione 1 (ADC1_CH0) |
| GPIO2 | ADC_DIV2 | Partitore tensione 2 (ADC1_CH1) |
| GPIO3 | OPT1_OUT | Uscita optoisolatore 1 (active-LOW) |
| GPIO4 | OPT2_OUT | Uscita optoisolatore 2 (active-LOW) |
| GPIO5–7 | EXP1–3 | Liberi — header espansione |
| GPIO8 | SDA | I2C dati |
| GPIO9 | SCL | I2C clock |
| GPIO10 | OW_DATA | Bus 1-Wire |
| GPIO11 | HB1_DIR_A | H-bridge 1 direzione A |
| GPIO12 | HB1_DIR_B | H-bridge 1 direzione B |
| GPIO13 | HB1_EN | H-bridge 1 enable |
| GPIO14 | HB2_DIR_A | H-bridge 2 direzione A |
| GPIO15 | HB2_DIR_B | H-bridge 2 direzione B |
| GPIO16 | HB2_EN | H-bridge 2 enable |
| GPIO17 | REL1 | Relay 1 generale |
| GPIO18 | REL2 | Relay 2 generale |
| GPIO21 | LED_DATA | WS2812B stato |
| GPIO43 | UART_TX | Debug seriale |
| GPIO44 | UART_RX | Debug seriale |

> Non usare GPIO26–GPIO32 su moduli con PSRAM (bus SPI interno). GPIO46 è strapping pin.

Documentazione completa: `Document/pcb_universale_esp32s3.md`

---

## Hardware — HMI (Waveshare ESP32-S3-Knob-Touch-LCD-1.8)

Architettura dual-MCU:

| MCU | Ruolo |
| --- | --- |
| **ESP32-S3R8** (LX7 dual-core 240 MHz, 8MB PSRAM) | ESP-Mesh, display QSPI, touch I2C, audio I2S, LVGL |
| **ESP32-U4WDH** (LX6 dual-core 240 MHz, 4MB Flash) | Gestione encoder, relay eventi a ESP32-S3 via UART interno |

| Periferica | Dettaglio |
| --- | --- |
| Display | 1.8" IPS rotondo 360×360, driver ST77916 via QSPI, backlight PWM GPIO47 |
| Touch | CST816, I2C addr 0x15 — gesture swipe; navigazione principale via encoder |
| Audio | PCM5100A (I2S), jack 3.5mm |
| Vibrazione | DRV2605 via I2C — feedback aptico alert |
| Alimentazione | LiPo 800mAh PH1.25, carica USB-C; sorgente 12V opzionale |

---

## Stack tecnologico

- **Framework**: **Arduino** (ESP32 Arduino Core) — scelta definitiva per semplicità
- **Linguaggio**: C++ (strutture semplici per nodo; niente ereditarietà profonda)
- **RTOS**: FreeRTOS sotto Arduino (trasparente — si usa `loop()` e state machine)
- **Mesh**: **painlessMesh** (sopra Wi-Fi 802.11, auto-organizzante)
- **Libreria condivisa**: `Code/arduino_lib/domoc/` — protocollo, mesh wrapper, LED, motore
- **UI (HMI)**: LVGL 8.3+ su driver QSPI ST77916 (display 360×360) — Fase 3
- **OTA**: `ArduinoOTA` o `Update.h` (integrati nell'ESP32 Arduino Core)

> Il codice ESP-IDF precedente rimane in `Code/nodes/` e `Code/Base/` come riferimento,
> ma lo sviluppo attivo avviene esclusivamente in `Code/nodes_arduino/`.

---

## Architettura della comunicazione mesh

Tutti i messaggi sono binari, header 4 byte + payload max 200 byte + CRC8.

```c
#define MSG_PAYLOAD_MAX 200

typedef struct __attribute__((packed)) {
    uint8_t  msg_type;               // msg_type_t
    uint8_t  src_id;                 // Nodo mittente
    uint8_t  dst_id;                 // Nodo destinatario — 0xFF = broadcast tutti
    uint8_t  seq_num;                // Sequenza per ACK/dedup
    uint8_t  payload[MSG_PAYLOAD_MAX];
} mesh_msg_t;
```

**Tipi di messaggio** (file `Document/messaggi_mesh.md`):

| Codice | Nome | Direzione | Note |
| --- | --- | --- | --- |
| 0x01 | MSG_COMMAND | HMI → Nodo | Azione (apri, chiudi, luci on, …) |
| 0x02 | MSG_STATUS | Nodo → ROOT/HMI | Stato corrente + errori |
| 0x03 | MSG_ALERT | Qualsiasi → ROOT/HMI | Alert/evento (deprecato — usare specifici) |
| 0x05 | MSG_REGISTER | Nodo → ROOT | Registrazione al boot |
| 0x06 | MSG_HEARTBEAT | Nodo → ROOT | Ogni 5s (± 2s jitter) |
| 0x07 | MSG_DESCRIPTOR | Nodo → ROOT | Auto-descrizione azioni/proprietà |
| 0x08 | MSG_DESCRIPTOR_REQ | HMI → ROOT | Richiesta descriptor nodo specifico |
| 0x09 | MSG_REGISTER_ACK | ROOT → Nodo | ID logico assegnato |
| 0x0A | MSG_STATUS_REQ | HMI → ROOT | Richiede dump completo rete |
| 0x0B | MSG_STATUS_RESP | ROOT → HMI | Forwarding heartbeat nodo |
| 0x0C | MSG_REGISTRY_DUMP | ROOT → HMI | Dump registry + tutti i descriptor |
| 0x0D | MSG_NODE_JOINED | ROOT → HMI | Nuovo nodo in rete |
| 0x0E | MSG_NODE_WARNING | ROOT → HMI | Nodo non risponde da > 15s |
| 0x0F | MSG_NODE_OFFLINE | ROOT → HMI | Nodo offline da > 30s |
| 0x10 | MSG_NODE_LOST | ROOT → HMI | Nodo perso da > 120s |
| 0x11 | **MSG_KEY_ON** | ROOT → **broadcast** | Chiave accensione inserita |
| 0x12 | **MSG_KEY_OFF** | ROOT → **broadcast** | Chiave tolta |
| 0x13 | **MSG_STEP_OPEN** | STEP → **broadcast** | Scaletta aperta |
| 0x20 | MSG_OTA_START | ROOT → Nodo | Avvio sessione OTA |
| 0x21 | MSG_OTA_CHUNK | ROOT → Nodo | Blocco firmware (max 200 byte) |
| 0x22 | MSG_OTA_END | ROOT → Nodo | Fine trasferimento, avvia verifica CRC |
| 0x23 | MSG_OTA_ACK | Nodo → ROOT | Conferma chunk / esito OTA |

**Reazioni ai broadcast di sicurezza:**
- `MSG_KEY_ON` → STEP e GREY_WATER chiudono automaticamente se aperti; HMI mostra badge
- `MSG_KEY_OFF` → HMI rimuove badge; nessuna azione automatica sugli attuatori
- `MSG_STEP_OPEN` → HMI mostra icona scaletta aperta (blu); warning se KEY_ON attivo

---

## Configurazione nodo — node_config.json

Salvato in SPIFFS (partizione `config`, 64 KB). Il firmware legge questo file al boot e
configura ogni risorsa hardware in base al campo `"role"`.

Struttura — esempio nodo STEP:

```json
{
  "node_name": "STEP",
  "node_type": "STEP",
  "mesh": { "channel": 6, "mesh_id": "DomoC01" },
  "hardware": {
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
      "motor_run_ms": 4000,
      "opt_fc_closed": "opt1", "opt_fc_open": "opt2"
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "unused" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
    "adc1": { "role": "unused", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "i2c": {
      "gpio_sda": 8, "gpio_scl": 9, "freq_hz": 400000,
      "devices": [{ "address": "0x44", "type": "SHT31", "role": "temp_humidity" }]
    },
    "onewire": {
      "gpio": 10,
      "devices": [{ "address": "28FF641D1C040000", "role": "temp_external" }]
    }
  }
}
```

**Ruoli disponibili:**
- H-bridge (`hb1`, `hb2`): `"motor"`, `"unused"`
- Relay (`rel1`, `rel2`): `"camera"`, `"valve_nc"`, `"lights"`, `"generic_no"`, `"unused"`
- Optoisolatore (`opt1`, `opt2`): `"key_on"`, `"fc_closed"`, `"fc_open"`, `"door_sensor"`, `"button"`, `"generic_di"`, `"unused"`
- ADC (`adc1`, `adc2`): `"vbat_engine"`, `"vbat_service"`, `"voltage_generic"`, `"unused"`
- I2C devices (`i2c.devices[]`): `{address, type, role}` — type: `"INA219"`, `"SHT31"`, `"generic"`
- 1-Wire devices (`onewire.devices[]`): `"temp_ambient"`, `"temp_external"`, `"temp_water"`, `"temp_engine"`, `"unused"`

**Mapping nodi → ruoli PCB:**

| Nodo | HB1 | HB2 | REL1 | REL2 | OPT1 | OPT2 | ADC1 | ADC2 | I2C |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ROOT | — | — | — | — | key_on | — | vbat_eng | — | — |
| STEP | motor | — | — | — | fc_closed | fc_open | — | — | SHT31 |
| GREY_WATER | motor | — | camera | — | fc_closed | fc_open | vbat_svc | — | — |
| FRESH_WATER | — | — | valve_nc | — | — | — | — | — | — |
| FRONT_DOOR | motor | — | — | — | fc_closed | fc_open | — | — | — |

Documentazione completa: `Document/node_config_file.md`

---

## Node Descriptor (auto-descrizione per HMI)

Ogni nodo invia al ROOT (e il ROOT forwarda all'HMI) un `node_descriptor_t` statico
(140 byte) compilato nel firmware. L'HMI costruisce il carosello di icone completamente
da questi dati — nessuna conoscenza hardcoded dei tipi di nodo nell'HMI.

Il descriptor dichiara:
- Icona del nodo nel carosello (`hmi_icon_t`)
- Fino a 4 azioni: codice, icona, tipo controllo UI (`CTRL_BUTTON/TOGGLE/STEPPER/CONFIRM`), label
- Fino a 4 proprietà: tipo dato, offset nel payload, widget LVGL (`WIDGET_LABEL/THERMOMETER/BATTERY/…`), unità, range

Documentazione completa: `Document/comunicazione_nodi.md`

---

## Struttura directory del repository

```
DOMOC/
├── CLAUDE.md               # Questo file
├── BOM.md / PART_LIST.md   # Bill of Materials (duplicati in Document/)
├── Document/               # Documentazione architettura e nodi
└── Code/
    ├── arduino_lib/
    │   └── domoc/          # ✅ Libreria condivisa Arduino (installare in ~/Arduino/libraries/)
    │       ├── library.properties
    │       ├── domoc_protocol.h    # Protocollo binario: struct packed, enum, CRC8
    │       ├── domoc_descriptor.h  # NodeDescriptor per auto-descrizione HMI (140 byte)
    │       ├── DomocMesh.h/cpp     # Wrapper painlessMesh: registrazione, heartbeat, standalone
    │       ├── DomocLed.h          # Helper WS2812B: stati → colori, lampeggio
    │       └── DomocMotor.h        # Driver H-bridge relay con sequenza K_ENABLE sicura
    ├── nodes_arduino/       # ✅ Firmware attivi — sviluppo qui
    │   ├── README.md
    │   ├── master/          # ROOT mesh, registry, KEY_ON, forwarding HMI
    │   ├── step/            # Gradino + SHT31
    │   ├── grey_water/      # Valvola + telecamera portellone + batteria servizio
    │   ├── fresh_water/     # Elettrovalvola NC
    │   ├── front_door/      # Porta motorizzata
    │   ├── thermo_bunk/     # Termostato letto castello
    │   ├── thermo_loft/     # Termostato mansarda
    │   ├── thermo_kitchen/  # Termostato cucina
    │   ├── hmi/             # HMI stub (LVGL Fase 3)
    │   ├── rear_cam/        # ESP32-CAM retromarcia — HTTP MJPEG, NO mesh
    │   └── cam_ext/         # ESP32-CAM esterne — HTTP MJPEG + motion detection
    ├── nodes/               # ⚠ Legacy ESP-IDF — solo riferimento, non sviluppare qui
    └── Base/                # ⚠ Legacy ESP-IDF — solo riferimento
```

**Struttura di ogni nodo Arduino:**

```
Code/nodes_arduino/<nodo>/
├── config.h        # GPIO, timing, costanti specifiche del nodo
├── <nodo>.ino      # setup(), loop(), macchina a stati, callbacks mesh
└── [opzionale]     # file .h/.cpp per logica separabile (sensori, registry, ecc.)
```

---

## Pattern e convenzioni di codice

**Struttura ogni nodo (Arduino):**

```
setup()  →  init periferiche → DomocMesh::begin() → stato iniziale da finecorsa/GPIO
loop()   →  mesh.update() → check_endstops() → check_timeouts() → led.update()
```

**DomocMesh gestisce in autonomia:**
- Registrazione al ROOT (MSG_REGISTER + attesa MSG_REGISTER_ACK)
- Heartbeat ogni 5s ± jitter (chiama il callback `set_on_heartbeat_due`)
- Standalone detection (30s senza mesh → chiama `set_on_standalone_enter`)
- KEY_ON tracking (aggiornato automaticamente da MSG_KEY_ON/OFF)
- CRC8 e codifica hex per il trasporto su painlessMesh

**Ogni nodo dichiara:**
- Un `NodeDescriptor` statico (140 byte) — inviato al ROOT dopo REGISTER_ACK
- Un payload di stato (`XxxStatus` struct packed) — inviato come heartbeat
- Una callback `on_message()` per gestire MSG_COMMAND e altri messaggi rilevanti

**Sicurezza attuatori:**
- Il firmware imposta sempre `K_ENABLE=OFF` prima di cambiare `K_DIR_A/B`
- Ogni attuatore ha un timeout hardware: se il finecorsa non arriva entro N ms, il motore si ferma e transita in `STATE_ERROR`
- Non occorre validare input interni — i valori JSON vengono validati al boot; dopo il boot si fidano dei valori in RAM

**Gestione energia (vincolo critico):**
- `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` su tutti i nodi (riduce consumo Wi-Fi del 30–60%)
- TX power fisso a 10 dBm: `esp_wifi_set_max_tx_power(40)` (sufficiente per < 5m nel camper)
- Periferiche inutilizzate disabilitate via `periph_module_disable()`
- `gpio_hold_en()` prima del light sleep sugli attuatori per mantenere lo stato fisico
- Budget energetico target sistema completo idle: ~80–120 mA @ 12V

**Standalone mode:**
- Se la mesh è assente per > 30s, il nodo entra in STANDALONE: logica locale attiva, mesh disabilitata, LED arancio
- Alla riconnessione: `MSG_REGISTER` con `reconnect=true`, ROOT conferma ID esistente (persistito in NVS)
- La logica di sicurezza (KEY_ON broadcast) funziona anche in STANDALONE perché i nodi si ascoltano direttamente

**Broadcast di sicurezza:**
- `dst_id = 0xFF` — nessun ACK richiesto
- Ogni nodo controlla il proprio stato e reagisce autonomamente, senza attendere conferma da ROOT

**Heartbeat e node registry:**
- Heartbeat ogni 5s ± jitter casuale (0–2s) per evitare burst simultanei
- ROOT marca OFFLINE dopo 15s senza heartbeat, rimuove dopo 120s
- Registry persistita in NVS: al reboot del ROOT i nodi già registrati si ri-registrano con `reconnect=true`

**OTA:**
- Partizione flash dual-bank (`ota_0` / `ota_1`)
- Chunk da 1 KB con CRC per ogni chunk
- Rollback automatico se il nodo non invia heartbeat entro 60s dal reboot

**Commenti nel codice:**
- Solo quando il PERCHÉ non è ovvio (vincolo nascosto, workaround, comportamento sorprendente)
- Non commentare COSA fa il codice — i nomi delle funzioni e variabili devono bastare

---

## File di documentazione

| File | Contenuto |
| --- | --- |
| `Document/esp32-mesh-architecture.md` | Architettura generale mesh, topologia, energia, fasi di sviluppo |
| `Document/comunicazione_nodi.md` | Strutture dati, descriptor, flussi di comunicazione |
| `Document/messaggi_mesh.md` | Elenco completo messaggi con payload, codici azione, sequenze |
| `Document/node_config_file.md` | Formato e validazione node_config.json |
| `Document/pcb_universale_esp32s3.md` | Schema PCB v3.0, blocchi hardware, BOM (~20–24€/scheda) |
| `Document/nodes/master.md` | Firmware MASTER/ROOT: registry, OTA, forwarding HMI |
| `Document/nodes/hmi.md` | Firmware HMI: Waveshare Knob, LVGL, carosello, dual-MCU |
| `Document/nodes/step.md` | Firmware STEP: macchina a stati gradino, SHT31 |
| `Document/nodes/grey_water.md` | Firmware GREY_WATER: valvola, telecamera portellone, batteria servizio |
| `Document/nodes/front_door.md` | Firmware FRONT_DOOR: porta motorizzata, finecorsa |
| `Document/nodes/fresh_water.md` | Firmware FRESH_WATER: valvola NC |
| `Document/nodes/thermo_bunk.md` | Firmware THERMO_BUNK: termostato letto castello |
| `Document/nodes/thermo_kitchen.md` | Firmware THERMO_KITCHEN: termostato cucina |
| `Document/nodes/thermo_loft.md` | Firmware THERMO_LOFT: termostato mansarda, telecamere |
| `Document/nodes/rear_cam.md` | Firmware REAR_CAM: telecamera retromarcia, stream MJPEG HTTP |
| `Document/nodes/cam_ext.md` | Firmware CAM_EXT: telecamere esterne, motion detection |
| `Document/nodes/garage.md` | ⚠ Legacy — funzionalità redistribuite in grey_water.md e front_door.md |
| `Document/ota_process.md` | Processo OTA: flusso completo, payload, rollback automatico |
| `Document/standalone_mode.md` | Modalità standalone: comportamento senza mesh, scenari failure |
| `Document/piano_di_sviluppo.md` | Roadmap fase per fase: Base → STEP → MASTER → HMI → valvole → OTA |
| `Document/BOM.md` | Bill of Materials |
| `Document/PART_LIST.md` | Lista parti con costi e fornitori |
