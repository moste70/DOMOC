# DomoC — Nodo HMI (Display portatile)

---

## Descrizione

Il nodo HMI è il **pannello di controllo portatile** del sistema DomoC. È un nodo mesh come tutti gli altri: si connette e disconnette liberamente senza impatto sulla rete o sui nodi funzione.

Caratteristiche chiave:
- **Portatile**: alimentato da batteria LiPo interna, con carica opzionale dal bus 12V del camper
- **Non è il root della mesh**: la mesh funziona correttamente anche con HMI spento o fuori range
- **Solo monitor e controllo manuale**: non esegue logica automatica — quella vive nei nodi
- **Multi-istanza**: possono coesistere più nodi HMI sulla stessa mesh (es. uno fisso nel camper, uno portatile)

> **Principio**: l'HMI vede tutto ciò che accade sulla mesh, può dare comandi manuali all'utente, ma non è mai nel percorso critico di nessuna decisione automatica.

---

## Hardware

### Microcontrollore

- **ESP32-S3** — scelto per:
  - CPU dual-core 240 MHz (UI su Core 1, mesh su Core 0)
  - PSRAM esterna 8MB (necessaria per LVGL con display ad alta risoluzione)
  - USB-C nativa (USB OTG FS) — flash/debug senza adattatori
  - Wi-Fi 802.11 b/g/n per ESP-Mesh

### Alimentazione — tripla sorgente

```
[Bus 12V camper] ──→ [Buck 12V→5V, 1A] ──→ ┐
                                             ├──→ [Caricabatteria LiPo] ──→ [Batteria LiPo 3.7V]
[USB-C 5V esterno] ──────────────────────────┘                                      │
                                                                                     ▼
                                                                          [Boost/LDO 3.7→3.3V]
                                                                                     │
                                                                                ESP32-S3 + Display
```

- **Batteria LiPo**: 2000–3000 mAh — autonomia stimata 6–12 ore con display attivo, 24–48 ore in standby
- **Carica da 12V camper**: quando il camper è alimentato, la batteria si ricarica automaticamente
- **Carica da USB-C**: tramite alimentatore esterno o PC
- **Chip di gestione batteria**: TP4056 (carica) + DW01 (protezione) oppure BQ24079 (soluzione integrata)
- **Indicatore batteria**: lettura ADC sul pin VBAT diviso per 2 — percentuale visualizzata in header display
- **Protezione sottotensione**: spegnimento automatico sotto 3.2V per preservare la batteria

#### Moduli di ricarica e protezione batteria

**Opzione 1: TP4056 + DW01** (soluzione separata, consigliata per flessibilità)
- **TP4056**: modulo di ricarica LiPo completo a 1A (~€2-3)
  - Ingresso: 5V USB-C o 12V da buck converter
  - Protezione da sovraccarica, scarica rapida sicura
  - LED stato ricarica
  
- **DW01-A**: protezione batteria (sottotensione + sovracorrente) (~€1-2)
  - Spegne automaticamente sotto 2.4V
  - Protegge da corto circuito

*Pro*: Moduli testate, economiche, standard nel DIY  
*Contro*: Due componenti separate da gestire

**Opzione 2: BQ24079** (soluzione integrata, moderna)
- IC unico con gestione completa:
  - Ricarica LiPo da 5V/12V
  - Protezione integrata
  - Massima semplicità PCB
  - I2C opzionale per monitor remoto

*Pro*: Un solo componente, minore footprint, migliore integrazione  
*Contro*: Leggermente più costoso (€5-8)

**Opzione 3: TP4056 + MB8365** (alternativa con separazione input)
- Se vuoi ricaricare da **entrambe le fonti** (12V camper + USB-C esterno) contemporaneamente:
  - **TP4056** con **MB8365** (diodo OR-ing)
  - Seleziona automaticamente la fonte di potenza migliore

*Pro*: Massima flessibilità di alimentazione  
*Contro*: Più componenti e complessità

**Configurazione consigliata per DOMOC HMI**:
```
Buck 12V→5V 1A + USB-C 5V
        │
        └──→ [TP4056] ──→ [DW01] ──→ [Batteria LiPo 3.7V]
                                           │
                                           └→ [Boost/LDO 3.7→3.3V] → ESP32-S3
```

**Alternativa all-in-one**: DFRobot BQ24075 Charging Module
- Include TP4056 + protezione DW01 + step-up per 3.3V
- PCB compatta con tutte le capacità necessarie (~€8-10)
- Riferimento circuito e discussione TI: https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/707357/bq24075-input-voltage-drops-by-500mv-when-battery-is-inserted

