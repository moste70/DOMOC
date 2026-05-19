# DomoC — Nodo HMI (Knob Controller)

---

## Descrizione

Il nodo HMI è il **controller portatile a manopola** del sistema DomoC. È un nodo mesh come tutti gli altri: si connette e disconnette liberamente senza impatto sulla rete o sui nodi funzione.

Caratteristiche chiave:

- **Compatto e portatile**: scocca CNC in metallo (66×22mm), batteria LiPo 800mAh integrata, carica via USB-C
- **Non è il root della mesh**: la mesh funziona correttamente anche con HMI spento o fuori range
- **Solo monitor e controllo manuale**: non esegue logica automatica — quella vive nei nodi
- **Navigazione a icone**: rotazione encoder scorre un carosello di icone (una per nodo/funzione), push su encoder o tocco display entra nell'elemento selezionato — nessun menu testuale
- **Multi-istanza**: possono coesistere più nodi HMI sulla stessa mesh

> **Principio**: l'HMI vede tutto ciò che accade sulla mesh, può dare comandi manuali, ma non è mai nel percorso critico di nessuna decisione automatica.

---

## Hardware

### Modulo base

### Waveshare ESP32-S3-Knob-Touch-LCD-1.8

Modulo tutto-in-uno con doppio MCU, display rotondo, encoder doppio, audio, vibrazione e batteria integrati.

### Architettura dual-MCU

Il modulo integra due microcontrollori che comunicano via UART interno:

| MCU | Ruolo nel progetto |
| --- | --- |
| **ESP32-S3R8** (LX7 dual-core 240MHz, 8MB PSRAM) | MCU principale: ESP-Mesh, display QSPI, touch I2C, audio I2S, LVGL |
| **ESP32-U4WDH** (LX6 dual-core 240MHz, 4MB Flash) | MCU secondario: gestione encoder dedicato, relay eventi a ESP32-S3 via UART |

La mesh ESP-Mesh gira esclusivamente sull'**ESP32-S3** (Wi-Fi integrato). L'ESP32 secondario è trasparente alla rete — trasmette solo eventi encoder.

### Display

| Parametro | Valore |
| --- | --- |
| **Dimensione** | 1.8" IPS rotondo |
| **Risoluzione** | 360×360 pixel, 262K colori |
| **Driver** | ST77916 |
| **Interfaccia** | QSPI (Quad SPI) — 4 bit dati in parallelo |
| **Backlight** | GPIO47, PWM dimmerabile |

> L'interfaccia QSPI garantisce bandwidth sufficiente per animazioni LVGL fluide a 360×360. Il driver ST77916 è supportato nell'`esp-bsp` Waveshare (componente `esp_lcd_st77916`).

### Touch

- **Controller**: CST816, I2C, indirizzo `0x15`
- **Reset timing**: HIGH 10ms → LOW 10ms → HIGH 50ms (sequenza specifica per inizializzazione affidabile)
- Usato per gesture swipe di riserva; la navigazione principale è via encoder

### Input fisici

| Gesto | Effetto |
| --- | --- |
| **Rotazione encoder primario** (ESP32-S3) | Scorre il carosello di icone — sinistra/destra |
| **Push encoder primario** | Entra nell'icona selezionata (nodo o azione) |
| **Push lungo encoder** (>800ms) | Torna al livello superiore (azioni → nodi, nodi → home) |
| **Tocco display** | Equivalente a push encoder — seleziona/entra |
| **Encoder secondario** (ESP32-U4WDH → UART) | Regolazione backlight; in futuro configurabile |
| **Power button** | Accensione/spegnimento con salvataggio stato su NVS |

### Audio e feedback

- **DAC audio**: PCM5100A (I2S), jack 3.5mm — toni di conferma differenziati per severità alert
- **Microfono MEMS**: digitale, per visualizzazione spettro (funzione secondaria)
- **Motore vibrazione**: DRV2605 via I2C — feedback aptico su conferma comando e alert critico

### Alimentazione

```
[USB-C 5V] ──→ [Charger LiPo integrato] ──→ [Batteria LiPo 800mAh PH1.25]
                                                         │
                                              [Regolatore 3.3V integrato]
                                                         │
                                           ESP32-S3 + Display + Periferiche
```

