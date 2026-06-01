// DomoC — Nodo STEP (Arduino + painlessMesh)
//
// Controlla il gradino motorizzato bidirezionale (H-bridge relay).
// Sensore SHT31: temperatura e umidità esterna.
//
// Librerie: painlessMesh, FastLED, Adafruit SHT31 + Adafruit BusIO
// Board: ESP32S3 Dev Module — Partition: Huge APP

#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "../../arduino_lib/domoc/DomocMotor.h"
#include "config.h"
#include "sensors.h"

// ── Stato ────────────────────────────────────────────────────────────────────
enum StepState : uint8_t {
    STATE_INITIALIZING = 0,
    STATE_CLOSED       = 1,
    STATE_OPEN         = 2,
    STATE_OPENING      = 3,
    STATE_CLOSING      = 4,
    STATE_AUTO_CLOSING = 5,
    STATE_ERROR        = 6,
    STATE_STANDALONE   = 7,
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  state;         // offset 0
    uint8_t  fc_closed;     // offset 1
    uint8_t  fc_open;       // offset 2
    uint8_t  error_code;    // offset 3
    uint16_t last_move_ms;  // offset 4
    uint8_t  _pad;          // offset 6
    float    temperature;   // offset 7
    float    humidity;      // offset 11
} StepStatus;               // 15 byte
#pragma pack(pop)

// ── Descriptor HMI ───────────────────────────────────────────────────────────
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
        { PROP_STATE,       0,  PAYLOAD_UINT8,   WIDGET_LABEL,       0,   0,    "",   "%s"   },
        { PROP_TEMPERATURE, 7,  PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 150, 400,  "°C", "%.1f" },
        { PROP_HUMIDITY,    11, PAYLOAD_FLOAT32, WIDGET_PROGRESS,    0,   1000, "%",  "%.0f" },
    },
};

// ── Oggetti ──────────────────────────────────────────────────────────────────
static DomocMesh  mesh(NODE_ID_STEP, "STEP", 0x02);
static DomocMotor motor(PIN_HB_DIR_A, PIN_HB_DIR_B, PIN_HB_EN);
static CRGB       led_buf[1];
static DomocLed   led(led_buf);
static StepState  state = STATE_INITIALIZING;
static float      temperature = 0.0f, humidity = 0.0f;

// Finecorsa (polling con debounce)
static bool     fc_closed_prev = false, fc_open_prev = false;
static uint32_t last_fc_ms     = 0;
static uint32_t last_sht31_ms  = 0;

// ── Invio stato ──────────────────────────────────────────────────────────────
static void send_status() {
    StepStatus s{};
    s.state       = (uint8_t)state;
    s.fc_closed   = !digitalRead(PIN_FC_CLOSED) ? 1 : 0;
    s.fc_open     = !digitalRead(PIN_FC_OPEN)   ? 1 : 0;
    s.error_code  = (state == STATE_ERROR) ? ERR_TIMEOUT : ERR_NONE;
    s.last_move_ms= motor.last_move_ms();
    s.temperature = temperature;
    s.humidity    = humidity;
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

// ── Transizione di stato ─────────────────────────────────────────────────────
static void enter_state(StepState next) {
    state = next;

    switch (next) {
        case STATE_CLOSED:
            led.set(LedState::OK_CLOSED);
            break;
        case STATE_OPEN:
            led.set(LedState::OK_OPEN);
            // Broadcast: scaletta aperta
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

// ── Finecorsa ────────────────────────────────────────────────────────────────
static void check_endstops() {
    bool fc_c = !digitalRead(PIN_FC_CLOSED);
    bool fc_o = !digitalRead(PIN_FC_OPEN);
    uint32_t now = millis();

    if (fc_c && !fc_closed_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (state == STATE_CLOSING || state == STATE_AUTO_CLOSING) {
            motor.stop();
            enter_state(STATE_CLOSED);
        }
    }
    if (fc_o && !fc_open_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (state == STATE_OPENING) {
            motor.stop();
            enter_state(STATE_OPEN);
        }
    }
    fc_closed_prev = fc_c;
    fc_open_prev   = fc_o;
}

// ── Timeout motore ────────────────────────────────────────────────────────────
static void check_motor_timeout() {
    if (!motor.running()) return;
    if (millis() - motor.start_ms() > MOTOR_TIMEOUT_MS) {
        motor.stop();
        enter_state(STATE_ERROR);
    }
}

// ── Messaggi mesh ────────────────────────────────────────────────────────────
static void on_message(const DomocMsg* msg, size_t) {
    switch (msg->msg_type) {

        case MSG_REGISTER_ACK:
            // Invia descriptor all'HMI tramite ROOT
            mesh.send_to_root(MSG_DESCRIPTOR,
                              (const uint8_t*)&STEP_DESC, sizeof(STEP_DESC));
            enter_state(state);  // aggiorna status nel ROOT
            break;

        case MSG_KEY_ON:
            if (state == STATE_OPEN || state == STATE_OPENING) {
                motor.stop();
                motor.start(MotorDir::CLOSE);
                enter_state(STATE_AUTO_CLOSING);
            }
            break;

        case MSG_COMMAND: {
            const CmdPayload* cmd = (const CmdPayload*)msg->payload;
            if (mesh.key_on() && cmd->action_code != ACTION_GET_STATUS) break;

            if (cmd->action_code == ACTION_OPEN && state == STATE_CLOSED) {
                motor.start(MotorDir::OPEN);
                enter_state(STATE_OPENING);
            } else if (cmd->action_code == ACTION_CLOSE && state == STATE_OPEN) {
                motor.start(MotorDir::CLOSE);
                enter_state(STATE_CLOSING);
            } else if (cmd->action_code == ACTION_GET_STATUS) {
                send_status();
            }
            break;
        }

        default: break;
    }
}

// ── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(40);

    motor.begin();
    pinMode(PIN_FC_CLOSED, INPUT_PULLUP);
    pinMode(PIN_FC_OPEN,   INPUT_PULLUP);

    if (!sensors_init()) Serial.println("SHT31 non trovato");

    // Stato iniziale dai finecorsa
    state = !digitalRead(PIN_FC_CLOSED) ? STATE_CLOSED : STATE_OPEN;

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.set_on_standalone_enter([]() { enter_state(STATE_STANDALONE); });
    mesh.set_on_standalone_exit([]()  { enter_state(state); });
    mesh.begin();
}

void loop() {
    mesh.update();
    uint32_t now = millis();

    check_endstops();
    check_motor_timeout();

    if (now - last_sht31_ms >= SHT31_INTERVAL_MS) {
        last_sht31_ms = now;
        sensors_read(temperature, humidity);
    }

    led.update();
}
