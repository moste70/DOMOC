# DomoC — Logica di comunicazione tra i nodi

---

## Architettura di comunicazione

Tutti i nodi DomoC comunicano tramite **ESP-Mesh** (Wi-Fi mesh). La comunicazione è basata su messaggi binari strutturati, trasmessi in broadcast o indirizzati a uno specifico nodo.

### Tipi di messaggi principali

- **MSG_COMMAND**: comando manuale (es. apri/chiudi attuatore)
- **MSG_STATUS**: stato periodico inviato dai nodi (heartbeat)
- **MSG_ALERT**: evento/alert generato da un nodo (es. chiave inserita, errore)
- **MSG_OTA**: aggiornamento firmware OTA
- **MSG_REGISTER**: registrazione nodo al ROOT
- **MSG_DESCRIPTOR**: autodescrizione del nodo — azioni e proprietà esposte all'HMI per configurazione automatica del carosello
- **MSG_DESCRIPTOR_REQ**: richiesta descriptor per un nodo specifico (HMI → ROOT)

---

## Struttura generale di un messaggio (esempio C)

```c
#define MSG_PAYLOAD_MAX 200

typedef enum {
    MSG_COMMAND          = 0x01,
    MSG_STATUS           = 0x02,
    MSG_ALERT            = 0x03,  // deprecato — usare messaggi specifici
    MSG_HEARTBEAT        = 0x06,
    MSG_REGISTER         = 0x05,
    MSG_REGISTER_ACK     = 0x09,
    MSG_DESCRIPTOR       = 0x07,
    MSG_DESCRIPTOR_REQ   = 0x08,
    MSG_NODE_JOINED      = 0x0D,
    MSG_NODE_WARNING     = 0x0E,
    MSG_NODE_OFFLINE     = 0x0F,
    MSG_NODE_LOST        = 0x10,
    MSG_STATUS_REQ       = 0x0A,
    MSG_STATUS_RESP      = 0x0B,
    MSG_REGISTRY_DUMP    = 0x0C,
    MSG_KEY_ON           = 0x11,  // ROOT → broadcast: chiave inserita
    MSG_KEY_OFF          = 0x12,  // ROOT → broadcast: chiave tolta
    MSG_STEP_OPEN        = 0x13,  // STEP → broadcast: scaletta aperta
    MSG_OTA_START        = 0x20,
    MSG_OTA_CHUNK        = 0x21,
    MSG_OTA_END          = 0x22,
    MSG_OTA_ACK          = 0x23,
} msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t  msg_type;               // msg_type_t
    uint8_t  src_id;                 // Nodo mittente
    uint8_t  dst_id;                 // Nodo destinatario (0xFF = broadcast tutti)
    uint8_t  seq_num;                // Sequenza per ACK/dedup
    uint8_t  payload[MSG_PAYLOAD_MAX];
} mesh_msg_t;
```

---

## Esempi di payload

### 1. Comando manuale (MSG_COMMAND)

```c
typedef struct __attribute__((packed)) {
    uint8_t action;      // ACTION_OPEN, ACTION_CLOSE, ACTION_GET_STATUS
    uint8_t param;       // Parametro opzionale
} cmd_payload_t;
```

### 2. Stato nodo (MSG_STATUS)

```c
typedef struct __attribute__((packed)) {
    uint8_t state;       // Stato attuatore (aperto, chiuso, errore...)
    uint8_t error_code;  // Codice errore
    uint16_t uptime_s;   // Uptime nodo
} status_payload_t;
```

### 3. Alert/evento (MSG_ALERT)

```c
typedef struct __attribute__((packed)) {
    uint8_t alert_code;  // Es. chiave inserita, timeout, blocco
    char    msg[24];     // Messaggio testuale breve
} alert_payload_t;
```

---

## Autodescrizione nodi — Node Descriptor

Ogni nodo espone un **descriptor statico** che l’HMI usa per costruire il carosello di icone in modo completamente automatico. Aggiungere un nuovo nodo alla rete non richiede alcuna modifica al firmware dell’HMI.