- **Batteria**: 800mAh — autonomia stimata 4–8h display attivo, 12–20h in standby con backlight spento
- **Carica**: solo via USB-C 5V (modulo già integrato nel board)
- **Alimentazione da camper**: collegare USB-C a un buck 12V→5V sul bus 12V del camper per ricaricare automaticamente quando il veicolo è alimentato
- **Protezione sottotensione**: gestita dal charger integrato del modulo

### Memoria e storage

- **Flash**: 16MB SPI (su ESP32-U4WDH, condivisa)
- **PSRAM**: 8MB (integrata in ESP32-S3R8) — necessaria per i frame buffer LVGL a 360×360
- **MicroSD**: slot integrato — log eventi persistente

### GPIO principali (ESP32-S3)

| GPIO | Funzione |
| --- | --- |
| GPIO13 | Display QSPI CLK |
| GPIO14 | Display QSPI CS |
| GPIO15 | Display QSPI D0 |
| GPIO16 | Display QSPI D1 |
| GPIO17 | Display QSPI D2 |
| GPIO18 | Display QSPI D3 |
| GPIO21 | Display RST |
| GPIO47 | Display backlight PWM |
| SDA/SCL* | Touch CST816 I2C (addr 0x15) |
| I2S* | Audio PCM5100A |
| I2C* | DRV2605 vibrazione |
| UART* | Comunicazione con ESP32 secondario |

> I GPIO contrassegnati con * sono da verificare sullo schematico ufficiale Waveshare. I GPIO del display sono confermati dalla community Tasmota.

---

## Integrazione software — Driver display

Il driver ST77916 via QSPI non è incluso nell'ESP-IDF standard. Usare il componente Waveshare:

```cmake
# idf_component.yml
dependencies:
  espressif/esp_lcd_st77916: "^1.0.0"
```

Inizializzazione LVGL con QSPI:

```c
esp_lcd_panel_io_handle_t io_handle;
esp_lcd_panel_handle_t panel_handle;

esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num      = -1,          // QSPI non usa DC pin
    .cs_gpio_num      = GPIO_LCD_CS,
    .pclk_hz          = 80 * 1000 * 1000,
    .lcd_cmd_bits     = 32,
    .lcd_param_bits   = 8,
    .spi_mode         = 0,
    .trans_queue_depth = 10,
    .flags.quad_mode  = true,       // QSPI abilitato
};

// Reset con timing specifico CST816
gpio_set_level(GPIO_TOUCH_RST, 1); vTaskDelay(pdMS_TO_TICKS(10));
gpio_set_level(GPIO_TOUCH_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
gpio_set_level(GPIO_TOUCH_RST, 1); vTaskDelay(pdMS_TO_TICKS(50));
```

---

## Funzioni software

### 1. Connessione alla mesh

```c
esp_mesh_cfg_t cfg = {
    .channel              = MESH_CHANNEL,
    .mesh_id              = MESH_ID,
    .mesh_ap.max_connection = 0,  // HMI non accetta figli
    .mesh_type            = MESH_NODE,
};
esp_mesh_fix_root(false);  // mai root
```

Alla connessione invia `MSG_REGISTER` al ROOT con `node_type = HMI`, poi richiede dump completo registry (`MSG_STATUS_REQ` broadcast).

### 2. Sincronizzazione node registry e descriptor

Alla connessione, l'HMI richiede il dump completo al ROOT. Il ROOT risponde con tutti i `node_info_t` e un `MSG_DESCRIPTOR` per ogni nodo — l'HMI costruisce il carosello automaticamente senza conoscere i tipi di nodo in anticipo.

