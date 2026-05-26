# DomoC — File di configurazione nodo (JSON)

---

## Perché JSON e non XML

| Criterio | JSON | XML |
|---|---|---|
| Parser su ESP-IDF | **cJSON integrato** — nessuna dipendenza | libexpat ~50 KB extra |
| Dimensione file | Compatto, nessun tag chiusura | Verboso, raddoppia le dimensioni |
| Scrittura in NVS | Veloce — meno byte → meno cicli flash | Più lento |
| Leggibilità umana | Alta | Alta (ma più rumorosa) |
| Generazione da PC/tool | Qualsiasi linguaggio, REST nativo | Richiede XML builder |
| Commenti nel file | Non standard (non usarli) | Supportati (`<!-- -->`) |
| Validazione schema | JSON Schema, ajv | XSD |

**Decisione**: JSON con cJSON (già incluso in ESP-IDF 4.x/5.x). File salvato in NVS oppure
flashato nella partizione `config` dedicata.

---

## Dove vive il file

```
Flash ESP32
├── app0        (firmware)
├── app1        (OTA slot)
├── nvs         (parametri runtime — setpoint, contatori)
└── config      (partizione FAT/SPIFFS 64 KB — node_config.json)
```

Il file viene scritto una sola volta (provisioning iniziale o aggiornamento config via OTA-config).
Al boot il firmware lo legge, lo valida e popola le strutture C interne.

---

## Struttura generale del file

```json
{
  "version": 1,
  "node": { ... },
  "mesh": { ... },
  "hardware": { ... },
  "behavior": { ... },
  "descriptor": { ... },
  "subscriptions": [ ... ]
}
```

| Sezione | Obbligatoria | Descrizione |
|---|---|---|
| `version` | Sì | Versione schema — il firmware rifiuta file con versione non supportata |
| `node` | Sì | Identità del nodo |
| `mesh` | Sì | Parametri rete mesh |
| `hardware` | Sì | Mappatura GPIO e periferiche |
| `behavior` | Sì | Parametri comportamento (timeout, soglie) |
| `descriptor` | No | Override del descriptor HMI (se assente → descriptor compilato nel firmware) |
| `subscriptions` | No | Override delle sottoscrizioni eventi mesh |

---

## Sezione `node` — Identità

```json
"node": {
  "id": 3,
  "type": "STEP",
  "label": "Gradino accesso",
  "hw_revision": "1.1",
  "fw_min_version": "2.0.0"
}
```

| Campo | Tipo | Descrizione |
|---|---|---|
| `id` | `uint8` 1–254 | ID univoco sulla mesh (0 = non assegnato, 255 = broadcast) |
| `type` | `string` | Tipo funzionale: `ROOT`, `STEP`, `GREY_WATER`, `FRESH_WATER`, `FRONT_DOOR`, `THERMO_BUNK`, `THERMO_LOFT`, `THERMO_KITCHEN`, `REAR_CAM`, `CAM_EXT`, `HMI` |
| `label` | `string` max 32 | Nome leggibile mostrato sull'HMI in caso di fallback |
| `hw_revision` | `string` | Revisione PCB — per diagnostica |
| `fw_min_version` | `string` semver | Il firmware si rifiuta di avviarsi se la propria versione è inferiore |

---

## Sezione `mesh` — Rete

```json
"mesh": {
  "mesh_id": [0x77, 0x77, 0x77, 0x77, 0x77, 0x77],
  "channel": 6,
  "password": "domoc_mesh_2024",
  "max_layer": 4,
  "root_id": 1,
  "tx_power_dbm": 10
}
```

| Campo | Tipo | Descrizione |
|---|---|---|
| `mesh_id` | `array[6]` byte | BSSID virtuale della mesh — tutti i nodi devono avere lo stesso |
| `channel` | `uint8` 1–13 | Canale Wi-Fi (fisso per mesh a canale singolo) |
| `password` | `string` | Password mesh WPA2 |
| `max_layer` | `uint8` | Massimo numero di livelli gerarchia mesh (default 4) |
| `root_id` | `uint8` | ID del nodo ROOT — usato per indirizzare registrazione e descriptor |
| `tx_power_dbm` | `int8` | Potenza TX in dBm (2–20). Ridurre per camper: 10 dBm è sufficiente a <5 m |

