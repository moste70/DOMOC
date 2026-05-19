# DomoC — Elenco completo messaggi mesh

---

## Codici messaggio (`msg_type_t`)

```c
typedef enum {
    // ── Comandi e stato ───────────────────────────────────────────
    MSG_COMMAND          = 0x01,  // Comando manuale HMI → nodo
    MSG_STATUS           = 0x02,  // Stato spontaneo / risposta comando
    MSG_ALERT            = 0x03,  // Alert/evento (deprecated → usare messaggi specifici)
    MSG_HEARTBEAT        = 0x06,  // Battito periodico nodo → ROOT (ogni 5 s)

    // ── Registrazione e discovery ────────────────────────────────
    MSG_REGISTER         = 0x05,  // Nodo → ROOT: richiesta registrazione
    MSG_REGISTER_ACK     = 0x09,  // ROOT → Nodo: conferma ID assegnato
    MSG_DESCRIPTOR       = 0x07,  // Nodo → ROOT: autodescrizione azioni/proprietà
    MSG_DESCRIPTOR_REQ   = 0x08,  // HMI  → ROOT: richiesta descriptor nodo specifico
    MSG_NODE_JOINED      = 0x0D,  // ROOT → HMI:  nuovo nodo entrato in rete
    MSG_NODE_WARNING     = 0x0E,  // ROOT → HMI:  nodo non risponde da > 15 s
    MSG_NODE_OFFLINE     = 0x0F,  // ROOT → HMI:  nodo offline da > 30 s
    MSG_NODE_LOST        = 0x10,  // ROOT → HMI:  nodo perso da > 120 s

    // ── Richieste di stato ────────────────────────────────────────
    MSG_STATUS_REQ       = 0x0A,  // HMI  → ROOT: richiede dump completo rete
    MSG_STATUS_RESP      = 0x0B,  // ROOT → HMI:  forwarding heartbeat nodo
    MSG_REGISTRY_DUMP    = 0x0C,  // ROOT → HMI:  dump registry + tutti i descriptor

    // ── Broadcast di sicurezza (inviati a tutti i nodi) ──────────
    MSG_KEY_ON           = 0x11,  // ROOT → BROADCAST: positivo sotto chiave ATTIVO
    MSG_KEY_OFF          = 0x12,  // ROOT → BROADCAST: positivo sotto chiave SPENTO
    MSG_STEP_OPEN        = 0x13,  // STEP → BROADCAST: scaletta aperta

    // ── OTA ───────────────────────────────────────────────────────
    MSG_OTA              = 0x04,  // (envelope generico, non usare direttamente)
    MSG_OTA_START        = 0x20,  // ROOT → Nodo: avvio sessione OTA
    MSG_OTA_CHUNK        = 0x21,  // ROOT → Nodo: blocco firmware (max 200 byte)
    MSG_OTA_END          = 0x22,  // ROOT → Nodo: fine trasferimento, avvia verifica CRC
    MSG_OTA_ACK          = 0x23,  // Nodo → ROOT: conferma ricezione chunk / esito OTA
} msg_type_t;
```

---

## Struttura header comune

```c
#define MSG_PAYLOAD_MAX 200

typedef struct __attribute__((packed)) {
    uint8_t  msg_type;              // msg_type_t
    uint8_t  src_id;                // ID nodo mittente (0xFF = broadcast ROOT)
    uint8_t  dst_id;                // ID nodo destinatario (0xFF = broadcast tutti)
    uint8_t  seq_num;               // Numero di sequenza (ACK/ritrasmissione)
    uint8_t  payload[MSG_PAYLOAD_MAX];
} mesh_msg_t;
```

> `dst_id = 0xFF` → messaggio broadcast, ricevuto da tutti i nodi.

---

## Dettaglio messaggi

---

### MSG_COMMAND `0x01`

**Direzione**: HMI → Nodo (via ROOT come router)

```c
typedef struct __attribute__((packed)) {
    uint8_t action_code;   // ACTION_OPEN, ACTION_CLOSE, ACTION_CAM_ON, ...
    uint8_t param;         // Parametro opzionale (es. setpoint × 10)
} cmd_payload_t;
```

| `action_code` | Valore | Descrizione |
| --- | --- | --- |
| `ACTION_OPEN`      | 0x01 | Apri attuatore (gradino, valvola, portellone) |
| `ACTION_CLOSE`     | 0x02 | Chiudi attuatore |
| `ACTION_CAM_ON`    | 0x03 | Accendi videocamera (GREY_WATER) |
| `ACTION_CAM_OFF`   | 0x04 | Spegni videocamera |
| `ACTION_LIGHT_ON`  | 0x05 | Accendi luci (GARAGE) |
| `ACTION_LIGHT_OFF` | 0x06 | Spegni luci |
| `ACTION_TEMP_UP`   | 0x07 | Aumenta setpoint temperatura (THERMO) |
| `ACTION_TEMP_DN`   | 0x08 | Diminuisci setpoint temperatura |
| `ACTION_GET_STATUS`| 0x0F | Richiedi stato immediato |