```c
// Ricezione registry dump + descriptor dal ROOT
void on_registry_dump(registry_dump_t *dump) {
    for (int i = 0; i < dump->count; i++) {
        update_local_node(dump->nodes[i]);
        // I descriptor arrivano come MSG_DESCRIPTOR separati subito dopo
    }
}

// Ricezione descriptor per un nodo (sia al dump iniziale che per nuovi nodi live)
void on_descriptor_received(mesh_msg_t *msg) {
    node_info_t *node = local_registry_find(msg->node_id);
    if (!node) return;
    memcpy(&node->descriptor, msg->payload, sizeof(node_descriptor_t));
    node->descriptor_valid = true;
    ui_carousel_add_or_update(node);  // aggiunge/aggiorna icona nel carosello
}

void on_node_status_update(node_status_t *update) {
    update_local_node_by_id(update->node_id, update->status, update->payload);
    ui_carousel_refresh_icon(update->node_id);   // aggiorna colore stato icona
    if (ui_get_selected_node() == update->node_id)
        ui_detail_refresh(update->node_id);       // aggiorna schermata dettaglio se visibile
}
```

### 3. Costruzione dinamica del carosello

Il carosello è interamente derivato dai descriptor ricevuti — nessun nodo è hardcodato nell'HMI:

```c
void ui_carousel_add_or_update(node_info_t *node) {
    if (!node->descriptor_valid) {
        // Nodo senza descriptor: icona generica, nessuna azione
        carousel_set_node(node->node_id, ICON_GENERIC, NULL, 0);
        return;
    }
    carousel_set_node(
        node->node_id,
        node->descriptor.node_icon,
        node->descriptor.actions,
        node->descriptor.action_count
    );
    // Ricostruisce Livello 1 (azioni) per questo nodo
    carousel_set_actions(node->node_id,
        node->descriptor.actions,
        node->descriptor.action_count);
}

// Lettura proprietà dal payload di stato usando il descriptor
void ui_detail_refresh(uint16_t node_id) {
    node_info_t *node = local_registry_find(node_id);
    if (!node || !node->descriptor_valid) return;

    for (int p = 0; p < node->descriptor.property_count; p++) {
        property_descriptor_t *pd = &node->descriptor.properties[p];
        char buf[16];
        // Estrae il valore grezzo dal payload_cache usando offset e tipo
        float val = extract_payload_value(
            node->payload_cache, pd->payload_offset, pd->payload_type);
        snprintf(buf, sizeof(buf), pd->fmt, val);
        ui_detail_set_row(p, buf, pd->unit);
    }
}
```

### 4. Discovery state machine

La procedura di discovery è la sequenza formale che l'HMI esegue all'accensione per costruire l'intera UI da zero senza conoscere i tipi di nodo in anticipo.

```text
[BOOT] ──▶ [MESH_CONNECTING] ──▶ [REGISTERING] ──▶ [REQUESTING_DUMP]
                                                             │
                                                             ▼
                                                  [RECEIVING_REGISTRY]
                                                             │
                                                  ricevuti tutti i node_info_t
                                                             │
                                                             ▼
                                                  [RECEIVING_DESCRIPTORS]
                                                             │
                                                  descriptor_valid==true
                                                  per tutti i nodi (o timeout 3s)
                                                             │
                                                             ▼
                                                  [BUILDING_UI]
                                                             │
                                                  build_property_widget()
                                                  build_action_controls()
                                                  per ogni nodo
                                                             │
                                                             ▼
                                                        [ONLINE]
                                                             │
                                            nuovo MSG_DESCRIPTOR ─────▶ torna a BUILDING_UI
                                            (nodo aggiunto live)         per quel nodo
```

| Stato | Azione |
| --- | --- |
| `MESH_CONNECTING` | ESP-Mesh init, display mostra spinner "Connessione..." |
| `REGISTERING` | Invia `MSG_REGISTER` al ROOT (`node_type=HMI`), attende `MSG_REGISTER_ACK` |
| `REQUESTING_DUMP` | Invia `MSG_STATUS_REQ` broadcast |
| `RECEIVING_REGISTRY` | Riceve `MSG_REGISTRY_DUMP` con array `node_info_t[]`; conta nodi attesi |
| `RECEIVING_DESCRIPTORS` | Per ogni nodo atteso, aspetta `MSG_DESCRIPTOR`; timeout 3s per nodo → `descriptor_valid=false` (icona generica) |
| `BUILDING_UI` | Itera registry; per ogni nodo chiama `build_node_ui()`; al termine mostra carosello |
| `ONLINE` | Operazione normale; nuovi `MSG_DESCRIPTOR` aggiornano il carosello in tempo reale senza reboot |