### Strutture dati descriptor

```c
// Icone HMI — Livello 0 (nodo) e Livello 1 (azioni)
typedef enum {
    // --- icone nodo (carosello principale) ---
    ICON_GENERIC      = 0x00,  // fallback
    ICON_STEP         = 0x01,  // gradino accesso
    ICON_VALVE_GREY   = 0x02,  // valvola acque grigie
    ICON_VALVE_FRESH  = 0x03,  // valvola acque chiare
    ICON_DOOR         = 0x04,  // porta motorizzata
    ICON_THERMOMETER  = 0x05,  // termostato / zona termica
    ICON_CAMERA       = 0x06,  // telecamera
    ICON_KEY          = 0x07,  // chiave accensione
    ICON_BATTERY      = 0x08,  // batteria
    ICON_SENSOR       = 0x09,  // sensore generico
    ICON_LIGHT        = 0x0A,  // luce
    // --- icone azione (carosello secondario) ---
    ICON_ACT_OPEN     = 0x20,
    ICON_ACT_CLOSE    = 0x21,
    ICON_ACT_INFO     = 0x22,
    ICON_ACT_TEMP_UP  = 0x23,
    ICON_ACT_TEMP_DN  = 0x24,
    ICON_ACT_CAM_ON   = 0x25,
    ICON_ACT_CAM_OFF  = 0x26,
    ICON_ACT_LIGHT_ON = 0x27,
    ICON_ACT_LIGHT_OFF= 0x28,
    ICON_ACT_RESET    = 0x29,
    ICON_ACT_OTA      = 0x2A,
} hmi_icon_t;

// Tipo del valore nel payload di stato
typedef enum {
    PAYLOAD_UINT8    = 0,
    PAYLOAD_INT8     = 1,
    PAYLOAD_UINT16   = 2,
    PAYLOAD_INT16    = 3,
    PAYLOAD_FLOAT32  = 4,
} payload_type_t;

// ID proprietà — permette all’HMI di interpretare correttamente i dati
typedef enum {
    PROP_STATE       = 0x01,  // stato enum (stringa da tabella locale)
    PROP_TEMPERATURE = 0x02,  // float °C
    PROP_HUMIDITY    = 0x03,  // float %RH
    PROP_BATTERY_V   = 0x04,  // float V
    PROP_SETPOINT    = 0x05,  // float °C (termostato)
    PROP_VALVE_ON    = 0x06,  // uint8 bool (valvola aria calda)
    PROP_LIGHT_ON    = 0x07,  // uint8 bool
    PROP_DOOR_OPEN   = 0x08,  // uint8 bool (portellone)
    PROP_CAM_ON      = 0x09,  // uint8 bool
} property_id_t;

// Widget LVGL da istanziare per visualizzare una proprietà
typedef enum {
    WIDGET_LABEL       = 0x00, // lv_label — testo: stato enum ("CHIUSO", "APERTO", "ERRORE")
    WIDGET_VALUE_UNIT  = 0x01, // lv_label — "{valore} {unità}" ("12.3 V", "68 %")
    WIDGET_GAUGE       = 0x02, // lv_arc   — arco circolare; richiede range_min/range_max
    WIDGET_PROGRESS    = 0x03, // lv_bar   — barra orizzontale; richiede range_min/range_max
    WIDGET_INDICATOR   = 0x04, // lv_obj   — pallino colorato: verde se val≠0, grigio se val=0
    WIDGET_THERMOMETER = 0x05, // widget composito: lv_arc + lv_label sovrapposta
    WIDGET_BATTERY     = 0x06, // widget composito: icona segmentata + percentuale
} widget_type_t;
// Fallback: widget_type non riconosciuto → WIDGET_LABEL

// Tipo di controllo UI per inviare un’azione
typedef enum {
    CTRL_BUTTON  = 0x00, // lv_btn singolo — tap diretto → MSG_COMMAND
    CTRL_TOGGLE  = 0x01, // lv_btn stateful — stato visivo da linked_property
                         // due azioni con stesso group_id: prima=ON, seconda=OFF
    CTRL_STEPPER = 0x02, // coppia lv_btn [−] valore [+] — azioni con stesso group_id
                         // action_code minore → pulsante "−", maggiore → "+"
                         // valore centrale mostrato dal linked_property
    CTRL_CONFIRM = 0x03, // lv_btn + dialog conferma — due tap per eseguire
    CTRL_SLIDER  = 0x04, // lv_slider — valore continuo (futuro)
} ctrl_type_t;

// Flag per action_descriptor_t.flags
#define FLAG_CONFIRM_REQUIRED  0x01  // mostra dialog di conferma prima di inviare
#define FLAG_KEY_BLOCKED       0x02  // disabilitato quando KEY_ON è attivo

#define NODE_DESC_MAX_ACTIONS     4
#define NODE_DESC_MAX_PROPERTIES  4

// Un’azione che il nodo espone all’HMI
typedef struct __attribute__((packed)) {
    uint8_t action_code;      // valore per cmd_payload_t.action
    uint8_t icon_id;          // hmi_icon_t — icona nel carosello azioni
    uint8_t ctrl_type;        // ctrl_type_t — quale controllo UI generare
    uint8_t group_id;         // 0 = azione indipendente
                              // >0 = raggruppa con altre azioni con lo stesso group_id
    uint8_t linked_property;  // property_id_t riflesso da questo controllo (0 = nessuno)
                              // CTRL_TOGGLE: proprietà che mostra stato ON/OFF
                              // CTRL_STEPPER: proprietà mostrata come valore centrale
    uint8_t flags;            // FLAG_CONFIRM_REQUIRED | FLAG_KEY_BLOCKED
    char    label[8];         // "APRI", "CHIUDI", "TEMP +" ...
} action_descriptor_t;        // 14 byte

// Una proprietà del payload di stato da visualizzare
typedef struct __attribute__((packed)) {
    uint8_t  property_id;     // property_id_t
    uint8_t  payload_offset;  // byte offset nel payload di stato
    uint8_t  payload_type;    // payload_type_t
    uint8_t  widget_type;     // widget_type_t — come renderizzare il valore
    int16_t  range_min;       // valore minimo ×10 (es. 150 = 15.0°C) — per GAUGE/PROGRESS/BATTERY
    int16_t  range_max;       // valore massimo ×10 (es. 300 = 30.0°C)
    char     unit[4];         // "°C", "V", "%", ""
    char     fmt[8];          // "%.1f", "%d", "%s"
} property_descriptor_t;      // 20 byte

// Descriptor completo — inviato come payload di MSG_DESCRIPTOR
typedef struct __attribute__((packed)) {
    uint8_t               node_icon;                                       // icona Livello 0
    uint8_t               action_count;                                    // 0..MAX_ACTIONS
    uint8_t               property_count;                                  // 0..MAX_PROPERTIES
    uint8_t               _pad;
    action_descriptor_t   actions[NODE_DESC_MAX_ACTIONS];                  // 4×14 = 56 byte
    property_descriptor_t properties[NODE_DESC_MAX_PROPERTIES];            // 4×20 = 80 byte
} node_descriptor_t;        // 140 byte — entra nel payload da 255 byte di mesh_msg_t
```