### Display

- **Dimensioni**: 3.5"–4.3" (480×320 o 800×480)
- **Interfaccia**: SPI su bus dedicato (ILI9488 o ST7796) — **bus SPI separato dalla SD card**
- **Touch**: capacitivo (FT5336 o GT911, I2C) — preferibile al resistivo per usabilità in ambiente camper
- **Backlight**: PWM dimmerabile via LEDC — si riduce al 30% dopo 30s, si spegne dopo 60s
- **Riattivazione**: qualsiasi tocco, rotazione encoder, o ricezione di un `MSG_ALERT` dalla mesh

### Input fisico

- **Encoder rotativo** con pulsante integrato (navigazione menu senza touch — utile con guanti o vibrazioni)
- **Pulsante fisico "CHIUDI TUTTO"**: comando rapido pre-partenza — invia ACTION_CLOSE broadcast a tutti gli attuatori con conferma display

### Connettività

| Interfaccia | Bus | Uso |
|---|---|---|
| Wi-Fi (ESP-Mesh) | — | Comunicazione con tutti i nodi e ROOT |
| Display SPI | SPI0 | Rendering UI |
| MicroSD | SPI1 (separato) | Log eventi persistente (opzionale) |
| Touch I2C | I2C0 | Input utente |
| USB-C | USB OTG | Flash/debug + carica batteria |
| UART | Header pin | Debug seriale in campo |

### GPIO principali (ESP32-S3)

| GPIO | Funzione |
|---|---|
| GPIO5–9 | Display SPI (CLK, MOSI, CS, DC, RST) |
| GPIO15 | Display backlight PWM |
| GPIO10–11 | SD card SPI |
| GPIO3–4 | Touch I2C (SDA, SCL) |
| GPIO40–41 | Encoder rotativo (A, B) |
| GPIO42 | Encoder pulsante |
| GPIO43 | Pulsante "CHIUDI TUTTO" |
| GPIO1 | ADC batteria (VBAT/2) |
| GPIO2 | Presenza 12V (optoisolatore) |

---

## Funzioni software

### 1. Connessione alla mesh

L'HMI si connette come **nodo normale** — non come root:

```c
// Configurazione mesh HMI — NON è root
esp_mesh_cfg_t cfg = {
    .channel = MESH_CHANNEL,
    .mesh_id = MESH_ID,
    .mesh_ap.max_connection = 0,  // HMI non accetta figli
    .mesh_type = MESH_NODE,       // nodo foglia
};
esp_mesh_fix_root(false);  // mai root
```

Alla connessione, invia `MSG_REGISTER` al ROOT con `node_type = HMI`.
Subito dopo richiede un dump completo della node registry al ROOT (`MSG_STATUS_REQ` broadcast).

### 2. Sincronizzazione node registry

```c
// Alla ricezione della registry dal ROOT
void on_registry_dump(registry_dump_t *dump) {
    for (int i = 0; i < dump->count; i++) {
        update_local_node(dump->nodes[i]);
        refresh_ui_tile(dump->nodes[i].node_id);
    }
    ui_show_dashboard();
}

// Aggiornamenti incrementali durante l'uso
void on_node_status_update(node_status_t *update) {
    update_local_node_by_id(update->node_id, update->status, update->payload);
    refresh_ui_tile(update->node_id);  // aggiorna solo il tile coinvolto
}
```

### 3. Ricezione alert

Gli alert (`MSG_ALERT`) arrivano dalla mesh e vengono gestiti passivamente:

```c
void on_mesh_alert(mesh_msg_t *msg) {
    alert_payload_t *alert = (alert_payload_t*)msg->payload;

    // 1. Aggiorna stato locale del nodo mittente
    update_local_node_status(msg->node_id, alert->new_state);

    // 2. Mostra notifica sul display
    ui_push_notification(alert->severity, alert->message);

    // 3. Se critico: accende backlight e buzzer
    if (alert->severity == ALERT_CRITICAL) {
        backlight_set(100);
        buzzer_beep(3);
    }

    // 4. Logga su SD
    sd_log_event(msg->node_id, alert->message, get_timestamp());
}
```

> L'HMI **non reagisce** agli alert con comandi automatici. Visualizza, suona, logga. Punto.

### 4. Invio comandi manuali

Solo da interazione esplicita dell'utente:

