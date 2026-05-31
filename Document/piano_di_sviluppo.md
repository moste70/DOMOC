# DomoC — Piano di Sviluppo

Roadmap incrementale del firmware. Ogni fase produce codice compilabile e
testabile prima di passare alla successiva. L'ordine segue le dipendenze:
la libreria comune prima, il cuore della mesh subito dopo, poi i nodi uno
alla volta partendo da quello più critico per la sicurezza (STEP).

---

## Fase 0 — Libreria Base `Code/Base` ✅ COMPLETATA

Componente ESP-IDF C++ condiviso: protocollo mesh e classe `NodeBase`.

| Deliverable | File | Stato |
| --- | --- | --- |
| Protocollo sul filo | `include/mesh_protocol.hpp` | ✅ |
| Auto-descrizione nodo | `include/node_descriptor.hpp` | ✅ |
| Classe base | `include/node_base.hpp` | ✅ |
| Implementazione | `src/node_base.cpp` | ✅ |
| Build system | `CMakeLists.txt` | ✅ |

**Criteri di completamento:** `idf.py build` compila senza errori su un
progetto stub che include `domoc_base`.

---

## Fase 1 — Nodo STEP `Code/nodes/step/`

Primo nodo concreto — il più critico per la sicurezza (chiusura automatica
con KEY\_ON). Valida l'intera catena di ereditarietà da `NodeBase`.

### 1.1 Struttura progetto

```
Code/nodes/step/
├── CMakeLists.txt          # progetto ESP-IDF, EXTRA_COMPONENT_DIRS → ../../Base
├── partitions.csv          # dual-bank OTA: ota_0/ota_1 + config (64KB SPIFFS)
├── sdkconfig.defaults      # CONFIG_MESH_ROOT_FIXED, CONFIG_PARTITION_TABLE_CUSTOM
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp            # app_main: istanzia StepNode, legge node_config.json
│   ├── step_node.hpp       # class StepNode : public NodeBase
│   └── step_node.cpp       # macchina a stati gradino + lettura SHT31
└── node_config.json        # sulla partizione SPIFFS (non compilato nel firmware)
```

### 1.2 Macchina a stati gradino

```
IDLE ──[CMD_OPEN]──► OPENING ──[fc_open | timeout]──► OPEN
OPEN ──[CMD_CLOSE]──► CLOSING ──[fc_closed | timeout]──► IDLE
OPEN ──[KEY_ON]────► CLOSING  (chiusura automatica di sicurezza)
OPENING/CLOSING ──[timeout]──► ERROR
ERROR ──[CMD_RESET]──► IDLE
```

| Stato | LED RGB | Note |
| --- | --- | --- |
| IDLE (chiuso) | Verde fisso | Finecorsa fc\_closed attivo |
| OPENING | Blu lampeggio lento | H-bridge in moto verso apertura |
| OPEN | Blu fisso | Finecorsa fc\_open attivo |
| CLOSING | Arancio lampeggio lento | H-bridge in moto verso chiusura |
| STANDALONE | Arancio fisso | Mesh assente > 30s |
| ERROR | Rosso lampeggio | Timeout motore senza finecorsa |

### 1.3 Payload di stato (`StepStatus`)

```cpp
struct __attribute__((packed)) StepStatus {
    uint8_t  state;          // StepState enum (IDLE/OPENING/OPEN/CLOSING/ERROR)
    uint8_t  error_code;     // 0 = nessun errore
    int16_t  temp_x10;       // SHT31: temperatura ×10 (es. 235 = 23.5°C)
    uint16_t humidity_x10;   // SHT31: umidità ×10 (es. 650 = 65.0%)
};
```

### 1.4 NodeDescriptor STEP

| Campo | Valore |
| --- | --- |
| `node_icon` | `ICON_STEP` |
| Azioni | OPEN (`ICON_ACT_OPEN`, `CTRL_CONFIRM`), CLOSE (`ICON_ACT_CLOSE`, `CTRL_CONFIRM`) |
| Proprietà | `PROP_STATE` (WIDGET\_INDICATOR), `PROP_TEMPERATURE` (WIDGET\_THERMOMETER), `PROP_HUMIDITY` (WIDGET\_VALUE\_UNIT) |

### 1.5 Task aggiuntivo

