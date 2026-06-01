# DomoC — Modalità Standalone

---

## Panoramica

La modalità **STANDALONE** è il comportamento di un nodo funzione quando perde la connessione alla mesh ESP-Mesh per un periodo prolungato (> 30s). In questa modalità il nodo continua a operare autonomamente, mantenendo tutta la logica di sicurezza e funzionale locale.

**Principio fondamentale del sistema DomoC**: la logica di sicurezza vive nei nodi, non nel coordinatore. Se il ROOT o l'HMI sono offline, ogni nodo continua ad operare correttamente.

---

## Trigger della modalità standalone

Un nodo entra in STANDALONE quando:

```c
#define STANDALONE_TIMEOUT_MS 30000  // 30s senza connessione mesh

void mesh_monitor_task(void *pvParam) {
    while (1) {
        if (!esp_mesh_is_connected()) {
            uint32_t disconnected_ms = esp_timer_get_time() / 1000 - last_connected_ms;
            if (disconnected_ms > STANDALONE_TIMEOUT_MS && !standalone_mode) {
                enter_standalone_mode();
            }
        } else {
            last_connected_ms = esp_timer_get_time() / 1000;
            if (standalone_mode) {
                exit_standalone_mode();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Comportamento in STANDALONE per tipo di nodo

### Nodi attuatori (STEP, GREY_WATER, FRESH_WATER, FRONT_DOOR)

```c
void enter_standalone_mode(void) {
    standalone_mode = true;
    led_set(LED_WHITE_SOLID);            // LED bianco fisso = standalone
    gpio_intr_enable(GPIO_LOCAL_BUTTON); // abilita pulsante fisico locale
    log_local("Mesh persa > 30s — modalità standalone attiva");
    // La macchina a stati continua normalmente
    // I finecorsa hardware continuano a funzionare
}
```

In STANDALONE i nodi attuatori:
- Mantengono lo stato attuale (porta/valvola aperta o chiusa)
- Rispondono ai finecorsa hardware (sempre attivi)
- Accettano comandi dal pulsante fisico locale (se presente)
- **Continuano a ricevere e reagire ai broadcast mesh** (MSG_KEY_ON, MSG_STEP_OPEN): i broadcast usano `dst_id = 0xFF` e vengono ricevuti anche senza ROOT perché i nodi si ascoltano direttamente tra loro via ESP-Mesh

### Nodi termostato (THERMO_BUNK, THERMO_LOFT, THERMO_KITCHEN)

In STANDALONE i termostati:
- Continuano il ciclo di controllo temperatura con l'ultimo setpoint noto
- Attuano la valvola aria calda in base alla logica locale
- Mostrano `[STANDALONE]` sul display locale
- Accettano comandi dal display touch locale
- Riscaldamento non si interrompe

### Nodo ROOT

Il ROOT non ha una "modalità standalone" nel senso tradizionale. Se il ROOT si riavvia:
- I nodi funzione entrano in STANDALONE entro 30s
- La logica di sicurezza broadcast (KEY_ON) continua a funzionare perché i nodi si ascoltano direttamente
- L'HMI mostra `ROOT: OFFLINE`

---

## Broadcast di sicurezza in STANDALONE

I messaggi di sicurezza con `dst_id = 0xFF` (broadcast) continuano a funzionare anche senza ROOT:

```
Scenario: ROOT è offline, KEY_ON viene inserita

[ROOT offline]

[Nodo KEY_ON (optoisolatore)]
    │
    │ MSG_KEY_ON broadcast (dst_id=0xFF)
    │ Trasmesso via ESP-Mesh direttamente
    ▼
