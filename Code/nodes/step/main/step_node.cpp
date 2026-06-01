// ⚠ FILE LEGACY — versione ESP-IDF del nodo STEP.
// Lo sviluppo attivo avviene in Code/nodes_arduino/step/.
// Questo file è mantenuto solo come riferimento architetturale.
// NOTA: la logica dei finecorsa qui usa active-LOW (gpio == 0 = attivato),
// ma l'hardware reale ha switch NC con pull-up: la posizione raggiunta
// corrisponde a gpio == 1 (HIGH). Il bug è stato corretto nella versione Arduino.

#include "step_node.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

namespace domoc {

static const char* TAG = "step";

// ─────────────────────────────────────────────────────────────────────────────
// ARCHITETTURA DEL NODO
//
// Il nodo STEP usa tre task FreeRTOS indipendenti che comunicano tra loro
// tramite una coda (fc_queue_):
//
//   ┌──────────────┐    fc_queue_    ┌──────────────┐
//   │  fc_task     │ ─── FC_EVT ──→  │  ctrl_task   │
//   │  (prio 7)    │                 │  (prio 6)    │
//   │  polling     │                 │  macchina    │
//   │  finecorsa   │                 │  a stati     │
//   └──────────────┘                 └──────────────┘
//
//   ┌──────────────┐
//   │  sensor_task │   lettura SHT31 ogni 60 s
//   │  (prio 3)    │   scrive temperature_ e humidity_
//   └──────────────┘
//
// Separazione dei task:
//   fc_task     — priorità alta (7): il rilevamento del finecorsa è time-critical.
//                 Un ritardo nel fermare il motore potrebbe danneggiare il meccanismo.
//   ctrl_task   — priorità media (6): gestisce la logica di stato e i comandi mesh.
//                 Blocca su xQueueReceive con timeout 100 ms per controllare anche
//                 il watchdog di timeout motore.
//   sensor_task — priorità bassa (3): la lettura ambientale non è urgente.
//                 Un ritardo di qualche secondo è accettabile.
// ─────────────────────────────────────────────────────────────────────────────

// ── Descriptor statico (auto-descrizione per l'HMI) ─────────────────────────
//
// Struttura da 140 byte inviata al ROOT dopo la registrazione.
// L'HMI usa questi dati per costruire la schermata del nodo senza
// conoscere staticamente i tipi di nodo esistenti nel sistema.
//
// Gli offset nelle proprietà corrispondono ai campi di StepStatus:
//   state=0 (uint8), temperature=7 (float32), humidity=11 (float32)
const NodeDescriptor StepNode::kDesc = {
    .node_icon      = ICON_STEP,
    .action_count   = 3,
    .property_count = 3,
    ._pad           = 0,
    .actions = {
        // FLAG_KEY_BLOCKED: l'HMI disabilita questi pulsanti quando KEY_ON è attivo.
        // L'operatore non può aprire il gradino mentre il motore del camper è acceso.
        { ACTION_OPEN,       ICON_ACT_OPEN,  CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  CTRL_BUTTON, 0, 0, 0,                "INFO"   },
        {},  // slot vuoto (DESC_MAX_ACTIONS = 4)
    },
    .properties = {
        { PROP_STATE,       0,  PAYLOAD_UINT8,   WIDGET_LABEL,       0,   0,    "",                 "%.0f" },
        // \xC2\xB0\x43 = "°C" in UTF-8 (IDF non usa literal UTF-8 nei sorgenti C++)
        { PROP_TEMPERATURE, 7,  PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 150, 400,  "\xC2\xB0\x43",    "%.1f" },
        // range_min/max nel descriptor sono ×10: 0→0%, 1000→100.0%
        { PROP_HUMIDITY,    11, PAYLOAD_FLOAT32, WIDGET_PROGRESS,    0,   1000, "%",                "%.0f" },
        {},  // slot vuoto (DESC_MAX_PROPERTIES = 4)
    },
};

// NodeBase riceve ID logico, tipo e nome nodo — usati per la registrazione mesh.
StepNode::StepNode() : NodeBase(NODE_ID_STEP, NodeType::STEP, "STEP") {}

// ── Inizializzazione GPIO ────────────────────────────────────────────────────

void StepNode::gpio_init() {
    // Configura i tre GPIO dell'H-bridge come uscite digitali in un'unica chiamata.
    // gpio_config() è più efficiente di tre pinMode() separati perché scrive
    // i registri hardware in un solo passaggio.
    gpio_config_t out_cfg = {};
    out_cfg.pin_bit_mask = (1ULL << GPIO_HB_DIR_A) |
                           (1ULL << GPIO_HB_DIR_B) |
                           (1ULL << GPIO_HB_EN);
    out_cfg.mode         = GPIO_MODE_OUTPUT;
    out_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    out_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&out_cfg);

