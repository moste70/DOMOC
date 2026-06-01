#pragma once
// ESP32-C3 — non usa PCB Universale v3.0
#define NODE_LOGICAL_ID  NODE_ID_THERMO_BUNK  // 0x05
#define NODE_NAME        "THERMO_BUNK"
#define NODE_TYPE        0x05

#define PIN_VALVE        5   // relay valvola aria calda
#define PIN_ONEWIRE      6   // DS18B20
#define PIN_LED          8   // WS2812B (GPIO diverso da S3 — C3 ha meno pin)

#define TEMP_READ_MS     10000   // lettura temperatura ogni 10s
#define SETPOINT_MIN     5.0f
#define SETPOINT_MAX     35.0f
#define SETPOINT_STEP    0.5f
#define SETPOINT_DEFAULT 20.0f
#define HYSTERESIS       0.3f    // evita cicli troppo rapidi della valvola
