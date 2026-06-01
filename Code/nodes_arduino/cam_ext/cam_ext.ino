// DomoC — Nodo CAM_EXT (Arduino, NO mesh)
//
// Telecamere esterne: stream MJPEG HTTP + motion detection frame-differencing.
// Come REAR, questo nodo non entra nella mesh.
// Il motion alert viene esposto via HTTP GET /motion (polling dall'HMI).
//
// Board: AI Thinker ESP32-CAM

#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include "config.h"

static WebServer server(HTTP_PORT);
static bool      motion_detected   = false;
static uint32_t  last_motion_ms    = 0;

// Frame precedente per frame-differencing (allocato in PSRAM se disponibile)
static uint8_t* prev_frame = nullptr;
static size_t   prev_len   = 0;

// ── Camera init (identica a REAR) ────────────────────────────────────────
static bool camera_init() {
    camera_config_t cfg{};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0 = CAM_PIN_D0; cfg.pin_d1 = CAM_PIN_D1;
    cfg.pin_d2 = CAM_PIN_D2; cfg.pin_d3 = CAM_PIN_D3;
    cfg.pin_d4 = CAM_PIN_D4; cfg.pin_d5 = CAM_PIN_D5;
    cfg.pin_d6 = CAM_PIN_D6; cfg.pin_d7 = CAM_PIN_D7;
    cfg.pin_xclk = CAM_PIN_XCLK; cfg.pin_pclk = CAM_PIN_PCLK;
    cfg.pin_vsync = CAM_PIN_VSYNC; cfg.pin_href = CAM_PIN_HREF;
    cfg.pin_sscb_sda = CAM_PIN_SIOD; cfg.pin_sscb_scl = CAM_PIN_SIOC;
    cfg.pin_pwdn = CAM_PIN_PWDN; cfg.pin_reset = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size   = FRAMESIZE_VGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 2;
    return esp_camera_init(&cfg) == ESP_OK;
}

// ── Motion detection (frame-differencing su thumbnail QVGA) ─────────────────
static void check_motion(const uint8_t* frame, size_t len) {
    if (millis() - last_motion_ms < MOTION_COOLDOWN_MS) return;
    if (!prev_frame || prev_len != len) {
        if (!prev_frame) prev_frame = (uint8_t*)ps_malloc(len);
        if (prev_frame) { memcpy(prev_frame, frame, len); prev_len = len; }
        return;
    }

    // Conta byte con differenza > soglia (approssimazione su dati JPEG compressi)
    uint32_t diff = 0;
    for (size_t i = 0; i < len; i++)
        if (abs((int)frame[i] - (int)prev_frame[i]) > MOTION_THRESHOLD) diff++;

    if (diff > len / 20) {  // più del 5% dei byte cambiati
        motion_detected = true;
        last_motion_ms  = millis();
        Serial.printf("Motion detected: %u byte diff\n", diff);
    }

    memcpy(prev_frame, frame, len);
}

static void handle_stream() {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    while (client.connected()) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { delay(10); continue; }

        check_motion(fb->buf, fb->len);

        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                       fb->len);
        client.write(fb->buf, fb->len);
        client.println();
        esp_camera_fb_return(fb);
        delay(33);
    }
}

static void handle_motion() {
    // Risponde con JSON: {"motion": true/false, "ts": <millis>}
    // L'HMI fa polling periodico per sapere se c'è movimento
    String json = "{\"motion\":";
    json += motion_detected ? "true" : "false";
    json += ",\"ts\":";
    json += String(last_motion_ms);
    json += "}";
    server.send(200, "application/json", json);
    motion_detected = false;  // consuma il flag dopo la lettura
}

void setup() {
    Serial.begin(115200);

    if (!camera_init()) {
        Serial.println("Camera init fallita");
        while (true) delay(1000);
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

    server.on(STREAM_PATH, handle_stream);
    server.on(MOTION_PATH, handle_motion);
    server.begin();
}

void loop() {
    server.handleClient();
}
