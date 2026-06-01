// ═══════════════════════════════════════════════════════════════════════════════
// step.cpp — Nodo STEP (Arduino + painlessMesh)
//
// Controlla il gradino motorizzato bidirezionale del camper.
// Il gradino è mosso da un motore DC tramite H-bridge a relay (PCB Universale v3.0).
// Due finecorsa NC (normally closed) segnalano le posizioni limite: CHIUSO e APERTO.
// Un sensore SHT31 misura temperatura e umidità esterna (montato sul gradino stesso).
//
// Principio architetturale: la logica di sicurezza vive in questo nodo.
// Se il MASTER o l'HMI sono offline, il nodo continua a funzionare in autonomia:
//   - i finecorsa fermano il motore indipendentemente dalla mesh
//   - il timeout hardware ferma il motore se i finecorsa non arrivano
//   - MSG_KEY_ON chiude automaticamente il gradino se la chiave viene inserita
//
// Librerie: painlessMesh, FastLED, Adafruit SHT31 + Adafruit BusIO
// Board: ESP32S3 Dev Module — Partition: Huge APP
// ═══════════════════════════════════════════════════════════════════════════════

// PlatformIO in modalità monorepo non aggiunge Arduino.h automaticamente ai .cpp
#include <Arduino.h>
#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "../../arduino_lib/domoc/DomocMotor.h"
#include "config.h"
#include "sensors.h"

// ── Macchina a stati ─────────────────────────────────────────────────────────
//
// Il nodo è modellato come una macchina a stati esplicita.
// Ogni transizione avviene tramite enter_state() che aggiorna LED e invia
// lo stato aggiornato via mesh.
//
// Grafo delle transizioni:
//
//   INITIALIZING ──(setup finecorsa)──→ CLOSED o OPEN
//
//   CLOSED ──(ACTION_OPEN)──────────────→ OPENING
//   OPENING ──(fc_open attivato)────────→ OPEN
//   OPENING ──(timeout motore)──────────→ ERROR
//
//   OPEN ──(ACTION_CLOSE)───────────────→ CLOSING
//   OPEN ──(MSG_KEY_ON)─────────────────→ AUTO_CLOSING
//   CLOSING ──(fc_closed attivato)──────→ CLOSED
//   CLOSING ──(timeout motore)──────────→ ERROR
//   AUTO_CLOSING ──(fc_closed attivato)─→ CLOSED
//   AUTO_CLOSING ──(timeout motore)─────→ ERROR
//
//   OPENING/CLOSING ──(MSG_KEY_ON)──────→ AUTO_CLOSING  (interrompe apertura)
//
//   Qualsiasi ──(mesh persa > 30s)──────→ STANDALONE
//   STANDALONE ──(mesh recuperata)──────→ stato precedente
enum StepState : uint8_t {
    STATE_INITIALIZING = 0,  // boot: stato non ancora determinato dai finecorsa
    STATE_CLOSED       = 1,  // gradino completamente ritirato, fc_closed attivo
    STATE_OPEN         = 2,  // gradino completamente esteso, fc_open attivo
    STATE_OPENING      = 3,  // motore in marcia verso OPEN, in attesa di fc_open
    STATE_CLOSING      = 4,  // motore in marcia verso CLOSED, in attesa di fc_closed
    STATE_AUTO_CLOSING = 5,  // chiusura automatica su KEY_ON (non interrompibile dall'HMI)
    STATE_ERROR        = 6,  // timeout motore: finecorsa non raggiunto entro MOTOR_TIMEOUT_MS
    STATE_STANDALONE   = 7,  // mesh assente da > 30 s: nodo in autonomia locale
};

