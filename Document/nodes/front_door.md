# DomoC — Nodo FRONT_DOOR (Porta ingresso motorizzata)

---

## Descrizione

Il nodo `FRONT_DOOR` (ID: `0x0009`) controlla il motore della porta di ingresso principale del camper. È un nodo **autonomo**: contiene tutta la logica per operare in sicurezza indipendentemente dallo stato del ROOT o dell'HMI.

Funzioni principali:
- Apertura e chiusura motorizzata della porta tramite H-bridge relay
- Rilevamento stato fisico tramite finecorsa (aperto / chiuso / in movimento / errore)
- Reazione automatica a `MSG_KEY_ON`: chiusura automatica se la porta è aperta
- Reazione a `MSG_STEP_OPEN`: l'HMI segnala all'utente l'accesso al camper senza comandi autonomi
- Pubblicazione dello stato sulla mesh per ROOT e HMI

**Differenza con STEP**: la porta di ingresso è l'accesso principale al camper — la sua apertura/chiusura impatta la sicurezza del veicolo. I comandi manuali da HMI sono bloccati con chiave inserita.

---

## Hardware

### Microcontrollore

- **ESP32-S3-MINI-1** su PCB Universale v3.0

### Mappatura risorse PCB

| Risorsa | Ruolo | GPIO |
|---|---|---|
| HB1 (H-bridge relay) | `motor` — porta ingresso | GPIO11 (DIR_A), GPIO12 (DIR_B), GPIO13 (EN) |
| HB2 | `unused` | — |
| REL1 | `unused` (espandibile per sblocco elettrico) | GPIO17 |
| REL2 | `unused` | GPIO18 |
| OPT1 | `fc_closed` — finecorsa porta chiusa | GPIO3 |
| OPT2 | `fc_open` — finecorsa porta aperta | GPIO4 |
| ADC1 | `unused` | GPIO1 |
| ADC2 | `unused` | GPIO2 |
| I2C | `unused` | GPIO8/9 |
| 1-Wire | `unused` | GPIO10 |
| LED RGB | stato nodo | GPIO21 |

### Schema GPIO rilevanti

| GPIO | Segnale | Funzione |
|---|---|---|
| GPIO3 | OPT1_OUT | Finecorsa porta chiusa (active-LOW) |
| GPIO4 | OPT2_OUT | Finecorsa porta aperta (active-LOW) |
| GPIO11 | HB1_DIR_A | H-bridge — direzione A (apri porta) |
| GPIO12 | HB1_DIR_B | H-bridge — direzione B (chiudi porta) |
| GPIO13 | HB1_EN | H-bridge — enable (OFF in standby) |
| GPIO21 | LED_DATA | WS2812B stato |

### Motore porta

| Parametro | Valore tipico |
|---|---|
| Tensione | 12V DC |
| Corrente di picco | 1–3A (dipende dal peso porta) |
| Tempo ciclo | 3–8 secondi |
| Finecorsa | 2× microswitch SPDT (opzionali ma consigliati) |

---

## Logica H-bridge — sequenza sicura

```c
void motor_set_direction(motor_dir_t dir) {
    gpio_set_level(GPIO_HB1_EN, 0);       // sempre OFF prima di cambiare direzione
    vTaskDelay(pdMS_TO_TICKS(50));        // pausa sicurezza relay
    if (dir == DIR_OPEN) {
        gpio_set_level(GPIO_HB1_DIR_A, 1);
        gpio_set_level(GPIO_HB1_DIR_B, 0);
    } else {
        gpio_set_level(GPIO_HB1_DIR_A, 0);
        gpio_set_level(GPIO_HB1_DIR_B, 1);
    }
    gpio_set_level(GPIO_HB1_EN, 1);
    start_safety_timer(DOOR_TIMEOUT_MS);
}

void motor_stop(void) {
    gpio_set_level(GPIO_HB1_EN, 0);
    gpio_set_level(GPIO_HB1_DIR_A, 0);
    gpio_set_level(GPIO_HB1_DIR_B, 0);
    cancel_safety_timer();
}
```

---

## Logica autonoma — Macchina a stati