---

## Sezione `hardware` — GPIO e periferiche

La struttura varia per tipo di nodo. Ogni campo non usato può essere omesso o impostato a `null`.

### Struttura `hardware` per PCB universale v3.0

Sul PCB universale ogni risorsa fisica è identificata da un nome fisso (`hb1`, `hb2`,
`rel1`, `rel2`, `opt1`, `opt2`, `adc1`, `adc2`, `i2c`, `onewire`). Il firmware legge
il campo `"role"` di ogni risorsa e configura l'IO di conseguenza.

```json
"hardware": {
  "hb1": {
    "role": "motor",
    "gpio_dir_a": 11,
    "gpio_dir_b": 12,
    "gpio_enable": 13,
    "motor_run_ms": 4000,
    "opt_fc_closed": "opt1",
    "opt_fc_open":   "opt2"
  },
  "hb2": { "role": "unused" },
  "rel1": { "role": "unused" },
  "rel2": { "role": "unused" },
  "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
  "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
  "adc1": { "role": "vbat_engine",  "gpio": 1 },
  "adc2": { "role": "unused",       "gpio": 2 },
  "i2c": {
    "gpio_sda": 8,
    "gpio_scl": 9,
    "freq_hz": 400000,
    "devices": [
      { "address": "0x44", "type": "SHT31", "role": "temp_humidity" }
    ]
  },
  "onewire": {
    "gpio": 10,
    "devices": [
      { "address": "28FF641D1C040000", "role": "temp_external" }
    ]
  }
}
```

#### Ruoli per tipo di risorsa

**H-bridge** (`hb1`, `hb2`): `"motor"` | `"unused"`

**Relay** (`rel1`, `rel2`): `"camera"` | `"valve_nc"` | `"lights"` | `"generic_no"` | `"unused"`

**Optoisolatore** (`opt1`, `opt2`):

| role | Funzione |
|---|---|
| `"key_on"` | Positivo sotto chiave — broadcast MSG_KEY_ON/OFF |
| `"fc_closed"` | Finecorsa chiuso (collegato a `hb_id`) |
| `"fc_open"` | Finecorsa aperto (collegato a `hb_id`) |
| `"door_sensor"` | Sensore porta/portellone |
| `"button"` | Pulsante esterno |
| `"generic_di"` | Ingresso digitale generico |
| `"unused"` | Disabilitato |

**Partitore ADC** (`adc1`, `adc2`): `"vbat_engine"` | `"vbat_service"` | `"voltage_generic"` | `"unused"`

**Dispositivi I2C** (`i2c.devices[]`): lista di oggetti `{ "address", "type", "role" }`

| type | role | Dispositivo |
|---|---|---|
| `"INA219"` | `"vbat_service"` / `"vbat_engine"` | Monitor batteria (V + A) |
| `"SHT31"` | `"temp_humidity"` | Temperatura + umidità |
| `"generic"` | `"sensor"` | Sensore I2C generico |

**Dispositivi 1-Wire** (`onewire.devices[]`): lista di oggetti `{ "address", "role" }`

| role | Funzione |
|---|---|
| `"temp_ambient"` | Temperatura ambiente nodo |
| `"temp_external"` | Sonda esterna remota |
| `"temp_water"` | Temperatura impianto idrico |
| `"temp_engine"` | Temperatura vano motore/batterie |
| `"unused"` | Ignorato |

> Per nodi con display (THERMO_BUNK, THERMO_LOFT, HMI) che non montano il PCB
> universale, la sezione `hardware` usa ancora il formato `gpio` classico con i nomi
> funzionali diretti (`relay_valve_heat`, `btn_temp_up`, ecc.).

---

## Sezione `behavior` — Parametri comportamento

```json
"behavior": {
  "motor_run_ms": 3000,
  "motor_timeout_ms": 10000,
  "debounce_fc_ms": 50,
  "standalone_timeout_s": 30,
  "heartbeat_interval_s": 5,
  "mesh_reconnect_interval_s": 10,
  "thermo": {
    "setpoint_default_c": 20.5,
    "setpoint_min_c": 15.0,
    "setpoint_max_c": 30.0,
    "setpoint_step_c": 0.5,
    "hysteresis_c": 0.5
  },
  "sensor_read_interval_s": 60,
  "led_brightness": 80
}
```