---

### MSG_STATUS `0x02`

**Direzione**: Nodo → ROOT/HMI (risposta a comando o evento)

Il payload è specifico per tipo nodo — vedi descriptor del nodo per l'interpretazione.
Esempio STEP:

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;          // 0=CLOSED 1=OPEN 2=OPENING 3=CLOSING 4=ERROR
    uint8_t  error_code;     // 0=nessuno, 1=timeout, 2=finecorsa mancante
    uint16_t uptime_s;       // Uptime nodo in secondi
    float    temperature;    // °C (DHT11 onboard)
    float    humidity;       // %RH (DHT11 onboard)
} step_status_payload_t;
```

---

### MSG_HEARTBEAT `0x06`

**Direzione**: Nodo → ROOT (ogni 5 s, periodico)

Stesso payload di `MSG_STATUS` per il nodo mittente. Il ROOT aggiorna `last_seen_ms`,
aggiorna la registry e fa forwarding all'HMI come `MSG_STATUS_RESP`.

---

### MSG_REGISTER `0x05` / MSG_REGISTER_ACK `0x09`

**Direzione**: Nodo → ROOT (all'avvio o al riconnessione)

```c
typedef struct __attribute__((packed)) {
    char    name[16];        // Es. "STEP", "GREY_WATER"
    uint8_t node_type;       // Enum node_type_t
    uint8_t reconnect;       // 1 = rientro dopo reboot (già in registry)
    uint8_t mac[6];          // MAC address fisico
} reg_payload_t;

