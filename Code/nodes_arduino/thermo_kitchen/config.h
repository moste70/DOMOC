#pragma once
#define NODE_LOGICAL_ID  NODE_ID_THERMO_KITCHEN
#define NODE_NAME        "THERMO_KITCHEN"
#define NODE_TYPE        0x07

#define PIN_VALVE        5
#define PIN_ONEWIRE      6
#define PIN_LED          8

#define TEMP_READ_MS     10000
#define SETPOINT_MIN     5.0f
#define SETPOINT_MAX     35.0f
#define SETPOINT_STEP    0.5f
#define SETPOINT_DEFAULT 20.0f
#define HYSTERESIS       0.3f