[STEP — standalone]     → riceve MSG_KEY_ON → chiude gradino se aperto
[GREY_WATER — standalone] → riceve MSG_KEY_ON → chiude valvola se aperta
[FRESH_WATER — standalone] → riceve MSG_KEY_ON → chiude valvola se aperta
[HMI — connesso]        → riceve MSG_KEY_ON → mostra badge KEY_ON
```

> I broadcast mesh funzionano peer-to-peer tra nodi vicini anche senza ROOT. Il ROOT in ESP-Mesh è necessario per il routing multi-hop, ma i nodi sullo stesso livello si raggiungono direttamente.

---

## Uscita dalla modalità standalone

Quando la mesh viene ripristinata:

```c
void exit_standalone_mode(void) {
    standalone_mode = false;
    gpio_intr_disable(GPIO_LOCAL_BUTTON); // disabilita pulsante locale
    led_restore_normal_state();

    // Ri-registrazione al ROOT con reconnect=true
    mesh_register();
    // Il ROOT risponde con MSG_REGISTER_ACK confermando l'ID esistente (da NVS)
    // Poi il nodo invia il proprio stato corrente
    send_status_update();

    log_local("Mesh ripristinata — uscita standalone, stato pubblicato");
}
```

### Sequenza ri-registrazione

```
Nodo                    ROOT
  │                       │
  │  MSG_REGISTER         │
  │  (reconnect=true,     │
  │   mac=<MAC nodo>)     │
  │──────────────────────►│
  │                       │ Trova MAC in NVS
  │                       │ Conferma ID esistente
  │                       │ node_status → ONLINE
  │  MSG_REGISTER_ACK     │
  │◄──────────────────────│
  │  MSG_DESCRIPTOR       │
  │──────────────────────►│ ROOT aggiorna registry
  │                       │ ROOT forwarda all'HMI
  │  MSG_STATUS           │
  │──────────────────────►│ ROOT/HMI aggiornano UI