> Il descriptor è **statico e compilato nel firmware** di ogni nodo. Non cambia mai a runtime. L’HMI non conosce i tipi di nodo — li scopre esclusivamente tramite il descriptor.

### Mappa proprietà → widget e azione → controllo

| Tipo proprietà | `widget_type` | Widget LVGL | Note |
| --- | --- | --- | --- |
| stato enum | `WIDGET_LABEL` | `lv_label` | testo da `state_labels[]` del nodo |
| temperatura | `WIDGET_THERMOMETER` | arco + label | range colorato, aggiornamento live |
| umidità | `WIDGET_PROGRESS` | `lv_bar` | 0–100%, range_min=0 range_max=1000 |
| tensione batteria | `WIDGET_BATTERY` | icona segmentata | rosso sotto soglia critica |
| setpoint termostato | `WIDGET_GAUGE` | `lv_arc` | posizione = valore impostato |
| booleano (luce, cam, valvola) | `WIDGET_INDICATOR` | pallino colorato | verde=ON, grigio=OFF |

| Tipo azione | `ctrl_type` | Controllo UI | Note |
| --- | --- | --- | --- |
| singola critica | `CTRL_CONFIRM` | `lv_btn` + dialog | due tap per confermare |
| singola normale | `CTRL_BUTTON` | `lv_btn` | tap diretto → MSG_COMMAND |
| accendi/spegni | `CTRL_TOGGLE` | `lv_btn` stateful | stesso `group_id`, stato da `linked_property` |
| incrementa/decrementa | `CTRL_STEPPER` | `[−] valore [+]` | stesso `group_id`, valore da `linked_property` |

