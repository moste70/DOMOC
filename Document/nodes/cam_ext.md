# DomoC — Nodo CAM_EXT (Telecamere esterne + motion detection)

---

## Descrizione

Il nodo `CAM_EXT` (ID: `0x000A`) gestisce le telecamere esterne del camper con funzione di sorveglianza e motion detection. Fornisce stream video MJPEG via HTTP e invia alert sulla mesh quando viene rilevato movimento.

Funzioni principali:
- Stream video MJPEG in tempo reale via HTTP (fuori mesh, connessione diretta)
- Motion detection via frame differencing (elaborazione on-board)
- Alert mesh (`MSG_ALERT`) con payload foto/metadati quando rileva movimento
- Snapshot JPEG su richiesta HTTP
- Controllo accensione/spegnimento stream da HMI tramite mesh

**Differenza con REAR_CAM**: CAM_EXT è orientato alla sorveglianza perimetrale del veicolo parcheggiato, con motion detection attiva. REAR_CAM è ottimizzato per la retromarcia, bassa latenza, stream sempre disponibile durante la guida.

---

## Hardware

### Modulo

- **ESP32-CAM** (AI-Thinker o compatibile)
  - ESP32-S (Xtensa LX6 dual-core 240 MHz)
  - 4MB Flash, PSRAM 4MB (frame buffer + motion detection)
  - OV2640 camera integrata (max 2MP)
  - Slot MicroSD per registrazione locale (opzionale)
  - Antenna Wi-Fi PCB integrata

### Sensore camera

| Parametro | Valore |
|---|---|
| Sensore | OV2640 |
| Risoluzione stream | SVGA 800×600 o VGA 640×480 |
| Risoluzione motion detection | QVGA 320×240 (ridotta per efficienza CPU) |
| FPS sorveglianza | 10–15 fps |
| Obiettivo consigliato | Grand angolo 160° fisheye, focale fissa |

### Alimentazione

```
[Bus 12V camper] ──→ [Buck 12V→5V, 500mA] ──→ ESP32-CAM (pin 5V)
```

- Consumo standby (motion detection attiva, stream spento): ~100–150 mA @ 5V
- Consumo stream attivo: 300–500 mA @ 5V
- Consigliato aggiungere condensatore bulk 470µF sull'alimentazione 5V (spike durante Wi-Fi TX)

---

## Architettura software

### Separazione video / controllo mesh

```
CAM_EXT (ESP32-CAM)
├── Task Mesh         → ESP-Mesh (controllo, alert, stato)
│   ├── MSG_REGISTER  → ROOT: boot registration
│   ├── MSG_HEARTBEAT → ROOT: ogni 5s + URL stream
│   ├── MSG_ALERT     → ROOT/HMI: movimento rilevato (con thumbnail opzionale)
│   ├── MSG_COMMAND   ← HMI: accendi/spegni stream, reset motion counter
│   └── MSG_STATUS    → ROOT/HMI: stato, FPS, eventi motion, URL
│
└── HTTP Server       → Wi-Fi locale (client diretto)
    ├── GET /stream   → stream MJPEG continuo
    ├── GET /capture  → snapshot JPEG
    └── GET /motion   → JSON storico eventi motion
```

---

## Motion Detection

### Algoritmo frame differencing

```c
#define MOTION_THRESHOLD    20     // differenza pixel per considerarlo "mosso"
#define MOTION_PIXEL_RATIO  0.02f  // 2% dei pixel devono cambiare per trigger

bool detect_motion(uint8_t *frame_curr, uint8_t *frame_prev, int width, int height) {
    int changed_pixels = 0;
    int total_pixels = width * height;

    for (int i = 0; i < total_pixels; i++) {
        int diff = abs((int)frame_curr[i] - (int)frame_prev[i]);
        if (diff > MOTION_THRESHOLD) {
            changed_pixels++;
        }
    }

    float ratio = (float)changed_pixels / total_pixels;
    return ratio >= MOTION_PIXEL_RATIO;
}
```

