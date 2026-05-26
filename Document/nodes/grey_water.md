# DomoC — Nodo GREY_WATER (Valvola acque grigie + telecamera portellone)

---

## Descrizione

Il nodo `GREY_WATER` (ID: `0x0003`) controlla la valvola di scarico delle acque grigie del camper tramite H-bridge relay, gestisce l'alimentazione della telecamera sul portellone posteriore e monitora la tensione della batteria di servizio.

Funzioni principali:
- Apertura e chiusura motorizzata della valvola acque grigie tramite H-bridge (inversione di polarità)
- Alimentazione della telecamera portellone tramite relay dedicato
- Monitoraggio tensione batteria di servizio (ADC con partitore)
- Reazione automatica a `MSG_KEY_ON`: chiusura valvola se aperta, senza attendere comandi dal ROOT
- Pubblicazione stato sulla mesh per ROOT e HMI

**Principio architetturale**: la logica di sicurezza (chiusura su KEY_ON) è nel nodo e funziona anche in assenza di ROOT o HMI.

---

## Hardware

### Microcontrollore

- **ESP32-S3-MINI-1** su PCB Universale v3.0

### Mappatura risorse PCB

| Risorsa | Ruolo | GPIO |
|---|---|---|
| HB1 (H-bridge relay) | `motor` — valvola acque grigie | GPIO11 (DIR_A), GPIO12 (DIR_B), GPIO13 (EN) |
| HB2 | `unused` | — |
| REL1 | `camera` — alimentazione telecamera portellone | GPIO17 |
| REL2 | `unused` | — |
| OPT1 | `fc_closed` — finecorsa valvola chiusa (opzionale) | GPIO3 |
| OPT2 | `fc_open` — finecorsa valvola aperta (opzionale) | GPIO4 |
| ADC1 | `vbat_service` — batteria di servizio | GPIO1 |
| ADC2 | `unused` | GPIO2 |
| I2C | `unused` | GPIO8/9 |
| 1-Wire | `unused` | GPIO10 |
| LED RGB | stato nodo | GPIO21 |

### Schema GPIO rilevanti

| GPIO | Segnale | Funzione |
|---|---|---|
| GPIO1 | ADC_DIV1 | Partitore tensione batteria servizio (0–16.5V → 0–3.3V) |
| GPIO3 | OPT1_OUT | Finecorsa valvola chiusa (active-LOW, opzionale) |
| GPIO4 | OPT2_OUT | Finecorsa valvola aperta (active-LOW, opzionale) |
| GPIO11 | HB1_DIR_A | H-bridge — direzione A (apri valvola) |
| GPIO12 | HB1_DIR_B | H-bridge — direzione B (chiudi valvola) |
| GPIO13 | HB1_EN | H-bridge — enable (OFF in standby, taglia 12V_SW) |
| GPIO17 | REL1 | Relay alimentazione telecamera portellone |
| GPIO21 | LED_DATA | WS2812B stato |

### Alimentazione

```
[Bus 12V camper] ──→ [Buck 12V→3.3V] ──→ ESP32-S3
                ──→ [H-bridge relay K1-K3] ──→ Valvola/motore 12V
                ──→ [REL1 K7]              ──→ Telecamera portellone 12V
```

---

## Logica H-bridge — controllo valvola

La valvola usa un attuatore bipolare (inversione polarità). Il firmware rispetta sempre la sequenza sicura:

1. `HB1_EN = OFF`
2. Imposta `HB1_DIR_A / HB1_DIR_B` per la direzione desiderata
3. `HB1_EN = ON` → valvola in movimento
4. Al finecorsa o scadenza timeout → `HB1_EN = OFF`

| HB1_DIR_A | HB1_DIR_B | HB1_EN | Risultato |
|---|---|---|---|
| OFF | OFF | OFF | Standby sicuro (0 mA sul motore) |
| ON | OFF | ON | MOT_A=+12V, MOT_B=GND → **APRI** |
| OFF | ON | ON | MOT_A=GND, MOT_B=+12V → **CHIUDI** |
| ON | ON | ON | ⚠ Cortocircuito — **vietato** |

---