// ── Payload di stato mesh ────────────────────────────────────────────────────
//
// Inviato come payload di MSG_HEARTBEAT e MSG_STATUS al ROOT ogni 5 s.
// Il ROOT lo forwarda all'HMI che aggiorna la UI.
// Gli offset dei campi devono corrispondere ai PropertyDesc nel descriptor.
#pragma pack(push, 1)
typedef struct {
    uint8_t  state;         // offset 0  — StepState corrente
    uint8_t  fc_closed;     // offset 1  — 1 se il finecorsa CLOSED è attivo (pin HIGH)
    uint8_t  fc_open;       // offset 2  — 1 se il finecorsa OPEN è attivo (pin HIGH)
    uint8_t  error_code;    // offset 3  — DOMOC_ERR_NONE o DOMOC_ERR_TIMEOUT
    uint16_t last_move_ms;  // offset 4  — durata dell'ultimo movimento in ms (debug)
    uint8_t  _pad;          // offset 6  — allineamento a 4 byte per i float seguenti
    float    temperature;   // offset 7  — temperatura esterna in °C (SHT31)
    float    humidity;      // offset 11 — umidità relativa in %RH (SHT31)
} StepStatus;               // 15 byte totali
#pragma pack(pop)

// ── Descriptor HMI ───────────────────────────────────────────────────────────
//
// Struttura statica (140 byte) inviata al ROOT subito dopo MSG_REGISTER_ACK.
// Il ROOT la forwarda all'HMI che costruisce l'interfaccia grafica senza
// avere conoscenza hardcoded di questo nodo.
//
// Azioni dichiarate:
//   - APRI / CHIUDI: pulsanti di controllo manuale
//     FLAG_KEY_BLOCKED = queste azioni vengono disabilitate dall'HMI quando
//     KEY_ON è attivo (motore acceso → non è sicuro aprire il gradino)
//   - INFO: richiede un aggiornamento dello stato attuale
//
// Proprietà dichiarate:
//   - PROP_STATE:       mostra lo stato come stringa (es. "CLOSED", "OPENING")
//   - PROP_TEMPERATURE: widget termometro, range 15–40 °C
//   - PROP_HUMIDITY:    widget progress bar, range 0–100 %RH
//     (range_min/max nel descriptor sono ×10, quindi 0→0, 1000→100.0)
static const NodeDescriptor STEP_DESC = {
    .node_icon      = ICON_STEP,
    .action_count   = 3,
    .property_count = 3,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,       ICON_ACT_OPEN,  CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  CTRL_BUTTON, 0, 0, 0,               "INFO"   },
    },
    .properties = {
        // offset 0 = state (uint8), offset 7 = temperature (float), offset 11 = humidity (float)
        { PROP_STATE,       0,  PAYLOAD_UINT8,   WIDGET_LABEL,       0,   0,    "",   "%s"   },
        { PROP_TEMPERATURE, 7,  PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 150, 400,  "°C", "%.1f" },
        { PROP_HUMIDITY,    11, PAYLOAD_FLOAT32, WIDGET_PROGRESS,    0,   1000, "%",  "%.0f" },
    },
};

// ── Oggetti globali ──────────────────────────────────────────────────────────
//
// DomocMesh: gestisce registrazione, heartbeat, standalone, CRC, codifica hex.
//   Parametri: ID logico (0x02), nome nodo, tipo nodo.
//
// DomocMotor: driver H-bridge con sequenza di sicurezza ENABLE-OFF → DIR → ENABLE-ON.
//   Parametri: GPIO DIR_A, DIR_B, ENABLE.
//
// DomocLed: mappa ogni LedState su colore + pattern di lampeggio per il WS2812B.
static DomocMesh  mesh(NODE_ID_STEP, "STEP", 0x02);
static DomocMotor motor(PIN_HB_DIR_A, PIN_HB_DIR_B, PIN_HB_EN);
static CRGB       led_buf[1];
static DomocLed   led(led_buf);

// ── Stato runtime ────────────────────────────────────────────────────────────
static StepState  state       = STATE_INITIALIZING;
static float      temperature = 0.0f;
static float      humidity    = 0.0f;