// ACK → ROOT assegna ID logico
typedef struct __attribute__((packed)) {
    uint8_t assigned_id;     // ID logico assegnato (1–254)
} reg_ack_payload_t;
```

---

### MSG_DESCRIPTOR `0x07` / MSG_DESCRIPTOR_REQ `0x08`

**Direzione**: Nodo → ROOT (subito dopo ACK); HMI → ROOT (richiesta)

Payload = `node_descriptor_t` (140 byte). Vedere `comunicazione_nodi.md` per la struttura
completa con `action_descriptor_t` e `property_descriptor_t`.

---

### MSG_NODE_JOINED `0x0D` / MSG_NODE_WARNING `0x0E` / MSG_NODE_OFFLINE `0x0F` / MSG_NODE_LOST `0x10`

**Direzione**: ROOT → HMI

```c
typedef struct __attribute__((packed)) {
    uint8_t node_id;         // ID logico del nodo coinvolto
} node_event_payload_t;
```

| Messaggio | Condizione |
| --- | --- |
| `NODE_JOINED`  | Nuovo `MSG_REGISTER` ricevuto |
| `NODE_WARNING` | Nessun heartbeat da > 15 s |
| `NODE_OFFLINE` | Nessun heartbeat da > 30 s |
| `NODE_LOST`    | Nessun heartbeat da > 120 s |

---

### MSG_STATUS_REQ `0x0A` / MSG_REGISTRY_DUMP `0x0C`

**Direzione**: HMI → ROOT (richiesta) / ROOT → HMI (risposta dump)

Inviato dall'HMI alla connessione iniziale. Il ROOT risponde con una sequenza:
1. `MSG_REGISTRY_DUMP` — lista di tutti i `node_info_t` (ID, nome, tipo, stato)
2. Un `MSG_DESCRIPTOR` per ogni nodo con descriptor valido in NVS

---

## Messaggi broadcast di sicurezza

Questi messaggi hanno sempre `dst_id = 0xFF` e vengono ricevuti da **tutti i nodi** in rete.

---

### MSG_KEY_ON `0x11` — Positivo sotto chiave ATTIVO

**Mittente**: ROOT (nodo ROOT/MASTER, optoisolatore GPIO7)

**Trigger**: fronte di salita su `ISO_OUT` — il veicolo è stato avviato o
la chiave è stata inserita (12V_KEY attivo).

```c
typedef struct __attribute__((packed)) {
    float    vbat_engine;    // Tensione batteria motore al momento dell'evento (V)
    uint32_t timestamp_s;    // Uptime ROOT in secondi
} key_on_payload_t;
```

**Reazioni attese per nodo:**

| Nodo | Comportamento su MSG_KEY_ON |
| --- | --- |
| **STEP** | Se gradino aperto (`OPEN`): avvia chiusura automatica, invia warning HMI |
| **GREY_WATER** | Se valvola aperta: avvia chiusura automatica, invia warning HMI |
| **FRESH_WATER** | Nessuna azione automatica (valvola NC — già sicura) |
| **GARAGE** | Log evento; le luci restano nello stato corrente (non impatta sicurezza) |
| **HMI** | Mostra badge "CHIAVE ON" in header per tutta la sessione |
| **THERMO_BUNK / THERMO_LOFT** | Nessuna azione automatica |

> **Logica firmware di ricezione** (esempio STEP):
> ```c
> case MSG_KEY_ON:
>     if (step_state == STEP_OPEN) {
>         hmi_send_warning("⚠ Gradino aperto — chiusura automatica");
>         step_start_close();           // avvia motore in direzione CHIUDI
>     }
>     break;
> ```

---

### MSG_KEY_OFF `0x12` — Positivo sotto chiave SPENTO

**Mittente**: ROOT (fronte di discesa su `ISO_OUT`)

```c
typedef struct __attribute__((packed)) {
    float    vbat_engine;    // Tensione batteria motore (V)
    uint32_t timestamp_s;
} key_off_payload_t;
```

**Reazioni attese per nodo:**

| Nodo | Comportamento su MSG_KEY_OFF |
| --- | --- |
| **HMI** | Rimuove badge "CHIAVE ON" dall'header |
| **ROOT** | Log evento, aggiorna stato in registry |
| **Altri nodi** | Nessuna azione automatica (KEY_OFF non è un evento pericoloso) |

---

### MSG_STEP_OPEN `0x13` — Scaletta aperta

**Mittente**: STEP (broadcast, `dst_id = 0xFF`)

**Trigger**: il nodo STEP transita nello stato `OPEN` (finecorsa di apertura raggiunto
**oppure** timeout motore scattato senza finecorsa — gradino presunto aperto).

```c
typedef struct __attribute__((packed)) {
    uint8_t open_reason;     // 0 = finecorsa FC_OPEN, 1 = timeout motore, 2 = comando manuale
    uint8_t key_on_active;   // 1 se in questo momento KEY_ON è attivo (veicolo in marcia)
    float   temperature;     // °C — temperatura zona gradino (DHT11)
} step_open_payload_t;
```

**Reazioni attese per nodo:**

| Nodo | Comportamento su MSG_STEP_OPEN |
| --- | --- |
| **HMI** | Mostra icona "scaletta aperta" (colore blu) nel carosello, badge warning se KEY_ON attivo |
| **ROOT** | Aggiorna stato STEP in registry, persiste su NVS |
| **GARAGE** | Nessuna azione automatica |
| **THERMO / GREY_WATER / FRESH_WATER** | Nessuna azione automatica |

> **Nota**: se `key_on_active = 1` nel payload, il nodo STEP ha già avviato la chiusura
> automatica (aveva ricevuto `MSG_KEY_ON`). `MSG_STEP_OPEN` con `key_on_active = 1` è
> quindi insolito e indica un'apertura avvenuta nonostante la chiave fosse inserita
> (es. comando manuale forzato dall'HMI con flag `FLAG_KEY_BLOCKED` ignorato).

---

## Sequenze di scambio messaggi — casi d'uso principali

### 1. Boot nodo

```
Nodo                ROOT                HMI
  │                   │                  │
  ├──MSG_REGISTER────►│                  │
  │◄─MSG_REGISTER_ACK─┤                  │
  ├──MSG_DESCRIPTOR──►│                  │
  │                   ├──MSG_NODE_JOINED►│
  │                   ├──MSG_DESCRIPTOR─►│   ← HMI aggiorna carosello
  │                   │                  │
  ├──MSG_HEARTBEAT───►│ (ogni 5 s)       │
  │                   ├──MSG_STATUS_RESP►│
```

---

### 2. Comando HMI → STEP apri gradino

```
HMI                 ROOT               STEP
  │                   │                  │
  ├──MSG_COMMAND──────►│                  │
  │   ACTION_OPEN      ├──MSG_COMMAND────►│
  │                   │                  │ motore in moto…
  │                   │◄─MSG_STATUS──────┤ state=OPENING
  │◄─MSG_STATUS_RESP──┤                  │
  │                   │◄─MSG_STATUS──────┤ state=OPEN (finecorsa)
  │                   │◄─MSG_STEP_OPEN───┤ (broadcast → tutti i nodi)
  │◄─MSG_STATUS_RESP──┤                  │
