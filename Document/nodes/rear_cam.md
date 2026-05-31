# DomoC — Nodo REAR_CAM (Telecamera retromarcia)

---

## Descrizione

Il nodo `REAR_CAM` (ID: `0x0008`) è la telecamera di retromarcia del camper. Fornisce uno stream video MJPEG via HTTP direttamente all'HMI o a qualsiasi client sulla rete locale. Il video **non transita attraverso la mesh ESP-Mesh** — il canale dati video è completamente separato.

Funzioni principali:
- Stream video MJPEG in tempo reale via HTTP (porta 80, path `/stream`)
- Snapshot JPEG su richiesta HTTP (path `/capture`)
- Presenza sulla mesh ESP-Mesh per segnalazione stato e ricezione comandi (accensione/spegnimento)
- Attivazione automatica su `MSG_KEY_ON` (retromarcia — opzionale, configurabile)
- Bassa latenza: stream diretto HTTP senza intermediari mesh

**Architettura video**: l'HMI si connette direttamente all'IP del nodo REAR_CAM per ricevere il video. La mesh è usata solo per i messaggi di controllo e stato (max 200 byte/msg).

---

## Hardware

### Modulo

- **ESP32-CAM** (AI-Thinker o compatibile)
  - ESP32-S (Xtensa LX6 dual-core 240 MHz)
  - 4MB Flash, PSRAM 4MB (per frame buffer)
  - OV2640 camera integrata (max 2MP, 1600×1200)
  - Antenna Wi-Fi integrata (PCB trace)
  - Nessun GPIO extra fisicamente accessibile (tutti occupati dalla camera e SD)

### Sensore camera

| Parametro | Valore |
|---|---|
| Sensore | OV2640 |
| Risoluzione max | UXGA 1600×1200 (retromarcia: VGA 640×480 sufficiente) |
| Formato stream | MJPEG |
| FPS retromarcia | 15–25 fps @ VGA (sufficiente per manovra) |
| Latenza | 100–200ms (HTTP over Wi-Fi) |
| Obiettivo | Grand angolo 120–160° (campo visivo retromarcia camper) |

### Alimentazione

```
[Bus 12V camper] ──→ [Buck 12V→5V, 500mA] ──→ ESP32-CAM (pin 5V)
```

- L'ESP32-CAM richiede 5V (non 3.3V direttamente)
- Consumo picco durante stream: 300–500 mA @ 5V
- Opzionale: relay di alimentazione comandato da nodo esterno per accendere/spegnere la camera

### GPIO ESP32-CAM

| GPIO | Funzione | Note |
|---|---|---|
| GPIO0 | BOOT mode | LOW per flash; HIGH per normale avvio |
| GPIO4 | Flash LED | Illuminatore integrato — usare con cautela (consumo elevato) |
| GPIO33 | LED rosso | Status LED integrato sulla scheda |
| UART0 (TX/RX) | Debug / flash | Attraverso pin header |

> I GPIO camera (D0–D7, PCLK, VSYNC, HREF, SIOD, SIOC, PWDN, RESET) sono occupati internamente — non modificare.

---

## Architettura software

### Separazione dati video / messaggi mesh

```
ESP32-CAM
├── Task Wi-Fi / Mesh     → ESP-Mesh sul canale mesh (es. ch.6)
│   ├── MSG_REGISTER      → ROOT: registrazione boot
│   ├── MSG_HEARTBEAT     → ROOT: ogni 5s, include URL stream
│   ├── MSG_COMMAND       ← HMI: accendi/spegni stream
│   └── MSG_STATUS        → ROOT/HMI: stato camera, FPS, errori
│
└── Task HTTP Server      → Wi-Fi STA (stesso AP mesh)
    ├── GET /stream       → client: stream MJPEG continuo
    ├── GET /capture      → client: snapshot JPEG singolo
    └── GET /status       → client: JSON stato camera
```

L'HMI riceve l'URL dello stream dal payload di stato mesh (`MSG_STATUS`) e poi si connette direttamente via HTTP per il video.

### Inizializzazione camera

```c
esp_err_t camera_init(void) {
    camera_config_t config = {
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer   = LEDC_TIMER_0,
        .pin_d0 = Y2_GPIO_NUM,   // OV2640 data bus
        .pin_d1 = Y3_GPIO_NUM,
        // ... (tutti i pin OV2640 standard ESP32-CAM)
        .xclk_freq_hz = 20000000,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_VGA,   // 640×480 per retromarcia
        .jpeg_quality = 12,              // 0–63, minore = migliore qualità
        .fb_count     = 2,               // doppio buffer in PSRAM
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    };
    return esp_camera_init(&config);
}
```

### HTTP stream handler