    // Stato iniziale sicuro: tutti i relay dell'H-bridge aperti.
    // Il motore risulta scollegato (EN=0 stacca il 12V_SW prima ancora
    // che i relay di direzione abbiano uno stato definito).
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);
    gpio_set_level(GPIO_HB_DIR_B, 0);

    // Finecorsa: input con pull-up hardware abilitato dall'IDF.
    // Il PCB ha già pull-up da 4.7 kΩ sugli optoisolatori, ma abilitare
    // anche il pull-up interno dell'ESP32 aggiunge ridondanza durante il boot
    // prima che il circuito esterno sia stabile.
    // Interrupt disabilitato: i finecorsa vengono letti in polling da fc_task
    // (meno overhead di un ISR per eventi rari come l'arrivo a fine corsa).
    gpio_config_t in_cfg = {};
    in_cfg.pin_bit_mask  = (1ULL << GPIO_FC_CLOSED) | (1ULL << GPIO_FC_OPEN);
    in_cfg.mode          = GPIO_MODE_INPUT;
    in_cfg.pull_up_en    = GPIO_PULLUP_ENABLE;
    in_cfg.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    in_cfg.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&in_cfg);
}

// ── Inizializzazione I2C (nuova API IDF 5.x master bus) ─────────────────────

void StepNode::i2c_init() {
    // Crea il bus I2C master. glitch_ignore_cnt=7 filtra glitch fino a 7 cicli
    // di clock (~17 µs a 400 kHz): sufficiente per il cablaggio tipico di un camper
    // (fili da 0.5-1 m, capacità parassita moderata).
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.clk_source              = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port                = I2C_NUM_0;
    bus_cfg.scl_io_num              = GPIO_SCL;
    bus_cfg.sda_io_num              = GPIO_SDA;
    bus_cfg.glitch_ignore_cnt       = 7;
    bus_cfg.flags.enable_internal_pullup = false; // pull-up 4.7 kΩ fissi sul PCB

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));

    // Aggiunge il SHT31 come device sul bus. La velocità di 400 kHz (Fast Mode)
    // è supportata dal SHT31 e riduce il tempo di ogni transazione I2C.
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length      = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address       = SHT31_I2C_ADDR;  // 0x44 (ADDR pin a GND)
    dev_cfg.scl_speed_hz         = 400000;

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &sht31_dev_));
}

// ── Avvio dei task applicativi ───────────────────────────────────────────────

void StepNode::start_tasks() {
    gpio_init();
    i2c_init();

    // Coda da 4 slot: capienza generosa rispetto al numero di eventi attesi
    // (massimo 2 durante un ciclo completo: FC_EVT_OPEN + FC_EVT_CLOSED).
    // Lo slot extra protegge da rimbalzi che passano il debounce.
    fc_queue_ = xQueueCreate(4, sizeof(uint8_t));

    // Determina lo stato iniziale leggendo i finecorsa fisici.
    // ⚠ NOTA: questa versione usa active-LOW (== 0 = attivato). L'hardware reale
    // ha switch NC con pull-up → HIGH = attivato. Bug corretto nella versione Arduino.
    bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0;
    bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;
    if (fc_c)      transition_to(STATE_CLOSED);
    else if (fc_o) transition_to(STATE_OPEN);
    // Se nessun finecorsa è attivo all'avvio (gradino in posizione intermedia
    // dopo un reboot durante un movimento) rimane STATE_INITIALIZING.
    // Il nodo non muove nulla finché non arriva un comando o KEY_ON.

    // I task FreeRTOS richiedono una funzione statica come entry point.
    // Il pattern "trampoline" (wrapper statico → metodo d'istanza) è l'idioma
    // standard in C++ con FreeRTOS per usare metodi non-statici come task.
    xTaskCreate(fc_trampoline,     "fc_task",     2048, this, 7, &fc_task_);
    xTaskCreate(ctrl_trampoline,   "ctrl_task",   3072, this, 6, &ctrl_task_);
    xTaskCreate(sensor_trampoline, "sensor_task", 3072, this, 3, &sensor_task_);
}