```
                    ┌──────────────────────────────────────────────┐
                    │                                              │
         BOOT       │  comando APRI        finecorsa APERTO        │
           │        │  ────────────►  [OPENING]  ──────────►       │
           ▼        │                                              │
     [INITIALIZING] │  finecorsa           comando CHIUDI          │
           │        │  CHIUSO  ◄──  [CLOSED]  ◄──────────         │
           ▼        │                  │                           │
       [STOPPED]────┤                  │ MSG_KEY_ON                │
         /    \     │                  │ (se OPEN/OPENING)         │
   fc_chiuso nessuno│                  ▼                           │
       │        │   │           [AUTO_CLOSING]                     │
    [CLOSED] [OPEN] │                  │                           │
                    │          finecorsa CHIUSO                    │
                    │                  │                           │
                    │              [CLOSED]                        │
                    │                                              │
                    │   timeout > DOOR_TIMEOUT_MS senza finecorsa  │
                    │   [OPENING/CLOSING] ──────────► [ERROR]      │
                    └──────────────────────────────────────────────┘
```

### Stati

| Stato | Descrizione | LED |
|---|---|---|
| `INITIALIZING` | Boot, lettura finecorsa, registrazione mesh | Bianco lampeggiante |
| `CLOSED` | Porta chiusa, motore fermo, HB1_EN=OFF | Verde fisso |
| `OPEN` | Porta aperta, motore fermo, HB1_EN=OFF | Blu fisso |
| `OPENING` | Motore attivo verso apertura | Blu lampeggiante |
| `CLOSING` | Motore attivo verso chiusura | Arancio lampeggiante |
| `AUTO_CLOSING` | Chiusura automatica su MSG_KEY_ON | Rosso lampeggiante veloce |
| `ERROR` | Timeout finecorsa — porta bloccata o guasto | Rosso fisso |
| `STANDALONE` | Mesh assente > 30s | Bianco fisso |

---

## Sottoscrizione eventi mesh

```c
const mesh_subscription_t front_door_subscriptions[] = {
    { .source_node_id = NODE_ID_BROADCAST, .msg_type = MSG_KEY_ON    }, // chiave inserita
    { .source_node_id = NODE_ID_HMI,       .msg_type = MSG_COMMAND   }, // comandi manuali
    { .source_node_id = NODE_ID_ROOT,      .msg_type = MSG_STATUS_REQ },
    { .source_node_id = NODE_ID_ROOT,      .msg_type = MSG_OTA_START  },
};
```

### Reazione a MSG_KEY_ON

```c
void on_key_on_event(void) {
    if (current_state == STATE_OPEN || current_state == STATE_OPENING) {
        motor_stop();
        transition_to(STATE_AUTO_CLOSING);
        motor_set_direction(DIR_CLOSE);
        send_alert(ALERT_FRONT_DOOR_AUTO_CLOSING,
                   "Porta ingresso: chiusura automatica per KEY_ON");
    }
}
```

### Reazione a MSG_COMMAND dall'HMI

```c
void on_hmi_command(mesh_msg_t *msg) {
    cmd_payload_t *cmd = (cmd_payload_t*)msg->payload;
    if (msg->target_id != NODE_ID_FRONT_DOOR) return;

    switch (cmd->action) {
        case ACTION_OPEN:
            if (current_state == STATE_CLOSED) {
                transition_to(STATE_OPENING);
                motor_set_direction(DIR_OPEN);
                send_ack(msg->seq_num, ACK_OK);
            } else {
                send_ack(msg->seq_num, ACK_REJECTED);
            }
            break;

        case ACTION_CLOSE:
            if (current_state == STATE_OPEN) {
                transition_to(STATE_CLOSING);
                motor_set_direction(DIR_CLOSE);
                send_ack(msg->seq_num, ACK_OK);
            } else {
                send_ack(msg->seq_num, ACK_REJECTED);
            }
            break;

        case ACTION_GET_STATUS:
            send_status_response();
            break;
    }
}
```

### Interrupt finecorsa (priorità massima nel nodo)

```c
void IRAM_ATTR finecorsa_isr(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t event = (uint8_t)(uintptr_t)arg;
    xQueueSendFromISR(finecorsa_queue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void finecorsa_task(void *pvParam) {
    uint8_t event;
    while (1) {
        if (xQueueReceive(finecorsa_queue, &event, portMAX_DELAY)) {
            motor_stop();
            if (event == FC_CLOSED) {
                transition_to(STATE_CLOSED);
            } else if (event == FC_OPEN) {
                transition_to(STATE_OPEN);
            }
            send_status_update();
        }
    }
}
```

> `finecorsa_task` ha la **priorità più alta** del nodo: fermare il motore al finecorsa è operazione di sicurezza non preemptabile.

---

## Comportamento in STANDALONE_MODE

```c
void on_mesh_disconnected(void) {
    standalone_mode = true;
    gpio_intr_enable(GPIO_LOCAL_BUTTON);
    led_set(LED_WHITE_SOLID);
}

void on_mesh_reconnected(void) {
    standalone_mode = false;
    gpio_intr_disable(GPIO_LOCAL_BUTTON);
    mesh_register();       // reconnect=true
    send_status_update();
}
```

