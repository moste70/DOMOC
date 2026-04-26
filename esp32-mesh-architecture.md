# DomoC — Sistema di Controllo Camper su Rete Mesh ESP32

---

## 0. CONTESTO DEL PROGETTO

**DomoC** è un sistema domotico embedded per il controllo completo di un camper, basato su una rete mesh di microcontrollori ESP32. Ogni funzionalità del camper è gestita da un nodo dedicato.

### Principio architetturale

Il sistema adotta una **separazione netta tra infrastruttura mesh e interfaccia utente**:

- Il **nodo ROOT** (ESP32-C3, fisso, sempre alimentato a 12V) è il cuore della rete: gestisce il routing mesh, la node registry e la persistenza dello stato. Non ha display né logica applicativa.
- Il **nodo HMI** (ESP32-S3 + display touch + batteria) è un nodo come gli altri: si connette e disconnette liberamente dalla mesh senza impatto sulla rete. Può essere usato come telecomando portatile.
- **Ogni nodo funzione** è completamente autonomo: contiene la propria logica di sicurezza e reagisce agli eventi mesh senza dipendere dal ROOT o dall'HMI.

### Nodi del sistema

| ID | Nome nodo | Tipo | Funzione |
|---|---|---|---|
| `0x0001` | **ROOT** | Infrastruttura | Root fisso mesh + registry persistente + sensori critici (chiave, batteria motore) |
| `0x0002` | **STEP** | Sensore + Attuatore | Gradino motorizzato + rilevamento stato + sensore temperatura esterna |
| `0x0003` | **GARAGE (GREY_WATER)** | Sensore + Attuatore | Valvola acque grigie bipolare + monitoraggio batteria di servizio (critico) |
| `0x0004` | **FRESH_WATER** | Attuatore | Valvola acque chiare normalmente chiusa (NC) |
| `0x0005` | **THERMO_BUNK** | Sensore + Attuatore | Termostato letto + comando valvola aria calda |
| `0x0006` | **THERMO_LOFT** | Sensore + Attuatore | Termostato mansarda + comando valvola aria calda |
| `0x0007` | **THERMO_KITCHEN** | Sensore + Attuatore | Termostato cucina + comando valvola aria calda |
| `0x0008` | **REAR_CAM** | Sensore video | Telecamera retromarcia stream MJPEG |
| `0x0009` | **FRONT_DOOR** | Attuatore | Porta ingresso motorizzata + finecorsa |
| `0x000A` | **CAM_EXT** | Sensore video | Telecamere esterne (fronte/lato/retro) + motion detection |
| `0x000B` | **HMI** | Interfaccia utente | Display touch 3.5-4.3" + batteria LiPo — monitoraggio e controllo manuale |

### Note architetturali specifiche per il camper

- **Alimentazione**: tutti i nodi sono alimentati a 12V DC dalla batteria del camper (AGM/LiFePO4, capacità tipica 100–200Ah). Il bus 12V viene convertito localmente a 3.3V tramite buck converter ad alta efficienza (es. MP2307, efficienza >90%). **Il consumo energetico è un vincolo critico di progetto**: ogni milliampere risparmiato si traduce in maggiore autonomia. Ogni nodo deve essere progettato con la minima dissipazione possibile, specialmente nelle fasi di idle e standby.
- **Ambiente**: vibrazioni, umidità, sbalzi termici. Preferire connettori con lock meccanico (es. JST-GH) e conformal coating sulla PCB.
- **Telecamera retromarcia** (`REAR_CAM`): è un caso speciale. Il video non transita sulla mesh ESP-Mesh (troppa banda). Il nodo ESP32-CAM eroga uno **stream MJPEG via HTTP** su Wi-Fi diretto; il master si connette allo stream solo quando richiesto (es. retromarcia inserita). Il nodo comunica sulla mesh solo per segnalare stato e trigger.
- **Telecamere esterne di sicurezza** (`CAM_EXT`): ogni camera esterna (fronte, lato dx/sx, retro) è un ESP32-CAM indipendente che eroga stream MJPEG HTTP. Un nodo coordinatore `CAM_EXT` gestisce il rilevamento movimento (tramite analisi frame lato firmware) e invia alert sulla mesh al master. Il master può richiedere lo switch tra stream via comando mesh. Considerare alimentazione con cavo dedicato e custodie IP65 per l'esterno.
- **Sicurezza attuatori**: gradino (`STEP`) e porta (`FRONT_DOOR`) devono implementare un **safe state fisico** (finecorsa hardware) indipendente dal firmware, per evitare danni in caso di blocco software.
- **ROOT (nodo base/infrastruttura)**: 
  - Rilevamento **chiave accensione**: legge il segnale +12V dal contatto di accensione del veicolo tramite optoisolatore (PC817). Quando il segnale passa da OFF→ON o ON→OFF, invia immediatamente `MSG_ALERT` broadcast sulla mesh.
  - Monitoraggio **batteria motore (12V starter)**: tensione tramite partitore ADC (100kΩ/27kΩ). Segnala accensione motore (>13.5V = alternatore in carica).