// Trampoline statici: FreeRTOS chiama queste funzioni statiche passando `this`
// come parametro void*, che viene ricastato al tipo corretto per chiamare
// il metodo d'istanza reale.
void StepNode::ctrl_trampoline(void* s)   { static_cast<StepNode*>(s)->ctrl_loop(); }
void StepNode::fc_trampoline(void* s)     { static_cast<StepNode*>(s)->fc_loop(); }
void StepNode::sensor_trampoline(void* s) { static_cast<StepNode*>(s)->sensor_loop(); }

// ── fc_loop: rilevamento finecorsa con debounce ──────────────────────────────
//
// Gira in polling ogni 10 ms. Quando rileva un fronte di salita su un finecorsa,
// attende FC_DEBOUNCE_MS e ri-legge il pin: se è ancora attivo, mette l'evento
// nella coda fc_queue_ per ctrl_loop.
//
// Strategia di debounce: attesa + rilettura.
// Questa tecnica è diversa da quella della versione Arduino (confronto temporale):
// qui il task si sospende per FC_DEBOUNCE_MS lasciando la CPU ad altri task,
// poi verifica che il pin sia ancora stabile. Se il pin è tornato a zero nel
// frattempo (rimbalzo), l'evento viene scartato silenziosamente.

void StepNode::fc_loop() {
    bool prev_closed = false;
    bool prev_open   = false;

    while (tasks_running_) {
        // ⚠ Vedi nota in start_tasks(): active-LOW qui, ma hardware è NC pull-up.
        bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0;
        bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;

        // Fronte di salita su FC_CLOSED (transizione da non-attivo ad attivo)
        if (fc_c && !prev_closed) {
            vTaskDelay(pdMS_TO_TICKS(FC_DEBOUNCE_MS)); // sospendi task per debounce
            if (gpio_get_level(GPIO_FC_CLOSED) == 0) { // pin ancora stabile?
                uint8_t evt = FC_EVT_CLOSED;
                // xQueueSend con timeout 0: se la coda è piena, l'evento viene perso.
                // In condizioni normali la coda non dovrebbe mai essere piena
                // (ctrl_loop svuota la coda velocemente), ma con timeout=0 si evita
                // di bloccare fc_task (che ha priorità alta e non deve aspettare).
                xQueueSend(fc_queue_, &evt, 0);
            }
        }

        // Fronte di salita su FC_OPEN
        if (fc_o && !prev_open) {
            vTaskDelay(pdMS_TO_TICKS(FC_DEBOUNCE_MS));
            if (gpio_get_level(GPIO_FC_OPEN) == 0) {
                uint8_t evt = FC_EVT_OPEN;
                xQueueSend(fc_queue_, &evt, 0);
            }
        }

        prev_closed = fc_c;
        prev_open   = fc_o;

        // Polling ogni 10 ms: sufficiente per rilevare un evento che dura
        // almeno FC_DEBOUNCE_MS (50 ms). La finestra di campionamento è
        // 5× inferiore al debounce → nessun evento può essere perso.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(nullptr);
}

// ── ctrl_loop: macchina a stati principale ───────────────────────────────────
//
// Attende eventi dalla coda fc_queue_ con timeout di 100 ms.
// Se arriva un evento finecorsa → ferma il motore, transita di stato.
// Se la coda è vuota (timeout) → controlla il watchdog di timeout motore.
//
// Il doppio uso del timeout (evento O watchdog) è il pattern classico per
// integrare reazioni a eventi con controlli periodici in un singolo task FreeRTOS,
// senza bisogno di un secondo task o di un timer software.
//
// Grafo delle transizioni gestite da questo loop:
//
//   [FC_EVT_CLOSED ricevuto]
//     STATE_CLOSING / AUTO_CLOSING  →  STATE_CLOSED
//
//   [FC_EVT_OPEN ricevuto]
//     STATE_OPENING  →  STATE_OPEN  + broadcast MSG_STEP_OPEN
//
//   [timeout motore]
//     STATE_OPENING / CLOSING / AUTO_CLOSING  →  STATE_ERROR
//
// Le altre transizioni (CLOSED→OPENING, OPEN→CLOSING, KEY_ON→AUTO_CLOSING)
// avvengono negli hook on_command() e on_key_on(), chiamati dal thread mesh.

void StepNode::ctrl_loop() {
    uint8_t fc_evt;

    while (tasks_running_) {
        // Blocca fino a 100 ms aspettando un evento finecorsa.
        // Se pdTRUE → evento ricevuto, elaboralo.
        // Se pdFALSE → timeout 100 ms, controlla il watchdog.
        if (xQueueReceive(fc_queue_, &fc_evt, pdMS_TO_TICKS(100)) == pdTRUE) {

            // ── Gestione arrivo a finecorsa ───────────────────────────────────
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);  // ms

            // Calcola la durata del movimento per diagnostica (campo last_move_ms
            // nel payload di stato, utile per rilevare rallentamenti meccanici).
            if (motor_start_ms_ > 0)
                last_move_ms_ = (uint16_t)(now - motor_start_ms_);

            motor_stop();        // ferma il motore: sequenza EN=0 → DIR=0,0
            motor_start_ms_ = 0; // resetta il timer di avvio (disabilita il watchdog)
            error_code_ = ERR_NONE;

            if (fc_evt == FC_EVT_CLOSED) {
                // Il finecorsa CLOSED è scattato: gradino completamente ritirato.
                // Questa transizione è valida da CLOSING e AUTO_CLOSING.
                // Arrivare qui da altri stati (es. STATE_OPEN) indicherebbe
                // un problema hardware — il motor_start_ms_=0 sopra impedisce
                // comunque azioni indesiderate.
                transition_to(STATE_CLOSED);

            } else {
                // FC_EVT_OPEN: gradino completamente esteso.
                transition_to(STATE_OPEN);

                // Broadcast MSG_STEP_OPEN a tutti i nodi della mesh.
                // Motivo: l'HMI deve mostrare l'icona "scaletta aperta" e
                // può emettere un warning se KEY_ON è attivo contemporaneamente
                // (situazione pericolosa: camper in movimento con gradino fuori).
                StepOpenPayload p{};
                p.open_reason   = 0;               // 0 = apertura manuale
                p.key_on_active = key_on_active() ? 1 : 0;
                p.temperature   = temperature_;    // incluso per contestualizzare l'evento
                broadcast(MSG_STEP_OPEN, &p, sizeof(p));
            }

            send_status_update();
            continue;  // torna subito ad aspettare il prossimo evento
        }

        // ── Watchdog timeout motore ───────────────────────────────────────────
        //
        // Raggiunto solo se xQueueReceive ha fatto timeout (100 ms senza eventi).
        // Se il motore gira da più di MOTOR_TIMEOUT_MS senza finecorsa
        // → ostruzione meccanica, cablaggio aperto, finecorsa rotto.
        // Il nodo ferma il motore e transita in STATE_ERROR per segnalare
        // il problema all'HMI. Non esce da STATE_ERROR autonomamente:
        // serve intervento fisico + reset.
        if (state_ == STATE_OPENING || state_ == STATE_CLOSING ||
            state_ == STATE_AUTO_CLOSING) {

            uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - motor_start_ms_;

            // motor_start_ms_ == 0 significa che il motore non è in moto
            // (stop già avvenuto). Doppia guardia per evitare falsi timeout
            // se il watchdog viene raggiunto dopo uno stop.
            if (motor_start_ms_ > 0 && elapsed > MOTOR_TIMEOUT_MS) {
                motor_stop();
                last_move_ms_   = (uint16_t)elapsed;
                motor_start_ms_ = 0;
                error_code_     = ERR_TIMEOUT;
                transition_to(STATE_ERROR);
                send_status_update();
                ESP_LOGE(TAG, "timeout motore — STATE_ERROR");
            }
        }
    }
    vTaskDelete(nullptr);
}

