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

---

## Struttura generale di un messaggio (esempio C)

```c
#define MSG_PAYLOAD_SIZE 32

typedef enum {
    MSG_COMMAND = 0x01,
    MSG_STATUS  = 0x02,
    MSG_ALERT   = 0x03,
    MSG_OTA     = 0x04,
    MSG_REGISTER= 0x05
} msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t     src_id;      // Nodo mittente
    uint8_t     dst_id;      // Nodo destinatario (o broadcast)
    uint8_t     type;        // Tipo messaggio (msg_type_t)
    uint8_t     seq_num;     // Sequenza per ACK/ritrasmissione
    uint8_t     payload[MSG_PAYLOAD_SIZE]; // Dati specifici
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

## Flusso tipico di comunicazione

1. **Registrazione**: ogni nodo si registra al ROOT all’avvio (MSG_REGISTER)
2. **Heartbeat**: ogni 5s i nodi inviano MSG_STATUS (stato attuale)
3. **Comando**: l’HMI invia MSG_COMMAND a un nodo attuatore (es. apri valvola)
4. **Reazione**: il nodo esegue il comando e aggiorna lo stato
5. **Alert**: se si verifica un evento (es. chiave inserita), il nodo invia MSG_ALERT in broadcast
6. **OTA**: il ROOT/HMI può inviare MSG_OTA per aggiornamento firmware

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