- **STEP (gradino)**: controllo motorizzato + rilevamento finecorsa. **Integra sensore SHT31 I2C** (temperatura ±0.3°C / umidità ±2% RH) in custodia Gore-Tex per misure ambientali esterne. Pubblica letture ogni 60s sulla mesh.
- **GARAGE (GREY_WATER - valvola acque grigie)**: controllo valvola bipolare tramite **H-bridge DRV8833** (inversione polarità). **Monitora criticamente batteria di servizio**: AGM/LiFePO4 via partitore ADC (100kΩ/27kΩ) o INA219 (consigliato). Invia alert sotto 11.8V (warning) e 11.5V (critico — blocco comandi).
- **Valvola acque chiare** (`FRESH_WATER`): elettrovalvola **normalmente chiusa (NC)**, si apre solo se alimentata (relay/MOSFET). Torna chiusa automaticamente in assenza di alimentazione.
- **Timeout di sicurezza hardware**: valvole e motori si fermano automaticamente dopo N secondi senza conferma mesh (configurabile via NVS).

### Strategia energetica per nodo — vincolo camper a batteria

> **Principio guida**: ogni nodo deve consumare il minimo indispensabile. La batteria del camper è la sola fonte di energia; un sistema mal progettato può scaricarla in poche ore quando il motore è spento.

| Nodo | Strategia energetica | Consumo target idle |
|---|---|---|
| **ROOT** | Always-on, nessun display; modem sleep abilitato; sensore chiave interrupt-driven | < 20 mA |
| **HMI** | Batteria LiPo + alimentazione 12V opzionale; display off dopo 60s; light sleep quando non usato | < 5 mA sleep, ~180 mA attivo |
| **STEP** | Light sleep tra i comandi; attuatore alimentato solo durante il movimento; SHT31 ogni 60s | < 8 mA idle |
| **GARAGE (GREY_WATER)** | Light sleep; valvola NC — non consuma se chiusa; monitor batteria ogni 30s (critico) | < 10 mA idle |
| **FRESH_WATER** | Light sleep; valvola NC — non consuma se chiusa | < 5 mA idle |
| **THERMO_BUNK/LOFT/KITCHEN** | Light sleep; sensore temperatura ogni 30s; valvola aperta solo se necessario | < 10 mA idle |
| **REAR_CAM** | Spento di default; attivato solo quando retromarcia inserita | 0 mA spento |
| **CAM_EXT** | Modalità motion detection a basso consumo; stream attivo solo su richiesta | < 30 mA standby |
| **FRONT_DOOR** | Light sleep; attuatore alimentato solo durante apertura/chiusura | < 5 mA idle |
| **KEY_ON** | Interrupt-driven (wakeup da GPIO su fronte di salita/discesa); idle quasi zero | < 1 mA idle |
| **BATT_ENGINE** | Light sleep; lettura ADC ogni 60s; wakeup su variazione > 0.2V | < 3 mA idle |
| **BATT_SERVICE** | Light sleep; lettura ADC ogni 30s; alert sotto soglia critica | < 3 mA idle |
| **ENV_EXT** | Light sleep profondo; wakeup timer ogni 60s; sensore DHT22/SHT31 a basso consumo | < 2 mA idle |

