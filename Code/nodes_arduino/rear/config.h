#pragma once
// ESP32-CAM (AI Thinker) — fuori dalla mesh, server HTTP diretto
// Vedere Document/nodes/rear.md

#define WIFI_SSID     "DomoC01"
#define WIFI_PASSWORD "domoc2024"
#define HTTP_PORT     80
#define STREAM_PATH   "/stream"
#define STATUS_PATH   "/status"

// GPIO 13 (ADC2_CH4) libero senza SD card — partitore 5:1 per 0–16.5V → 0–3.3V.
// ADC2 è condiviso col Wi-Fi: la lettura introduce rumore accettabile (±200 mV)
// per monitoraggio batteria 12V.
#define ADC_VBAT_PIN    13
#define ADC_VBAT_RATIO  5.0f   // (R1+R2)/R2 — R1=33kΩ, R2=8.2kΩ

// Camera AI Thinker pinout
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      38
#define CAM_PIN_D3      37
#define CAM_PIN_D2      36
#define CAM_PIN_D1      21
#define CAM_PIN_D0      19
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22
