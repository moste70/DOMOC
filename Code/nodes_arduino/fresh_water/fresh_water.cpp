// DomoC — Nodo FRESH_WATER (Arduino + painlessMesh)
//
// Controlla l'elettrovalvola acque chiare (relay NC su REL1/GPIO17).
// La valvola è normalmente chiusa: HIGH=aperta, LOW=chiusa.
//
// Librerie: painlessMesh, FastLED
// Board: ESP32S3 Dev Module

#include <Arduino.h>
#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "config.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t valve_open;   // offset 0: 1=aperta
    uint8_t error_code;   // offset 1
} FreshWaterStatus;
#pragma pack(pop)

static const NodeDescriptor FW_DESC = {
    .node_icon      = ICON_VALVE_FRESH,
    .action_count   = 2,
    .property_count = 1,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,  ICON_ACT_OPEN,  CTRL_CONFIRM, 0, 0, 0, "APRI"   },
        { ACTION_CLOSE, ICON_ACT_CLOSE, CTRL_BUTTON,  0, 0, 0, "CHIUDI" },
    },
    .properties = {
        { PROP_VALVE_ON, 0, PAYLOAD_UINT8, WIDGET_INDICATOR, 0, 1, "", "%d" },
    },
};

static DomocMesh mesh(NODE_ID_FRESH_WATER, "FRESH_WATER", 0x04);
static CRGB      led_buf[1];
static DomocLed  led(led_buf);
static bool      valve_open = false;

static void set_valve(bool open) {
    valve_open = open;
    digitalWrite(PIN_VALVE, open ? HIGH : LOW);
    led.set(open ? LedState::VALVE_OPEN : LedState::VALVE_CLOSED);
}

static void send_status() {
    FreshWaterStatus s{ valve_open ? 1u : 0u, DOMOC_ERR_NONE };
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

static void on_message(const DomocMsg* msg, size_t) {
    if (msg->msg_type == MSG_REGISTER_ACK) {
        mesh.send_to_root(MSG_DESCRIPTOR, (const uint8_t*)&FW_DESC, sizeof(FW_DESC));
        send_status();
        return;
    }
    if (msg->msg_type != MSG_COMMAND) return;
    const CmdPayload* cmd = (const CmdPayload*)msg->payload;
    if      (cmd->action_code == ACTION_OPEN)       set_valve(true);
    else if (cmd->action_code == ACTION_CLOSE)      set_valve(false);
    else if (cmd->action_code == ACTION_GET_STATUS) send_status();
    send_status();
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(40);

    pinMode(PIN_VALVE, OUTPUT);
    set_valve(false);  // stato sicuro: chiusa

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.begin();
}

void loop() {
    mesh.update();
    led.update();
}
