# DomoC — Nodo FRESH_WATER (Valvola acque chiare)

---

## Descrizione

Il nodo `FRESH_WATER` controlla l'elettrovalvola di carico delle acque chiare del camper. Utilizza una **elettrovalvola normalmente chiusa (NC)**: 

- **Se alimentata (12V ON)**: la valvola si apre
- **Se non alimentata (12V OFF)**: la valvola si chiude automaticamente (stato di sicurezza)

Non è necessaria l'inversione di polarità: un solo relay o MOSFET controlla l'alimentazione.

## Hardware

- **ESP32-C3**
- **Relay o MOSFET** per commutazione alimentazione 12V
- **Elettrovalvola 12V NC** (normalmente chiusa)

## Logica di controllo

- Comandi di apertura/chiusura ricevuti via mesh (`MSG_COMMAND`)
- Timeout di sicurezza: la valvola si richiude automaticamente dopo N secondi se non riceve altri comandi
- In assenza di mesh, può essere comandata localmente (pulsante fisico opzionale)
- Alimentazione fornita solo durante l'apertura

## Logica autonoma — Macchina a stati

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
         BOOT       │  comando APRI        comando CHIUDI      │
           │        │  ────────────►  [OPEN]  ◄────────────   │
           ▼        │                                         │
     [INITIALIZING] │                                         │
           │        │                                         │
       [CLOSED]─────┘                                         │
           │                                                  │
           ▼                                                  │
        [OPEN]                                                │
                    │                                         │
                    │   evento KEY_ON con valvola aperta       │
                    │   [OPEN] ────────────────► [CLOSED]      │
                    └─────────────────────────────────────────┘
```

### Stati

| Stato         | Descrizione                                   |
|-------------- |-----------------------------------------------|
| `INITIALIZING`| Boot, registrazione mesh                      |
| `CLOSED`      | Valvola chiusa, alimentazione OFF             |
| `OPEN`        | Valvola aperta, alimentazione ON              |

---

## Sequenza operazioni con pulsante HMI

1. **Prima pressione pulsante "Apri valvola chiare"**
    - Alimenta la valvola (relay ON)
    - Stato: OPEN
    - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque chiare aperta"

2. **Seconda pressione pulsante "Apri valvola chiare"**
    - Disalimenta la valvola (relay OFF)
    - Stato: CLOSED
    - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque chiare chiusa"

---

## Gestione evento KEY_ON (positivo sotto chiave)

Se il nodo KEY_ON rileva l'inserimento della chiave (+12V attivo) mentre la valvola acque chiare è aperta:

1. KEY_ON invia un MSG_ALERT (chiave inserita) in broadcast sulla mesh
2. Il nodo FRESH_WATER riceve l'alert, verifica lo stato della valvola
3. Se la valvola è aperta:
    - Invia un messaggio di warning all'HMI/ROOT (es. "⚠️ Valvola acque chiare aperta con chiave inserita")
    - L'allarme rimane attivo sullo schermo per 10 secondi
    - Disalimenta la valvola (relay OFF)
    - Stato: CLOSED
    - Invia MSG_STATUS/MSG_ALERT al master: "Valvola acque chiare chiusa"

---

## Blocco comandi con chiave inserita

Quando il segnale KEY_ON (positivo sotto chiave) è attivo, i comandi di apertura/chiusura valvola vengono bloccati:

- L’interfaccia utente (HMI) mostra il pulsante di comando disabilitato (non cliccabile)
- Non vengono inviati comandi al nodo finché la chiave resta inserita
- Un messaggio di stato/informazione può essere visualizzato (es. "Comando disabilitato: chiave inserita")

---

## Sicurezza

- **Timeout software**: se il comando rimane attivo per più di N secondi (es. 10s), il firmware disattiva automaticamente il relay
- **Stato di sicurezza**: senza alimentazione la valvola torna sempre chiusa (normalmente chiusa - NC)
- **Protezione chiave accensione**: quando KEY_ON è attivo, i comandi di apertura vengono bloccati (prevenzione apertura indesiderata durante marcia)

---

## Task principali

- `mesh_rx_task`: ricezione comandi mesh
- `valve_control_task`: gestione relay alimentazione
- `safety_task`: monitoraggio timeout e stato finecorsa

---

## Consumo energetico

- Idle: < 5 mA
- Apertura: 500–1000 mA per 1–2 secondi

---

## Eventi mesh gestiti

- `MSG_COMMAND` da HMI/ROOT
- `MSG_ALERT` da KEY_ON (chiusura automatica in caso di partenza)

---

## Payload di stato

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;       // offset 0 — CLOSED/OPEN
    uint8_t  error_code;  // offset 1
} fresh_water_status_t;   // 2 byte
```

## Descriptor HMI

```c
static const node_descriptor_t FRESH_WATER_DESCRIPTOR = {
    .node_icon      = ICON_VALVE_FRESH,
    .action_count   = 3,
    .property_count = 1,
    .actions = {
        { ACTION_OPEN,       ICON_ACT_OPEN,  "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  "INFO"   },
    },
    .properties = {
        { PROP_STATE, 0, PAYLOAD_UINT8, "", "%s" },
    },
};
```

## Stato pubblicato

- Aperta / Chiusa / Errore

---

## Note pratiche

- Usare relay/MOSFET dimensionati per il carico
- Preferire elettrovalvole a basso consumo e con ritorno a molla (NC)
