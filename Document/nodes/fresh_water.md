# DomoC — Nodo FRESH_WATER (Valvola acque chiare)

---

## Descrizione

Il nodo `FRESH_WATER` (ID: `0x0004`) controlla l'elettrovalvola di carico delle acque chiare del camper. Utilizza una **elettrovalvola normalmente chiusa (NC)**:

- **Se alimentata (REL1 ON, 12V)**: la valvola si apre
- **Se non alimentata (REL1 OFF)**: la valvola si chiude automaticamente — stato sicuro intrinseco

Non è necessaria l'inversione di polarità: un solo relay SPDT (REL1, GPIO17) controlla l'alimentazione 12V alla valvola.

**Principio architetturale**: su `MSG_KEY_ON` non è richiesta nessuna azione automatica — la valvola NC è già nello stato sicuro (chiusa) senza alimentazione. Il nodo però blocca i comandi di apertura quando KEY_ON è attivo.

---

## Hardware

### Microcontrollore

- **ESP32-S3-MINI-1** su **PCB Universale v3.0** — doppio core LX7 240 MHz, Wi-Fi

### Mappatura risorse PCB

| Risorsa | Ruolo | GPIO |
|---|---|---|
| HB1 | `unused` | — |
| HB2 | `unused` | — |
| REL1 | `valve_nc` — elettrovalvola acque chiare | GPIO17 |
| REL2 | `unused` | — |
| OPT1 | `unused` | GPIO3 |
| OPT2 | `unused` | GPIO4 |
| ADC1 | `unused` | GPIO1 |
| ADC2 | `unused` | GPIO2 |
| I2C | `unused` | GPIO8/9 |
| 1-Wire | `unused` | GPIO10 |
| LED RGB | stato nodo | GPIO21 |

### Schema GPIO rilevanti

| GPIO | Segnale | Funzione |
|---|---|---|
| GPIO17 | REL1 | Relay SPDT — alimentazione elettrovalvola 12V NC |
| GPIO21 | LED_DATA | WS2812B stato |

### Alimentazione

```
[Bus 12V camper] ──→ [Buck 12V→3.3V] ──→ ESP32-S3
                ──→ [REL1 K7 COM/NO]  ──→ Elettrovalvola 12V NC
```

---

## Logica di controllo — REL1

La valvola NC ha un solo relay di alimentazione. Non serve H-bridge.

| REL1 | Descrizione |
|---|---|
| OFF (GPIO17=0) | Valvola chiusa — stato sicuro, nessun consumo sulla valvola |
| ON (GPIO17=1) | Valvola aperta — 12V applicati alla bobina |

```c
void valve_open(void) {
    gpio_set_level(GPIO_REL1, 1);
    current_state = STATE_OPEN;
    send_status_update();
}

void valve_close(void) {
    gpio_set_level(GPIO_REL1, 0);
    current_state = STATE_CLOSED;
    send_status_update();
}
```

---

## Logica autonoma — Macchina a stati

```
     BOOT
       │
 [INITIALIZING]
       │
   [CLOSED] ◄────── comando CHIUDI
       │
  comando APRI
       │
    [OPEN]
```

### Stati

| Stato | Descrizione | LED |
|---|---|---|
| `INITIALIZING` | Boot, registrazione mesh | Bianco lampeggiante |
| `CLOSED` | REL1=OFF, valvola chiusa | Verde fisso |
| `OPEN` | REL1=ON, valvola aperta | Blu fisso |
| `STANDALONE` | Mesh assente > 30s | Bianco fisso |

---

## Reazione a MSG_KEY_ON

La valvola NC è intrinsecamente sicura: senza alimentazione è chiusa. Pertanto:

- Su `MSG_KEY_ON` **non viene eseguita nessuna azione automatica** — se la valvola è aperta rimane aperta (scenario: rifornimento acqua mentre il motore gira)
- I comandi di apertura da HMI sono bloccati quando KEY_ON è attivo (`FLAG_KEY_BLOCKED`)
- La valvola viene chiusa solo da un comando esplicito dell'utente o da perdita di alimentazione 12V

```c
void on_key_on_event(void) {
    key_on_active = true;
    // Nessuna chiusura automatica — valvola NC è già sicura
    send_status_update();
}

void on_key_off_event(void) {
    key_on_active = false;
    send_status_update();
}
```

---

## Blocco comandi con chiave inserita

