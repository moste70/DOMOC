// DomoC — Nodo GREY_WATER (Arduino + painlessMesh)
//
// Controlla la valvola acque grigie (H-bridge relay + finecorsa),
// la telecamera del portellone (relay REL1) e legge la batteria servizio (ADC).
//
// Librerie: painlessMesh, FastLED
// Board: ESP32S3 Dev Module

#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "../../arduino_lib/domoc/DomocMotor.h"
#include "config.h"

enum ValveState : uint8_t {
    VALVE_CLOSED   = 0,
    VALVE_OPEN     = 1,
    VALVE_OPENING  = 2,
    VALVE_CLOSING  = 3,
    VALVE_ERROR    = 4,
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  state;         // offset 0
    uint8_t  fc_closed;     // offset 1
    uint8_t  fc_open;       // offset 2
    uint8_t  cam_on;        // offset 3
    uint8_t  error_code;    // offset 4
    float    vbat_service;  // offset 5
} GreyWaterStatus;          // 9 byte
#pragma pack(pop)

static const NodeDescriptor GW_DESC = {
    .node_icon      = ICON_VALVE_GREY,
    .action_count   = 4,
    .property_count = 3,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,    ICON_ACT_OPEN,    CTRL_BUTTON, 0, 0, 0, "APRI"   },
        { ACTION_CLOSE,   ICON_ACT_CLOSE,   CTRL_BUTTON, 0, 0, 0, "CHIUDI" },
        { ACTION_CAM_ON,  ICON_ACT_CAM_ON,  CTRL_TOGGLE, 0, 0, 0, "CAM ON" },
        { ACTION_CAM_OFF, ICON_ACT_CAM_OFF, CTRL_TOGGLE, 0, 0, 0, "CAM OFF"},
    },
    .properties = {
        { PROP_STATE,    0, PAYLOAD_UINT8,   WIDGET_LABEL,   0, 0,    "",  "%d"   },
        { PROP_CAM_ON,   3, PAYLOAD_UINT8,   WIDGET_INDICATOR,0,1,   "",  "%d"   },
        { PROP_BATTERY_V,5, PAYLOAD_FLOAT32, WIDGET_BATTERY, 100,150,"V", "%.1f" },
    },
};

static DomocMesh  mesh(NODE_ID_GREY_WATER, "GREY_WATER", 0x03);
static DomocMotor motor(PIN_HB_DIR_A, PIN_HB_DIR_B, PIN_HB_EN);
static CRGB       led_buf[1];
static DomocLed   led(led_buf);

static ValveState valve_state = VALVE_CLOSED;
static bool       cam_on      = false;
static float      vbat_svc    = 0.0f;
static uint32_t   last_vbat_ms = 0;
static bool       fc_closed_prev = false, fc_open_prev = false;
static uint32_t   last_fc_ms  = 0;

static void read_vbat() {
    int raw = analogRead(PIN_ADC_VBAT_SVC);
    vbat_svc = (raw / ADC_RESOLUTION) * ADC_VREF * ADC_VBAT_FACTOR;
}

static void send_status() {
    GreyWaterStatus s{};
    s.state       = (uint8_t)valve_state;
    s.fc_closed   = !digitalRead(PIN_FC_CLOSED) ? 1 : 0;
    s.fc_open     = !digitalRead(PIN_FC_OPEN)   ? 1 : 0;
    s.cam_on      = cam_on ? 1 : 0;
    s.error_code  = (valve_state == VALVE_ERROR) ? ERR_TIMEOUT : ERR_NONE;
    s.vbat_service= vbat_svc;
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

static void enter_valve(ValveState next) {
    valve_state = next;
    switch (next) {
        case VALVE_CLOSED:  led.set(LedState::VALVE_CLOSED); break;
        case VALVE_OPEN:    led.set(LedState::VALVE_OPEN);   break;
        case VALVE_OPENING: led.set(LedState::MOVING_OPEN);  break;
        case VALVE_CLOSING: led.set(LedState::MOVING_CLOSE); break;
        case VALVE_ERROR:   led.set(LedState::ERROR);        break;
    }
    send_status();
}

static void check_endstops() {
    bool fc_c = !digitalRead(PIN_FC_CLOSED);
    bool fc_o = !digitalRead(PIN_FC_OPEN);
    uint32_t now = millis();
    if (fc_c && !fc_closed_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (valve_state == VALVE_CLOSING) { motor.stop(); enter_valve(VALVE_CLOSED); }
    }
    if (fc_o && !fc_open_prev && (now - last_fc_ms > FC_DEBOUNCE_MS)) {
        last_fc_ms = now;
        if (valve_state == VALVE_OPENING) { motor.stop(); enter_valve(VALVE_OPEN); }
    }
    fc_closed_prev = fc_c;
    fc_open_prev   = fc_o;
}

static void check_motor_timeout() {
    if (!motor.running()) return;
    if (millis() - motor.start_ms() > MOTOR_TIMEOUT_MS) {
        motor.stop();
        enter_valve(VALVE_ERROR);
    }
}

static void on_message(const DomocMsg* msg, size_t) {
    if (msg->msg_type == MSG_REGISTER_ACK) {
        mesh.send_to_root(MSG_DESCRIPTOR, (const uint8_t*)&GW_DESC, sizeof(GW_DESC));
        send_status();
        return;
    }
    if (msg->msg_type != MSG_COMMAND) return;
    const CmdPayload* cmd = (const CmdPayload*)msg->payload;
    switch (cmd->action_code) {
        case ACTION_OPEN:
            if (valve_state == VALVE_CLOSED) {
                motor.start(MotorDir::OPEN);
                enter_valve(VALVE_OPENING);
            }
            break;
        case ACTION_CLOSE:
            if (valve_state == VALVE_OPEN) {
                motor.start(MotorDir::CLOSE);
                enter_valve(VALVE_CLOSING);
            }
            break;
        case ACTION_CAM_ON:
            cam_on = true;
            digitalWrite(PIN_CAMERA_REL, HIGH);
            send_status();
            break;
        case ACTION_CAM_OFF:
            cam_on = false;
            digitalWrite(PIN_CAMERA_REL, LOW);
            send_status();
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
    pinMode(PIN_FC_CLOSED,   INPUT_PULLUP);
    pinMode(PIN_FC_OPEN,     INPUT_PULLUP);
    pinMode(PIN_CAMERA_REL,  OUTPUT);
    digitalWrite(PIN_CAMERA_REL, LOW);
    analogReadResolution(12);

    valve_state = !digitalRead(PIN_FC_CLOSED) ? VALVE_CLOSED : VALVE_OPEN;

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.begin();

    read_vbat();
}

void loop() {
    mesh.update();
    uint32_t now = millis();
    check_endstops();
    check_motor_timeout();
    if (now - last_vbat_ms >= VBAT_READ_MS) { last_vbat_ms = now; read_vbat(); }
    led.update();
}
