// DomoC — Nodo FRONT_DOOR (Arduino + painlessMesh)
//
// Porta di ingresso motorizzata bidirezionale (H-bridge relay + finecorsa).
// Librerie: painlessMesh, FastLED
// Board: ESP32S3 Dev Module

#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "../../arduino_lib/domoc/DomocMotor.h"
#include "config.h"

enum DoorState : uint8_t {
    DOOR_CLOSED   = 0,
    DOOR_OPEN     = 1,
    DOOR_OPENING  = 2,
    DOOR_CLOSING  = 3,
    DOOR_ERROR    = 4,
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  state;
    uint8_t  fc_closed;
    uint8_t  fc_open;
    uint8_t  error_code;
    uint16_t last_move_ms;
} DoorStatus;  // 6 byte
#pragma pack(pop)

static const NodeDescriptor DOOR_DESC = {
    .node_icon      = ICON_DOOR,
    .action_count   = 3,
    .property_count = 1,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,       ICON_ACT_OPEN,  CTRL_BUTTON,  0, 0, FLAG_KEY_BLOCKED, "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, CTRL_BUTTON,  0, 0, FLAG_KEY_BLOCKED, "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  CTRL_BUTTON,  0, 0, 0,               "INFO"   },
    },
    .properties = {
        { PROP_DOOR_OPEN, 0, PAYLOAD_UINT8, WIDGET_INDICATOR, 0, 1, "", "%d" },
    },
};

static DomocMesh  mesh(NODE_ID_FRONT_DOOR, "FRONT_DOOR", 0x09);
static DomocMotor motor(PIN_HB_DIR_A, PIN_HB_DIR_B, PIN_HB_EN);
static CRGB       led_buf[1];
static DomocLed   led(led_buf);
static DoorState  door_state = DOOR_CLOSED;
static bool       fc_closed_prev = false, fc_open_prev = false;
static uint32_t   last_fc_ms = 0;

static void send_status() {
    DoorStatus s{};
    s.state       = (uint8_t)door_state;
    s.fc_closed   = !digitalRead(PIN_FC_CLOSED) ? 1 : 0;
    s.fc_open     = !digitalRead(PIN_FC_OPEN)   ? 1 : 0;
    s.error_code  = (door_state == DOOR_ERROR) ? ERR_TIMEOUT : ERR_NONE;
    s.last_move_ms= motor.last_move_ms();
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

static void enter_door(DoorState next) {
    door_state = next;
    switch (next) {
        case DOOR_CLOSED:  led.set(LedState::OK_CLOSED);    break;
        case DOOR_OPEN:    led.set(LedState::OK_OPEN);      break;
        case DOOR_OPENING: led.set(LedState::MOVING_OPEN);  break;
        case DOOR_CLOSING: led.set(LedState::MOVING_CLOSE); break;
        case DOOR_ERROR:   led.set(LedState::ERROR);        break;
    }
    send_status();
}

static void check_endstops() {
    bool fc_c = !digitalRead(PIN_FC_CLOSED);
    bool fc_o = !digitalRead(PIN_FC_OPEN);
    uint32_t now = millis();
    if (fc_c && !fc_closed_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (door_state == DOOR_CLOSING) { motor.stop(); enter_door(DOOR_CLOSED); }
    }
    if (fc_o && !fc_open_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (door_state == DOOR_OPENING) { motor.stop(); enter_door(DOOR_OPEN); }
    }
    fc_closed_prev = fc_c;
    fc_open_prev   = fc_o;
}

static void check_motor_timeout() {
    if (!motor.running()) return;
    if (millis() - motor.start_ms() > MOTOR_TIMEOUT_MS) {
        motor.stop();
        enter_door(DOOR_ERROR);
    }
}

static void on_message(const DomocMsg* msg, size_t) {
    if (msg->msg_type == MSG_REGISTER_ACK) {
        mesh.send_to_root(MSG_DESCRIPTOR, (const uint8_t*)&DOOR_DESC, sizeof(DOOR_DESC));
        send_status();
        return;
    }
    if (msg->msg_type != MSG_COMMAND) return;
    const CmdPayload* cmd = (const CmdPayload*)msg->payload;
    if (mesh.key_on() && cmd->action_code != ACTION_GET_STATUS) return;
    switch (cmd->action_code) {
        case ACTION_OPEN:
            if (door_state == DOOR_CLOSED) { motor.start(MotorDir::OPEN);  enter_door(DOOR_OPENING); }
            break;
        case ACTION_CLOSE:
            if (door_state == DOOR_OPEN)   { motor.start(MotorDir::CLOSE); enter_door(DOOR_CLOSING); }
            break;
        case ACTION_GET_STATUS:
            send_status();
            break;
    }
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(40);
    motor.begin();
    pinMode(PIN_FC_CLOSED, INPUT_PULLUP);
    pinMode(PIN_FC_OPEN,   INPUT_PULLUP);
    door_state = !digitalRead(PIN_FC_CLOSED) ? DOOR_CLOSED : DOOR_OPEN;
    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.begin();
}

void loop() {
    mesh.update();
    check_endstops();
    check_motor_timeout();
    led.update();
}