```

---

### 3. Chiave inserita — gradino aperto

```
ROOT               STEP               GREY_WATER          HMI
  │                   │                    │                │
  │ (KEY_ON rilev.)   │                    │                │
  ├──MSG_KEY_ON──────►│ (broadcast 0xFF)   │                │
  │                   ├──────────────────► (tutti i nodi)   │
  │                   │                    │                │
  │                   │ (verifica stato)   │                │
  │◄─MSG_ALERT────────┤ "⚠ Gradino aperto"│                │
  │                   │ avvia chiusura     │                │
  ├────────────────────────────────────────────MSG_ALERT───►│ warning 10 s
  │                   │◄─MSG_STATUS────────┤ (se val. aperta → chiude)
  │◄─MSG_STATUS───────┤ state=CLOSING      │                │
  │◄─MSG_STATUS───────┤ state=CLOSED       │                │
```

---

### 4. HMI si connette (già rete attiva)

```
HMI                 ROOT
  │                   │
  ├──MSG_STATUS_REQ──►│
  │◄─MSG_REGISTRY_DUMP┤   ← tutti i node_info_t
  │◄─MSG_DESCRIPTOR───┤   ← descriptor nodo 1
  │◄─MSG_DESCRIPTOR───┤   ← descriptor nodo 2
  │        …          │   (un MSG_DESCRIPTOR per nodo con desc. valido)
```

---

## Tabella riepilogativa — chi invia, chi riceve

| Messaggio | Mittente | Destinatario | Broadcast |
| --- | --- | --- | :---: |
| `MSG_COMMAND` | HMI | Nodo specifico | — |
| `MSG_STATUS` | Nodo | ROOT / HMI | — |
| `MSG_HEARTBEAT` | Nodo | ROOT | — |
| `MSG_ALERT` | Qualsiasi | ROOT / HMI | — |
| `MSG_REGISTER` | Nodo | ROOT | — |
| `MSG_REGISTER_ACK` | ROOT | Nodo | — |
| `MSG_DESCRIPTOR` | Nodo / ROOT | ROOT / HMI | — |
| `MSG_DESCRIPTOR_REQ` | HMI | ROOT | — |
| `MSG_NODE_JOINED` | ROOT | HMI | — |
| `MSG_NODE_WARNING` | ROOT | HMI | — |
| `MSG_NODE_OFFLINE` | ROOT | HMI | — |
| `MSG_NODE_LOST` | ROOT | HMI | — |
| `MSG_STATUS_REQ` | HMI | ROOT | — |
| `MSG_STATUS_RESP` | ROOT | HMI | — |
| `MSG_REGISTRY_DUMP` | ROOT | HMI | — |
| **`MSG_KEY_ON`** | **ROOT** | **Tutti (0xFF)** | **✓** |
| **`MSG_KEY_OFF`** | **ROOT** | **Tutti (0xFF)** | **✓** |
| **`MSG_STEP_OPEN`** | **STEP** | **Tutti (0xFF)** | **✓** |
| `MSG_OTA_START` | ROOT | Nodo specifico | — |
| `MSG_OTA_CHUNK` | ROOT | Nodo specifico | — |
| `MSG_OTA_END` | ROOT | Nodo specifico | — |
| `MSG_OTA_ACK` | Nodo | ROOT | — |

---

## Codici errore (`error_code` in MSG_STATUS)

```c
typedef enum {
    ERR_NONE          = 0x00,
    ERR_TIMEOUT       = 0x01,  // Timeout motore senza finecorsa
    ERR_ENDSTOP       = 0x02,  // Finecorsa mancante o entrambi attivi
    ERR_OVERCURRENT   = 0x03,  // Sovracorrente (solo se INA219 presente)
    ERR_SENSOR        = 0x04,  // Errore lettura sensore (DHT11, SHT31)
    ERR_MESH          = 0x05,  // Perdita connessione mesh
    ERR_NVS           = 0x06,  // Errore persistenza NVS
    ERR_OTA           = 0x07,  // Errore aggiornamento OTA
} error_code_t;
```

---

## Note implementative

- Tutti i messaggi usano header fisso 4 byte + payload variabile (max 200 byte)
- `seq_num` incrementale per nodo — il destinatario ignora duplicati con stesso `seq_num`
- ACK esplicito solo per `MSG_COMMAND` con `FLAG_CONFIRM_REQUIRED` e per OTA
- I messaggi broadcast (`dst_id = 0xFF`) non richiedono ACK
- CRC a 8 bit (CRC-8/MAXIM) aggiunto in coda al payload per integrità — non incluso nella struct C, calcolato e verificato dal layer mesh