La chiusura automatica su KEY_ON funziona anche in STANDALONE: il broadcast `MSG_KEY_ON` con `dst_id=0xFF` è ricevuto direttamente dal nodo anche senza ROOT.

---

## Blocco comandi con chiave inserita

Con `MSG_KEY_ON` attivo, i comandi di apertura restituiscono `ACK_REJECTED`. L'HMI mostra il pulsante APRI disabilitato con tooltip "Comando disabilitato: chiave inserita".

---

## Heartbeat e payload di stato

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;         // offset 0 — CLOSED/OPEN/OPENING/CLOSING/AUTO_CLOSING/ERROR/STANDALONE
    uint8_t  fc_closed;     // offset 1 — 1 = finecorsa chiuso attivo
    uint8_t  fc_open;       // offset 2 — 1 = finecorsa aperto attivo
    uint8_t  error_code;    // offset 3 — 0=ok, 1=timeout finecorsa
    uint16_t last_move_ms;  // offset 4-5 — durata ultimo movimento (diagnostica)
    uint8_t  _pad[2];       // offset 6-7 — allineamento
} front_door_status_t;      // 8 byte
```

---

## Descriptor HMI

```c
static const node_descriptor_t FRONT_DOOR_DESCRIPTOR = {
    .node_icon      = ICON_DOOR,
    .action_count   = 3,
    .property_count = 2,
    .actions = {
        // action_code        icon_id         ctrl_type     group_id  linked_property  flags              label
        { ACTION_OPEN,       ICON_ACT_OPEN,  CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  CTRL_BUTTON,  0,        0,               0,                 "INFO"   },
    },
    .properties = {
        // property_id      offset  type            widget_type    range_min  range_max  unit  fmt
        { PROP_STATE,       0,      PAYLOAD_UINT8,  WIDGET_LABEL,  0,         0,         "",   "%s" },
        { PROP_FC_CLOSED,   1,      PAYLOAD_UINT8,  WIDGET_INDICATOR, 0,      0,         "",   "%s" },
    },
};
```

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, filtro, dispatch |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, alert, ACK |
| `door_control_task` | 6 | 3 KB | Macchina a stati, gestione H-bridge |
| `finecorsa_task` | 7 | 2 KB | Processamento interrupt finecorsa (massima priorità) |
| `ota_receiver_task` | 2 | 6 KB | Ricezione e applicazione OTA |

---

## Gestione energetica

- **HB1_EN=OFF** in standby → corrente sul motore zero
- `WIFI_PS_MIN_MODEM` in tutti gli stati non-movimento
- TX power fisso 10 dBm
- **Consumo stimato**:
  - Idle (porta chiusa): ~4 mA
  - Mesh attiva in ricezione: ~8 mA
  - Motore in movimento: 500–2000 mA @ 12V per 3–8 secondi

---

## node_config.json

```json
{
  "node_name": "FRONT_DOOR",
  "node_type": "FRONT_DOOR",
  "mesh": { "channel": 6, "mesh_id": "DomoC01" },
  "hardware": {
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
      "motor_run_ms": 6000,
      "opt_fc_closed": "opt1", "opt_fc_open": "opt2"
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "unused" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" }
  }
}
```

---

## Considerazioni pratiche

- **Debounce finecorsa**: i microswitch meccanici rimbalzano — implementare debounce software di 50ms dopo il primo fronte ISR
- **Timeout sicurezza**: se il finecorsa non scatta entro `DOOR_TIMEOUT_MS` (es. 10s), il firmware ferma il motore e va in `STATE_ERROR` con alert al ROOT
- **Protezione meccanica**: evitare di attivare entrambe le direzioni contemporaneamente nel codice — il driver H-bridge via relay non ha protezione hardware intrinseca da cortocircuito
- **Cablaggio**: zona ad alta vibrazione — usare cavi con guaina rinforzata e connettori con locking per finecorsa e motore
- **Posizionamento**: il PCB va in custodia IP54 nella zona vano porta, al riparo dall'umidità

## Rilevamento posizione senza finecorsa

Se la porta non dispone di finecorsa fisici, la posizione viene stimata tramite timer software (`motor_run_ms`). Al termine, l'H-bridge si disattiva e lo stato transita in `OPEN` o `CLOSED`.

> Dimensionare `motor_run_ms` con margine del 20% rispetto al tempo meccanico reale. In assenza di finecorsa, il timeout di sicurezza deve essere leggermente maggiore di `motor_run_ms` per non generare falsi `STATE_ERROR`.