L'analisi avviene su frame GRAYSCALE ridotti a QVGA (320×240) per risparmiare CPU. Il frame a colori VGA/SVGA è usato solo per lo stream e per lo snapshot allegato all'alert.

### Cooldown e debounce

```c
#define MOTION_COOLDOWN_MS  10000  // 10s tra un alert e il successivo (evita spam mesh)
#define MOTION_CONFIRM_COUNT 2     // richiede 2 frame consecutivi con motion

static uint32_t last_motion_alert_ms = 0;
static int      motion_confirm = 0;

void motion_detection_task(void *pvParam) {
    uint8_t *prev_frame = NULL;

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        uint8_t *gray = rgb_to_gray(fb->buf, fb->width, fb->height);

        if (prev_frame && detect_motion(gray, prev_frame, fb->width, fb->height)) {
            motion_confirm++;
            if (motion_confirm >= MOTION_CONFIRM_COUNT) {
                uint32_t now = esp_timer_get_time() / 1000;
                if (now - last_motion_alert_ms > MOTION_COOLDOWN_MS) {
                    send_motion_alert(fb);  // invia alert mesh + snapshot opzionale
                    last_motion_alert_ms = now;
                }
                motion_confirm = 0;
            }
        } else {
            motion_confirm = 0;
        }

        free(prev_frame);
        prev_frame = gray;
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 fps per motion detection
    }
}
```