// ── sensor_loop: lettura periodica SHT31 ────────────────────────────────────
//
// Task a bassa priorità (3): gira ogni SENSOR_PERIOD_MS (60 s).
// Aggiorna temperature_ e humidity_ che vengono incluse nel prossimo heartbeat.
// Se la lettura fallisce, i valori precedenti restano invariati (no NaN nel payload).

void StepNode::sensor_loop() {
    // Attesa iniziale di 500 ms: lascia stabilizzare l'alimentazione e
    // il bus I2C dopo il boot prima di tentare la prima lettura.
    vTaskDelay(pdMS_TO_TICKS(500));

    while (tasks_running_) {
        float t, h;
        if (sht31_read(t, h) == ESP_OK) {
            // Aggiorna le variabili di istanza solo se la lettura è valida.
            // Nessun mutex necessario: temperature_ e humidity_ sono float
            // scritti da un solo task (sensor_loop) e letti da build_status_payload
            // (contesto mesh). Su ESP32 le scritture a 32 bit sono atomiche.
            temperature_ = t;
            humidity_    = h;
            ESP_LOGD(TAG, "SHT31: %.1f°C  %.0f%%RH", t, h);
        } else {
            ESP_LOGW(TAG, "SHT31 lettura fallita");
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
    vTaskDelete(nullptr);
}

// ── Driver SHT31 (raw I2C, API IDF 5.x) ─────────────────────────────────────
//
// Usa la nuova API i2c_master_transmit / i2c_master_receive di IDF 5.x
// (sostituisce i2c_master_write/read deprecati di IDF 4.x).
//
// Protocollo SHT31 — modalità single-shot alta ripetibilità:
//   1. Invia comando 0x2C 0x06 (misura sincrona, clock stretching abilitato)
//   2. Attendi ~15 ms per la conversione
//   3. Leggi 6 byte: [T_MSB T_LSB T_CRC H_MSB H_LSB H_CRC]
//      (i byte CRC vengono ignorati — l'I2C con ACK è già sufficientemente affidabile)
//   4. Converti con le formule del datasheet SHT31

esp_err_t StepNode::sht31_read(float& temp_c, float& hum_pct) {
    const uint8_t cmd[2] = {0x2C, 0x06};  // single-shot, high repeatability
    esp_err_t err = i2c_master_transmit(sht31_dev_, cmd, sizeof(cmd), 100);
    if (err != ESP_OK) return err;  // timeout I2C o sensore non raggiungibile

    // Il SHT31 in modalità single-shot impiega circa 15 ms per la conversione.
    // 20 ms aggiunge un margine del 33% per variazioni di temperatura e alimentazione.
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6] = {};  // [T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC]
    err = i2c_master_receive(sht31_dev_, data, sizeof(data), 100);
    if (err != ESP_OK) return err;

    // Ricostruzione dei valori raw a 16 bit (big-endian)
    uint16_t t_raw = ((uint16_t)data[0] << 8) | data[1];
    uint16_t h_raw = ((uint16_t)data[3] << 8) | data[4];

    // Formule di conversione dal datasheet SHT31:
    //   T [°C]  = -45 + 175 × S_T / (2^16 - 1)
    //   RH [%]  = 100 × S_RH / (2^16 - 1)
    temp_c  = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    hum_pct =          100.0f * (float)h_raw / 65535.0f;
    return ESP_OK;
}

// ── Controllo motore H-bridge ────────────────────────────────────────────────
//
// Sequenza obbligatoria in motor_open() e motor_close():
//   1. EN = 0     — stacca il 12V_SW prima di cambiare direzione
//   2. DIR_x = 0  — azzera la direzione opposta (evita condizione invalida)
//   3. DIR_y = 1  — imposta la nuova direzione
//   4. EN = 1     — applica il 12V con direzione già stabilizzata
//
// Non c'è delayMicroseconds tra i passi: gpio_set_level() su IDF è più lento
// di digitalWrite() su Arduino (include un lock del driver) e la latenza
// intrinseca è sufficiente per i relay.

void StepNode::motor_open() {
    gpio_set_level(GPIO_HB_EN,    0);  // stacca 12V_SW
    gpio_set_level(GPIO_HB_DIR_B, 0);  // azzera direzione opposta
    gpio_set_level(GPIO_HB_DIR_A, 1);  // DIR_A=1, DIR_B=0 → MOT_A=+12V, MOT_B=GND
    gpio_set_level(GPIO_HB_EN,    1);  // applica 12V
    motor_start_ms_ = (uint32_t)(esp_timer_get_time() / 1000);  // avvia watchdog
}

void StepNode::motor_close() {
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);  // azzera direzione opposta
    gpio_set_level(GPIO_HB_DIR_B, 1);  // DIR_A=0, DIR_B=1 → MOT_A=GND, MOT_B=+12V
    gpio_set_level(GPIO_HB_EN,    1);
    motor_start_ms_ = (uint32_t)(esp_timer_get_time() / 1000);
}