## Logica autonoma — Macchina a stati

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
         BOOT       │  comando APRI        fc/timer APERTO    │
           │        │  ────────────►  [OPENING]  ──────────►  │
           ▼        │                                         │
     [INITIALIZING] │  fc/timer           comando CHIUDI      │
           │        │  CHIUSO  ◄──  [CLOSED]  ◄──────────     │
           ▼        │                  │                      │
       [STOPPED]────┤                  │ MSG_KEY_ON           │
         /    \     │                  │ (se OPEN/OPENING)    │
   fc_chiuso nessuno│                  ▼                      │
       │        │   │           [AUTO_CLOSING]                │
    [CLOSED] [OPEN] │                  │                      │
                    │          fc/timer CHIUSO                │
                    │                  │                      │
                    │              [CLOSED]                   │
                    │                                         │
                    │   timeout > 5s senza finecorsa          │
                    │   [OPENING/CLOSING] ──────────► [ERROR] │
                    └─────────────────────────────────────────┘
```

### Stati

| Stato | Descrizione | LED |
|---|---|---|
| `INITIALIZING` | Boot, lettura finecorsa, registrazione mesh | Bianco lampeggiante |
| `CLOSED` | Valvola chiusa, HB1_EN=OFF | Verde fisso |
| `OPEN` | Valvola aperta, HB1_EN=OFF | Blu fisso |
| `OPENING` | HB1 attivo verso apertura | Blu lampeggiante |
| `CLOSING` | HB1 attivo verso chiusura | Arancio lampeggiante |
| `AUTO_CLOSING` | Chiusura automatica su MSG_KEY_ON | Rosso lampeggiante veloce |
| `ERROR` | Timeout valvola — bloccata o guasta | Rosso fisso |
| `STANDALONE` | Mesh assente > 30s | Bianco fisso |

---

## Reazione evento MSG_KEY_ON

```c
void on_key_on_event(void) {
    if (current_state == STATE_OPEN || current_state == STATE_OPENING) {
        hb_set_enable(false);
        hb_set_direction(DIR_CLOSE);
        hb_set_enable(true);
        transition_to(STATE_AUTO_CLOSING);
        start_safety_timer(VALVE_TIMEOUT_MS);
        send_alert(ALERT_GREY_WATER_AUTO_CLOSING,
                   "Acque grigie: chiusura automatica per KEY_ON");
    }
}
```

Il broadcast `MSG_KEY_ON` ha `dst_id = 0xFF` — nessun ACK richiesto. Il nodo reagisce autonomamente.

---

## Gestione telecamera portellone

La telecamera portellone (nodo fisicamente separato, tipicamente CAM_EXT) è alimentata tramite REL1:

```c
void on_camera_command(uint8_t action) {
    if (action == ACTION_CAM_ON) {
        gpio_set_level(GPIO_REL1, 1);
        cam_active = true;
        send_status_update();
    } else if (action == ACTION_CAM_OFF) {
        gpio_set_level(GPIO_REL1, 0);
        cam_active = false;
        send_status_update();
    }
}
```

> Il nodo GREY_WATER fornisce solo l'alimentazione 12V alla telecamera tramite REL1. Lo stream MJPEG è gestito direttamente dal nodo camera (CAM_EXT) in HTTP al di fuori della mesh dati.

---

## Monitoraggio batteria di servizio

ADC1 (GPIO1) legge la tensione della batteria di servizio tramite partitore 100kΩ/27kΩ:

```c
float read_service_battery_voltage(void) {
    int raw = adc1_get_raw(ADC1_CHANNEL_0);  // GPIO1
    float v_adc = (raw / 4095.0f) * 3.3f;
    return v_adc * (100.0f + 27.0f) / 27.0f;  // range 0–16.5V
}
```

- Lettura ogni 60s, inclusa nel payload di stato
- Alert mesh se < 11.8V (batteria scarica critica)
- Il nodo GREY_WATER è fisicamente vicino al vano batterie di servizio

---

## Heartbeat e payload di stato

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;       // offset 0 — CLOSED/OPEN/OPENING/CLOSING/AUTO_CLOSING/ERROR/STANDALONE
    uint8_t  error_code;  // offset 1 — 0=ok, 1=timeout valvola, 2=errore ADC
    float    battery_v;   // offset 2-5 — tensione batteria servizio (V)
    uint8_t  cam_active;  // offset 6 — 1 = REL1 attivo, telecamera alimentata
    uint8_t  fc_closed;   // offset 7 — 1 = finecorsa valvola chiusa attivo
    uint8_t  fc_open;     // offset 8 — 1 = finecorsa valvola aperta attivo
    uint8_t  _pad;        // offset 9 — allineamento
} grey_water_status_t;    // 10 byte
```

---

## Descriptor HMI