Con KEY_ON attivo, i comandi di apertura ricevuti dalla mesh restituiscono `ACK_REJECTED`. L'HMI mostra il pulsante APRI disabilitato.

```c
void on_hmi_command(mesh_msg_t *msg) {
    cmd_payload_t *cmd = (cmd_payload_t*)msg->payload;
    if (msg->dst_id != NODE_ID_FRESH_WATER) return;

    switch (cmd->action_code) {
        case ACTION_OPEN:
            if (key_on_active) {
                send_ack(msg->seq_num, ACK_REJECTED);
                return;
            }
            valve_open();
            send_ack(msg->seq_num, ACK_OK);
            break;
        case ACTION_CLOSE:
            valve_close();
            send_ack(msg->seq_num, ACK_OK);
            break;
        case ACTION_GET_STATUS:
            send_status_update();
            break;
    }
}
```

---

## Comportamento in STANDALONE_MODE

Se la mesh è assente per > 30s:

1. Entra in `STATE_STANDALONE`
2. Abilita eventuale pulsante fisico locale
3. La valvola mantiene lo stato corrente
4. Al ripristino mesh: ri-registrazione con `reconnect=true`, pubblica stato corrente

---

## Heartbeat e payload di stato

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;       // offset 0 — CLOSED/OPEN/STANDALONE
    uint8_t  error_code;  // offset 1 — 0=ok
    uint8_t  key_on;      // offset 2 — 1 = KEY_ON attivo
    uint8_t  _pad;        // offset 3 — allineamento
} fresh_water_status_t;   // 4 byte
```

---

## Descriptor HMI

```c
static const node_descriptor_t FRESH_WATER_DESCRIPTOR = {
    .node_icon      = ICON_VALVE_FRESH,
    .action_count   = 2,
    .property_count = 1,
    .actions = {
        // action_code    icon_id         ctrl_type     group_id  linked_property  flags              label
        { ACTION_OPEN,   ICON_ACT_OPEN,  CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "APRI"   },
        { ACTION_CLOSE,  ICON_ACT_CLOSE, CTRL_BUTTON,  0,        0,               0,                 "CHIUDI" },
    },
    .properties = {
        // property_id  offset  type            widget_type    range_min  range_max  unit  fmt
        { PROP_STATE,   0,      PAYLOAD_UINT8,  WIDGET_LABEL,  0,         0,         "",   "%s" },
    },
};
```

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, dispatch per msg_type |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, ACK |
| `function_task` | 4 | 4 KB | Logica valvola, gestione stato |
| `ota_receiver_task` | 2 | 6 KB | Ricezione e applicazione OTA |

---

## Gestione energetica

- REL1=OFF in standby → corrente sulla valvola zero
- `WIFI_PS_MIN_MODEM` attivo
- TX power fisso 10 dBm
- **Consumo stimato**:
  - Idle (valvola chiusa): ~4 mA
  - Valvola aperta (bobina energizzata): +50–200 mA @ 12V

---

## node_config.json

```json
{
  "version": 1,
  "node": {
    "id": 4,
    "type": "FRESH_WATER",
    "label": "Valvola acque chiare",
    "hw_revision": "3.0",
    "fw_min_version": "3.0.0"
  },
  "mesh": {
    "mesh_id": [119, 119, 119, 119, 119, 119],
    "channel": 6,
    "password": "domoc_mesh_2024",
    "max_layer": 4,
    "root_id": 1,
    "tx_power_dbm": 10
  },
  "hardware": {
    "hb1": { "role": "unused" },
    "hb2": { "role": "unused" },
    "rel1": { "role": "valve_nc", "gpio": 17 },
    "rel2": { "role": "unused" },
    "opt1": { "role": "unused", "gpio": 3 },
    "opt2": { "role": "unused", "gpio": 4 },
    "adc1": { "role": "unused", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "i2c":  { "gpio_sda": 8, "gpio_scl": 9, "freq_hz": 400000, "devices": [] },
    "onewire": { "gpio": 10, "devices": [] }
  },
  "behavior": {
    "standalone_timeout_s": 30,
    "heartbeat_interval_s": 5
  }
}
```

---

## Considerazioni pratiche

- Usare relay/MOSFET dimensionati per la corrente della bobina della valvola (tipico 0.5–2A @ 12V)
- Preferire elettrovalvole a basso consumo con ritorno a molla (NC) per massima sicurezza intrinseca
- Il cablaggio tra PCB e valvola può essere lungo (vano acqua) — usare sezione minima 0.5mm²