---

## Flusso tipico di comunicazione

1. **Registrazione**: ogni nodo si registra al ROOT all’avvio (`MSG_REGISTER`)
2. **Descriptor**: subito dopo l’ACK, il nodo invia `MSG_DESCRIPTOR` al ROOT — ROOT lo memorizza in NVS
3. **Heartbeat**: ogni 5s i nodi inviano `MSG_HEARTBEAT` (stato attuale)
4. **Comando**: l’HMI invia `MSG_COMMAND` a un nodo attuatore (es. apri valvola)
5. **Reazione**: il nodo esegue il comando e aggiorna lo stato
6. **Alert**: se si verifica un evento (es. chiave inserita), il nodo invia `MSG_ALERT` in broadcast
7. **OTA**: il ROOT può inviare `MSG_OTA_*` per aggiornamento firmware

### Flusso connessione HMI

```text
HMI si connette alla mesh
  │
  ├─▶ MSG_STATUS_REQ (broadcast) → ROOT
  │
  └─▶ ROOT risponde con:
        - MSG_REGISTRY_DUMP  (tutti i node_info_t)
        - MSG_DESCRIPTOR     (node_descriptor_t per ogni nodo registrato)
        → HMI costruisce il carosello automaticamente

Nuovo nodo arriva mentre HMI è già online:
  1. Nodo → MSG_REGISTER → ROOT → MSG_REGISTER_ACK
  2. Nodo → MSG_DESCRIPTOR → ROOT
  3. ROOT → MSG_DESCRIPTOR (forward) → HMI
  4. HMI aggiunge la nuova icona al carosello senza riavvio
```

---

## Esempi di scambio messaggi

- L’utente tocca "APRI GRIGIE" su HMI:
    - HMI → GREY_WATER: MSG_COMMAND (ACTION_OPEN)
    - GREY_WATER → ROOT/HMI: MSG_STATUS (aperta)
- Chiave inserita:
    - KEY_ON → broadcast: MSG_ALERT (chiave inserita)
    - STEP/GREY_WATER reagiscono autonomamente
    - STEP/GREY_WATER → ROOT/HMI: MSG_ALERT (azione automatica eseguita)

---

## Note

- Tutti i messaggi sono binari, compatti e con CRC
- ACK opzionale per comandi critici
- I payload possono essere estesi secondo necessità

---

## Esempio: Logica operativa nodo GREY_WATER (valvola acque grigie)

### Sequenza operazioni con pulsante HMI

1. **Prima pressione pulsante "Apri valvola scarico"**
    - Accende la videocamera di puntamento (es. per vedere lo scarico)
    - Nessuna azione sulla valvola (resta chiusa)

2. **Seconda pressione pulsante "Apri valvola scarico"**
    - Invia comando di apertura alla valvola (inversione polarità: direzione "apri")
    - Alimenta la valvola per 3 secondi (timer software)
    - Dopo 3 secondi:
        - Interrompe alimentazione alla valvola
        - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque grigie aperta"

