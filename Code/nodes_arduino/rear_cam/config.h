#pragma once
// ESP32-CAM (AI Thinker) — fuori dalla mesh, stream MJPEG HTTP diretto
// Vedere Document/nodes/rear_cam.md

#define WIFI_SSID     "DomoC01"      // stessa rete Wi-Fi della mesh
#define WIFI_PASSWORD "domoc2024"
#define HTTP_PORT     80
#define STREAM_PATH   "/stream"

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
