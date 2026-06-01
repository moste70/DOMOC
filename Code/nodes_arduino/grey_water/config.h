#pragma once
// ── GPIO (ESP32-S3-MINI-1 su PCB Universale v3.0) ───────────────────────────
#define PIN_FC_CLOSED    3   // OPT1 — finecorsa valvola chiusa (active-LOW)
#define PIN_FC_OPEN      4   // OPT2 — finecorsa valvola aperta  (active-LOW)
#define PIN_HB_DIR_A     11  // HB1_DIR_A
#define PIN_HB_DIR_B     12  // HB1_DIR_B
#define PIN_HB_EN        13  // HB1_EN
#define PIN_CAMERA_REL   17  // REL1 — alimentazione telecamera portellone
#define PIN_ADC_VBAT_SVC  2  // ADC1_CH1 — tensione batteria servizio
#define PIN_LED          21  // WS2812B

#define ADC_VBAT_FACTOR  5.7f
#define ADC_VREF         3.3f
#define ADC_RESOLUTION   4095.0f

#define MOTOR_TIMEOUT_MS  8000
#define FC_DEBOUNCE_MS    50
#define VBAT_READ_MS      15000