| Campo | Tipo | Default | Descrizione |
|---|---|---|---|
| `motor_run_ms` | `uint32` | 3000 | Durata alimentazione motore/valvola (ms) |
| `motor_timeout_ms` | `uint32` | 10000 | Timeout sicurezza — se il finecorsa non scatta entro questo tempo → STATE_ERROR |
| `debounce_fc_ms` | `uint32` | 50 | Debounce microswitch finecorsa (ms) |
| `standalone_timeout_s` | `uint32` | 30 | Secondi senza mesh prima di entrare in STANDALONE_MODE |
| `heartbeat_interval_s` | `uint16` | 5 | Frequenza heartbeat MSG_STATUS sulla mesh |
| `mesh_reconnect_interval_s` | `uint16` | 10 | Intervallo tentativi riconnessione mesh |
| `thermo.setpoint_default_c` | `float` | 20.0 | Setpoint iniziale al primo avvio (poi salvato in NVS) |
| `thermo.setpoint_min_c` | `float` | 15.0 | Limite inferiore setpoint |
| `thermo.setpoint_max_c` | `float` | 30.0 | Limite superiore setpoint |
| `thermo.setpoint_step_c` | `float` | 0.5 | Incremento per pulsante Temp+/- |
| `thermo.hysteresis_c` | `float` | 0.5 | Isteresi termostato (°C) |
| `sensor_read_interval_s` | `uint16` | 60 | Frequenza lettura sensori (DHT11) |
| `led_brightness` | `uint8` 0–255 | 128 | Luminosità LED PWM |

---

## Sezione `descriptor` — Override HMI (opzionale)

Se omessa, il firmware usa il descriptor **statico compilato nel firmware**. Questa sezione permette
di ridefinirlo senza ricompilare — utile per customizzazione sul campo.

```json
"descriptor": {
  "node_icon": "ICON_STEP",
  "actions": [
    {
      "action_code": 1,
      "icon": "ICON_ACT_OPEN",
      "ctrl_type": "CTRL_BUTTON",
      "group_id": 0,
      "linked_property": null,
      "flags": ["FLAG_KEY_BLOCKED"],
      "label": "APRI"
    },
    {
      "action_code": 2,
      "icon": "ICON_ACT_CLOSE",
      "ctrl_type": "CTRL_BUTTON",
      "group_id": 0,
      "linked_property": null,
      "flags": ["FLAG_KEY_BLOCKED"],
      "label": "CHIUDI"
    },
    {
      "action_code": 3,
      "icon": "ICON_ACT_INFO",
      "ctrl_type": "CTRL_BUTTON",
      "group_id": 0,
      "linked_property": null,
      "flags": [],
      "label": "INFO"
    }
  ],
  "properties": [
    {
      "property_id": "PROP_STATE",
      "payload_offset": 0,
      "payload_type": "PAYLOAD_UINT8",
      "widget": "WIDGET_LABEL",
      "range_min": null,
      "range_max": null,
      "unit": "",
      "fmt": "%s"
    },
    {
      "property_id": "PROP_TEMPERATURE",
      "payload_offset": 7,
      "payload_type": "PAYLOAD_FLOAT32",
      "widget": "WIDGET_THERMOMETER",
      "range_min": 15.0,
      "range_max": 40.0,
      "unit": "°C",
      "fmt": "%.1f"
    },
    {
      "property_id": "PROP_HUMIDITY",
      "payload_offset": 11,
      "payload_type": "PAYLOAD_FLOAT32",
      "widget": "WIDGET_PROGRESS",
      "range_min": 0.0,
      "range_max": 100.0,
      "unit": "%",
      "fmt": "%.0f"
    }
  ]
}
```

### Valori enum validi per `descriptor`

**`node_icon`**

| Stringa JSON | Valore C | Descrizione |
|---|---|---|
| `"ICON_GENERIC"` | `0x00` | Fallback |
| `"ICON_STEP"` | `0x01` | Gradino accesso |
| `"ICON_VALVE_GREY"` | `0x02` | Valvola acque grigie |
| `"ICON_VALVE_FRESH"` | `0x03` | Valvola acque chiare |
| `"ICON_DOOR"` | `0x04` | Porta motorizzata |
| `"ICON_THERMOMETER"` | `0x05` | Termostato |
| `"ICON_CAMERA"` | `0x06` | Telecamera |
| `"ICON_KEY"` | `0x07` | Chiave accensione |
| `"ICON_BATTERY"` | `0x08` | Batteria |
| `"ICON_SENSOR"` | `0x09` | Sensore generico |
| `"ICON_LIGHT"` | `0x0A` | Luce |