| Task | Priorità | Stack | Funzione |
| --- | --- | --- | --- |
| `step_task` | 4 | 4 KB | Polling finecorsa, controllo H-bridge, timeout motore, lettura SHT31 ogni 30s |

### 1.6 Criteri di completamento

- `idf.py build` senza errori
- Flash + monitor su hardware reale: gradino apre e chiude su CMD\_OPEN/CLOSE
- KEY\_ON broadcast provoca chiusura automatica da stato OPEN
- Temperatura/umidità presenti nell'heartbeat
- Timeout motore → STATE\_ERROR se finecorsa non arriva entro `motor_run_ms`

---

## Fase 2 — Nodo MASTER/ROOT `Code/nodes/master/`

Cuore della mesh: registry nodi, routing messaggi, forwarding verso HMI.
Non ha logica applicativa — solo infrastruttura.

### 2.1 Responsabilità

| Funzione | Descrizione |
| --- | --- |
| Registry | Mappa `node_id → {mac, descriptor, last_seen_ms}` persistita in NVS |
| Routing | Ridefinisce `transmit_frame()` per instradare verso nodo specifico (downstream) |
| Heartbeat monitor | Marca WARNING > 15s, OFFLINE > 30s, LOST > 120s; pubblica MSG\_NODE\_* verso HMI |
| Forwarding HMI | Ogni MSG\_STATUS ricevuto → MSG\_STATUS\_RESP verso HMI |
| KEY\_ON | Legge optoisolatore GPIO3 → broadcast MSG\_KEY\_ON / MSG\_KEY\_OFF |
| Battery monitor | Legge ADC GPIO1 (partitore) → include `vbat_engine` nel MSG\_KEY\_ON |

### 2.2 Override `transmit_frame()`

Il MASTER riceve tutti i frame upstream (è ROOT). Quando deve inviare a un
nodo specifico:

```cpp
esp_err_t MasterNode::transmit_frame(uint8_t dst_id, const uint8_t* frame, size_t len) {
    mesh_addr_t dst_mac;
    if (!registry_.get_mac(dst_id, dst_mac)) return ESP_ERR_NOT_FOUND;
    mesh_data_t data{ .data=const_cast<uint8_t*>(frame), .size=len,
                      .proto=MESH_PROTO_BIN, .tos=MESH_TOS_P2P };
    return esp_mesh_send(&dst_mac, &data, MESH_DATA_P2P, nullptr, 0);
}
```

### 2.3 Task aggiuntivi

| Task | Priorità | Stack | Funzione |
| --- | --- | --- | --- |
| `heartbeat_monitor` | 4 | 2 KB | Scansione registry ogni 5s, emit MSG\_NODE\_* |
| `nvs_persist` | 2 | 2 KB | Scrittura asincrona registry su NVS |
| `key_monitor` | 5 | 2 KB | Polling optoisolatore GPIO3 con debounce 50ms |

### 2.4 Criteri di completamento

