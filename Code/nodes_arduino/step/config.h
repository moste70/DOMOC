#pragma once

// ── GPIO ────────────────────────────────────────────────────────────────────
#define PIN_FC_CLOSED   3   // OPT1 — finecorsa chiuso (active-LOW)
#define PIN_FC_OPEN     4   // OPT2 — finecorsa aperto  (active-LOW)
#define PIN_HB_DIR_A    11  // H-bridge relay DIR_A (apri)
#define PIN_HB_DIR_B    12  // H-bridge relay DIR_B (chiudi)
#define PIN_HB_EN       13  // H-bridge relay ENABLE
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9
#define PIN_LED         21  // WS2812B

// ── Mesh ────────────────────────────────────────────────────────────────────
#define MESH_PREFIX     "DomoC01"
#define MESH_PASSWORD   "domoc2024"
#define MESH_PORT       5555

// ── Timing ──────────────────────────────────────────────────────────────────
#define HEARTBEAT_MS        5000
#define HEARTBEAT_JITTER_MS 2000
#define MOTOR_TIMEOUT_MS    10000   // ferma il motore se il fc non scatta entro 10s
#define FC_DEBOUNCE_MS      50
#define MESH_TIMEOUT_MS     30000   // entra in standalone dopo 30s senza mesh
#define SHT31_INTERVAL_MS   60000

// ── ID nodi (deve corrispondere a mesh_protocol.hpp) ────────────────────────
#define NODE_ID_ROOT        0x01
#define NODE_ID_STEP        0x02
#define NODE_ID_HMI         0x0B
#define NODE_ID_BROADCAST   0xFF

// ── Tipi messaggio ──────────────────────────────────────────────────────────
#define MSG_COMMAND         0x01
#define MSG_STATUS          0x02
#define MSG_REGISTER        0x05
#define MSG_HEARTBEAT       0x06
#define MSG_DESCRIPTOR      0x07
#define MSG_REGISTER_ACK    0x09
#define MSG_KEY_ON          0x11
#define MSG_KEY_OFF         0x12
#define MSG_STEP_OPEN       0x13

// ── Codici azione ────────────────────────────────────────────────────────────
#define ACTION_OPEN         0x01
#define ACTION_CLOSE        0x02
#define ACTION_GET_STATUS   0x0F
