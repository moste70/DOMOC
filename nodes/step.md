# DomoC — Nodo STEP (Gradino di accesso)

---

## Descrizione

Il nodo `STEP` controlla il motore del gradino di accesso al camper. È un nodo **autonomo**: contiene tutta la logica necessaria per operare in sicurezza indipendentemente dallo stato del master o della connessione mesh.

Funzioni principali:
- Apertura e chiusura motorizzata del gradino
- Rilevamento stato fisico reale (aperto / chiuso / in movimento / errore)
- Reazione automatica agli eventi mesh rilevanti (es. chiave inserita)
- Pubblicazione dello stato sulla mesh per il ROOT (registry) e per l'HMI (visualizzazione)

---

## Hardware

### Microcontrollore

- **ESP32-C3** — sufficiente per questo nodo (single-core, basso consumo, Wi-Fi)

### Componenti

| Componente | Tipo | Note |
|---|---|---|
| Motore gradino | DC 12V con riduttore | Già presente sul camper — interfacciato via H-bridge |
| Driver H-bridge | **DRV8833** (TI) | Controllo bidirezionale, 2A per canale, protezione termica — [Datasheet](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |
| **Sensore SHT31** | I2C, ±0.3°C / ±2% RH | Temperatura e umidità esterna — custodia Gore-Tex IP65, rilevamenti ogni 60s |
| Alimentazione | 12V bus camper → buck 3.3V | Il motore prende 12V direttamente, non dal buck |
| Relay di potenza | Relay 12V/10A o MOSFET | Taglia l'alimentazione al driver motore in standby |

### Schema connessioni GPIO (ESP32-C3)

| GPIO | Funzione | Tipo |
|---|---|---|
| GPIO2 | Driver motore — DIR A (apri) | Output |
| GPIO3 | Driver motore — DIR B (chiudi) | Output |
| GPIO4 | Driver motore — ENABLE | Output |
| GPIO7 | Relay alimentazione motore | Output |
| GPIO10 | LED stato (RGB o singolo) | Output PWM |
| GPIO8/9 | **I2C (SDA/SCL)** — **SHT31** | I2C bus, sensore temperatura/umidità esterna |

> **Sicurezza software**: il nodo STEP implementa un **timeout hardware firmware** (es. 5 secondi). Se il motore rimane attivo oltre la durata prevista senza ricevere conferma di fine corsa, il firmware spegne automaticamente l'H-bridge. Questo protegge da blocchi software. In alternativa, il motore ha limiti meccanici fisici (riduttore con stop meccanico) che prevengono sovraccarico.

---

## Logica autonoma — Macchina a stati

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
         BOOT       │  comando APRI        finecorsa APERTO   │
           │        │  ────────────►  [OPENING]  ──────────►  │
           ▼        │                                         │
     [INITIALIZING] │  finecorsa           comando CHIUDI     │
           │        │  CHIUSO  ◄──  [CLOSED]  ◄──────────     │
           ▼        │                  │                      │
       [STOPPED]────┤                  │ KEY_ON evento        │
         /    \     │                  │ se APERTO            │
  fc_chiuso  nessuno│                  ▼                      │
       │        │   │           [AUTO_CLOSING]                │
    [CLOSED] [OPEN] │                  │                      │
                    │          finecorsa CHIUSO               │
                    │                  │                      │
                    │              [CLOSED]                   │
                    │                                         │
                    │   timeout > 3s senza finecorsa          │
                    │   [OPENING/CLOSING] ──────────► [ERROR] │
                    └─────────────────────────────────────────┘
```

### Stati

| Stato | Descrizione | LED |
|---|---|---|
| `INITIALIZING` | Boot, lettura finecorsa, registrazione mesh | Bianco lampeggiante |
| `CLOSED` | Gradino chiuso, motore fermo, relay off | Verde fisso |
| `OPEN` | Gradino aperto, motore fermo, relay off | Blu fisso |
| `OPENING` | Motore in moto verso posizione aperta | Blu lampeggiante |
| `CLOSING` | Motore in moto verso posizione chiusa | Arancio lampeggiante |
| `AUTO_CLOSING` | Chiusura automatica per evento KEY_ON | Rosso lampeggiante veloce |
| `ERROR` | Timeout finecorsa — motore bloccato o guasto | Rosso fisso |
| `STANDALONE` | Mesh assente — operazione locale via pulsante fisico | Bianco fisso |

---

## Sottoscrizione eventi mesh

Il nodo STEP **non risponde a tutti i messaggi mesh** — filtra esclusivamente quelli rilevanti:

```c
// Tabella sottoscrizioni eventi — definita a compile time in NVS
const mesh_subscription_t step_subscriptions[] = {
    { .source_node_id = NODE_ID_KEY_ON,  .msg_type = MSG_ALERT  },  // chiave inserita/disinserita
    { .source_node_id = NODE_ID_HMI,     .msg_type = MSG_COMMAND }, // comandi manuali dall'HMI
    { .source_node_id = NODE_ID_ROOT,    .msg_type = MSG_STATUS_REQ }, // richiesta stato dal ROOT
    { .source_node_id = NODE_ID_ROOT,    .msg_type = MSG_OTA_START }, // aggiornamento firmware
};
```

### Reazione agli eventi sottoscritti

#### Evento: `KEY_ON` → MSG_ALERT (chiave inserita)

```c
void on_key_on_event(key_on_alert_t *alert) {
    if (alert->key_state == KEY_INSERTED) {
        if (current_state == STATE_OPEN) {
            // Chiusura automatica senza aspettare il master
            log_mesh_event("KEY_ON ricevuto: avvio chiusura automatica gradino");
            transition_to(STATE_AUTO_CLOSING);
            motor_start(DIR_CLOSE);
            // Notifica il master dell'azione intrapresa
            send_alert(ALERT_STEP_AUTO_CLOSING, "Gradino: chiusura automatica per chiave inserita");
        }
        // Se già chiuso: nessuna azione necessaria
    }
}
```

#### Evento: `MSG_COMMAND` dall'HMI (comando manuale utente)

```c
void on_hmi_command(mesh_msg_t *msg) {
    cmd_payload_t *cmd = (cmd_payload_t*)msg->payload;

    // Verifica che il comando sia destinato a questo nodo
    if (msg->target_id != NODE_ID_STEP && msg->target_id != NODE_ID_BROADCAST) return;

    switch (cmd->action) {
        case ACTION_OPEN:
            if (current_state == STATE_CLOSED || current_state == STATE_AUTO_CLOSING) {
                transition_to(STATE_OPENING);
                motor_start(DIR_OPEN);
                send_ack(msg->seq_num, ACK_OK);
            } else {
                send_ack(msg->seq_num, ACK_REJECTED); // già aperto o in errore
            }
            break;

        case ACTION_CLOSE:
            if (current_state == STATE_OPEN) {
                transition_to(STATE_CLOSING);
                motor_start(DIR_CLOSE);
                send_ack(msg->seq_num, ACK_OK);
            } else {
                send_ack(msg->seq_num, ACK_REJECTED);
            }
            break;

        case ACTION_GET_STATUS:
            send_status_response();
            break;
    }
}
```

#### Interrupt finecorsa (hardware — non mesh)

```c
// ISR — eseguita immediatamente al raggiungimento del finecorsa
void IRAM_ATTR finecorsa_isr(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t event = (uint8_t)(uintptr_t)arg; // FC_CLOSED o FC_OPEN
    xQueueSendFromISR(finecorsa_queue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task che processa i finecorsa
void finecorsa_task(void *pvParam) {
    uint8_t event;
    while (1) {
        if (xQueueReceive(finecorsa_queue, &event, portMAX_DELAY)) {
            motor_stop();
            relay_power_off(); // taglia alimentazione motore → consumo zero in standby

            if (event == FC_CLOSED) {
                transition_to(STATE_CLOSED);
                send_status_update(); // notifica ROOT (aggiorna registry) e HMI (aggiorna display)
            } else if (event == FC_OPEN) {
                transition_to(STATE_OPEN);
                send_status_update();
            }
        }
    }
}
```

---

## Comportamento in STANDALONE_MODE (mesh assente)

Se il nodo perde la connessione mesh per più di 30 secondi:

1. Entra in `STATE_STANDALONE`
2. Attiva un **pulsante fisico locale** (montato sul pannello del camper) per apertura/chiusura manuale
3. I finecorsa continuano a funzionare normalmente (hardware)
4. Quando la mesh torna disponibile, il nodo si ri-registra e pubblica lo stato attuale
5. **Comportamento sicuro**: se il nodo era in `AUTO_CLOSING` al momento della perdita mesh, completa comunque la chiusura

```c
void on_mesh_disconnected(void) {
    standalone_mode = true;
    gpio_intr_enable(GPIO_LOCAL_BUTTON); // abilita pulsante locale
    log_local("Mesh persa — modalità standalone attiva");
}

void on_mesh_reconnected(void) {
    standalone_mode = false;
    gpio_intr_disable(GPIO_LOCAL_BUTTON);
    mesh_register();       // ri-registrazione al ROOT (reconnect=true — conferma ID esistente)
    send_status_update();  // pubblica stato attuale → ROOT aggiorna registry, HMI aggiorna display
}
```

---

## Blocco comandi con chiave inserita

Quando il segnale KEY_ON (positivo sotto chiave) è attivo, i comandi di apertura/chiusura gradino vengono bloccati:

- L’interfaccia utente (HMI) mostra il pulsante di comando disabilitato (non cliccabile)
- Non vengono inviati comandi al nodo finché la chiave resta inserita
- Un messaggio di stato/informazione può essere visualizzato (es. "Comando disabilitato: chiave inserita")

---

## Heartbeat e payload di stato

Ogni 5 secondi il nodo invia un heartbeat con il proprio stato corrente:

```c
typedef struct __attribute__((packed)) {
    uint8_t  state;          // Enum: CLOSED, OPEN, OPENING, CLOSING, ERROR, STANDALONE
    uint8_t  fc_closed;      // 1 = finecorsa chiuso attivo
    uint8_t  fc_open;        // 1 = finecorsa aperto attivo
    uint8_t  motor_active;   // 1 = motore in movimento
    uint16_t last_move_ms;   // Durata ultimo movimento in ms (diagnostica)
    uint8_t  error_code;     // 0 = nessun errore, 1 = timeout finecorsa
} step_status_t;
```

---

## Funzioni aggiuntive del nodo STEP

Oltre al controllo del gradino, il nodo STEP svolge anche le seguenti funzioni:

- **Alimentazione motore**: gestisce direttamente il relay di potenza che abilita/disabilita l'alimentazione al driver motore. L'alimentazione viene attivata solo durante il movimento (apertura/chiusura) e disattivata in stato di riposo per ridurre i consumi e aumentare la sicurezza.

- **Lettura sensore gradino aperto**: monitora costantemente il finecorsa "aperto" tramite GPIO dedicato. Questo permette di rilevare con precisione quando il gradino è completamente esteso e di fermare il motore in sicurezza.

- **Lettura temperatura esterna**: può essere collegato a un sensore di temperatura digitale (es. DS18B20 su 1-Wire o sensore analogico su ADC). La temperatura esterna viene letta periodicamente e pubblicata sulla mesh, rendendola disponibile sia all'HMI che agli altri nodi (es. per logica di automazione o visualizzazione).

> In questo modo il nodo STEP integra sia funzioni di attuazione (motore gradino) sia di sensing (posizione gradino, temperatura esterna), riducendo la necessità di nodi separati e semplificando il cablaggio.

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 3 KB | Ricezione messaggi mesh, filtro sottoscrizioni, dispatch |
| `mesh_tx_task` | 5 | 3 KB | Invio heartbeat, alert, ACK |
| `step_control_task` | 6 | 3 KB | Macchina a stati principale, gestione motore |
| `finecorsa_task` | 7 | 2 KB | Processamento interrupt finecorsa (alta priorità) |
| `ota_receiver_task` | 2 | 6 KB | Ricezione e applicazione aggiornamenti OTA |

> `finecorsa_task` ha la **priorità più alta** del nodo: fermare il motore al finecorsa è un'operazione di sicurezza che non deve mai essere preemptata.

---

## Gestione energetica

- **Relay di potenza motore**: il relay che alimenta il driver L298N è aperto (OFF) in tutti gli stati statici (`CLOSED`, `OPEN`, `STANDALONE`). Il driver consuma ~70 mA anche a motore fermo — con il relay off, il consumo scende a < 1 mA
- **Modem sleep**: `WIFI_PS_MIN_MODEM` attivo in tutti gli stati non-movimento
- **Potenza TX**: 10 dBm (distanza dal master < 5m nel camper)
- **Consumo stimato**:
  - Idle (gradino chiuso, relay off): ~4 mA
  - Mesh attiva in ricezione: ~8 mA
  - Motore in movimento: 500–2000 mA (12V, dipende dal carico) per 2–5 secondi

---

## Considerazioni pratiche

- **Timeout sicurezza motore**: se il finecorsa non scatta entro 10 secondi dall'avvio motore, il firmware ferma tutto e va in `STATE_ERROR`. Segnala alert al master e accende LED rosso fisso.
- **Debounce finecorsa**: i microswitch meccanici rimbalzano. Implementare debounce software di 50ms dopo il primo fronte ISR prima di processare l'evento.
- **Protezione inversione polarità motore**: il driver L298N gestisce già l'inversione — non applicare mai entrambe le DIR simultaneamente nel codice.
- **Cablaggio**: il gradino è vicino alla porta — zona con vibrazioni elevate. Usare cavo con guaina rinforzata per i finecorsa e connettori con locking.

## Rilevamento posizione senza finecorsa

Se il gradino non dispone di finecorsa, la posizione viene stimata tramite:

- **Timer software**: il firmware attiva il motore per un tempo fisso (circa 3 secondi per apertura/chiusura), sufficiente a garantire il movimento completo. Al termine del timer, il motore viene disattivato.

> **Nota**: In assenza di feedback diretto, è importante dimensionare il timer con un margine di sicurezza e prevedere un timeout massimo per evitare surriscaldamenti o danni in caso di blocco meccanico.