// Valori precedenti dei finecorsa per il rilevamento del fronte di salita.
// Il debounce è temporale: si accetta solo una transizione ogni FC_DEBOUNCE_MS.
static bool     fc_closed_prev = false;
static bool     fc_open_prev   = false;
static uint32_t last_fc_ms     = 0;    // timestamp dell'ultimo evento finecorsa accettato

static uint32_t last_sht31_ms  = 0;   // timestamp dell'ultima lettura SHT31

// ── Invio stato mesh ─────────────────────────────────────────────────────────
//
// Impacchetta lo stato corrente in StepStatus e lo invia come heartbeat.
// Chiamato:
//   - da DomocMesh ogni 5 s (callback set_on_heartbeat_due)
//   - da enter_state() a ogni transizione (notifica immediata all'HMI)
//   - in risposta a ACTION_GET_STATUS
static void send_status() {
    StepStatus s{};
    s.state       = (uint8_t)state;
    // I finecorsa sono NC con pull-up: HIGH = switch aperto = posizione raggiunta.
    s.fc_closed   = digitalRead(PIN_FC_CLOSED) ? 1 : 0;
    s.fc_open     = digitalRead(PIN_FC_OPEN)   ? 1 : 0;
    s.error_code  = (state == STATE_ERROR) ? DOMOC_ERR_TIMEOUT : DOMOC_ERR_NONE;
    s.last_move_ms= motor.last_move_ms();
    s.temperature = temperature;
    s.humidity    = humidity;
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

// ── Transizione di stato ─────────────────────────────────────────────────────
//
// Punto di ingresso unico per tutte le transizioni di stato.
// Aggiorna il LED e invia immediatamente lo stato aggiornato via mesh,
// così l'HMI riflette il cambio di stato in tempo reale senza aspettare
// il prossimo heartbeat periodico.
//
// Il broadcast MSG_STEP_OPEN viene inviato solo all'ingresso di STATE_OPEN:
// gli altri nodi (es. HMI) possono reagire (mostrare icona scaletta aperta,
// avvisare se KEY_ON è attivo contemporaneamente).
static void enter_state(StepState next) {
    state = next;

    switch (next) {
        case STATE_CLOSED:
            led.set(LedState::OK_CLOSED);
            break;

        case STATE_OPEN:
            led.set(LedState::OK_OPEN);
            // Notifica broadcast: tutti i nodi sanno che la scaletta è aperta.
            // Il payload include il flag KEY_ON così l'HMI può mostrare
            // un warning se la scaletta è aperta mentre il motore è acceso.
            {
                uint8_t pl[3] = { 0, mesh.key_on() ? 1u : 0u, 0 };
                mesh.broadcast(MSG_STEP_OPEN, pl, 3);
            }
            break;

        case STATE_OPENING:      led.set(LedState::MOVING_OPEN);   break;
        case STATE_CLOSING:      led.set(LedState::MOVING_CLOSE);  break;
        case STATE_AUTO_CLOSING: led.set(LedState::AUTO_CLOSING);  break;
        case STATE_ERROR:        led.set(LedState::ERROR);         break;
        case STATE_STANDALONE:   led.set(LedState::STANDALONE);    break;
        default:                 led.set(LedState::INITIALIZING);  break;
    }

    send_status();
}

// ── Polling finecorsa ────────────────────────────────────────────────────────
//
// Chiamato ogni ciclo di loop(). Rileva il fronte di salita (LOW→HIGH)
// dei finecorsa con debounce temporale.
//
// Perché fronte e non livello:
//   Rilevare il livello HIGH causerebbe chiamate ripetute a motor.stop()
//   e enter_state() finché il finecorsa rimane attivo. Il fronte garantisce
//   una sola transizione per evento di arresto.
//
// Perché debounce temporale e non per campionamento multiplo:
//   Il debounce per campionamento (N letture consecutive uguali) aggiunge
//   latenza proporzionale al periodo di campionamento. Con FC_DEBOUNCE_MS=50
//   la latenza è fissa e trascurabile per questo tipo di attuatore.
static void check_endstops() {
    bool fc_c = digitalRead(PIN_FC_CLOSED);  // HIGH = scaletta in posizione CLOSED
    bool fc_o = digitalRead(PIN_FC_OPEN);    // HIGH = scaletta in posizione OPEN
    uint32_t now = millis();

    // Fronte di salita su fc_closed + debounce superato
    if (fc_c && !fc_closed_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        // Il finecorsa CLOSED è rilevante solo se il motore stava chiudendo.
        // In qualsiasi altro stato (es. STATE_OPEN) ignorare questo segnale
        // evita transizioni spurie causate da rimbalzi meccanici.
        if (state == STATE_CLOSING || state == STATE_AUTO_CLOSING) {
            motor.stop();
            enter_state(STATE_CLOSED);
        }
    }

    // Fronte di salita su fc_open + debounce superato
    if (fc_o && !fc_open_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (state == STATE_OPENING) {
            motor.stop();
            enter_state(STATE_OPEN);
        }
    }

    // Aggiorna i valori precedenti per rilevare il prossimo fronte
    fc_closed_prev = fc_c;
    fc_open_prev   = fc_o;
}

// ── Watchdog timeout motore ───────────────────────────────────────────────────
//
// Se il motore gira per più di MOTOR_TIMEOUT_MS senza che nessun finecorsa
// si attivi, significa che qualcosa è andato storto (ostruzione meccanica,
// finecorsa rotto, cablaggio aperto). Il nodo ferma il motore e transita
// in STATE_ERROR per segnalare il problema all'HMI.
//
// Il sistema non esce autonomamente da STATE_ERROR: serve un comando esplicito
// dall'HMI (o un reboot) dopo che l'operatore ha verificato il problema fisico.
static void check_motor_timeout() {
    if (!motor.running()) return;  // nessun timeout se il motore è fermo
    if (millis() - motor.start_ms() > MOTOR_TIMEOUT_MS) {
        motor.stop();
        enter_state(STATE_ERROR);
    }
}

// ── Ricezione messaggi mesh ───────────────────────────────────────────────────
//
// Callback registrata in DomocMesh. Viene chiamata dal loop di painlessMesh
// ogni volta che arriva un messaggio indirizzato a questo nodo (dst_id=0x02
// o broadcast dst_id=0xFF).
static void on_message(const DomocMsg* msg, size_t) {
    switch (msg->msg_type) {

        // Il ROOT ha confermato la registrazione e assegnato l'ID logico.
        // Questo è il momento giusto per inviare il descriptor all'HMI:
        // prima di questo punto il nodo non è ancora nel registry del ROOT.
        case MSG_REGISTER_ACK:
            mesh.send_to_root(MSG_DESCRIPTOR,
                              (const uint8_t*)&STEP_DESC, sizeof(STEP_DESC));
            enter_state(state);  // ri-invia lo stato corrente per sincronizzare il ROOT
            break;

        // La chiave di accensione del camper è stata inserita.
        // Regola di sicurezza: il gradino deve essere ritirato prima di muoversi.
        // Se è aperto o in apertura → inversione immediata verso la chiusura.
        // Se è già in chiusura o chiuso → nessuna azione (già sicuro).
        case MSG_KEY_ON:
            if (state == STATE_OPEN || state == STATE_OPENING) {
                motor.stop();                    // ferma eventuale movimento in corso
                motor.start(MotorDir::CLOSE);    // inverti verso chiusura
                enter_state(STATE_AUTO_CLOSING); // stato speciale: non interrompibile dall'HMI
            }
            break;

        // Comando manuale dall'HMI.
        case MSG_COMMAND: {
            const CmdPayload* cmd = (const CmdPayload*)msg->payload;

            // Con KEY_ON attivo blocca tutti i comandi manuali tranne GET_STATUS.
            // La chiusura automatica (AUTO_CLOSING) non deve essere interrotta
            // dall'operatore mentre il camper si sta mettendo in moto.
            if (mesh.key_on() && cmd->action_code != ACTION_GET_STATUS) break;

            if (cmd->action_code == ACTION_OPEN && state == STATE_CLOSED) {
                // Apertura consentita solo da STATE_CLOSED: evita comandi doppi
                // o apertura durante un movimento già in corso.
                motor.start(MotorDir::OPEN);
                enter_state(STATE_OPENING);

            } else if (cmd->action_code == ACTION_CLOSE && state == STATE_OPEN) {
                // Chiusura consentita solo da STATE_OPEN per la stessa ragione.
                motor.start(MotorDir::CLOSE);
                enter_state(STATE_CLOSING);

            } else if (cmd->action_code == ACTION_GET_STATUS) {
                // Risposta immediata allo stato attuale senza aspettare il prossimo heartbeat.
                send_status();
            }
            break;
        }

        default: break;
    }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // LED: FastLED gestisce il timing del protocollo WS2812B via GPIO21.
    // Luminosità al 40% (range 0-255): sufficiente per la visibilità in interno
    // senza abbagliare in ambienti bui del camper.
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(40);

    // Motor + finecorsa
    motor.begin();
    // INPUT_PULLUP: il PCB ha optoisolatori con pull-up hardware, ma il pull-up
    // software sul GPIO dell'ESP32 aggiunge ridondanza se l'optoisolatore
    // non è alimentato durante il boot.
    pinMode(PIN_FC_CLOSED, INPUT_PULLUP);
    pinMode(PIN_FC_OPEN,   INPUT_PULLUP);

    if (!sensors_init()) Serial.println("SHT31 non trovato");

    // Determina lo stato iniziale leggendo i finecorsa fisici.
    // Finecorsa NC con pull-up: HIGH = posizione raggiunta.
    // Se il gradino è in posizione intermedia all'accensione (es. dopo un reboot
    // durante un movimento), assume STATE_OPEN come stato conservativo
    // (il nodo non muoverà nulla fino al prossimo comando o KEY_ON).
    state = digitalRead(PIN_FC_CLOSED) ? STATE_CLOSED : STATE_OPEN;

    // Registra le callback in DomocMesh prima di chiamare begin().
    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.set_on_standalone_enter([]() { enter_state(STATE_STANDALONE); });
    // All'uscita dallo standalone il nodo si ri-registra al ROOT e
    // ri-invia descriptor e stato corrente (gestito da on_message REGISTER_ACK).
    mesh.set_on_standalone_exit([]()  { enter_state(state); });

    // Avvia la mesh: connessione al ROOT, registrazione, heartbeat periodico.
    mesh.begin();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
//
// Il loop è non-bloccante: nessuna delay() o attesa attiva.
// Ogni funzione controlla il proprio timer interno con millis().
//
// Ordine di esecuzione:
//   1. mesh.update()        — processa i pacchetti ricevuti, invia heartbeat se dovuto
//   2. check_endstops()     — rileva arrivo ai finecorsa e ferma il motore
//   3. check_motor_timeout()— ferma il motore se gira da troppo tempo
//   4. lettura SHT31        — ogni SHT31_INTERVAL_MS (60 s)
//   5. led.update()         — aggiorna il pattern di lampeggio LED
void loop() {
    mesh.update();

    uint32_t now = millis();

    check_endstops();
    check_motor_timeout();

    // Lettura periodica del sensore ambientale.
    // La variabile temperature/humidity viene aggiornata solo se la lettura
    // è valida (no NaN): in caso di errore temporaneo del sensore il nodo
    // continua a trasmettere l'ultimo valore valido.
    if (now - last_sht31_ms >= SHT31_INTERVAL_MS) {
        last_sht31_ms = now;
        sensors_read(temperature, humidity);
    }

    led.update();
}