### 5. Costruzione widget proprietà — `build_property_widget()`

```c
lv_obj_t* build_property_widget(property_descriptor_t *pd, lv_obj_t *parent) {
    switch (pd->widget_type) {

    case WIDGET_GAUGE:
        // lv_arc con range [range_min/10 .. range_max/10]
        lv_obj_t *arc = lv_arc_create(parent);
        lv_arc_set_range(arc, pd->range_min, pd->range_max);  // valori ×10
        lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
        return arc;

    case WIDGET_THERMOMETER:
        // arco + label valore sovrapposta — widget composito
        return create_thermometer_widget(parent,
            pd->range_min / 10.0f, pd->range_max / 10.0f);

    case WIDGET_PROGRESS:
        lv_obj_t *bar = lv_bar_create(parent);
        lv_bar_set_range(bar, pd->range_min, pd->range_max);
        return bar;

    case WIDGET_BATTERY:
        return create_battery_widget(parent,
            pd->range_min / 10.0f, pd->range_max / 10.0f);

    case WIDGET_INDICATOR:
        // pallino colorato: verde se val≠0, grigio se val=0
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_set_size(dot, 20, 20);
        lv_obj_add_style(dot, &style_indicator_off, 0);
        return dot;

    case WIDGET_VALUE_UNIT:
        return lv_label_create(parent);

    case WIDGET_LABEL:
    default:
        return lv_label_create(parent);
    }
}

// Aggiornamento widget quando arriva nuovo payload
void update_property_widget(lv_obj_t *widget, property_descriptor_t *pd,
                            uint8_t *payload_cache) {
    float val = extract_payload_value(payload_cache,
                                      pd->payload_offset, pd->payload_type);
    char buf[16];
    switch (pd->widget_type) {
    case WIDGET_GAUGE:
    case WIDGET_THERMOMETER:
        lv_arc_set_value(widget, (int)(val * 10));
        snprintf(buf, sizeof(buf), pd->fmt, val);
        lv_label_set_text(lv_obj_get_child(widget, 0), buf);
        break;
    case WIDGET_PROGRESS:
    case WIDGET_BATTERY:
        lv_bar_set_value(widget, (int)(val * 10), LV_ANIM_ON);
        break;
    case WIDGET_INDICATOR:
        lv_obj_set_style_bg_color(widget,
            val != 0.0f ? lv_color_hex(0x00C853) : lv_color_hex(0x616161), 0);
        break;
    default:
        snprintf(buf, sizeof(buf), pd->fmt, val);
        lv_label_set_text(widget, buf);
        break;
    }
}
```

### 6. Costruzione controlli azione — `build_action_controls()`

```c
void build_action_controls(node_info_t *node, lv_obj_t *parent) {
    bool processed[NODE_DESC_MAX_ACTIONS] = {false};

    for (int i = 0; i < node->descriptor.action_count; i++) {
        if (processed[i]) continue;
        action_descriptor_t *a = &node->descriptor.actions[i];

        if (a->group_id != 0) {
            // Cerca il partner con lo stesso group_id
            int partner = -1;
            for (int j = i + 1; j < node->descriptor.action_count; j++) {
                if (node->descriptor.actions[j].group_id == a->group_id) {
                    partner = j;
                    break;
                }
            }
            if (partner >= 0) {
                processed[partner] = true;
                if (a->ctrl_type == CTRL_STEPPER) {
                    // TEMP+ / TEMP- → widget stepper con valore centrale = linked_property
                    create_stepper_control(parent, a,
                        &node->descriptor.actions[partner],
                        node->node_id, a->linked_property);
                } else if (a->ctrl_type == CTRL_TOGGLE) {
                    // LUCE ON / LUCE OFF → toggle; stato visivo da linked_property
                    create_toggle_control(parent, a,
                        &node->descriptor.actions[partner],
                        node->node_id, a->linked_property);
                }
                processed[i] = true;
                continue;
            }
        }

        // Azione non raggruppata
        switch (a->ctrl_type) {
        case CTRL_BUTTON:
            create_button_control(parent, a, node->node_id);
            break;
        case CTRL_CONFIRM:
            create_confirm_button_control(parent, a, node->node_id);
            break;
        default:
            create_button_control(parent, a, node->node_id);
        }
        processed[i] = true;
    }
}
```