```c
esp_err_t stream_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    camera_fb_t *fb = NULL;
    while (stream_active) {
        fb = esp_camera_fb_get();
        if (!fb) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

        // Invia header parte MJPEG
        char part_buf[64];
        int hlen = snprintf(part_buf, sizeof(part_buf),
                            "--frame\r\nContent-Type: image/jpeg\r\n"
                            "Content-Length: %zu\r\n\r\n", fb->len);
        httpd_resp_send_chunk(req, part_buf, hlen);
        // Invia dati JPEG
        httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        httpd_resp_send_chunk(req, "\r\n", 2);

        esp_camera_fb_return(fb);
    }
    return ESP_OK;
}
```

---

## Payload di stato mesh

```c
typedef struct __attribute__((packed)) {
    uint8_t  stream_active;   // offset 0 — 1 = stream HTTP attivo
    uint8_t  fps_current;     // offset 1 — FPS corrente (0–30)
    uint8_t  error_code;      // offset 2 — 0=ok, 1=camera init fail, 2=Wi-Fi fail
    uint8_t  _pad;            // offset 3 — allineamento
    uint32_t frame_count;     // offset 4-7 — frame totali dall'accensione
    char     stream_url[32];  // offset 8-39 — URL stream es. "http://192.168.4.2/stream"
} rear_cam_status_t;          // 40 byte
```

L'HMI legge `stream_url` dal payload e apre il player video direttamente all'indirizzo indicato.

---

## Descriptor HMI

```c
static const node_descriptor_t REAR_CAM_DESCRIPTOR = {
    .node_icon      = ICON_CAMERA_REAR,
    .action_count   = 2,
    .property_count = 2,
    .actions = {
        // action_code        icon_id          ctrl_type     group_id  linked_property   flags  label
        { ACTION_CAM_ON,     ICON_ACT_CAM_ON, CTRL_TOGGLE,  0,        PROP_STREAM_ACTIVE, 0,   "AVVIA"   },
        { ACTION_CAM_OFF,    ICON_ACT_CAM_OFF,CTRL_TOGGLE,  0,        PROP_STREAM_ACTIVE, 0,   "FERMA"   },
    },
    .properties = {
        // property_id         offset  type            widget_type        range_min  range_max  unit   fmt
        { PROP_STREAM_ACTIVE,  0,      PAYLOAD_UINT8,  WIDGET_INDICATOR,  0,         0,         "",    "%s"   },
        { PROP_FPS_CURRENT,    1,      PAYLOAD_UINT8,  WIDGET_LABEL,      0,         30,        "fps", "%d"   },
    },
};
```

---

## Attivazione automatica (opzionale)

Il nodo può essere configurato per attivarsi automaticamente quando il ROOT trasmette `MSG_KEY_ON` (motore acceso → retromarcia in uso):

```c
// In node_config.json, sezione "behavior"
{
  "behavior": {
    "auto_stream_on_key_on": true,   // accendi stream automaticamente su KEY_ON
    "auto_stream_off_key_off": true  // spegni stream su KEY_OFF
  }
}
```

Questa funzione è opzionale: la camera retromarcia è utile anche quando il motore è spento (manovra manuale, parcheggio).

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, dispatch |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, stato, ACK |
| `http_server_task` | 4 | 8 KB | Server HTTP (stream MJPEG, snapshot, status JSON) |
| `camera_task` | 6 | 4 KB | Acquisizione frame da OV2640, gestione buffer |
| `ota_receiver_task` | 2 | 6 KB | Ricezione OTA |

> `camera_task` ha priorità alta per garantire frame rate costante durante lo stream.

---

## Gestione energetica

- **Stream spento**: camera in standby, Wi-Fi mesh attivo, consumo ~30–50 mA @ 5V
- **Stream attivo**: consumo 250–500 mA @ 5V (dipende da risoluzione e FPS)
- **Flash LED**: non usare il flash LED integrato durante la guida — abbagliante per i veicoli dietro
- Opzionale: relay di alimentazione esterno per spegnere completamente il modulo quando non serve

---

## Considerazioni pratiche

- **Posizionamento**: targa posteriore o paraurti — custodia IP65, cavo coassiale o cavo alimentazione schermato
- **Obiettivo**: grand angolo 120°+ per visione completa dell'area posteriore del camper
- **Latenza HTTP**: 100–200ms su Wi-Fi locale è accettabile per la manovra; non idoneo per guida ad alta velocità
- **Risoluzione stream**: VGA (640×480) bilanciata per qualità/latenza; SVGA (800×600) se si vuole più dettaglio
- **Indirizzo IP fisso**: configurare IP statico o DHCP reservation per un URL stream prevedibile
- **Protezione GPIO0**: non tenere GPIO0 a GND durante l'avvio normale — il modulo entrerebbe in modalità flash

## Limitazioni note

- ESP32-CAM non ha GPIO extra per interfacce fisiche aggiuntive — se si vuole un LED di stato visibile separato, usare una scheda esterna
- La qualità Wi-Fi dell'antenna PCB integrata è inferiore ai moduli con antenna esterna — valutare antenna esterna se la distanza dall'HMI è > 5m
- Lo stream MJPEG richiede che l'HMI e REAR_CAM siano sulla stessa rete Wi-Fi (stesso AP mesh o bridge)