```c
void on_user_command(uint16_t target_node_id, uint8_t action) {
    mesh_msg_t msg = {
        .version    = PROTOCOL_VERSION,
        .msg_type   = MSG_COMMAND,
        .node_id    = NODE_ID_HMI,
        .target_id  = target_node_id,
        .seq_num    = next_seq_num(),
    };
    cmd_payload_t cmd = { .action = action };
    memcpy(msg.payload, &cmd, sizeof(cmd));
    msg.payload_len = sizeof(cmd);
    msg.crc16 = crc16_calc(&msg);

    enqueue_mesh_tx(&msg);  // retry con backoff gestito da mesh_tx_task
}
```

### 5. Gestione sorgente alimentazione

```c
void power_monitor_task(void *pvParam) {
    while (1) {
        bool v12_present = gpio_get_level(GPIO_V12_DETECT);
        float vbat = adc_read_vbat();  // mV
        uint8_t batt_pct = vbat_to_percent(vbat);

        if (!v12_present && batt_pct < 15) {
            ui_show_banner(BANNER_BATT_LOW, "Batteria HMI bassa — collegare al 12V");
            buzzer_beep(1);
        }
        if (!v12_present && vbat < 3200) {
            ui_show_banner(BANNER_SHUTDOWN, "Batteria critica — spegnimento");
            vTaskDelay(pdMS_TO_TICKS(3000));
            power_off();  // salva stato su NVS e spegne
        }

        ui_update_header_battery(batt_pct, v12_present);
        vTaskDelay(pdMS_TO_TICKS(10000));  // check ogni 10s
    }
}
```

---

## Interfaccia utente — Schermate

### Schermata 1 — Dashboard principale

```
┌──────────────────────────────────────────────────┐
│  DomoC    🔋78%  🔌12V  RSSI:-42  ROOT:🟢 14:32 │
├──────────────┬───────────────────────────────────┤
│ 🟢 STEP      │ 🟢 GREY_WATER                    │
│  Chiuso      │  Chiusa                           │
├──────────────┼───────────────────────────────────┤
│ 🟢 FRESH     │ 🟢 FRONT_DOOR                    │
│  Chiusa      │  Chiusa                           │
├──────────────┼───────────────────────────────────┤
│ 🟢 T.BUNK    │ 🟢 T.LOFT      │ 🟢 T.KITCHEN   │
│  19.2°C      │  21.0°C        │  20.5°C         │
├──────────────┴───────────────────────────────────┤
│ 🌡 12.4°C  💧 68%  🔋Mot:12.8V  Serv:12.6V     │
│                              [LOG] [⚙] [CHIUDI◼]│
└──────────────────────────────────────────────────┘
```

- **Header**: batteria HMI, sorgente alimentazione, RSSI verso mesh, stato ROOT
- Icona nodo: 🟢 online / 🟡 warning / 🔴 offline / ⚫ non registrato
- **[CHIUDI◼]**: pulsante "Chiudi tutto" — conferma richiesta prima dell'invio

### Schermata 2 — Dettaglio nodo

- Stato corrente con icona e timestamp ultimo aggiornamento
- Pulsanti azione contestuali (APRI/CHIUDI per attuatori, SET per termostati)
- Statistiche: RSSI, hop count, uptime, versione firmware
- Storico eventi recenti del nodo (ultimi 5)

### Schermata 3 — Allarmi e log

- Lista eventi con timestamp (ultimi 100, scroll)
- Filtro per severità e per nodo
- Possibilità di esportare log su SD

### Schermata 4 — Topologia mesh

- Albero testuale della rete con ROOT in cima
- Per ogni nodo: hop count, RSSI verso parent, stato
- Indica visivamente se l'HMI è connesso come foglia

### Schermata 5 — Configurazione

- Soglie allarmi batteria (modificabili e inviabili ai nodi)
- Timeout backlight
- Nomi personalizzati nodi
- Avvio OTA su nodo selezionato (invia richiesta al ROOT)

---

## Display HMI scelto

Per l'interfaccia HMI viene utilizzato il display **ELECROW ESP32 7" HMI RGB TFT LCD Touch Screen**:

- **Modello:** ELECROW ESP32 7" HMI RGB TFT LCD Touch Screen
- **Processore:** ESP32-S3-WROOM-1-N4R8, dual-core LX6 fino a 240 MHz
- **Risoluzione:** 800×480 pixel, formato 16:9
- **Touch:** Capacitivo, multi-touch
- **Compatibilità:** Arduino IDE, ESP-IDF, PlatformIO, MicroPython, LVGL
- **Connettività:** WiFi, Bluetooth integrati
- **Espandibilità:** Slot TF card, interfacce USB, speaker, batteria
- **Alimentazione:** 5V (USB o batteria)
- **Applicazioni:** HMI, domotica, automazione, dashboard, IoT
- **Note:** Non include case, supporto tecnico e tutorial disponibili