**Tecniche firmware obbligatorie per ogni nodo**:
- Usare **`esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`** o `WIFI_PS_MAX_MODEM` per attivare il modem sleep Wi-Fi — riduce il consumo del modulo Wi-Fi del 30–60% senza interrompere la connessione mesh
- Disabilitare i **periferici inutilizzati** via `periph_module_disable()` (ADC, DAC, UART non usate, Bluetooth se non necessario)
- Ridurre la **potenza TX Wi-Fi** al minimo sufficiente per la copertura: `esp_wifi_set_max_tx_power(40)` (10 dBm) invece dei 20 dBm default
- Usare **GPIO hold** (`gpio_hold_en()`) prima di entrare in light sleep sugli attuatori, per mantenere lo stato fisico senza alimentare il driver
- Spegnere il **display dell'HMI** dopo un timeout di inattività (es. 60s); riattivare su tocco/encoder o alert mesh
- Monitorare il **voltage bus 12V** con un ADC dedicato sul nodo BATT_SERVICE; notificare tutti i nodi tramite MSG_ALERT mesh

### Schema logico del camper

```
┌──────────────────────────────────────────────────────────┐
│                        CAMPER                            │
│                                                          │
│  [ROOT — ESP32-C3, fisso 12V]  ←→  rete ESP-Mesh         │
│         │  (root fissa, sempre attiva)                   │
│    ┌────┼──────────────────────────────┐                 │
│    │    │         │          │         │                 │
│  [STEP] [GREY_W] [FRESH_W] [FRONT_DOOR] [KEY_ON]         │
│                                                          │
│  [THERMO_BUNK]  [THERMO_LOFT]  [THERMO_KITCHEN]          │
│                                                          │
│  [REAR_CAM]  ──── stream MJPEG ──→ [HMI]                 │
│  [CAM_EXT×N] ──── stream MJPEG + alert movimento         │
│                                                          │
│  [BATT_ENGINE]  [BATT_SERVICE]  [ENV_EXT]                │
│                                                          │
│  [HMI] ←→ mesh (nodo come gli altri, connessione libera) │
│   ESP32-S3 + display + batteria — telecomando portatile  │
└──────────────────────────────────────────────────────────┘
```

---

## 1. ARCHITETTURA GENERALE

### Topologia

Si adotta una **topologia mesh ibrida a grafo aciclico diretto (DAG)** con struttura tree-of-trees, caratteristica di ESP-Mesh (basato su IEEE 802.11):

```
        [ROOT — fisso 12V]
              |
    ┌─────────┴──────────────┐
 [STEP]     [KEY_ON]     [THERMO_*]
   |                        |
 [GREY_W]              [BATT_*]
   |                        |
 [FRESH_W]             [ENV_EXT]
   |
 [FRONT_DOOR]

 [HMI] ←→ (si connette/disconnette liberamente)
```

- **Profondità massima consigliata**: 3–4 livelli (il camper è un ambiente compatto)
- **Ogni nodo** può avere fino a 10 figli (configurabile con `esp_mesh_set_ap_connections`)
- **Auto-healing**: se un nodo intermedio cade, i suoi figli cercano un nuovo parent automaticamente
- **HMI mobile**: l'assenza dell'HMI non influenza la topologia — è una foglia della mesh

### Ruoli dei nodi

| Ruolo | Funzione | Hardware consigliato |
|---|---|---|
| **ROOT** | Root fissa mesh, node registry, persistenza NVS | ESP32-C3 (no display, fisso 12V) |
| **HMI** | Interfaccia utente portatile, monitoraggio, comandi manuali | ESP32-S3 + display + batteria |
| **Nodo Funzione** | Sensore / attuatore / controllo — logica autonoma | ESP32-C3 (low cost) o ESP32 |

> **Nota pratica**: i relay non sono nodi fisici dedicati. In ESP-Mesh, ogni nodo intermedio fa automaticamente da relay. Non serve firmware separato.

### Discovery e Routing

- **Discovery**: automatico tramite ESP-Mesh (scan periodico degli AP mesh vicini, selezione del parent basata su RSSI + numero di hop)
- **Routing**: ESP-Mesh costruisce internamente una routing table basata sul MAC address di ogni nodo
- **Root election**: il nodo ROOT deve essere impostato come root fisso (`esp_mesh_fix_root(true)`) — l'HMI non partecipa mai alla root election (configurato come nodo normale)

### BOM Componenti critici — H-Bridge per attuatori

I seguenti nodi utilizzano motori o valvole bipolari controllate da **H-bridge DRV8833** (Texas Instruments):

