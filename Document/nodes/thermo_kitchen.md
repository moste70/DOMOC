# DomoC — Nodo THERMO_KITCHEN (Termostato cucina)

---

## Descrizione

Il nodo `THERMO_KITCHEN` (ID: `0x0007`) è il termostato locale dedicato alla zona cucina del camper. Gestisce autonomamente la valvola dell'aria calda in base alla temperatura impostata e rilevata.

Funzioni principali:
- **Controllo valvola aria calda**: attiva/disattiva la valvola in base al confronto tra temperatura letta e setpoint con isteresi
- **Rilevamento temperatura locale**: sensore digitale posizionato nella zona cucina
- **Visualizzazione locale**: display touch compatto che mostra temperatura, setpoint e stato valvola
- **Modalità locale/centrale**: in modalità locale l'utente gestisce il setpoint; in modalità centrale la gestione passa all'HMI
- **Pubblicazione dati sulla mesh**: stato valvola, temperatura e setpoint pubblicati periodicamente per ROOT e HMI

La cucina è una zona con variazioni termiche rapide (cottura). L'isteresi del termostato va dimensionata tenendo conto di questa dinamica (consigliato ±1.5°C invece dei ±0.5°C degli altri termostati).

---

## Hardware

### Microcontrollore

- **ESP32-C3** — singolo core, basso consumo, Wi-Fi mesh
- Non monta il PCB Universale v3.0 (come tutti i nodi THERMO)

### Componenti

| Componente | Tipo | Note |
|---|---|---|
| Sensore temperatura | DS18B20 (1-Wire) o SHT31 (I2C) | Posizionato lontano dai fuochi — zona parete/soffitto cucina |
| Valvola aria calda | Elettrovalvola 12V NC | Si chiude al de-energize (sicurezza) |
| Driver valvola | Relay SPDT 12V/10A o MOSFET | Commutazione alimentazione 12V alla valvola |
| Display | Touch screen 3.2" IPS capacitivo | CrowPanel ESP32 o equivalente — gestione locale |
| Alimentazione | Bus 12V camper → buck 3.3V | Il relay valvola prende 12V direttamente |

### GPIO (ESP32-C3)

| GPIO | Funzione | Tipo |
|---|---|---|
| GPIO2 | Relay/MOSFET valvola aria calda | Output |
| GPIO3 | Sensore DS18B20 (1-Wire) | I/O |
| GPIO8 | I2C SDA (SHT31 se usato) | I2C |
| GPIO9 | I2C SCL (SHT31 se usato) | I2C |
| GPIO10 | LED stato | Output PWM |

---

## Logica di funzionamento

### Modalità locale ON

```c
void thermostat_tick(void) {
    float temp = read_temperature();
    float hysteresis = 1.5f;  // cucina: isteresi maggiore per dinamica termica rapida

    if (mode_local && valve_on) {
        if (temp >= setpoint + hysteresis) {
            valve_off();
        }
    } else if (mode_local && !valve_on) {
        if (temp <= setpoint - hysteresis) {
            valve_on();
        }
    }
    // Mesh: pubblica stato se è cambiato qualcosa
    if (state_changed()) send_status_update();
}
```

Il ciclo `thermostat_tick` viene eseguito dal `function_task` ogni 30s o ad ogni variazione significativa di temperatura (> 0.3°C).

### Modalità locale OFF (centralizzata)

- Il nodo non attua autonomamente la valvola
- Rimane in ascolto di comandi `MSG_COMMAND` dall'HMI
- Temperatura e stato sono comunque pubblicati sulla mesh
- Il display mostra lo stato aggiornato ma i controlli di setpoint sono disabilitati localmente

### Logica valvola

La valvola è normalmente chiusa (NC): senza alimentazione è chiusa (stato sicuro). Il relay la mantiene aperta finché è energizzato.

```c
void valve_on(void) {
    gpio_set_level(GPIO_RELAY_VALVE, 1);
    valve_active = true;
}

void valve_off(void) {
    gpio_set_level(GPIO_RELAY_VALVE, 0);
    valve_active = false;
}
```

---

## Macchina a stati termostato

| Stato | Condizione attivazione | Azione |
|---|---|---|
| `IDLE_LOCAL_OFF` | Modalità locale OFF | Nessuna attuazione autonoma; ascolta mesh |
| `IDLE_LOCAL_ON` | Locale ON, temp OK (entro isteresi) | Mantiene stato valvola corrente |
| `HEATING` | Locale ON, temp < setpoint − hys | Valvola aperta (relay ON) |
| `SATISFIED` | Locale ON, temp > setpoint + hys | Valvola chiusa (relay OFF) |
| `ERROR` | Sensore assente o lettura fallita × 3 | Valvola chiusa, alert mesh |
| `STANDALONE` | Mesh assente > 30s | Funziona in locale con ultimo setpoint noto |

---

## Display locale

