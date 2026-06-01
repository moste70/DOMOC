#pragma once

// ── GPIO (ESP32-C3 su PCB Universale v3.0 — ruoli ROOT) ─────────────────────
#define PIN_OPT1_KEY_ON   3   // optoisolatore 1: KEY_ON (active-LOW, 12V sotto chiave)
#define PIN_ADC_VBAT_ENG  1   // ADC1_CH0: partitore tensione batteria motore (0–16.5V → 0–3.3V)
#define PIN_LED           21  // WS2812B

// ── Divisore ADC ─────────────────────────────────────────────────────────────
// R1=47kΩ, R2=10kΩ → factor = (47+10)/10 = 5.7; Vref=3.3V; ADC 12bit
#define ADC_VBAT_FACTOR   5.7f
#define ADC_VREF          3.3f
#define ADC_RESOLUTION    4095.0f

// ── Registry ─────────────────────────────────────────────────────────────────
#define REGISTRY_MAX_NODES  14
#define KEY_DEBOUNCE_MS     100
#define VBAT_READ_MS        10000  // lettura tensione batteria ogni 10s
