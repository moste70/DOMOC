#pragma once
// ESP32-CAM — telecamere esterne + motion detection
// Stessi pin fisici di REAR_CAM (stesso modulo AI Thinker)
#define WIFI_SSID      "DomoC01"
#define WIFI_PASSWORD  "domoc2024"
#define HTTP_PORT      80
#define STREAM_PATH    "/stream"
#define MOTION_PATH    "/motion"

// Motion detection
#define MOTION_THRESHOLD  30    // differenza pixel per considerare movimento
#define MOTION_COOLDOWN_MS 5000 // intervallo minimo tra alert consecutivi

// Stessa pinout AI Thinker di REAR_CAM
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