Schermata principale (touch, 3.2" 240×320):

```
┌──────────────────────────────────┐
│  CUCINA                  [LOCAL] │
│                                  │
│      T attuale: 21.3°C           │
│      Setpoint:  22.0°C           │
│                                  │
│      Valvola: ██ ON              │
│                                  │
│  [ − TEMP ]        [ TEMP + ]    │
│  [ LOCALE / CENTRALE ]           │
└──────────────────────────────────┘
```

- Tocco su `[ − TEMP ]` / `[ TEMP + ]`: modifica setpoint di 0.5°C (range 15–30°C)
- Tocco su `[ LOCALE / CENTRALE ]`: commuta modalità
- Dopo 60s di inattività il display si spegne automaticamente (primo tocco lo riattiva)

---

## Payload di stato

```c
typedef struct __attribute__((packed)) {
    float    temperature;  // offset 0-3  — °C rilevata dal sensore
    float    setpoint;     // offset 4-7  — °C impostata
    uint8_t  valve_on;     // offset 8    — 1 = valvola aria calda aperta (relay ON)
    uint8_t  mode_local;   // offset 9    — 1 = controllo locale, 0 = centrale (da HMI)
    uint8_t  error_code;   // offset 10   — 0=ok, 1=errore sensore, 2=timeout mesh
    uint8_t  _pad;         // offset 11   — allineamento
} thermo_kitchen_status_t; // 12 byte
```

---

## Descriptor HMI

```c
static const node_descriptor_t THERMO_KITCHEN_DESCRIPTOR = {
    .node_icon      = ICON_THERMOMETER,
    .action_count   = 4,
    .property_count = 3,
    .actions = {
        // action_code       icon_id             ctrl_type      group_id  linked_property  flags  label
        { ACTION_TEMP_UP,   ICON_ACT_TEMP_UP,   CTRL_STEPPER,  1,        PROP_SETPOINT,   0,     "TEMP +"  },
        { ACTION_TEMP_DN,   ICON_ACT_TEMP_DN,   CTRL_STEPPER,  1,        PROP_SETPOINT,   0,     "TEMP -"  },
        { ACTION_MODE_LOCAL, ICON_ACT_LOCAL,    CTRL_TOGGLE,   2,        PROP_MODE_LOCAL, 0,     "LOCALE"  },
        { ACTION_MODE_CENTRAL, ICON_ACT_CENTRAL,CTRL_TOGGLE,   2,        PROP_MODE_LOCAL, 0,     "CENTRALE"},
    },
    .properties = {
        // property_id      offset  type             widget_type          range_min  range_max  unit   fmt
        { PROP_TEMPERATURE, 0,      PAYLOAD_FLOAT32, WIDGET_THERMOMETER,  150,       350,       "°C",  "%.1f" },
        { PROP_SETPOINT,    4,      PAYLOAD_FLOAT32, WIDGET_GAUGE,        150,       300,       "°C",  "%.1f" },
        { PROP_VALVE_ON,    8,      PAYLOAD_UINT8,   WIDGET_INDICATOR,    0,         0,         "",    "%s"   },
    },
};
```

---

## Comportamento in STANDALONE_MODE

Se la mesh è assente per > 30s:

1. Entra in `STATE_STANDALONE`
2. Continua a funzionare in modalità locale con l'ultimo setpoint noto
3. Il display mostra `[STANDALONE]` in header
4. Al ripristino della mesh: ri-registrazione con `reconnect=true`, pubblica stato corrente

```c
void on_mesh_disconnected(void) {
    standalone_mode = true;
    // Continua il ciclo termostato localmente — nessuna interruzione del riscaldamento
}
```

---

## Sicurezza e fallback

- **Errore sensore**: in caso di lettura fallita per 3 cicli consecutivi, la valvola viene chiusa e viene inviato `MSG_ALERT` con `error_code = 1`
- **Perdita mesh**: il nodo continua in modalità locale con l'ultimo setpoint — il riscaldamento non si interrompe
- **Valvola NC**: allo spegnimento del nodo o in caso di reset, la valvola si chiude automaticamente (nessuna alimentazione = chiusa)

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, dispatch per msg_type |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, alert, ACK |
| `function_task` | 4 | 4 KB | Ciclo termostato, lettura sensore, attuazione valvola |
| `display_task` | 3 | 6 KB | Rendering display touch, gestione input utente |
| `ota_receiver_task` | 2 | 6 KB | Ricezione e applicazione OTA |

---

## Gestione energetica

- `WIFI_PS_MIN_MODEM` attivo — Wi-Fi in modem sleep tra heartbeat
- TX power fisso 10 dBm
- Display con dimming automatico dopo 60s di inattività
- **Consumo stimato**:
  - Idle (valvola chiusa, display spento): ~5 mA
  - Display attivo: +20–40 mA
  - Valvola aperta (relay energizzato): +50–100 mA aggiuntivi sulla bobina relay

---

## Considerazioni pratiche

- **Posizionamento sensore**: lontano dai fuochi e dal forno — parete o soffitto della cucina, lato opposto ai fuochi per una misura rappresentativa della zona
- **Isteresi cucina**: aumentare l'isteresi a ±1.5–2°C rispetto ai termostati delle zone notte (±0.5–1°C) per evitare cicli brevi dovuti al calore di cottura
- **Display**: montare in posizione accessibile senza occupare spazio sul piano lavoro — parete laterale o cabinato cucina consigliati
- **Cablaggio valvola**: il cavo 12V alla valvola deve sopportare almeno 2A — usare sezione minima 0.75mm²
