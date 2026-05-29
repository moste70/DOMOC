# DomoC — Nodo THERMO_BUNK (Termostato letto a castello)

---

## Descrizione

Il nodo `THERMO_BUNK` è un termostato locale dedicato al letto a castello. Gestisce autonomamente la valvola dell'aria calda in base alla temperatura impostata e rilevata nella zona letto a castello.

Funzioni principali:

- **Controllo valvola aria calda**: attiva/disattiva la valvola in base al confronto tra temperatura letta e temperatura impostata.
- **Rilevamento temperatura locale**: sensore digitale (es. DS18B20, SHT31) posizionato nel letto a castello.
- **Visualizzazione locale**: display touch compatto che mostra temperatura, setpoint e stato valvola.
- **Gestione termostato locale/centrale**: se il termostato locale è "Off", la gestione della valvola passa al termostato centralizzato (HMI).
- **Pubblicazione dati sulla mesh**: stato valvola, temperatura letta, temperatura impostata e stato locale/centrale sono pubblicati periodicamente sulla mesh per visualizzazione e log.

---

## Controllo luci letto a castello

Oltre alla gestione della valvola aria calda, il nodo THERMO_BUNK controlla anche le luci del letto a castello:

- **Due luci indipendenti**: luce letto alto e luce letto basso, ciascuna comandabile dal display touch locale e dall'HMI.
- I comandi ricevuti via mesh dall'HMI aggiornano lo stato locale e il display.

---

## Hardware

- **ESP32-C3** (basso consumo, Wi-Fi mesh)
- **Sensore temperatura**: DS18B20 (1-Wire) o SHT31 (I2C)
- **Display touch**: 3.2" IPS capacitivo (es. CrowPanel ESP32 240×320)
- **Relay/MOSFET**: per attuazione valvola aria calda
- **Alimentazione**: 12V bus camper → buck 3.3V

---

## Logica di funzionamento

1. **Modalità locale ON**:
   - L'utente imposta la temperatura desiderata tramite i pulsanti.
   - Il nodo legge la temperatura ambiente.
   - Se la temperatura letta < setpoint - hysteresis → attiva la valvola (apre aria calda).
   - Se la temperatura letta > setpoint + hysteresis → disattiva la valvola.
   - Stato e valori sono mostrati sul display e pubblicati sulla mesh.

2. **Modalità locale OFF**:
   - Il nodo ignora i pulsanti Temp Up/Down e non attua la valvola e la apre.
   - Riceve eventuali comandi dal termostato centralizzato (es. HMI o ROOT) via mesh.
   - Stato e valori sono comunque visualizzati sul display.

3. **Gestione touch display**:
   - Temp+/Temp-: incrementano/decrementano il setpoint (step 0.5°C, range 15–30°C).
   - Locale/Centrale: commuta tra gestione locale e centralizzata (con feedback visivo).

4. **Pubblicazione mesh**:
   - Ogni variazione di stato/setpoint/temperatura viene pubblicata sulla mesh (MSG_STATUS, MSG_ALERT).
   - L'HMI può visualizzare e, se abilitato, modificare il setpoint da remoto.

---

## Display consigliato

