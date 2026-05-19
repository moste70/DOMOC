# DomoC — Contesto di progetto per Claude Code

## Cos'è DomoC

Sistema domotico embedded per il controllo completo di un camper. Ogni funzionalità
fisica (gradino, valvole, termostati, luci, batterie) è gestita da un nodo ESP32
dedicato e autonomo; tutti i nodi comunicano tramite una **rete ESP-Mesh Wi-Fi**.

**Principio architetturale fondamentale**: la logica di sicurezza vive nei nodi, non
nel coordinatore. Se il ROOT o l'HMI sono offline, ogni nodo continua ad operare
correttamente in autonomia.

---

## Nodi del sistema

| ID | Nome | MCU | Funzione |
| --- | --- | --- | --- |
| 0x01 | **ROOT** | ESP32-C3 | Root mesh fisso, registry nodi, NVS, routing OTA. Nessuna logica applicativa. |
| 0x02 | **STEP** | ESP32-C3 | Gradino motorizzato bidirezionale (H-bridge), finecorsa, chiusura automatica su KEY_ON |
| 0x03 | **GARAGE** | ESP32-C3 | Valvola grigie (H-bridge) + luci + portellone + monitor batterie (INA219/ADC) |
| 0x04 | **FRESH_WATER** | ESP32-C3 | Elettrovalvola acque chiare NC (relay singolo) |
| 0x05 | **THERMO_BUNK** | ESP32-C3 | Termostato letto a castello, valvola aria calda, luci zona |
| 0x06 | **THERMO_LOFT** | ESP32-C3 | Termostato mansarda, valvola aria calda, telecamere |
| 0x07 | **HMI** | ESP32-S3 | Controller portatile: display touch rotondo 1.8" 360×360, encoder, batteria LiPo |

> THERMO_BUNK, THERMO_LOFT e HMI usano moduli con display integrato e **non** montano
> il PCB universale v2.0 descritto in questo progetto.

---

## Hardware — PCB Universale v2.0

Ogni nodo (eccetto HMI e THERMO con display) monta lo stesso PCB, sempre completamente
popolato. Il ruolo di ogni IO è assegnato da `node_config.json`.

**Risorse fisiche per scheda:**

| Risorsa | Qtà | GPIO |
| --- | --- | --- |
| H-bridge relay (K1-K3) | 2 | HB1: GPIO11/12/13 — HB2: GPIO14/15/16 |
| Relay SPDT generale | 2 | GPIO17 (REL1), GPIO18 (REL2) |
| Optoisolatore PC817 (12V isolato) | 4 | GPIO3/4/5/6 |
| Partitore ADC (0–16.5V→0–3.3V) | 2 | GPIO1 (ADC_DIV1), GPIO2 (ADC_DIV2) |
| Bus 1-Wire (DS18B20) | 1 | GPIO10 |
| I2C espansione | 1 | GPIO8 (SDA), GPIO9 (SCL) |
| LED RGB WS2812B | 1 | GPIO21 |
| UART debug | 1 | GPIO43/44 |

Documentazione completa: `Document/pcb_universale_esp32s3.md`

---

## Stack tecnologico

- **Framework**: ESP-IDF 4.x/5.x — non Arduino
- **Linguaggio**: C (con cJSON per la configurazione)
- **RTOS**: FreeRTOS (integrato in ESP-IDF)
- **Mesh**: ESP-Mesh (libreria ufficiale Espressif, sopra Wi-Fi 802.11)
- **Storage**: NVS (persistenza runtime) + SPIFFS partizione `config` (JSON)
- **UI (HMI)**: LVGL 8.3+ su driver QSPI ST77916 (display 360×360)
- **OTA**: dual-bank app0/app1 con rollback automatico entro 60s

---

## Architettura della comunicazione mesh

Tutti i messaggi sono binari, header 4 byte + payload max 200 byte + CRC8.

```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;   // msg_type_t (vedere sotto)
    uint8_t  src_id;     // Nodo mittente
    uint8_t  dst_id;     // Nodo destinatario — 0xFF = broadcast tutti
    uint8_t  seq_num;    // Sequenza per ACK/dedup
    uint8_t  payload[MSG_PAYLOAD_MAX];
} mesh_msg_t;
```

**Tipi di messaggio principali** (file `Document/messaggi_mesh.md`):

| Codice | Nome | Direzione | Note |
| --- | --- | --- | --- |
| 0x01 | MSG_COMMAND | HMI → Nodo | Azione (apri, chiudi, luci on, …) |
| 0x02 | MSG_STATUS | Nodo → ROOT/HMI | Stato corrente + errori |
| 0x05 | MSG_REGISTER | Nodo → ROOT | Registrazione al boot |
| 0x06 | MSG_HEARTBEAT | Nodo → ROOT | Ogni 5s (± 2s jitter) |
| 0x07 | MSG_DESCRIPTOR | Nodo → ROOT/HMI | Auto-descrizione azioni/proprietà |
| 0x11 | **MSG_KEY_ON** | ROOT → **broadcast** | Chiave accensione inserita |
| 0x12 | **MSG_KEY_OFF** | ROOT → **broadcast** | Chiave tolta |
| 0x13 | **MSG_STEP_OPEN** | STEP → **broadcast** | Scaletta aperta |
| 0x20–0x23 | MSG_OTA_* | ROOT ↔ Nodo | Aggiornamento firmware |

**Reazioni ai broadcast di sicurezza:**
- `MSG_KEY_ON` → STEP e GARAGE chiudono automaticamente se aperti; HMI mostra badge
- `MSG_STEP_OPEN` → HMI mostra icona scaletta aperta; warning se KEY_ON attivo

---

## Configurazione nodo — node_config.json

