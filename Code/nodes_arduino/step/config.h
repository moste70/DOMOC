#pragma once

// ── GPIO (ESP32-S3-MINI-1 su PCB Universale v3.0) ───────────────────────────
#define PIN_FC_CLOSED   3   // OPT1 — finecorsa chiuso (active-LOW)
#define PIN_FC_OPEN     4   // OPT2 — finecorsa aperto  (active-LOW)
#define PIN_HB_DIR_A    11  // HB1_DIR_A
#define PIN_HB_DIR_B    12  // HB1_DIR_B
#define PIN_HB_EN       13  // HB1_EN
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9
#define PIN_LED         21  // WS2812B

// ── Timing ──────────────────────────────────────────────────────────────────
#define MOTOR_TIMEOUT_MS    10000
#define FC_DEBOUNCE_MS      50
#define SHT31_INTERVAL_MS   60000