void StepNode::motor_stop() {
    // Ordine inverso rispetto all'avvio: prima stacca il 12V, poi azzera le direzioni.
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);
    gpio_set_level(GPIO_HB_DIR_B, 0);
    // motor_start_ms_ NON viene azzerato qui: il chiamante (ctrl_loop) lo azzera
    // dopo aver registrato last_move_ms_, così la durata del movimento è disponibile.
}

// Aggiorna la variabile di stato e logga la transizione.
// Non notifica la mesh: è compito del chiamante inviare send_status_update().
void StepNode::transition_to(StepState s) {
    state_ = s;
    ESP_LOGI(TAG, "stato: %d", (int)s);
}

// ── Hook NodeBase ────────────────────────────────────────────────────────────
//
// NodeBase (classe base) chiama questi metodi virtuali quando arrivano
// messaggi mesh rilevanti. Vengono eseguiti nel contesto del task mesh,
// non in ctrl_loop — per questo non usano la coda ma modificano direttamente
// lo stato (le variabili volatile garantiscono la visibilità tra task).

const NodeDescriptor& StepNode::descriptor() const {
    return kDesc;
}

// Impacchetta lo stato corrente in StepStatus e lo copia nel buffer `out`.
// Chiamato da NodeBase ogni volta che deve inviare un MSG_HEARTBEAT o MSG_STATUS.
size_t StepNode::build_status_payload(uint8_t* out, size_t max) {
    if (max < sizeof(StepStatus)) return 0;
    StepStatus s{};
    s.state        = (uint8_t)state_;
    // ⚠ active-LOW: == 0 significa finecorsa attivo. Vedi nota all'inizio del file.
    s.fc_closed    = gpio_get_level(GPIO_FC_CLOSED) == 0 ? 1 : 0;
    s.fc_open      = gpio_get_level(GPIO_FC_OPEN)   == 0 ? 1 : 0;
    s.error_code   = error_code_;
    s.last_move_ms = last_move_ms_;
    s.temperature  = temperature_;
    s.humidity     = humidity_;
    std::memcpy(out, &s, sizeof(s));
    return sizeof(s);
}