- STEP si registra al MASTER al boot
- MASTER forwarda heartbeat STEP verso HMI (visibile via monitor seriale)
- KEY\_ON reale (contatto 12V sull'optoisolatore) genera broadcast corretto
- Registry persistita: dopo reboot MASTER i nodi si ri-registrano con `reconnect=true`

---

## Fase 3 — Nodo HMI `Code/nodes/hmi/`

Controller portatile: display rotondo 1.8" 360×360, encoder, batteria LiPo.
Architettura dual-MCU (ESP32-S3R8 + ESP32-U4WDH).

### 3.1 Architettura software HMI

```
HMI firmware (ESP32-S3R8)
├── mesh_layer/          # NodeBase derivato — solo ricezione, nessun attuatore
├── ui_layer/            # LVGL 8.3: carosello nodi, pannello dettaglio
├── encoder_bridge/      # UART da ESP32-U4WDH → eventi LVGL
└── display_driver/      # ST77916 QSPI + CST816 touch I2C
```

### 3.2 Carosello nodi (UI principale)

- Un'icona per nodo, costruita dinamicamente dal `NodeDescriptor` ricevuto
- Selezione encoder → pannello dettaglio con azioni e proprietà del nodo
- Badge KEY\_ON (fisso in alto) quando `MSG_KEY_ON` attivo
- Badge STEP OPEN (fisso in alto) quando `MSG_STEP_OPEN` ricevuto e KEY\_ON attivo

### 3.3 Criteri di completamento

- Carosello mostra STEP dopo boot MASTER
- CMD\_OPEN/CLOSE inviati toccando il controllo LVGL corretto
- Badge KEY\_ON appare/scompare in risposta ai broadcast
- Temperatura/umidità STEP visibili nel pannello dettaglio

---

## Fase 4 — Nodi valvole: GREY\_WATER e FRESH\_WATER

Nodi più semplici — validano la porta `NodeBase` verso nodi senza sensori complessi.

### 4.1 GREY\_WATER (`Code/nodes/grey_water/`)

- H-bridge HB1: valvola acque grigie (fc\_closed/fc\_open su OPT1/OPT2)
- REL1: alimenta telecamera portellone (camera role)
- ADC1: tensione batteria servizio (0–16.5V → 0–3.3V)
- Chiusura automatica su KEY\_ON (stessa logica STEP)

### 4.2 FRESH\_WATER (`Code/nodes/fresh_water/`)

- REL1 GPIO17: elettrovalvola NC — relay ON = aperta, relay OFF = chiusa (fail-safe)
- Nessun finecorsa — stato dedotto dalla posizione relay
- KEY\_ON: nessuna azione automatica (NC è già stato sicuro)

---

## Fase 5 — Nodi termostato: THERMO\_BUNK, THERMO\_LOFT, THERMO\_KITCHEN

Stessa struttura — differiscono solo per il setpoint e la zona.

### 5.1 Struttura comune

- ESP32-C3 (non PCB universale v3.0)
- Sensore temperatura I2C (NTC o SHT31)
- Valvola aria calda: relay ON/OFF
- Controllo PID con isteresi ±0.5°C
- Setpoint persistito in NVS, modificabile da HMI (action TEMP\_UP/TEMP\_DN)

---

## Fase 6 — OTA distribuito

Aggiunto a tutti i nodi dopo che la mesh è stabile e testata.

### 6.1 Flusso OTA

```
HMI ──MSG_OTA_START──► ROOT ──MSG_OTA_START──► Nodo
                       ROOT ──MSG_OTA_CHUNK──► Nodo (loop)
                       ROOT ──MSG_OTA_END────► Nodo
                       Nodo ──MSG_OTA_ACK───► ROOT (per ogni chunk + finale)
```

### 6.2 Rollback automatico

- Dopo reboot post-OTA il nodo deve inviare heartbeat entro 60s
- ROOT non riceve heartbeat → marca il nodo per rollback e invia MSG\_OTA\_START
  con il firmware precedente

### 6.3 Deliverable

- `ota_receiver_task` (priorità 2, stack 6 KB) aggiunto a `NodeBase` o opzionale
  tramite `#define DOMOC_OTA_ENABLED`
- `ota_distributor_task` (priorità 1, stack 8 KB) nel MASTER
- Test: flash nodo STEP con firmware v2 via HMI, verifica rollback su boot fallito

---

## Fase 7 — Telecamere: REAR\_CAM e CAM\_EXT

Nodi ESP32-CAM fuori dalla mesh dati video — stream MJPEG via HTTP diretto.

### 7.1 Architettura

- Mesh: solo segnalazione (presenza, motion detected, stream URL)
- Video: HTTP server locale sulla stessa rete Wi-Fi, indipendente dalla mesh
- HMI apre stream via URL ricevuto nel MSG\_STATUS del nodo CAM

---

## Fase 8 — FRONT\_DOOR

Porta ingresso motorizzata — identica a STEP ma senza sensore SHT31.

---

## Ordine di implementazione consigliato

```
Fase 0 ✅ → Fase 1 → Fase 2 → Fase 3 → Fase 4 → Fase 5 → Fase 6 → Fase 7/8
 Base       STEP     MASTER    HMI      Valvole   Thermo    OTA      Cam/Door
```

Le fasi 4–8 sono parallelizzabili una volta completate le fasi 1–3.

---

## Convenzioni trasversali

- Ogni nodo ha `node_config.json` sulla partizione SPIFFS `config` (64 KB)
- Ogni nodo usa dual-bank OTA: `ota_0` + `ota_1` + `config`
- LED RGB WS2812B: verde=ok, blu=aperto/in_moto, arancio=standalone/chiusura, rosso=errore
- `idf.py build` deve essere pulito (zero warning) su ogni fase prima del merge
- La libreria `Code/Base` è l'unico punto di modifica del protocollo mesh
