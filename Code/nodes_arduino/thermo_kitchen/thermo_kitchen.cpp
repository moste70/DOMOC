// DomoC — Nodo THERMO_BUNK (Arduino + painlessMesh)
//
// Termostato letto a castello: legge DS18B20, comanda valvola aria calda.
// Logica ON/OFF con isteresi — nessun PID necessario per riscaldamento ad aria.
//
// Librerie: painlessMesh, FastLED, DallasTemperature + OneWire
// Board: ESP32-C3 Dev Module

#include <Arduino.h>
#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "config.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t  valve_open;   // offset 0
    uint8_t  error_code;   // offset 1
    float    temperature;  // offset 2
    float    setpoint;     // offset 6
} ThermoStatus;            // 10 byte
#pragma pack(pop)

static const NodeDescriptor THERMO_DESC = {
    .node_icon      = ICON_THERMOMETER,
    .action_count   = 2,
    .property_count = 2,
    ._pad           = 0,
    .actions = {
        { ACTION_TEMP_UP, ICON_ACT_TEMP_UP, CTRL_STEPPER, 0, 0, 0, "TEMP+" },
        { ACTION_TEMP_DN, ICON_ACT_TEMP_DN, CTRL_STEPPER, 0, 0, 0, "TEMP-" },
    },
    .properties = {
        { PROP_TEMPERATURE, 2, PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 50, 400, "\xC2\xB0""C", "%.1f" },
        { PROP_SETPOINT,    6, PAYLOAD_FLOAT32, WIDGET_VALUE_UNIT,  50, 350, "\xC2\xB0""C", "%.1f" },
    },
};

static DomocMesh       mesh(NODE_LOGICAL_ID, NODE_NAME, NODE_TYPE);
static OneWire         ow(PIN_ONEWIRE);
static DallasTemperature ds(&ow);
static CRGB            led_buf[1];
static DomocLed        led(led_buf);

static float    temperature  = 0.0f;
static float    setpoint     = SETPOINT_DEFAULT;
static bool     valve_open   = false;
static uint32_t last_temp_ms = 0;
static bool     sensor_ok    = false;

static void set_valve(bool open) {
    if (valve_open == open) return;
    valve_open = open;
    digitalWrite(PIN_VALVE, open ? HIGH : LOW);
    led.set(open ? LedState::THERMO_HEAT : LedState::THERMO_IDLE);
}

static void thermostat_tick() {
    if (!sensor_ok) return;
    if (!valve_open && temperature < (setpoint - HYSTERESIS)) set_valve(true);
    if ( valve_open && temperature > (setpoint + HYSTERESIS)) set_valve(false);
}

static void send_status() {
    ThermoStatus s{};
    s.valve_open  = valve_open ? 1 : 0;
    s.error_code  = sensor_ok ? DOMOC_ERR_NONE : DOMOC_ERR_SENSOR;
    s.temperature = temperature;
    s.setpoint    = setpoint;
    mesh.send_heartbeat((const uint8_t*)&s, sizeof(s));
}

static void on_message(const DomocMsg* msg, size_t) {
    if (msg->msg_type == MSG_REGISTER_ACK) {
        mesh.send_to_root(MSG_DESCRIPTOR, (const uint8_t*)&THERMO_DESC, sizeof(THERMO_DESC));
        send_status();
        return;
    }
    if (msg->msg_type != MSG_COMMAND) return;
    const CmdPayload* cmd = (const CmdPayload*)msg->payload;
    if (cmd->action_code == ACTION_TEMP_UP) {
        setpoint = min(setpoint + SETPOINT_STEP, SETPOINT_MAX);
        send_status();
    } else if (cmd->action_code == ACTION_TEMP_DN) {
        setpoint = max(setpoint - SETPOINT_STEP, SETPOINT_MIN);
        send_status();
    } else if (cmd->action_code == ACTION_GET_STATUS) {
        send_status();
    }
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(40);

    pinMode(PIN_VALVE, OUTPUT);
    set_valve(false);

    ds.begin();
    sensor_ok = (ds.getDeviceCount() > 0);
    if (!sensor_ok) Serial.println("DS18B20 non trovato");

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due(send_status);
    mesh.begin();
}

void loop() {
    mesh.update();
    uint32_t now = millis();

    if (now - last_temp_ms >= TEMP_READ_MS) {
        last_temp_ms = now;
        ds.requestTemperatures();
        float t = ds.getTempCByIndex(0);
        sensor_ok   = (t != DEVICE_DISCONNECTED_C);
        if (sensor_ok) temperature = t;
        thermostat_tick();
        send_status();
    }

    led.update();
}