**Regole di grouping**:

- **`CTRL_STEPPER`**: due azioni con stesso `group_id`; l'azione con `action_code` minore diventa "−", quella maggiore "+"; il valore centrale mostra il `linked_property` corrente dal payload
- **`CTRL_TOGGLE`**: due azioni con stesso `group_id`; prima (ordine descriptor) = ON, seconda = OFF; stato visivo (premuto/rilasciato) determinato da `linked_property`

### 7. Mappa widget e controlli

| Tipo proprietà | Widget LVGL | Comportamento |
| --- | --- | --- |
| stato enum | `lv_label` | testo da `state_labels[]` |
| temperatura | arco + label | range colorato, aggiornamento live |
| umidità | `lv_bar` | 0–100% con colore dinamico |
| tensione batteria | icona batteria | segmenti, rosso sotto soglia |
| setpoint | `lv_arc` | posizione manopola |
| booleano (luce, cam, valvola) | pallino colorato | verde=ON, grigio=OFF |

| Tipo azione | Controllo UI | Note |
| --- | --- | --- |
| singola (`group_id=0`) | `lv_btn` | tap → MSG_COMMAND diretto |
| toggle (`group_id>0`, `CTRL_TOGGLE`) | `lv_btn` con stato | stato riflesso da `linked_property` |
| stepper (`group_id>0`, `CTRL_STEPPER`) | coppia `lv_btn` +/− con label centrale | valore centrale = `linked_property` |
| critica (`CTRL_CONFIRM`) | `lv_btn` + dialog | due tap per confermare |

---

### 8. Ricezione alert

```c
void on_mesh_alert(mesh_msg_t *msg) {
    alert_payload_t *alert = (alert_payload_t*)msg->payload;

    update_local_node_status(msg->node_id, alert->new_state);
    ui_push_alert_overlay(alert->severity, alert->message);

    if (alert->severity == ALERT_CRITICAL) {
        backlight_set(100);
        drv2605_play_effect(DRV2605_EFFECT_STRONG_BUZZ);
        pcm5100a_play_tone(TONE_ALERT_CRITICAL);
    } else {
        drv2605_play_effect(DRV2605_EFFECT_SOFT_BUMP);
        pcm5100a_play_tone(TONE_ALERT_WARNING);
    }

    sd_log_event(msg->node_id, alert->message, get_timestamp());
}
```

> L'HMI **non reagisce** agli alert con comandi automatici. Visualizza, vibra, suona, logga. Punto.

### 9. Invio comandi manuali

Solo da conferma esplicita (click encoder sul pulsante azione):

```c
void on_user_command(uint16_t target_node_id, uint8_t action) {
    mesh_msg_t msg = {
        .version   = PROTOCOL_VERSION,
        .msg_type  = MSG_COMMAND,
        .node_id   = NODE_ID_HMI,
        .target_id = target_node_id,
        .seq_num   = next_seq_num(),
    };
    cmd_payload_t cmd = { .action = action };
    memcpy(msg.payload, &cmd, sizeof(cmd));
    msg.payload_len = sizeof(cmd);
    msg.crc16 = crc16_calc(&msg);

    drv2605_play_effect(DRV2605_EFFECT_CLICK);  // feedback aptico immediato
    enqueue_mesh_tx(&msg);
}
```

### 10. Comunicazione con ESP32 secondario

L'ESP32-U4WDH invia eventi encoder via UART all'ESP32-S3:

```c
typedef struct {
    uint8_t  encoder_id;   // 0 = primario, 1 = secondario
    int8_t   delta;        // +1 orario, -1 antiorario, 0 = click
} encoder_event_t;

void uart_rx_task(void *pvParam) {
    encoder_event_t evt;
    while (1) {
        if (uart_read_bytes(UART_NUM_1, &evt, sizeof(evt), portMAX_DELAY) == sizeof(evt)) {
            xQueueSend(encoder_event_queue, &evt, 0);
        }
    }
}
```

### 11. Gestione batteria