3. **Chiusura (comando chiudi o pressione dedicata)**
    - Inverte la polarità (direzione "chiudi")
    - Alimenta la valvola per 3 secondi (timer software)
    - Dopo 3 secondi:
        - Interrompe alimentazione alla valvola
        - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque grigie chiusa"
        - Spegne la videocamera di puntamento

### Stato e feedback

- Ogni cambio stato viene notificato al master (ROOT/HMI) tramite messaggio
- La videocamera resta accesa solo durante la fase di apertura/scarico
- Il timer software garantisce che la valvola non rimanga alimentata oltre il necessario
- Ulteriori pressioni del pulsante possono essere ignorate o usate per altre funzioni (es. forzatura)

---

### Gestione evento KEY_ON (positivo sotto chiave)

Se il nodo KEY_ON rileva l'inserimento della chiave (+12V attivo) mentre la valvola acque grigie è aperta:

1. KEY_ON invia un MSG_ALERT (chiave inserita) in broadcast sulla mesh
2. Il nodo GREY_WATER riceve l'alert, verifica lo stato della valvola
3. Se la valvola è aperta:
    - Invia un messaggio di warning all'HMI/ROOT (es. "⚠️ Valvola scarico aperta con chiave inserita")
    - L'allarme rimane attivo sullo schermo per 10 secondi
    - Avvia la sequenza di chiusura automatica:
        - Inverte la polarità (direzione "chiudi")
        - Alimenta la valvola per 3 secondi (timer software)
        - Dopo 3 secondi:
            - Interrompe alimentazione alla valvola
            - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque grigie chiusa"
            - Spegne la videocamera di puntamento

---

## Esempio: Logica operativa nodo STEP (scaletta/gradino)

### Sequenza operazioni con pulsante HMI

1. **Prima pressione pulsante "Apri gradino"**
    - Attiva il motore in direzione "apri" (inversione polarità)
    - Alimenta il motore per 3 secondi (timer software)
    - Dopo 3 secondi:
        - Interrompe alimentazione al motore
        - Invia MSG_STATUS/MSG_ALERT al master: "Gradino aperto"

2. **Seconda pressione pulsante "Apri gradino"**
    - Nessuna azione (già aperto)

3. **Chiusura (comando chiudi o pressione dedicata)**
    - Attiva il motore in direzione "chiudi" (inversione polarità)
    - Alimenta il motore per 3 secondi (timer software)
    - Dopo 3 secondi:
        - Interrompe alimentazione al motore
        - Invia MSG_STATUS/MSG_ALERT al master: "Gradino chiuso"

### Gestione evento KEY_ON (positivo sotto chiave)

Se il nodo KEY_ON rileva l'inserimento della chiave (+12V attivo) mentre il gradino è aperto:

1. KEY_ON invia un MSG_ALERT (chiave inserita) in broadcast sulla mesh
2. Il nodo STEP riceve l'alert, verifica lo stato del gradino
3. Se il gradino è aperto:
    - Invia un messaggio di warning all'HMI/ROOT (es. "⚠️ Gradino aperto con chiave inserita")
    - L'allarme rimane attivo sullo schermo per 10 secondi
    - Avvia la sequenza di chiusura automatica:
        - Attiva il motore in direzione "chiudi"
        - Alimenta il motore per 3 secondi (timer software)
        - Dopo 3 secondi:
            - Interrompe alimentazione al motore
            - Invia MSG_STATUS/MSG_ALERT al master: "Gradino chiuso"

---

### Esempio di flusso messaggi

1. HMI → GREY_WATER: MSG_COMMAND (ACTION_CAMERA_ON)
2. GREY_WATER: accende videocamera
3. HMI → GREY_WATER: MSG_COMMAND (ACTION_OPEN)
4. GREY_WATER: alimenta valvola 3s
5. GREY_WATER → ROOT/HMI: MSG_STATUS (valvola aperta)
6. HMI → GREY_WATER: MSG_COMMAND (ACTION_CLOSE)
7. GREY_WATER: inverte polarità, alimenta valvola 3s, spegne videocamera
8. GREY_WATER → ROOT/HMI: MSG_STATUS (valvola chiusa)

---