Salvato in SPIFFS (partizione `config`, 64 KB). Il firmware legge questo file al boot e
configura ogni risorsa hardware in base al campo `"role"`.

Struttura minima:

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
      "opt_fc_closed": "opt2", "opt_fc_open": "opt1"
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "unused" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_open",   "gpio": 3 },
    "opt2": { "role": "fc_closed", "gpio": 4 },
    "opt3": { "role": "unused",    "gpio": 5 },
    "opt4": { "role": "unused",    "gpio": 6 },
    "adc1": { "role": "unused",    "gpio": 1 },
    "adc2": { "role": "unused",    "gpio": 2 },
    "onewire": {
      "gpio": 10,
      "devices": [{ "address": "28FF641D1C040000", "role": "temp_external" }]
    }
  }
}
```

**Ruoli disponibili:**
- H-bridge: `"motor"`, `"unused"`
- Relay: `"camera"`, `"valve_nc"`, `"lights"`, `"generic_no"`, `"unused"`
- Optoisolatore: `"key_on"`, `"fc_closed"`, `"fc_open"`, `"door_sensor"`, `"button"`, `"generic_di"`, `"unused"`
- ADC: `"vbat_engine"`, `"vbat_service"`, `"voltage_generic"`, `"unused"`
- 1-Wire: `"temp_ambient"`, `"temp_external"`, `"temp_water"`, `"temp_engine"`, `"unused"`

Documentazione completa: `Document/node_config_file.md`

---

## Node Descriptor (auto-descrizione per HMI)

Ogni nodo invia al ROOT (e il ROOT forwarda all'HMI) un `node_descriptor_t` statico
(140 byte) compilato nel firmware. L'HMI costruisce l'interfaccia utente completamente
da questi dati — nessuna conoscenza hardcoded dei tipi di nodo nell'HMI.

Il descriptor dichiara:
- Icona del nodo nel carosello (`hmi_icon_t`)
- Fino a 4 azioni: codice, icona, tipo controllo UI (`CTRL_BUTTON/TOGGLE/STEPPER/CONFIRM`), label
- Fino a 4 proprietà: tipo dato, offset nel payload, widget LVGL (`WIDGET_LABEL/THERMOMETER/BATTERY/…`), unità, range

Documentazione completa: `Document/comunicazione_nodi.md`

---

## Struttura directory firmware (riferimento)

```
firmware/
├── shared/
│   ├── protocol/           # mesh_protocol.h, node_descriptor.h
│   ├── mesh_manager/       # Wrapper ESP-Mesh
│   └── nvs_store/          # Astrazione NVS
├── root/                   # Nodo ROOT
├── hmi/                    # Nodo HMI (LVGL, display, encoder)
└── nodes/
    ├── step/
    ├── grey_water/
    ├── fresh_water/
    ├── thermo/
    └── …
```

---

## Pattern e convenzioni di codice

**Task FreeRTOS per nodo funzione:**
- `mesh_rx_task` (prio 5): ricezione messaggi, dispatch per `msg_type`
- `state_machine_task` (prio 4): macchina a stati attuatore
- `sensor_task` (prio 3): lettura periodica sensori (ogni 60s)
- `heartbeat_task` (prio 2): invio MSG_HEARTBEAT ogni 5s ± jitter

**Sicurezza attuatori:**
- Il firmware imposta sempre `K_ENABLE=OFF` prima di cambiare `K_DIR_A/B`
- Ogni attuatore ha un timeout hardware: se il finecorsa non arriva entro N ms, il motore si ferma e transita in `STATE_ERROR`
- Non occorre validare input interni — i valori JSON vengono validati al boot; dopo il boot si fidano dei valori in RAM

**Gestione energia:**
- `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` su tutti i nodi
- TX power fisso a 10 dBm (sufficiente per < 5m nel camper)
- Periferiche inutilizzate disabilitate via `periph_module_disable()`

**Standalone mode:**
- Se la mesh è assente per > 30s, il nodo entra in STANDALONE: pulsante locale attivo, mesh disabilitata, LED arancio
- Alla riconnessione: `MSG_REGISTER` con `reconnect=true`, ROOT conferma ID esistente

**Broadcast di sicurezza:**
- `dst_id = 0xFF` — nessun ACK richiesto
- Ogni nodo controlla il proprio stato e reagisce autonomamente, senza attendere conferma da ROOT

**Commenti nel codice:**
- Solo quando il PERCHÉ non è ovvio (vincolo nascosto, workaround, comportamento sorprendente)
- Non commentare COSA fa il codice — i nomi delle funzioni e variabili devono bastare

---

## File di documentazione

| File | Contenuto |
| --- | --- |
| `Document/esp32-mesh-architecture.md` | Architettura generale mesh e topologia |
| `Document/comunicazione_nodi.md` | Strutture dati, descriptor, flussi di comunicazione |
| `Document/messaggi_mesh.md` | Elenco completo messaggi con payload e tabella reazioni |
| `Document/node_config_file.md` | Formato e validazione node_config.json |
| `Document/pcb_universale_esp32s3.md` | Schema PCB v2.0, blocchi hardware, BOM |
| `Document/nodes/root.md` | Firmware ROOT: registry, heartbeat monitor, NVS |
| `Document/nodes/hmi.md` | Firmware HMI: LVGL, carosello, discovery |
| `Document/nodes/step.md` | Firmware STEP: macchina a stati gradino |
| `Document/nodes/garage.md` | Firmware GARAGE: valvole, batteria, luci |
| `Document/nodes/fresh_water.md` | Firmware FRESH_WATER |
| `Document/nodes/thermo_bunk.md` | Firmware THERMO_BUNK |
| `Document/nodes/thermo_loft.md` | Firmware THERMO_LOFT |