### Alert mesh su motion detection

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id;         // offset 0 — NODE_ID_CAM_EXT
    uint8_t  alert_type;      // offset 1 — ALERT_MOTION_DETECTED
    uint32_t timestamp_ms;    // offset 2-5 — timestamp esp_timer
    uint8_t  confidence;      // offset 6 — % pixel cambiati × 100 (0–100)
    uint8_t  has_thumbnail;   // offset 7 — 1 se segue thumbnail (non in questo msg)
    char     stream_url[32];  // offset 8-39 — URL per vedere il live
} motion_alert_payload_t;     // 40 byte
```

> Il thumbnail JPEG non è incluso nel payload mesh (limite 200 byte). L'HMI può recuperare uno snapshot tramite `GET /capture` all'URL ricevuto nell'alert.

---

## Registrazione su MicroSD (opzionale)

Se è presente una scheda MicroSD, il nodo può salvare automaticamente un clip video di 10s ogni volta che rileva movimento:

```c
// In node_config.json, sezione "behavior"
{
  "behavior": {
    "record_on_motion": true,
    "record_duration_s": 10,
    "max_storage_mb": 512,  // cancella file più vecchi se supera questa soglia
    "filename_pattern": "/video/%Y%m%d_%H%M%S.jpg"  // snapshot JPEG per ora
  }
}
```

> La registrazione video (non snapshot) richiede libreria AVI su ESP32-CAM e riduce significativamente l'FPS. Consigliato partire con snapshot JPEG e aggiungere video in seguito.

---

## Payload di stato mesh

```c
typedef struct __attribute__((packed)) {
    uint8_t  stream_active;    // offset 0 — 1 = stream HTTP attivo
    uint8_t  motion_enabled;   // offset 1 — 1 = motion detection attiva
    uint8_t  fps_current;      // offset 2 — FPS stream corrente
    uint8_t  error_code;       // offset 3 — 0=ok, 1=camera fail, 2=SD fail
    uint32_t motion_count;     // offset 4-7 — eventi motion dall'accensione
    uint32_t last_motion_ms;   // offset 8-11 — timestamp ultimo evento motion
    char     stream_url[32];   // offset 12-43 — URL stream es. "http://192.168.4.3/stream"
} cam_ext_status_t;            // 44 byte
```

---

## Descriptor HMI

```c
static const node_descriptor_t CAM_EXT_DESCRIPTOR = {
    .node_icon      = ICON_CAMERA_EXT,
    .action_count   = 4,
    .property_count = 3,
    .actions = {
        // action_code          icon_id             ctrl_type     group_id  linked_property     flags  label
        { ACTION_CAM_ON,       ICON_ACT_CAM_ON,    CTRL_TOGGLE,  0,        PROP_STREAM_ACTIVE, 0,     "STREAM"    },
        { ACTION_CAM_OFF,      ICON_ACT_CAM_OFF,   CTRL_TOGGLE,  0,        PROP_STREAM_ACTIVE, 0,     "STOP"      },
        { ACTION_MOTION_ON,    ICON_ACT_MOTION,    CTRL_TOGGLE,  1,        PROP_MOTION_ENABLED,0,     "MOTION ON" },
        { ACTION_MOTION_OFF,   ICON_ACT_MOTION_OFF,CTRL_TOGGLE,  1,        PROP_MOTION_ENABLED,0,     "MOTION OFF"},
    },
    .properties = {
        // property_id          offset  type             widget_type        range_min  range_max  unit  fmt
        { PROP_STREAM_ACTIVE,   0,      PAYLOAD_UINT8,   WIDGET_INDICATOR,  0,         0,         "",   "%s" },
        { PROP_MOTION_ENABLED,  1,      PAYLOAD_UINT8,   WIDGET_INDICATOR,  0,         0,         "",   "%s" },
        { PROP_MOTION_COUNT,    4,      PAYLOAD_UINT32,  WIDGET_LABEL,      0,         9999,      "",   "%d" },
    },
};
```

---

## Comportamento in STANDALONE_MODE

Se la mesh è assente per > 30s:

1. Entra in modalità standalone
2. La motion detection continua a funzionare localmente
3. Gli alert non possono essere inviati sulla mesh — se presente MicroSD, gli eventi vengono loggati localmente
4. Al ripristino della mesh: ri-registrazione, pubblica stato corrente e numero di eventi motion accumulati

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, dispatch |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, alert motion, ACK |
| `motion_detection_task` | 6 | 4 KB | Acquisizione frame, frame differencing, alert |
| `http_server_task` | 4 | 8 KB | Server HTTP (stream MJPEG, snapshot, JSON status) |
| `sd_logger_task` | 1 | 3 KB | Scrittura snapshot/log su MicroSD (se presente) |
| `ota_receiver_task` | 2 | 6 KB | Ricezione OTA |

> `motion_detection_task` ha priorità più alta di `http_server_task`: il rilevamento motion non deve essere interrotto dallo streaming.

---

## Gestione energetica

- **Motion detection attiva, stream spento** (modalità sorveglianza): ~100–150 mA @ 5V
- **Stream attivo**: +200–350 mA aggiuntivi
- **Tutto spento** (nodo in sleep, solo mesh heartbeat): ~30–50 mA @ 5V
- Considerare alimentazione sempre attiva se usato come sorveglianza notturna

---

## Considerazioni pratiche

- **Posizionamento**: angolo del camper con campo visivo massimo — parafango o sponda laterale; custodia IP65
- **Obiettivo fisheye**: 160° o superiore per coprire l'intera fiancata con una sola camera
- **Illuminazione notturna**: OV2640 ha scarsa sensibilità in bassa luce — valutare modulo con LED IR integrato (telecamere "night vision" ESP32-CAM con illuminatore IR 850nm)
- **Connessione Wi-Fi**: l'antenna PCB integrata è sufficiente per distanze < 10m in campo aperto; per distanze maggiori usare modulo con connettore u.FL e antenna esterna
- **SD card**: usare schede di classe 10 o superiore (U1/U3) per evitare rallentamenti in scrittura durante motion recording
- **Falsi positivi**: in ambienti con vento forte o pioggia, il motion threshold va aumentato (`MOTION_THRESHOLD` e `MOTION_PIXEL_RATIO`)

## Limitazioni note

- ESP32-CAM ha un solo core libero per l'utente (l'altro gestisce Wi-Fi/BT) — limitare il frame rate se la motion detection rallenta lo stream
- La PSRAM del modulo AI-Thinker è collegata via SPI — throughput limitato rispetto a PSRAM su chip (es. ESP32-S3)
- Non espandibile con sensori aggiuntivi (tutti i GPIO occupati da OV2640 e SD)