```c
static const node_descriptor_t GREY_WATER_DESCRIPTOR = {
    .node_icon      = ICON_VALVE_GREY,
    .action_count   = 4,
    .property_count = 3,
    .actions = {
        // action_code      icon_id              ctrl_type     group_id  linked_property  flags              label
        { ACTION_OPEN,     ICON_ACT_OPEN,       CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "APRI"    },
        { ACTION_CLOSE,    ICON_ACT_CLOSE,      CTRL_BUTTON,  0,        0,               FLAG_KEY_BLOCKED,  "CHIUDI"  },
        { ACTION_CAM_ON,   ICON_ACT_CAM_ON,     CTRL_TOGGLE,  1,        PROP_CAM_ACTIVE, 0,                 "CAMERA"  },
        { ACTION_CAM_OFF,  ICON_ACT_CAM_OFF,    CTRL_TOGGLE,  1,        PROP_CAM_ACTIVE, 0,                 "CAM OFF" },
    },
    .properties = {
        // property_id      offset  type             widget_type        range_min  range_max  unit  fmt
        { PROP_STATE,       0,      PAYLOAD_UINT8,   WIDGET_LABEL,      0,         0,         "",   "%s"   },
        { PROP_BATTERY_V,   2,      PAYLOAD_FLOAT32, WIDGET_BATTERY,    110,       145,       "V",  "%.1f" },
        { PROP_CAM_ACTIVE,  6,      PAYLOAD_UINT8,   WIDGET_INDICATOR,  0,         0,         "",   "%s"   },
    },
};
```

---

## Comportamento in STANDALONE_MODE

Se la mesh è assente per > 30s:

1. Entra in `STATE_STANDALONE`
2. Abilita eventuale pulsante fisico locale
3. I finecorsa continuano a funzionare (hardware)
4. La chiusura automatica su KEY_ON funziona comunque: i broadcast mesh sono ricevuti direttamente anche senza ROOT

```c
void on_mesh_reconnected(void) {
    standalone_mode = false;
    mesh_register();       // reconnect=true — ROOT conferma ID esistente da NVS
    send_status_update();
}
```

---

## Blocco comandi con chiave inserita

Con `MSG_KEY_ON` attivo (KEY_ON flag = true), i comandi di apertura ricevuti dalla mesh restituiscono `ACK_REJECTED`. L'HMI mostra il pulsante APRI disabilitato.

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi, dispatch per msg_type |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, alert, ACK |
| `valve_control_task` | 6 | 3 KB | Macchina a stati, gestione H-bridge |
| `adc_monitor_task` | 2 | 2 KB | Lettura periodica tensione batteria servizio |
| `ota_receiver_task` | 2 | 6 KB | Ricezione e applicazione OTA |

---

## Gestione energetica

- **HB1_EN=OFF** in standby → corrente sul motore zero
- `WIFI_PS_MIN_MODEM` in tutti gli stati non-movimento
- TX power fisso 10 dBm
- `periph_module_disable()` per I2C e 1-Wire (non usati)
- **Consumo stimato**:
  - Idle (valvola chiusa, camera spenta): ~5 mA
  - Mesh attiva in ricezione: ~8–12 mA
  - Valvola in movimento: 200–800 mA @ 12V per 2–5 secondi
  - Camera alimentata (REL1 ON): +50–200 mA aggiuntivi

---

## node_config.json

```json
{
  "node_name": "GREY_WATER",
  "node_type": "GREY_WATER",
  "mesh": { "channel": 6, "mesh_id": "DomoC01" },
  "hardware": {
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
      "motor_run_ms": 5000,
      "opt_fc_closed": "opt1", "opt_fc_open": "opt2"
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "camera", "gpio": 17 },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
    "adc1": { "role": "vbat_service", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 }
  }
}
```

---

## Considerazioni pratiche

- **Posizionamento**: zona scarico acque grigie — custodia IP54 minima, preferire IP65
- **Cablaggio valvola**: usare cavi con guaina resistente all'umidità; connettori impermeabili
- **Finecorsa**: consigliati tipo IP65; in alternativa il firmware stima la posizione tramite timer (`motor_run_ms`)
- **Timeout sicurezza**: se il finecorsa non scatta entro 5s dall'avvio, il firmware ferma l'H-bridge e transita in `STATE_ERROR`
- **Batteria servizio**: il filo ADC1 corre fino al vano batterie — usare cavo schermato se il percorso è > 1m

## Rilevamento posizione senza finecorsa

Se la valvola non dispone di finecorsa fisici, la posizione viene stimata tramite timer software (`motor_run_ms` dal `node_config.json`). Al termine del timer, l'H-bridge viene disattivato e lo stato transita in `OPEN` o `CLOSED` in base alla direzione.

> Dimensionare `motor_run_ms` con un margine del 20% rispetto al tempo meccanico reale per garantire il completamento del movimento anche con alimentazione bassa.