```c
void power_monitor_task(void *pvParam) {
    while (1) {
        float vbat     = adc_read_vbat();
        uint8_t batt_pct = vbat_to_percent(vbat);

        ui_update_battery_indicator(batt_pct);

        if (batt_pct < 15) {
            ui_push_alert_overlay(ALERT_WARNING, "Batteria HMI bassa — collegare USB-C");
            drv2605_play_effect(DRV2605_EFFECT_SOFT_BUZZ);
        }
        if (vbat < 3200) {
            ui_show_shutdown_screen();
            vTaskDelay(pdMS_TO_TICKS(3000));
            power_off();  // salva stato su NVS, spegne
        }

        vTaskDelay(pdMS_TO_TICKS(30000));  // check ogni 30s
    }
}
```

---

## Interfaccia utente — Carosello di icone (360×360 rotondo)

L'intera navigazione si basa su **due livelli di carosello**. Non esistono menu testuali.

```text
  LIVELLO 0 — Carosello nodi          LIVELLO 1 — Carosello azioni
  (rotazione encoder)                 (push → entra, rotazione → scorre)

  ┌──────────────────┐                ┌──────────────────┐
 / 🔋78%       🔗5n  \              /        STEP        \
│                    │              │                     │
│  [🚰]  [🚪]  [🌡] │   ──push──▶ │  [🔓]  [🔒]  [ℹ]  │
│        ↑↑↑         │              │         ↑↑↑          │
│       STEP         │              │        APRI          │
│      CHIUSO        │              │                     │
 \                  /                \  push=ESEGUI      /
  └──────────────────┘                └──────────────────┘
                                             │
                                        push lungo
                                             │
                                          ◀ INDIETRO
```

**Rendering carosello**: l'icona centrale è a piena dimensione (~110×110px), le icone adiacenti sono visibili parzialmente ai bordi e dimmate al 40% — sfruttano la forma circolare del display. Lo stato del nodo colora l'icona: verde = ok, arancio = warning, rosso = errore/offline.

### Livello 0 — Carosello nodi

```text
           ╭───────────────────╮
          /  🔋78%       🔗5n  \
         │                      │
         │  ░[🚰]░  [🚪]  ░[🌡]░│
         │          ↑            │
         │        STEP           │
         │       CHIUSO          │
         │       14:30           │
          \                     /
           ╰───────────────────╯
```

- **Rotazione encoder**: scorre le icone (una per nodo registrato + icone sistema in fondo)
- **Icona centrale**: nodo selezionato — nome e stato mostrati sotto l'icona
- **Push encoder / tocco display**: entra nel Livello 1 (carosello azioni) del nodo
- **Header** (arco superiore): batteria HMI a sinistra, numero nodi mesh a destra
- Se lo stato di un nodo cambia mentre è visibile, l'icona aggiorna colore senza ridisegnare tutto

### Livello 1 — Carosello azioni nodo

```text
           ╭───────────────────╮
          /        STEP         \
         │                      │
         │  ░[🔓]░  [🔒]  ░[ℹ]░ │
         │          ↑            │
         │        CHIUDI         │
         │                      │
          \  push=ESEGUI        /
           ╰───────────────────╯
```

- **Rotazione encoder**: scorre le azioni disponibili per il nodo
- **Push encoder / tocco display**: esegue l'azione (con feedback aptico DRV2605 + conferma visiva 1s)
- **Push lungo encoder** (>800ms): torna al Livello 0 senza eseguire nulla
- Le azioni variano per tipo nodo:

| Tipo nodo | Azioni disponibili |
| --- | --- |
| Attuatore (STEP, valvole, porta) | 🔓 APRI — 🔒 CHIUDI — ℹ INFO |
| Sensore (temperatura, livello) | ℹ INFO — 📊 STORICO |
| Sistema (ROOT, mesh) | 📡 TOPOLOGIA — 📋 LOG |

### Icone sistema nel carosello (in coda ai nodi)

Due icone speciali sempre presenti in fondo al carosello principale:

| Icona | Funzione |
| --- | --- |
| **📡 MESH** | Mostra topologia: albero ROOT→nodi, RSSI, hop count |
| **📋 LOG** | Scorre gli ultimi 100 eventi (rotazione encoder = scroll) |

### Alert overlay — priorità massima

