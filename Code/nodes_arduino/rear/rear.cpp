// DomoC — Nodo REAR (Arduino, NO mesh)
//
// - Stream MJPEG telecamera retromarcia via HTTP: http://<ip>/stream
// - Snapshot JPEG: http://<ip>/snapshot
// - Stato + tensione batteria servizio: http://<ip>/status  (JSON)
//
// Board: AI Thinker ESP32-CAM
// Librerie: esp32-camera (inclusa in ESP32 Arduino board package)

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include "config.h"

static WebServer server(HTTP_PORT);
static float    vbat_v    = 0.0f;
static uint32_t last_vbat = 0;

// ── ADC batteria ─────────────────────────────────────────────────────────────
static void read_vbat() {
    // Media su 8 campioni per ridurre il rumore ADC2/Wi-Fi
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += analogRead(ADC_VBAT_PIN);
    float adc_mv = (sum / 8.0f) / 4095.0f * 3300.0f;
    vbat_v = adc_mv / 1000.0f * ADC_VBAT_RATIO;
}

// ── Camera ───────────────────────────────────────────────────────────────────
static bool camera_init() {
    camera_config_t cfg{};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0       = CAM_PIN_D0;
    cfg.pin_d1       = CAM_PIN_D1;
    cfg.pin_d2       = CAM_PIN_D2;
    cfg.pin_d3       = CAM_PIN_D3;
    cfg.pin_d4       = CAM_PIN_D4;
    cfg.pin_d5       = CAM_PIN_D5;
    cfg.pin_d6       = CAM_PIN_D6;
    cfg.pin_d7       = CAM_PIN_D7;
    cfg.pin_xclk     = CAM_PIN_XCLK;
    cfg.pin_pclk     = CAM_PIN_PCLK;
    cfg.pin_vsync    = CAM_PIN_VSYNC;
    cfg.pin_href     = CAM_PIN_HREF;
    cfg.pin_sscb_sda = CAM_PIN_SIOD;
    cfg.pin_sscb_scl = CAM_PIN_SIOC;
    cfg.pin_pwdn     = CAM_PIN_PWDN;
    cfg.pin_reset    = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size   = FRAMESIZE_VGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 2;
    return esp_camera_init(&cfg) == ESP_OK;
}

// ── Handlers HTTP ─────────────────────────────────────────────────────────────
static void handle_stream() {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    while (client.connected()) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { delay(10); continue; }

        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                       fb->len);
        client.write(fb->buf, fb->len);
        client.println();
        esp_camera_fb_return(fb);
        delay(33);  // ~30fps
    }
}

static void handle_snapshot() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { server.send(503, "text/plain", "camera error"); return; }
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

static void handle_status() {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"node\":\"REAR\",\"vbat_v\":%.2f,\"stream_url\":\"http://%s%s\"}",
             vbat_v, WiFi.localIP().toString().c_str(), STREAM_PATH);
    server.send(200, "application/json", buf);
}

void setup() {
    Serial.begin(115200);
    analogSetAttenuation(ADC_11db);  // range 0-3.3V su ADC2

    if (!camera_init()) {
        Serial.println("Camera init fallita");
        while (true) delay(1000);
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connessione WiFi");
    while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Stream: http://%s%s\n", WiFi.localIP().toString().c_str(), STREAM_PATH);

    read_vbat();

    server.on(STREAM_PATH,  handle_stream);
    server.on("/snapshot",  handle_snapshot);
    server.on(STATUS_PATH,  handle_status);
    server.begin();
}

void loop() {
    server.handleClient();

    if (millis() - last_vbat >= 30000UL) {
        read_vbat();
        last_vbat = millis();
    }
}