// Gestisce i comandi manuali inviati dall'HMI via MSG_COMMAND.
// Le guardie sullo stato evitano transizioni invalide (es. OPEN da uno stato
// che non sia CLOSED): il nodo ignora comandi prematuri o duplicati.
void StepNode::on_command(const CmdPayload& cmd, uint8_t /*seq*/) {
    switch (cmd.action_code) {

    case ACTION_OPEN:
        // Apertura consentita solo da STATE_CLOSED.
        // Se il gradino è già in moto o in errore, il comando viene ignorato.
        if (state_ == STATE_CLOSED) {
            transition_to(STATE_OPENING);
            motor_open();
        }
        break;

    case ACTION_CLOSE:
        // Chiusura consentita solo da STATE_OPEN per la stessa ragione.
        if (state_ == STATE_OPEN) {
            transition_to(STATE_CLOSING);
            motor_close();
        }
        break;

    case ACTION_GET_STATUS:
        // Risposta immediata: non aspetta il prossimo heartbeat periodico.
        send_status_update();
        break;

    default:
        break;
    }
}

// Reagisce al broadcast MSG_KEY_ON inviato dal ROOT quando la chiave del camper
// viene inserita. Se il gradino è aperto o in apertura, lo chiude immediatamente.
// Usa AUTO_CLOSING (non CLOSING) per distinguere la chiusura automatica da quella
// manuale: l'HMI mostra un indicatore diverso e l'operatore sa che il sistema
// sta reagendo a un evento di sicurezza, non a un suo comando.
void StepNode::on_key_on(const KeyOnPayload& /*p*/) {
    if (state_ == STATE_OPEN || state_ == STATE_OPENING) {
        transition_to(STATE_AUTO_CLOSING);
        motor_close();
        ESP_LOGI(TAG, "KEY_ON: chiusura automatica gradino");
    }
}

// Chiamato da NodeBase quando la mesh non risponde da > 30 s.
// Il nodo entra in STANDALONE: continua a funzionare localmente
// (i finecorsa e il watchdog rimangono attivi), ma smette di inviare heartbeat.
// Non entra in STANDALONE se è già in STATE_ERROR: l'errore hardware ha
// priorità sulla segnalazione di rete.
void StepNode::on_standalone_enter() {
    if (state_ != STATE_ERROR)
        transition_to(STATE_STANDALONE);
}

// Chiamato da NodeBase quando la mesh si ristabilisce dopo uno standalone.
// Ri-legge i finecorsa fisici per sincronizzare lo stato con la realtà hardware
// (il gradino potrebbe essere stato mosso manualmente durante il periodo offline).
// Invia subito un aggiornamento di stato per sincronizzare il ROOT e l'HMI.
void StepNode::on_standalone_exit() {
    bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0;  // ⚠ active-LOW legacy
    bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;
    if (fc_c)      transition_to(STATE_CLOSED);
    else if (fc_o) transition_to(STATE_OPEN);
    else           transition_to(STATE_INITIALIZING);  // posizione intermedia — attende comando
    send_status_update();
}

} // namespace domoc