```text
           ╭───────────────────╮
          /      ⚠  (1/2)       \
         │                      │
         │        [🚨]           │
         │    GREY_WATER         │
         │  Valvola bloccata     │
         │     14:35:22          │
          \   push = OK         /
           ╰───────────────────╯
```

- Si sovrappone a qualsiasi livello — congela il carosello sottostante
- Backlight 100%, vibrazione DRV2605, tono audio differenziato per severità
- Alert multipli: indicatore `(1/2)`, push scorre al successivo, push sull'ultimo chiude
- Se arriva un alert mentre si sta eseguendo un comando: l'overlay appare dopo la conferma del comando

### Conferma comando (overlay temporaneo 1.5s)

```text
           ╭───────────────────╮
          /                     \
         │        [✓]            │
         │      Inviato          │
         │       APRI            │
         │       STEP            │
          \                     /
           ╰───────────────────╯
```

Appare dopo ogni push su un'azione, poi svanisce automaticamente riportando al Livello 1.

---

## Task FreeRTOS (ESP32-S3)

| Task | Core | Priorità | Stack | Funzione |
| --- | --- | --- | --- | --- |
| `mesh_rx_task` | 0 | 5 | 4 KB | Ricezione messaggi mesh, dispatch a coda interna |
| `mesh_tx_task` | 0 | 5 | 4 KB | Invio comandi manuali, ACK, retry |
| `registry_sync_task` | 0 | 4 | 3 KB | Sync registry da ROOT, aggiornamenti incrementali |
| `alert_task` | 0 | 4 | 3 KB | MSG_ALERT → overlay UI → vibrazione → audio → log SD |
| `uart_rx_task` | 0 | 4 | 2 KB | Ricezione eventi encoder da ESP32 secondario |
| `encoder_task` | 1 | 4 | 2 KB | Lettura encoder primario + dispatch coda UI |
| `display_task` | 1 | 3 | 16 KB | Rendering LVGL, touch CST816, coda UI thread-safe |
| `power_monitor_task` | 0 | 3 | 2 KB | ADC batteria, banner e spegnimento |
| `sd_logger_task` | 0 | 1 | 3 KB | Scrittura asincrona log su SD (ring buffer interno) |

> `display_task` e `encoder_task` su **Core 1** — non interferiscono con la mesh (Core 0).
> Stack `display_task` a 16 KB: LVGL con double-buffer QSPI a 360×360 richiede più stack del normale.

---

## Comportamento in assenza di connessione mesh

Se l'HMI perde la mesh (fuori range):

1. Header mostra icona mesh barrata
2. I dati visualizzati rimangono congelati (con timestamp "Xs fa")
3. I comandi manuali vengono rifiutati con overlay "Connessione mesh assente"
4. Backlight si riduce al 20% dopo 60s per risparmiare batteria
5. Alla riconnessione: `registry_sync_task` richiede dump completo e aggiorna tutta la UI

> L'assenza dell'HMI non ha nessun effetto sulla mesh, sul ROOT o sui nodi funzione.

---

## Considerazioni hardware pratiche

- **Driver QSPI ST77916**: usare il componente `esp_lcd_st77916` dall'`esp-bsp` Waveshare. Non usare driver SPI standard — troppo lento per animazioni LVGL fluide
- **Reset CST816**: il timing HIGH→LOW→HIGH con 10/10/50ms è obbligatorio; senza, il touch non risponde
- **Double buffer LVGL**: con 8MB PSRAM su ESP32-S3R8 si possono allocare due frame buffer completi (360×360×2 byte = ~259KB cadauno) per scroll senza flickering
- **Alimentazione camper**: collegare USB-C a un buck 12V→5V (es. MP2307DN 1A) sul bus 12V del camper — la board si ricarica automaticamente quando il camper è alimentato
- **Vibrazione DRV2605**: pre-programmare gli effetti haptic all'avvio via I2C; la libreria Waveshare include wrapper pronti
- **Scocca CNC metallica**: ottima per vibrazioni in marcia — nessuna necessità di fissaggi aggiuntivi rispetto al display
- **Log SD**: lo slot MicroSD è integrato nel modulo; separato dal bus display QSPI — nessun conflitto