**`ctrl_type`**

| Stringa JSON | Valore C | Comportamento UI |
|---|---|---|
| `"CTRL_BUTTON"` | `0x00` | Tap singolo → MSG_COMMAND immediato |
| `"CTRL_TOGGLE"` | `0x01` | Pulsante stateful — ON/OFF da `linked_property` |
| `"CTRL_STEPPER"` | `0x02` | Coppia `[−] valore [+]` — valore da `linked_property` |
| `"CTRL_CONFIRM"` | `0x03` | Due tap richiesti (dialog conferma) |
| `"CTRL_SLIDER"` | `0x04` | Slider continuo (futuro) |

**`flags`** (array di stringhe)

| Flag | Effetto |
|---|---|
| `"FLAG_CONFIRM_REQUIRED"` | Mostra dialog di conferma prima di inviare il comando |
| `"FLAG_KEY_BLOCKED"` | Disabilita il controllo quando KEY_ON è attivo |

---

## Sezione `subscriptions` — Override sottoscrizioni (opzionale)

```json
"subscriptions": [
  { "source_node_id": 10, "msg_type": "MSG_ALERT" },
  { "source_node_id": 254, "msg_type": "MSG_COMMAND" },
  { "source_node_id": 1, "msg_type": "MSG_STATUS_REQ" },
  { "source_node_id": 1, "msg_type": "MSG_OTA_START" }
]
```

`source_node_id: 254` = broadcast (qualsiasi sorgente).

---

## Esempi completi per nodo

### STEP — Gradino di accesso

```json
{
  "version": 1,
  "node": {
    "id": 3,
    "type": "STEP",
    "label": "Gradino accesso",
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
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
      "motor_run_ms": 4000,
      "opt_fc_closed": "opt1", "opt_fc_open": "opt2"
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "unused" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
    "adc1": { "role": "unused", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "i2c": {
      "gpio_sda": 8, "gpio_scl": 9, "freq_hz": 400000,
      "devices": [
        { "address": "0x44", "type": "SHT31", "role": "temp_humidity" }
      ]
    },
    "onewire": {
      "gpio": 10,
      "devices": [
        { "address": "28FF641D1C040000", "role": "temp_external" }
      ]
    }
  },
  "behavior": {
    "motor_run_ms": 4000,
    "motor_timeout_ms": 10000,
    "debounce_fc_ms": 50,
    "standalone_timeout_s": 30,
    "heartbeat_interval_s": 5,
    "sensor_read_interval_s": 60
  }
}
```

---

### THERMO_BUNK — Termostato letto a castello

```json
{
  "version": 1,
  "node": {
    "id": 5,
    "type": "THERMO_BUNK",
    "label": "Termostato letto castello",
    "hw_revision": "1.0",
    "fw_min_version": "2.0.0"
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
    "gpio": {
      "relay_valve_heat": 5,
      "relay_light_hi": 6,
      "relay_light_lo": 7,
      "led_status": 10
    },
    "dht11": { "pin": 10 },
    "display": {
      "type": "TFT_ILI9341",
      "width": 240,
      "height": 320,
      "i2c_addr": null
    },
    "sensors": ["DHT11"]
  },
  "behavior": {
    "heartbeat_interval_s": 5,
    "sensor_read_interval_s": 30,
    "thermo": {
      "setpoint_default_c": 21.0,
      "setpoint_min_c": 15.0,
      "setpoint_max_c": 30.0,
      "setpoint_step_c": 0.5,
      "hysteresis_c": 0.5
    }
  }
}
```

---

### GREY_WATER — Valvola acque grigie

