# DomoC — Nodo GARAGE (Valvola acque grigie, monitoraggio batteria, luci garage)

---

## Descrizione

Il nodo garage (`GARAGE_NODE`) controlla l'elettrovalvola di scarico delle acque grigie del camper. Inoltre, si occupa della **lettura della tensione della batteria di servizio**, utilizzando un partitore resistivo o un sensore INA219, trovandosi fisicamente vicino alla batteria. Utilizza **inversione di polarità** (H-bridge) per aprire e chiudere la valvola, esattamente come il gradino di ingresso. Gestisce anche le luci e il portellone del garage.

- **Apertura**: applica polarità diretta (es. +12V su A, 0V su B)
- **Chiusura**: applica polarità inversa (es. 0V su A, +12V su B)
- **Stop**: nessuna tensione (entrambi i lati a 0V)

La posizione della valvola può essere rilevata tramite finecorsa o sensore di corrente (opzionale). La logica di sicurezza garantisce che la valvola non rimanga alimentata a fine corsa.

## Hardware

- **ESP32-C3**
- **Driver H-bridge DRV8833** (Texas Instruments) — 2A per canale, protezione termica integrata — [Datasheet](https://www.ti.com/lit/ds/symlink/drv8833.pdf)
- **Elettrovalvola 12V** (bipolare, inversione polarità)
- **Relay di potenza** per taglio alimentazione in standby
- **Partitore resistivo** o **sensore INA219** per la lettura della tensione della batteria di servizio

## Logica di controllo

- Comandi di apertura/chiusura ricevuti via mesh (`MSG_COMMAND`)
- **Timeout di sicurezza**: se la valvola non cambia stato entro **N secondi** (es. 3-5s), il firmware disattiva l'H-bridge e segnala errore
- Il timeout protegge da:
  - Motore/attuatore bloccato (meccanico)
  - Comando non completato per guasto hardware
  - Firmware crash durante il movimento
- In assenza di mesh, può essere comandata localmente (pulsante fisico opzionale)
- Dopo ogni movimento, H-bridge disattivato (standby 0mA)

## Logica autonoma — Macchina a stati

```text
                    ┌─────────────────────────────────────────┐
                    │                                         │
         BOOT       │  comando APRI        timer 3s scaduto    │
           │        │  ────────────►  [OPENING]  ──────────►  │
           ▼        │                                         │
     [INITIALIZING] │  comando CHIUDI      timer 3s scaduto    │
           │        │  ────────────►  [CLOSING] ──────────►   │
           ▼        │                                         │
       [STOPPED]────┤                                         │
         /    \     │                                         │
  nessuno   nessuno │                                         │
       │        │   │                                         │
    [CLOSED] [OPEN] │                                         │
                    │                                         │
                    │   evento KEY_ON con valvola aperta       │
                    │   [OPEN] ────────────────► [CLOSING]     │
                    │                                         │
                    │   timeout sicurezza (>3s)                │
                    │   [OPENING/CLOSING] ──────────► [ERROR]  │
                    └─────────────────────────────────────────┘
```

### Stati

| Stato         | Descrizione                                   |
|-------------- |-----------------------------------------------|
| `INITIALIZING`| Boot, registrazione mesh                      |
| `CLOSED`      | Valvola chiusa, alimentazione OFF             |
| `OPEN`        | Valvola aperta, alimentazione OFF             |
| `OPENING`     | Valvola in apertura, alimentazione ON         |
| `CLOSING`     | Valvola in chiusura, alimentazione ON         |
| `ERROR`       | Timeout apertura/chiusura                     |

---

## Sicurezza

- Timeout hardware: la valvola torna chiusa dopo N secondi se non riceve conferma di posizione
- Protezione inversione polarità: mai attivare entrambe le direzioni contemporaneamente

---

## Task principali

- `mesh_rx_task`: ricezione comandi mesh
- `valve_control_task`: gestione H-bridge e stato valvola
- `safety_task`: monitoraggio timeout e stato finecorsa

---

## Consumo energetico

- Idle: < 5 mA
- Movimento: 500–1500 mA per 1–3 secondi

---

## Eventi mesh gestiti

- `MSG_COMMAND` da HMI/ROOT
- `MSG_ALERT` da KEY_ON (chiusura automatica in caso di partenza)

---

## Payload di stato

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;       // offset 0 — CLOSED/OPEN/OPENING/CLOSING/ERROR
    uint8_t  error_code;  // offset 1
    float    battery_v;   // offset 2-5 — tensione batteria servizio (V)
    uint8_t  door_open;   // offset 6 — 1 = portellone garage aperto
    uint8_t  light_on;    // offset 7 — 1 = luci garage accese
} garage_status_t;        // 8 byte
```

## Descriptor HMI

```c
static const node_descriptor_t GARAGE_DESCRIPTOR = {
    .node_icon      = ICON_VALVE_GREY,
    .action_count   = 4,
    .property_count = 3,
    .actions = {
        // action_code        icon_id              ctrl_type     group_id  linked_property  flags              label
        { ACTION_OPEN,       ICON_ACT_OPEN,       CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "APRI"     },
        { ACTION_CLOSE,      ICON_ACT_CLOSE,      CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "CHIUDI"   },
        { ACTION_LIGHT_ON,   ICON_ACT_LIGHT_ON,   CTRL_TOGGLE,  1,        PROP_LIGHT_ON,   0,                 "LUCE"     },
        { ACTION_LIGHT_OFF,  ICON_ACT_LIGHT_OFF,  CTRL_TOGGLE,  1,        PROP_LIGHT_ON,   0,                 "LUCE OFF" },
    },
    .properties = {
        // property_id     offset  type             widget_type        range_min  range_max  unit  fmt
        { PROP_STATE,      0,      PAYLOAD_UINT8,   WIDGET_LABEL,      0,         0,         "",   "%s"   },
        { PROP_BATTERY_V,  2,      PAYLOAD_FLOAT32, WIDGET_BATTERY,    115,       145,       "V",  "%.1f" },
        { PROP_DOOR_OPEN,  6,      PAYLOAD_UINT8,   WIDGET_INDICATOR,  0,         0,         "",   "%s"   },
    },
};
```

## Stato pubblicato

- Aperta / Chiusa / In movimento / Errore
- Tensione batteria di servizio
- Stato portellone e luci garage

---

## Note pratiche

- Usare cavi e connettori robusti, zona esposta a umidità
- Preferire finecorsa a tenuta stagna o sensore di corrente per rilevamento posizione

---

## Rilevamento posizione senza finecorsa

Se la valvola non dispone di finecorsa, la posizione viene stimata tramite:

- **Timer software**: il firmware attiva la valvola per un tempo fisso (circa 3 secondi per apertura/chiusura), sufficiente a garantire il movimento completo. Al termine del timer, la valvola viene disattivata.

> **Nota**: In assenza di feedback diretto, è importante dimensionare il timer con un margine di sicurezza e prevedere un timeout massimo per evitare surriscaldamenti o danni in caso di blocco meccanico.

---

## Blocco comandi con chiave inserita

Quando il segnale KEY_ON (positivo sotto chiave) è attivo, i comandi di apertura/chiusura valvola vengono bloccati:

- L’interfaccia utente (HMI) mostra il pulsante di comando disabilitato (non cliccabile)
- Non vengono inviati comandi al nodo finché la chiave resta inserita
- Un messaggio di stato/informazione può essere visualizzato (es. "Comando disabilitato: chiave inserita")

---

## Funzioni aggiuntive: controllo garage

Oltre alla gestione della valvola acque grigie, il nodo garage svolge anche le seguenti funzioni per il garage (vano posteriore):

- **Controllo luci garage**: gestisce una o più uscite (relay/MOSFET) per accendere e spegnere le luci del garage. Le luci possono essere comandate sia localmente (pulsante fisico opzionale) sia da remoto tramite HMI.
- **Lettura sensore portellone garage**: monitora un ingresso digitale collegato a un sensore (reed/microswitch) che rileva l'apertura del portellone del garage.
- **Pubblicazione stato su mesh**: lo stato delle luci garage e del portellone viene pubblicato periodicamente sulla mesh. L'HMI visualizza indicatori dedicati per:
  - Portellone garage aperto/chiuso
  - Luci garage accese/spente
- **Logica di sicurezza**: se il portellone è aperto e le luci sono accese, viene visualizzato un alert sull'HMI (utile per evitare di lasciare il garage illuminato accidentalmente).

> In questo modo il nodo garage integra funzioni di automazione e monitoraggio del garage, riducendo la necessità di nodi separati e migliorando la sicurezza e la praticità d'uso.