Per il nodo THERMO_BUNK è consigliabile utilizzare un display touch screen di piccole dimensioni (max 3"), ad esempio un modulo TFT o OLED touch SPI/I2C. Il touch permette di gestire la regolazione della temperatura, il controllo delle luci e tutte le funzioni direttamente da interfaccia grafica, eliminando la necessità di pulsanti fisici.

- **Vantaggi**:
  - Interazione più intuitiva e moderna
  - Possibilità di visualizzare più informazioni e controlli su un'unica schermata compatta
  - Riduzione del cablaggio e dei componenti meccanici

> Esempi: display TFT touch SPI (es. 2.4"–2.8" ILI9341), OLED touch, moduli Nextion compatti, ecc.

---

## Display consigliato per questo nodo

Per questo nodo si consiglia l'uso di un display compatto ESP32 touch da circa 3 pollici, ad esempio:

- **Modello:** CrowPanel 3.2" ESP32 LCD IPS Touch Display
- **Risoluzione:** 240x320 pixel
- **Touch:** Capacitivo
- **Compatibilità:** Arduino IDE, ESP-IDF, PlatformIO, MicroPython, LVGL
- **Connettività:** WiFi, Bluetooth integrati
- **Alimentazione:** 5V (USB o batteria)

Questa soluzione offre le stesse funzionalità software e di sviluppo della versione 7" HMI, ma in formato ridotto, ideale per installazione in spazi compatti come il letto a castello.

---

## Esempio schermata display

```text
T: 19.5°C  [ON]
Set: 21.0°C
Valvola: ON
```

- [ON]/[OFF]: modalità locale attiva/disattiva
- T: temperatura letta
- Set: temperatura impostata
- Valvola: stato attuale attuatore

---

## Payload di stato

```c
typedef struct __attribute__((packed)) {
    float    temperature;  // offset 0-3  — °C rilevata
    float    setpoint;     // offset 4-7  — °C impostata
    uint8_t  valve_on;     // offset 8    — 1 = valvola aria calda aperta
    uint8_t  mode_local;   // offset 9    — 1 = controllo locale, 0 = centrale
    uint8_t  light_hi;     // offset 10   — 1 = luce letto alto accesa
    uint8_t  light_lo;     // offset 11   — 1 = luce letto basso accesa
    uint8_t  error_code;   // offset 12
} thermo_bunk_status_t;    // 13 byte
```

## Descriptor HMI

```c
static const node_descriptor_t THERMO_BUNK_DESCRIPTOR = {
    .node_icon      = ICON_THERMOMETER,
    .action_count   = 4,
    .property_count = 3,
    .actions = {
        // action_code        icon_id              ctrl_type      group_id  linked_property  flags  label
        { ACTION_TEMP_UP,    ICON_ACT_TEMP_UP,    CTRL_STEPPER,  1,        PROP_SETPOINT,   0,     "TEMP +" },
        { ACTION_TEMP_DN,    ICON_ACT_TEMP_DN,    CTRL_STEPPER,  1,        PROP_SETPOINT,   0,     "TEMP -" },
        { ACTION_LIGHT_ON,   ICON_ACT_LIGHT_ON,   CTRL_TOGGLE,   2,        PROP_LIGHT_ON,   0,     "LUCE"   },
        { ACTION_LIGHT_OFF,  ICON_ACT_LIGHT_OFF,  CTRL_TOGGLE,   2,        PROP_LIGHT_ON,   0,     "LUCE-"  },
    },
    .properties = {
        // property_id      offset  type             widget_type          range_min  range_max  unit   fmt
        { PROP_TEMPERATURE, 0,      PAYLOAD_FLOAT32, WIDGET_THERMOMETER,  150,       300,       "°C",  "%.1f" },
        { PROP_SETPOINT,    4,      PAYLOAD_FLOAT32, WIDGET_GAUGE,        150,       300,       "°C",  "%.1f" },
        { PROP_VALVE_ON,    8,      PAYLOAD_UINT8,   WIDGET_INDICATOR,    0,         0,         "",    "%s"   },
    },
};
```

## Sicurezza e fallback

- In caso di perdita mesh, il nodo continua a funzionare in modalità locale con l'ultimo setpoint.
- In caso di errore sensore, la valvola viene disattivata e viene pubblicato un alert sulla mesh.

---

## Vantaggi

- Comfort personalizzato per il letto a castello
- Basso consumo energetico
- Integrazione trasparente con la logica centralizzata del sistema DomoC

---

## Interazione esclusivamente touch

Nel nodo THERMO_BUNK, tutti i controlli (temperatura, valvola, luci) sono gestiti esclusivamente tramite il display touch screen: **non sono previsti pulsanti fisici**. Tutte le funzioni sono accessibili da interfaccia grafica.

- Dopo 1 minuto di inutilizzo, il display si spegne automaticamente per ridurre i consumi.
- Il display si riattiva al primo tocco.

> Questa soluzione semplifica il cablaggio, migliora l'estetica e rende l'interazione più intuitiva.