```json
{
  "version": 1,
  "node": {
    "id": 4,
    "type": "GREY_WATER",
    "label": "Valvola scarico acque grigie",
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
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
      "motor_run_ms": 3000,
      "opt_fc_closed": null, "opt_fc_open": null
    },
    "hb2":  { "role": "unused" },
    "rel1": { "role": "camera" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "fc_closed", "gpio": 3, "hb_id": "hb1" },
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
    "adc1": { "role": "vbat_service", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "i2c":  { "gpio_sda": 8, "gpio_scl": 9, "freq_hz": 400000, "devices": [] },
    "onewire": { "gpio": 10, "devices": [] }
  },
  "behavior": {
    "motor_run_ms": 5000,
    "motor_timeout_ms": 8000,
    "heartbeat_interval_s": 5,
    "adc_read_interval_s": 60,
    "battery_alert_threshold_v": 11.8
  }
}
```

---

### FRONT_DOOR — Porta ingresso motorizzata

```json
{
  "version": 1,
  "node": {
    "id": 9,
    "type": "FRONT_DOOR",
    "label": "Porta ingresso",
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
    "opt2": { "role": "fc_open",   "gpio": 4, "hb_id": "hb1" },
    "adc1": { "role": "unused", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "i2c":  { "gpio_sda": 8, "gpio_scl": 9, "freq_hz": 400000, "devices": [] },
    "onewire": { "gpio": 10, "devices": [] }
  },
  "behavior": {
    "motor_run_ms": 6000,
    "motor_timeout_ms": 10000,
    "debounce_fc_ms": 50,
    "standalone_timeout_s": 30,
    "heartbeat_interval_s": 5
  }
}
```

---

## Flusso di utilizzo del file

```
┌────────────────────────────────────────────────────────┐
│  PC / Tool di provisioning                             │
│  1. Genera node_config.json dal template               │
│  2. Imposta: id, type, GPIO, comportamento             │
│  3. Copia sul nodo via:                                │
│     a) esptool flash diretto (prima accensione)        │
│     b) OTA-config (nodo già in rete)                   │
│     c) File system upload via seriale (SPIFFS/LittleFS)│
└────────────────┬───────────────────────────────────────┘
                 │
                 ▼
┌────────────────────────────────────────────────────────┐
│  ESP32 — Boot                                          │
│  1. Legge /config/node_config.json da SPIFFS           │
│  2. Valida: versione schema, campi obbligatori         │
│  3. Popola strutture C (node_cfg_t, mesh_cfg_t ...)    │
│  4. Se presente sezione "descriptor" → override        │
│     del descriptor statico compilato                   │
│  5. Salva setpoint e contatori in NVS                  │
│  6. Avvia mesh, registrazione, heartbeat               │
└────────────────────────────────────────────────────────┘
```

### Aggiornamento configurazione a runtime (OTA-config)

Il ROOT può inviare `MSG_CONFIG_UPDATE` con il nuovo JSON come payload (o in chunked se >255 byte).
Il nodo:
1. Riceve e riassembla il file
2. Valida il JSON
3. Scrive il nuovo file su SPIFFS
4. Esegue un soft-reboot (`esp_restart()`) per applicarlo

---

## Parsing lato firmware (C / cJSON)

```c
#include "cJSON.h"
#include "esp_spiffs.h"

esp_err_t load_node_config(node_cfg_t *cfg) {
    char *buf = spiffs_read_file("/config/node_config.json");
    if (!buf) return ESP_ERR_NOT_FOUND;

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return ESP_ERR_INVALID_ARG;

    // Versione
    int ver = cJSON_GetObjectItem(root, "version")->valueint;
    if (ver != CONFIG_SCHEMA_VERSION) {
        cJSON_Delete(root); return ESP_ERR_NOT_SUPPORTED;
    }

    // Node identity
    cJSON *node = cJSON_GetObjectItem(root, "node");
    cfg->node_id   = cJSON_GetObjectItem(node, "id")->valueint;
    cfg->node_type = parse_node_type(cJSON_GetObjectItem(node, "type")->valuestring);
    strncpy(cfg->label, cJSON_GetObjectItem(node, "label")->valuestring, 32);

    // Behavior
    cJSON *beh = cJSON_GetObjectItem(root, "behavior");
    cfg->motor_run_ms     = cJSON_GetObjectItem(beh, "motor_run_ms")->valueint;
    cfg->motor_timeout_ms = cJSON_GetObjectItem(beh, "motor_timeout_ms")->valueint;
    cfg->heartbeat_s      = cJSON_GetObjectItem(beh, "heartbeat_interval_s")->valueint;

    // Thermo (solo se presente)
    cJSON *thermo = cJSON_GetObjectItem(beh, "thermo");
    if (thermo) {
        cfg->setpoint_default = (float)cJSON_GetObjectItem(thermo, "setpoint_default_c")->valuedouble;
        cfg->hysteresis       = (float)cJSON_GetObjectItem(thermo, "hysteresis_c")->valuedouble;
    }

    cJSON_Delete(root);
    return ESP_OK;
}
```

