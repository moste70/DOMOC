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
| `type` | `string` | Tipo funzionale: `STEP`, `GREY_WATER`, `FRESH_WATER`, `THERMO_BUNK`, `THERMO_LOFT`, `KEY_ON`, `CAMERA`, `ROOT`, `HMI` |
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

### Sottosezione `gpio`

```json
"hardware": {
  "gpio": {
    "motor_dir_a":  2,
    "motor_dir_b":  3,
    "motor_enable": 4,
    "relay_power":  7,
    "led_status":   10,
    "fc_closed":    5,
    "fc_open":      6,
    "button_local": 9
  },
  "i2c": {
    "sda": 8,
    "scl": 9,
    "freq_hz": 400000
  },
  "dht11": {
    "pin": 10
  },
  "display": {
    "type": "OLED_SSD1306",
    "width": 128,
    "height": 32,
    "i2c_addr": 60
  },
  "sensors": ["DHT11"]
}
```

#### GPIO — Nomi riservati

| Nome campo GPIO | Tipo nodo | Funzione |
|---|---|---|
| `motor_dir_a` | STEP, GREY_WATER | DIR A H-bridge (apertura) |
| `motor_dir_b` | STEP, GREY_WATER | DIR B H-bridge (chiusura) |
| `motor_enable` | STEP, GREY_WATER | Enable H-bridge |
| `relay_power` | STEP, GREY_WATER | Relay alimentazione motore/attuatore |
| `relay_valve` | FRESH_WATER | Relay elettrovalvola |
| `relay_valve_heat` | THERMO_BUNK, THERMO_LOFT | Relay valvola aria calda |
| `relay_light_hi` | THERMO_BUNK | Relay luce letto alto |
| `relay_light_lo` | THERMO_BUNK | Relay luce letto basso |
| `fc_closed` | STEP | Finecorsa posizione chiusa |
| `fc_open` | STEP | Finecorsa posizione aperta |
| `led_status` | Tutti | LED stato (RGB o singolo) |
| `button_local` | Tutti | Pulsante locale standalone |
| `btn_temp_up` | THERMO_* | Pulsante Temperatura + |
| `btn_temp_dn` | THERMO_* | Pulsante Temperatura - |
| `btn_onoff` | THERMO_* | Pulsante ON/OFF termostato locale |
| `key_signal` | KEY_ON | Segnale +12V chiave accensione |
| `camera_power` | GREY_WATER | Alimentazione videocamera |

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
    "hw_revision": "1.1",
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
      "motor_dir_a": 2,
      "motor_dir_b": 3,
      "motor_enable": 4,
      "relay_power": 7,
      "led_status": 10,
      "fc_closed": 5,
      "fc_open": 6,
      "button_local": 9
    },
    "dht11": { "pin": 10 },
    "sensors": ["DHT11"]
  },
  "behavior": {
    "motor_run_ms": 3000,
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
      "motor_dir_a": 2,
      "motor_dir_b": 3,
      "motor_enable": 4,
      "relay_power": 7,
      "camera_power": 8,
      "led_status": 10
    },
    "sensors": []
  },
  "behavior": {
    "motor_run_ms": 3000,
    "motor_timeout_ms": 8000,
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
version                           int      Schema version (attualmente: 1)
node.id                           uint8    ID unico nodo (1–253)
node.type                         string   Tipo funzionale
node.label                        string   Nome leggibile (max 32 char)
node.hw_revision                  string   Revisione PCB
node.fw_min_version               string   Versione firmware minima (semver)
mesh.mesh_id                      array[6] BSSID virtuale mesh
mesh.channel                      uint8    Canale Wi-Fi
mesh.password                     string   Password WPA2 mesh
mesh.root_id                      uint8    ID nodo ROOT
mesh.tx_power_dbm                 int8     Potenza TX (2–20 dBm)
hardware.gpio.*                   int      Pin GPIO (vedi tabella nomi)
hardware.i2c.sda/scl              int      Pin I2C
hardware.i2c.freq_hz              int      Frequenza I2C (tipico: 400000)
hardware.display.type             string   Tipo display (OLED_SSD1306, TFT_ILI9341...)
hardware.sensors                  array    Lista sensori abilitati
behavior.motor_run_ms             uint32   Durata impulso motore/valvola
behavior.motor_timeout_ms         uint32   Timeout sicurezza motore
behavior.debounce_fc_ms           uint32   Debounce finecorsa
behavior.standalone_timeout_s     uint32   Timeout prima di STANDALONE_MODE
behavior.heartbeat_interval_s     uint16   Frequenza heartbeat
behavior.sensor_read_interval_s   uint16   Frequenza lettura sensori
behavior.thermo.setpoint_default_c float  Setpoint iniziale
behavior.thermo.setpoint_min_c    float   Limite minimo setpoint
behavior.thermo.setpoint_max_c    float   Limite massimo setpoint
behavior.thermo.setpoint_step_c   float   Passo incremento setpoint
behavior.thermo.hysteresis_c      float   Isteresi termostato
descriptor.*                      object  Override descriptor HMI (opzionale)
subscriptions                     array   Override sottoscrizioni mesh (opzionale)
```