| Nodo | Carico | Driver | Datasheet |
|---|---|---|---|
| **STEP** | Motore gradino bidirezionale | DRV8833 ×1 | [TI DRV8833](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |
| **GARAGE (GREY_WATER)** | Valvola acque grigie bipolare | DRV8833 ×1 | [TI DRV8833](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |
| **FRONT_DOOR** | Motore porta bidirezionale | DRV8833 ×1 | [TI DRV8833](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |
| **Totale sistema** | — | **3× DRV8833** | — |

**Specifiche DRV8833**:
- 2 canali H-bridge indipendenti per chip
- Corrente: 2A continua, 3.5A picco per canale
- Protezione termica integrata (shutdown a 160°C)
- Package: QFN 16-pin (5×5 mm)
- Costo: ~€2-3 per chip

---

## 2. TECNOLOGIA DI MESH

### Scelta: ESP-Mesh (layer applicativo sopra Wi-Fi 802.11)

#### Confronto tecnologie

| Tecnologia | Latenza | Range | Consumo | Scalabilità | Note |
|---|---|---|---|---|---|
| **ESP-Mesh** | 10–100ms | ~200m outdoor | Medio | Alta (1000+ nodi) | Stack ufficiale Espressif |
| ESP-NOW | 1–5ms | ~200m | Basso | Bassa (20 peer) | Ottimo per point-to-point |
| BLE Mesh | 50–500ms | ~50m | Molto basso | Media | Adatto a battery nodes |
| Wi-Fi P2P | Alta | Limitato | Alto | Molto bassa | Sconsigliato |

### Motivazione della scelta ESP-Mesh

- **Routing automatico**: nessuna gestione manuale delle tabelle
- **Self-healing**: riconfigurazione automatica in caso di nodo caduto
- **Libreria ufficiale**: supportata in ESP-IDF, aggiornata da Espressif
- **Scalabilità reale**: testata con centinaia di nodi
- **Nessun broker esterno**: funziona completamente offline

### Alternativa valida: ESP-Mesh + ESP-NOW ibrido

Per nodi a batteria (sensori ambientali, attuatori lontani), si può usare **ESP-NOW** come ultimo miglio e un **nodo bridge** che traduce i pacchetti verso la mesh principale. Questo riduce drasticamente il consumo sui nodi leaf.

---

## 3. PROTOCOLLO DI COMUNICAZIONE

### Struttura del messaggio (formato binario compatto)

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;        // Versione protocollo (attuale: 0x01)
    uint8_t  msg_type;       // Tipo messaggio (vedi enum)
    uint16_t node_id;        // ID logico del nodo mittente
    uint16_t target_id;      // ID logico destinatario (0xFFFF = broadcast)
    uint32_t timestamp;      // Uptime ms del mittente
    uint16_t seq_num;        // Numero sequenza per dedup/ACK
    uint8_t  payload_len;    // Lunghezza payload (max 255)
    uint8_t  payload[255];   // Payload variabile
    uint16_t crc16;          // CRC sul messaggio intero (escluso CRC stesso)
} mesh_msg_t;                // Totale: 12 byte header + payload
```

### Tipi di messaggio (`msg_type`)

```c
typedef enum {
    MSG_HEARTBEAT     = 0x01,  // Keepalive periodico nodo → master
    MSG_REGISTER      = 0x02,  // Nodo nuovo si registra
    MSG_REGISTER_ACK  = 0x03,  // Master conferma registrazione
    MSG_COMMAND       = 0x10,  // Master → nodo: esegui azione
    MSG_COMMAND_ACK   = 0x11,  // Nodo → master: conferma esecuzione
    MSG_STATUS_REQ    = 0x20,  // Master richiede stato
    MSG_STATUS_RESP   = 0x21,  // Nodo risponde con stato
    MSG_ALERT         = 0x30,  // Nodo segnala evento critico
    MSG_OTA_START     = 0x40,  // Avvio aggiornamento OTA
    MSG_OTA_CHUNK     = 0x41,  // Chunk firmware
    MSG_OTA_END       = 0x42,  // Fine OTA, richiesta reboot
} msg_type_t;
```

### Addressing logico

- **node_id** assegnato dal ROOT al momento della registrazione (range 1–65534)
- **0x0001** riservato al ROOT (root fissa mesh)
- **0x000F** riservato all'HMI
- **0xFFFF** = broadcast
- Il ROOT mantiene la mappa `node_id ↔ MAC address` in NVS (persistente tra reboot) — necessaria per ESP-Mesh routing che usa MAC come indirizzo fisico
- L'HMI riceve una copia della node registry dal ROOT alla connessione mesh per popolare il proprio display

### Heartbeat

- Ogni nodo invia `MSG_HEARTBEAT` ogni **5 secondi** (configurabile)
- Il **ROOT** aggiorna il timestamp `last_seen` per ogni nodo e mantiene lo stato autorevole
- Se un nodo non risponde per **3 heartbeat consecutivi (15s)**, viene marcato `OFFLINE` nel ROOT
- Dopo **60s** senza heartbeat, il nodo viene rimosso dalla lista attivi nel ROOT
- L'**HMI**, quando connesso, riceve forwarding degli aggiornamenti di stato dal ROOT via `MSG_STATUS_RESP` broadcast; quando non connesso, alla riconnessione richiede un dump completo della registry

### Acknowledgment e ritrasmissioni

- Tutti i messaggi con tipo `_COMMAND`, `_STATUS_REQ`, `_OTA_*` richiedono ACK
- **Timeout ACK**: 500ms
- **Max retry**: 3 tentativi con backoff esponenziale (500ms → 1000ms → 2000ms)
- Deduplicazione basata su `(node_id, seq_num)` — buffer circolante degli ultimi 32 seq_num per nodo

### Gestione collisioni

- I comandi broadcast vengono scaglionati con jitter casuale (0–200ms) per evitare burst di ACK simultanei
- I nodi non inviano più di 1 pacchetto non-heartbeat per 100ms (rate limiting locale)

---

## 4. LOGICA DEL NODO ROOT

### Struttura dati interna (node registry)

```c
typedef struct {
    uint16_t  node_id;
    uint8_t   mac[6];
    char      name[16];          // Es. "STEP", "GREY_WATER"
    uint8_t   node_type;         // Enum: SENSOR, ACTUATOR, CONTROLLER, HMI
    uint8_t   status;            // ONLINE / OFFLINE / ERROR / UPDATING
    uint32_t  last_seen_ms;
    int8_t    rssi;
    uint8_t   payload_cache[32]; // Ultimo stato ricevuto
} node_info_t;

#define MAX_NODES 64
node_info_t nodes[MAX_NODES];    // Persistita su NVS
```

### Responsabilità del ROOT

1. **Assegnazione node_id**: risponde ai `MSG_REGISTER` con `MSG_REGISTER_ACK` contenente l'ID assegnato
2. **Aggiornamento registry**: aggiorna `last_seen` e `payload_cache` ad ogni heartbeat ricevuto
3. **Rilevamento offline**: marca i nodi OFFLINE dopo 15s, li rimuove dopo 60s
4. **Forwarding stato all'HMI**: quando l'HMI è connesso, il ROOT gli inoltra tutti i `MSG_ALERT` e le variazioni di stato (subscription-based)
5. **Persistenza NVS**: salva la registry su flash — al reboot i nodi già registrati non devono ri-registrarsi

### Comportamento in caso di perdita nodi

| Scenario | Comportamento ROOT |
|---|---|
| Nodo OFFLINE (temporaneo) | Marcato WARNING dopo 15s, OFFLINE dopo 30s; HMI notificato |
| Nodo OFFLINE (permanente) | Rimosso dalla lista dopo 120s; evento loggato su NVS |
| HMI si disconnette | Nessun impatto su mesh e nodi funzione; ROOT continua normalmente |
| ROOT si riavvia | Ricarica registry da NVS; nodi già registrati continuano senza interruzione |

> Il ROOT **non invia mai comandi agli attuatori** di propria iniziativa. I comandi manuali provengono esclusivamente dall'HMI. Le azioni automatiche di sicurezza sono eseguite dai singoli nodi autonomamente.

---

## 5. LOGICA DI UN NODO FUNZIONE

### Macchina a stati interna

```
[BOOT] → [MESH_CONNECTING] → [REGISTERING] → [ONLINE] ↔ [EXECUTING_CMD]
                                                  ↓
                                            [MESH_LOST]
                                                  ↓
                                          [STANDALONE_MODE]
                                                  ↓
                                         [MESH_RECONNECTING]
```

### Auto-registrazione

Alla connessione mesh, il nodo invia `MSG_REGISTER` al ROOT con:
- MAC address
- Tipo funzione (enum)
- Nome human-readable (stringa)
- Versione firmware
- Capacità (lista funzioni supportate, flag bitfield)
- Flag `reconnect` (true se il nodo ha già un node_id salvato in NVS)

Il **ROOT** risponde con `MSG_REGISTER_ACK` contenente il `node_id` assegnato (o confermato), che il nodo salva in NVS. Se `reconnect=true`, il ROOT semplicemente aggiorna il `last_seen` e non assegna un nuovo ID.

### Gestione comandi

```c
void handle_command(mesh_msg_t *msg) {
    cmd_payload_t *cmd = (cmd_payload_t*)msg->payload;

    switch(cmd->action) {
        case ACTION_SET_VALUE:   apply_value(cmd->param); break;
        case ACTION_GET_STATUS:  send_status_response(msg->node_id); break;
        case ACTION_RESET:       schedule_restart(); break;
        case ACTION_OTA_PREPARE: ota_begin(); break;
    }
    send_ack(msg->seq_num, ACK_OK);
}
```

### Fallback in assenza del master

- Il nodo continua la propria funzione con l'ultima configurazione ricevuta
- I dati di telemetria vengono bufferizzati in un ring buffer (NVS o RAM)
- Quando il master torna online, i dati bufferizzati vengono inviati in bulk
- Per attuatori critici: il **safe state** di default è configurabile via NVS

---

## 6. STRUTTURA SOFTWARE

### Task FreeRTOS — ROOT

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 4KB | Ricezione messaggi mesh, dispatch |
| `mesh_tx_task` | 5 | 4KB | Invio REGISTER_ACK, forwarding stato HMI |
| `heartbeat_monitor_task` | 4 | 2KB | Verifica last_seen, aggiorna registry |
| `nvs_persist_task` | 2 | 2KB | Salvataggio registry su NVS flash |
| `ota_manager_task` | 1 | 8KB | Distribuzione OTA ai nodi (su richiesta HMI) |

### Task FreeRTOS — HMI

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 4KB | Ricezione aggiornamenti stato e alert dalla mesh |
| `mesh_tx_task` | 5 | 4KB | Invio comandi manuali utente |
| `display_task` | 3 | 12KB | Rendering LVGL, gestione touch/encoder (Core 1) |
| `alert_display_task` | 4 | 3KB | Ricezione alert, aggiornamento UI, buzzer |
| `power_monitor_task` | 3 | 2KB | Gestione batteria HMI, banner sorgente alim. |
| `registry_sync_task` | 2 | 3KB | Sync node registry da ROOT alla connessione |
| `sd_logger_task` | 1 | 3KB | Log eventi su SD (opzionale) |

### Task FreeRTOS — NODO FUNZIONE

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3KB | Ricezione comandi dal master |
| `mesh_tx_task` | 5 | 3KB | Invio heartbeat e risposte |
| `function_task` | 4 | 4KB | Logica principale (sensore/attuatore) |
| `ota_receiver_task` | 2 | 6KB | Ricezione e applicazione OTA |

### Struttura cartelle firmware

```
firmware/
├── shared/                     # Codice comune a tutti i nodi
│   ├── protocol/               # Strutture messaggi, enum, CRC
│   ├── mesh_manager/           # Wrapper ESP-Mesh, init, callbacks
│   ├── nvs_store/              # Astrazione NVS flash
│   └── node_type.h             # Enum tipi nodo, ID riservati
│
├── root/                       # Firmware nodo ROOT
│   ├── main.c
│   ├── node_registry.c/h       # Gestione tabella nodi, persistenza
│   ├── heartbeat_monitor.c/h   # Rilevamento offline
│   └── ota_distributor.c/h     # Distribuzione OTA
│
├── hmi/                        # Firmware nodo HMI
│   ├── main.c
│   ├── display_ui/             # LVGL schermate, widget
│   ├── registry_sync.c/h       # Sincronizzazione registry da ROOT
│   ├── alert_engine.c/h        # Ricezione e visualizzazione alert
│   └── power_manager.c/h       # Gestione batteria, sorgente alim.
│
├── nodes/                      # Firmware nodi funzione
│   ├── step/                   # Gradino
│   ├── grey_water/             # Valvola acque grigie
│   ├── fresh_water/            # Valvola acque chiare
│   ├── thermo/                 # Template termostati
│   ├── key_on/                 # Sensore chiave
│   ├── batt_monitor/           # Template monitor batteria
│   ├── env_ext/                # Sensore ambientale
│   ├── front_door/             # Porta ingresso
│   └── cam/                    # Template telecamere
│
└── partitions.csv              # Partizione OTA dual-bank (comune)
```

### OTA aggiornamenti

- Partizione flash: schema **OTA dual-bank** (`ota_0` / `ota_1`)
- Il master distribuisce il firmware via `MSG_OTA_CHUNK` (chunk da 1KB)
- Il nodo scrive su `esp_ota_write()` e al completamento esegue `esp_ota_set_boot_partition()` + restart
- **Rollback automatico**: se il nodo non invia heartbeat entro 60s dal reboot, l'OTA viene marcata come fallita

---

## 7. FASI DI SVILUPPO

### Fase 1 — ROOT + protocollo base (1–2 settimane)
- [ ] Setup ESP-IDF, struttura cartelle firmware condivisa
- [ ] Firmware ROOT: mesh init, registrazione nodi, heartbeat monitor
- [ ] 2 nodi funzione (es. KEY_ON + STEP): register, heartbeat, MSG_ALERT
- [ ] Verifica persistenza NVS registry al reboot del ROOT
- **Goal**: validare stack mesh, registrazione, persistenza

### Fase 2 — Nodi funzione autonomi (2–3 settimane)
- [ ] Tutti i nodi funzione implementati con logica autonoma
- [ ] Test autonomia STEP: chiusura automatica su KEY_ON senza ROOT attivo
- [ ] Protocollo completo: register, heartbeat, command, ACK, MSG_ALERT
- [ ] Test auto-healing (spegnere ROOT temporaneamente — nodi continuano)
- **Goal**: validare autonomia dei nodi e resilienza

### Fase 3 — Nodo HMI (1–2 settimane)
- [ ] Firmware HMI: connessione mesh come nodo normale
- [ ] Sync registry dal ROOT alla connessione
- [ ] Dashboard LVGL: stati nodi, alert, log
- [ ] Comandi manuali inviabili da touch/encoder
- [ ] Test: HMI si disconnette e riconnette senza impatto sulla mesh
- **Goal**: interfaccia portatile operativa

### Fase 4 — Alimentazione HMI + OTA (1–2 settimane)
- [ ] Circuito dual-source HMI (12V + batteria LiPo)
- [ ] Gestione software sorgente alimentazione e indicatore batteria
- [ ] Meccanismo OTA distribuito con CRC per chunk
- [ ] Test rollback OTA su nodo singolo
- **Goal**: HMI portatile completo, sistema aggiornabile

### Fase 5 — Test di carico e resilienza (1–2 settimane)
- [ ] Stress test con tutti i nodi attivi
- [ ] Test perdita ROOT (nodi continuano in autonomia, HMI mostra ROOT offline)
- [ ] Test HMI portato fuori range (mesh invariata)
- [ ] Misurazione consumi reali vs. budget energetico
- [ ] Ottimizzazione parametri mesh (canale, potenza TX)
- **Goal**: sistema production-ready

---

## 8. CRITICITÀ E SOLUZIONI

### Congestione della rete

**Problema**: con molti nodi che inviano heartbeat simultaneamente si generano burst di traffico.

**Soluzioni**:
- Heartbeat con **jitter randomizzato**: ogni nodo aggiunge `rand() % 2000` ms all'intervallo base
- Traffico critico prioritario via `esp_mesh_send` con flag `MESH_DATA_P2P`
- Limitare la profondità dell'albero a 4 livelli massimi
- Monitorare `esp_mesh_get_tx_pending_num()` per rilevare saturazione

### Perdita di nodi

**Problema**: un relay che cade disconnette tutti i suoi figli.

**Soluzioni**:
- ESP-Mesh gestisce automaticamente il **re-parenting** (configurare `esp_mesh_set_attempts`)
- Il ROOT registra i nodi offline e notifica l'HMI
- I nodi in STANDALONE_MODE continuano a operare con l'ultima configurazione
- Topologia piatta (pochi relay) per installazioni critiche

### Consumo energetico ⚡ — CRITICITÀ PRIMARIA PER DOMOС

**Problema**: il camper opera su batteria (tipicamente 100–200Ah a 12V). Il sistema domotico è sempre attivo, anche a veicolo fermo. Un consumo eccessivo può scaricare la batteria in poche ore, rendendo il veicolo inutilizzabile.

**Soluzioni per DomoC**:
- **Modem sleep obbligatorio** su tutti i nodi: `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` — compatibile con ESP-Mesh, riduce consumo Wi-Fi del 30–60%
- **Potenza TX ridotta**: impostare `esp_wifi_set_max_tx_power(40)` (10 dBm) su tutti i nodi; la distanza inter-nodo nel camper è < 5 metri, 20 dBm è uno spreco
- **Periferici disabilitati**: ogni nodo disabilita Bluetooth, ADC/DAC, UART non usate al boot
- **Telecamere spente di default**: `REAR_CAM` si accende solo su segnale retromarcia; `CAM_EXT` entra in modalità motion-only con frame rate ridotto (1 fps per analisi) invece di stream continuo
- **Elettrovalvole NC (Normally Closed)**: scegliere elettrovalvole normalmente chiuse — consumano corrente solo quando aperte, non quando chiuse (stato normale)
- **Attuatori con alimentazione commutata**: gradino e porta ricevono 12V solo durante il movimento (relay di potenza comandato dal nodo), non in standby
- **Monitoraggio batteria**: un nodo dedicato o il master legge continuamente il voltage bus 12V; sotto 11.8V invia alert; sotto 11.5V può attivare una modalità ultra-low-power disabilitando le telecamere
- **Heartbeat adattivo**: quando la batteria è bassa, l'intervallo heartbeat viene aumentato automaticamente (da 5s a 30s) per ridurre il traffico Wi-Fi
- **Budget energetico stimato del sistema completo**:

  | Condizione | Consumo stimato totale |
  |---|---|
  | Sistema idle (nodi in light sleep, display off) | ~80–120 mA @ 12V ≈ **1–1.4 W** |
  | Sistema attivo (display acceso, 1–2 attuatori) | ~300–500 mA @ 12V ≈ **3.6–6 W** |
  | Telecamera stream attivo | +200–400 mA per camera |
  | Apertura valvola/gradino (picco) | +500–1500 mA per 1–3 secondi |

### Aggiornamenti remoti

**Problema**: OTA su mesh è lenta e può fallire a metà su nodi lontani.

**Soluzioni**:
- Trasferimento OTA chunk-by-chunk con CRC per ogni chunk
- **Aggiornamento scaglionato**: mai più del 20% dei nodi in OTA contemporaneamente
- **Canary deployment**: aggiorna prima 1 nodo, verifica heartbeat, poi propaga
- Rollback automatico: se il nodo reboota 3 volte senza registrarsi, torna al firmware precedente
- Firma del firmware con ECDSA (ESP32 supporta Secure Boot v2)

---

## Suggerimenti pratici per il firmware

1. **Usa ESP-IDF, non Arduino**: ESP-Mesh è pienamente supportato solo in ESP-IDF. Con Arduino perdi controllo su FreeRTOS, partizioni OTA e parametri mesh avanzati.

2. **Canale fisso per la mesh**: imposta un canale Wi-Fi fisso (`esp_mesh_set_channel(6)`) — riduce il tempo di connessione e i problemi di compatibilità con AP vicini.

3. **NVS obbligatoria**: ogni nodo deve salvare il proprio `node_id` in NVS al primo boot. Al reboot non si ri-registra, ma invia un `MSG_HEARTBEAT` con flag `reconnect=true`.

4. **Log mux multi-nodo**: usa uno script Python su PC per muxare i log di tutti i nodi via USB-hub (con prefisso nome nodo) per debug simultaneo.

5. **Watchdog obbligatorio**: configura il task watchdog (`CONFIG_ESP_TASK_WDT=y`) su tutti i task. Un nodo in loop non deve bloccare la mesh.

6. **RSSI come metrica di salute**: logga periodicamente `esp_mesh_get_parent_rssi()`. Sotto -75 dBm il nodo è instabile — valuta l'aggiunta di un relay fisico.

7. **Testa sempre il worst case**: simula la perdita del relay principale mentre 10 nodi stanno inviando dati. Il self-healing deve completarsi entro 30 secondi in una rete ben configurata.