---

## Validazione e sicurezza

| Regola | Comportamento in caso di violazione |
|---|---|
| Campo `version` mancante o non supportato | Boot interrotto — LED rosso fisso, nessuna connessione mesh |
| Campo `node.id` mancante o = 0 | Boot interrotto |
| `node.id` duplicato in rete | ROOT rifiuta la registrazione, nodo in errore |
| GPIO non valido (>= 22 su C3) | Il nodo ignora il pin e logga un warning |
| JSON malformato | Boot interrotto — errore in NVS log |
| `fw_min_version` superiore al firmware installato | Boot interrotto con alert mesh al ROOT |

---

## Riepilogo campi — Riferimento rapido

```
version                              int      Schema version (attualmente: 1)
node.id                              uint8    ID unico nodo (1–253)
node.type                            string   Tipo funzionale
node.label                           string   Nome leggibile (max 32 char)
node.hw_revision                     string   Revisione PCB
node.fw_min_version                  string   Versione firmware minima (semver)
mesh.mesh_id                         array[6] BSSID virtuale mesh
mesh.channel                         uint8    Canale Wi-Fi
mesh.password                        string   Password WPA2 mesh
mesh.root_id                         uint8    ID nodo ROOT
mesh.tx_power_dbm                    int8     Potenza TX (2–20 dBm)
hardware.hb1/hb2.role                string   "motor" | "unused"
hardware.hb1/hb2.gpio_dir_a/b/en     int      GPIO H-bridge (fissi: 11/12/13, 14/15/16)
hardware.hb1/hb2.motor_run_ms        uint32   Durata impulso motore (ms)
hardware.hb1/hb2.opt_fc_closed/open  string   Riferimento a opt1/opt2 o null
hardware.rel1/rel2.role              string   "camera"|"valve_nc"|"lights"|"generic_no"|"unused"
hardware.opt1/opt2.role              string   "key_on"|"fc_closed"|"fc_open"|"door_sensor"|...
hardware.opt1/opt2.gpio              int      GPIO optoisolatore (fissi: 3, 4)
hardware.opt1/opt2.hb_id             string   "hb1"|"hb2" (solo per fc_closed/fc_open)
hardware.adc1/adc2.role              string   "vbat_engine"|"vbat_service"|"voltage_generic"|"unused"
hardware.adc1/adc2.gpio              int      GPIO ADC (fissi: 1, 2)
hardware.i2c.gpio_sda/scl            int      GPIO I2C (fissi: 8, 9)
hardware.i2c.freq_hz                 int      Frequenza I2C (tipico: 400000)
hardware.i2c.devices[]               array    Lista dispositivi I2C {address, type, role}
hardware.onewire.gpio                int      GPIO 1-Wire (fisso: 10)
hardware.onewire.devices[]           array    Lista dispositivi 1-Wire {address, role}
behavior.motor_run_ms                uint32   Durata impulso motore/valvola
behavior.motor_timeout_ms            uint32   Timeout sicurezza motore
behavior.debounce_fc_ms              uint32   Debounce finecorsa
behavior.standalone_timeout_s        uint32   Timeout prima di STANDALONE_MODE
behavior.heartbeat_interval_s        uint16   Frequenza heartbeat
behavior.sensor_read_interval_s      uint16   Frequenza lettura sensori
behavior.thermo.setpoint_default_c   float    Setpoint iniziale
behavior.thermo.setpoint_min/max_c   float    Limiti setpoint
behavior.thermo.setpoint_step_c      float    Passo incremento setpoint
behavior.thermo.hysteresis_c         float    Isteresi termostato
descriptor.*                         object   Override descriptor HMI (opzionale)
subscriptions                        array    Override sottoscrizioni mesh (opzionale)
```