Questa scelta garantisce un'interfaccia moderna, ampia compatibilità software e hardware, e facilità di sviluppo per dashboard e controlli camper.

---

## Task FreeRTOS

| Task | Core | Priorità | Stack | Funzione |
|---|---|---|---|---|
| `mesh_rx_task` | 0 | 5 | 4 KB | Ricezione messaggi mesh, dispatch a coda interna |
| `mesh_tx_task` | 0 | 5 | 4 KB | Invio comandi manuali, ACK, retry |
| `registry_sync_task` | 0 | 4 | 3 KB | Sync registry da ROOT, aggiornamenti incrementali |
| `alert_display_task` | 0 | 4 | 3 KB | Ricezione MSG_ALERT → notifica UI → buzzer → log SD |
| `display_task` | 1 | 3 | 12 KB | Rendering LVGL, gestione touch/encoder, coda UI |
| `power_monitor_task` | 0 | 3 | 2 KB | ADC batteria, presenza 12V, banner e spegnimento |
| `sd_logger_task` | 0 | 1 | 3 KB | Scrittura asincrona log su SD (ring buffer interno) |

> `display_task` su **Core 1** — non interferisce con la mesh (Core 0).
> Tutti gli aggiornamenti LVGL passano per una coda UI thread-safe — nessun task tocca oggetti LVGL direttamente.

---

## Comportamento in assenza di connessione mesh

Se l'HMI perde la mesh (es. portato fuori range):

1. Header mostra `MESH: ✗ — offline` con RSSI -
2. I dati visualizzati rimangono congelati all'ultimo stato noto (con timestamp "ultimo aggiornamento: Xs fa")
3. I comandi manuali vengono rifiutati con messaggio "Connessione mesh assente"
4. Il display si dimezza automaticamente per risparmiare batteria
5. Alla riconnessione: `registry_sync_task` richiede dump completo al ROOT e aggiorna tutta la UI

> L'assenza dell'HMI non ha nessun effetto sulla mesh, sul ROOT o sui nodi funzione.

---

## Comportamento in assenza di alimentazione 12V

Quando l'HMI è alimentato solo via USB-C (es. durante manutenzione del camper):

1. Il banner `⚡ Alimentazione USB-C — modalità diagnostica` viene mostrato in header
2. I **comandi verso gli attuatori sono bloccati** (gradino, valvole, porta) — il bus di potenza 12V è assente e gli attuatori non funzionerebbero comunque
3. La **mesh rimane attiva**: i nodi ancora alimentati continuano a inviare heartbeat
4. Il **display e il log** funzionano normalmente — utile per diagnostica
5. Se il 12V torna, il banner scompare e i comandi vengono riabilitati automaticamente

```c
void power_monitor_task(void *pvParam) {
    while (1) {
        bool v12_present = gpio_get_level(GPIO_V12_DETECT);
        if (!v12_present) {
            ui_show_banner(BANNER_USB_MODE, "⚡ USB-C — comandi attuatori disabilitati");
            command_gate_set(false);  // blocca invio comandi a STEP, GREY_WATER, ecc.
        } else {
            ui_clear_banner(BANNER_USB_MODE);
            command_gate_set(true);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## Considerazioni hardware pratiche

- **SPI separati per display e SD**: condividere il bus causa freeze UI durante le scritture su SD (latenza 5–50ms per operazione filesystem)
- **Protezione ESD USB-C**: TVS diode USBLC6-2 sul connettore USB-C
- **Buzzer piezoelettrico passivo**: collegato a un pin LEDC per toni differenziati per severità allarme
- **LED RGB di stato**: visibile anche a display spento (verde = mesh ok, rosso lampeggiante = allarme, arancio = batteria bassa)
- **Fissaggio display**: viti + cornice rigida — il biadesivo non regge alle vibrazioni del camper in marcia
- **Connettore carica 12V**: usare connettore con locking (es. JST-GH 2 pin) — evitare jack barrel
- **Forma fattore**: prevedere un supporto/dock fisso nel camper dove l'HMI può essere appoggiato e ricaricato automaticamente tramite pogo pin o connettore magnetico