```

Il ROOT usa `reconnect=true` per:
1. Confermare l'ID logico già assegnato (non riassegna un nuovo ID)
2. Aggiornare `last_seen_ms` nel registry
3. Annullare il flag `NODE_OFFLINE` / `NODE_LOST` nel registry
4. Notificare l'HMI con `MSG_NODE_JOINED` (per aggiornare la visualizzazione)

---

## Pulsante fisico locale

Ogni nodo attuatore può avere un pulsante fisico di emergenza (GPIO interrupt). In modalità normale è disabilitato (i comandi arrivano dall'HMI via mesh). In STANDALONE viene abilitato:

```c
void IRAM_ATTR local_button_isr(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t event = BTN_LOCAL_PRESS;
    xQueueSendFromISR(button_queue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_task(void *pvParam) {
    uint8_t event;
    while (1) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            if (!standalone_mode) continue;  // ignora in modalità normale
            // Toggle: se chiuso apri, se aperto chiudi
            if (current_state == STATE_CLOSED) {
                transition_to(STATE_OPENING);
                motor_start(DIR_OPEN);
            } else if (current_state == STATE_OPEN) {
                transition_to(STATE_CLOSING);
                motor_start(DIR_CLOSE);
            }
        }
    }
}
```

> Il pulsante fisico è opzionale. I finecorsa hardware funzionano sempre, indipendentemente dalla modalità.

---

## LED di stato per modalità standalone

| Colore | Pattern | Significato |
|---|---|---|
| Bianco | Fisso | STANDALONE attivo |
| Verde | Fisso | Online, tutto ok |
| Verde | Lampeggiante lento (1Hz) | Online, qualche anomalia minore |
| Blu | Fisso | Aperto / attivo (specifico per ogni nodo) |
| Arancio | Lampeggiante | In movimento |
| Rosso | Lampeggiante veloce (5Hz) | Auto-closing (KEY_ON) |
| Rosso | Fisso | ERROR — richiede intervento |
| Bianco | Lampeggiante veloce | INITIALIZING / boot |

---

## Comportamento per nodo in STANDALONE

| Nodo | Funziona in standalone | Pulsante locale | Note |
|---|---|---|---|
| ROOT | N/A (è il root) | — | I nodi entrano in STANDALONE se il ROOT cade |
| STEP | Sì | Opzionale | Finecorsa attivi; KEY_ON broadcast funziona |
| GREY_WATER | Sì | Opzionale | Monitoraggio batteria ADC continua |
| FRESH_WATER | Sì | Opzionale | Valvola NC — torna chiusa senza alimentazione |
| FRONT_DOOR | Sì | Opzionale | Finecorsa attivi; KEY_ON broadcast funziona |
| THERMO_BUNK | Sì | Display locale | Ciclo termostato locale con ultimo setpoint |
| THERMO_LOFT | Sì | Display locale | Ciclo termostato locale con ultimo setpoint |
| THERMO_KITCHEN | Sì | Display locale | Ciclo termostato locale con ultimo setpoint |
| REAR | Parziale | — | Stream HTTP disponibile; alert motion non inviabili su mesh |
| CAM_EXT | Parziale | — | Motion detection locale; eventi loggati su SD se presente |
| HMI | Sì | Display touch | Visualizza stato cached; comandi bloccati verso nodi offline |

---

## Configurazione standalone

Il comportamento standalone è configurabile in `node_config.json`:

```json
{
  "behavior": {
    "standalone_timeout_ms": 30000,
    "local_button_gpio": 5,
    "local_button_enabled": true,
    "on_standalone_keep_state": true,
    "on_reconnect_publish_status": true
  }
}
```

- `standalone_timeout_ms`: tempo senza mesh prima di entrare in standalone (default 30s)
- `local_button_gpio`: GPIO del pulsante fisico locale (0 = nessuno)
- `on_standalone_keep_state`: se `true`, il nodo mantiene lo stato attuale; se `false`, esegue una chiusura di sicurezza
- `on_reconnect_publish_status`: pubblica lo stato corrente appena la mesh è ripristinata

---

## Scenari di failure e comportamento atteso

### Scenario 1: ROOT si riavvia (< 60s)

1. Nodi mesh: perdono la connessione root, ESP-Mesh tenta re-parenting
2. Dopo 30s senza mesh: nodi entrano in STANDALONE
3. ROOT torna online: ESP-Mesh si riconnette automaticamente
4. Nodi si ri-registrano con `reconnect=true`
5. Operatività completa ripristinata in 5–10s dal ritorno del ROOT

### Scenario 2: ROOT offline a lungo (> 30s)

1. Tutti i nodi entrano in STANDALONE
2. Ogni nodo opera in autonomia con logica locale
3. I broadcast di sicurezza (KEY_ON) continuano a funzionare
4. Quando il ROOT torna: stessa procedura dello scenario 1

### Scenario 3: HMI offline

1. I nodi non sono interessati (l'HMI non è root)
2. Il ROOT non riceve heartbeat dall'HMI: dopo 15s invia `MSG_NODE_WARNING`, dopo 30s `MSG_NODE_OFFLINE`
3. Il sistema continua a funzionare normalmente
4. I comandi manuali dall'HMI non sono disponibili; l'automazione e la sicurezza funzionano

### Scenario 4: singolo nodo offline

1. Il ROOT non riceve heartbeat dal nodo: `MSG_NODE_WARNING` dopo 15s, `MSG_NODE_OFFLINE` dopo 30s
2. L'HMI mostra il nodo in stato `🟡 WARNING` poi `🔴 OFFLINE`
3. Gli altri nodi non sono influenzati
4. La logica di sicurezza degli altri nodi continua (es. STEP si chiude su KEY_ON anche se GREY_WATER è offline)

### Scenario 5: alimentazione 12V assente

1. Tutti i nodi si spengono (alimentati da 12V)
2. Al ripristino del 12V: tutti i nodi si avviano in sequenza
3. Ogni nodo si registra al ROOT (con `reconnect=true` se già conosciuto)
4. Il ROOT ricostruisce il registry in pochi secondi
5. Le valvole NC (FRESH_WATER) sono chiuse per default senza alimentazione — stato sicuro automatico
