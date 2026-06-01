// DomoC — Nodo MASTER/ROOT (Arduino + painlessMesh)
//
// Funzioni: root mesh, registry nodi, rilevamento KEY_ON, forwarding HMI,
//           monitoraggio heartbeat (WARNING/OFFLINE/LOST).
//
// Librerie: painlessMesh, FastLED
// Board: ESP32-C3 Dev Module

#include <FastLED.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "../../arduino_lib/domoc/DomocLed.h"
#include "config.h"
#include "registry.h"

static DomocMesh mesh(NODE_ID_MASTER, "MASTER", 0x01, /*is_root=*/true);
static NodeRegistry registry;
static CRGB led_buf[1];
static DomocLed led(led_buf);

static bool    key_on          = false;
static bool    key_prev        = false;
static uint32_t key_debounce_ms= 0;
static float   vbat_engine     = 0.0f;
static uint32_t last_vbat_ms   = 0;
static uint32_t last_registry_tick_ms = 0;

// ── Forwarding verso HMI ─────────────────────────────────────────────────────
static void forward_to_hmi(uint8_t msg_type, const uint8_t* payload, size_t len) {
    uint32_t hmi_id = 0;
    if (registry.hmi_mesh_id(hmi_id))
        mesh.send_to(hmi_id, NODE_ID_HMI, msg_type, payload, len);
}

// ── Gestione KEY_ON ───────────────────────────────────────────────────────────
static void read_key_on() {
    bool pressed = !digitalRead(PIN_OPT1_KEY_ON);
    uint32_t now = millis();
    if (pressed == key_prev) { key_debounce_ms = now; return; }
    if (now - key_debounce_ms < KEY_DEBOUNCE_MS) return;

    key_debounce_ms = now;
    key_prev = pressed;

    if (pressed == key_on) return;
    key_on = pressed;

    KeyPayload kp{};
    kp.vbat_engine  = vbat_engine;
    kp.timestamp_s  = now / 1000;

    uint8_t msg_type = key_on ? MSG_KEY_ON : MSG_KEY_OFF;
    mesh.broadcast(msg_type, (const uint8_t*)&kp, sizeof(kp));
    led.set(key_on ? LedState::ERROR : LedState::OK_CLOSED);  // rosso=chiave, verde=off
}

// ── Lettura ADC batteria ─────────────────────────────────────────────────────
static void read_vbat() {
    int raw  = analogRead(PIN_ADC_VBAT_ENG);
    vbat_engine = (raw / ADC_RESOLUTION) * ADC_VREF * ADC_VBAT_FACTOR;
}

// ── Ricezione messaggi ───────────────────────────────────────────────────────
static void on_message(const DomocMsg* msg, size_t payload_len) {
    uint32_t from_mesh = 0;

    switch (msg->msg_type) {

        case MSG_REGISTER: {
            const RegPayload* reg = (const RegPayload*)msg->payload;
            registry.update_or_add(0 /* mesh id non noto qui */, msg->src_id,
                                   reg->node_type, reg->name);
            // Invia REGISTER_ACK direttamente al mittente
            // painlessMesh non espone il from nella callback — usiamo broadcast filtrato
            RegAckPayload ack{ msg->src_id };
            mesh.raw().sendBroadcast(""); // trick: invia ack come broadcast diretto
            // Workaround: il REGISTER viene inviato in broadcast dal nodo,
            // quindi rispondiamo in broadcast — solo il nodo con src_id lo processa
            mesh.broadcast(MSG_REGISTER_ACK, (const uint8_t*)&ack, sizeof(ack));

            // Notifica HMI del nuovo nodo
            NodeEventPayload ev{};
            ev.node_id   = msg->src_id;
            ev.node_type = ((const RegPayload*)msg->payload)->node_type;
            strlcpy(ev.name, reg->name, sizeof(ev.name));
            forward_to_hmi(MSG_NODE_JOINED, (const uint8_t*)&ev, sizeof(ev));
            break;
        }

        case MSG_HEARTBEAT:
            // Aggiorna registry e forwarda all'HMI
            {
                NodeInfo* n = registry.find_by_logical(msg->src_id);
                if (n) n->last_hb_ms = millis();
            }
            forward_to_hmi(MSG_STATUS_RESP, msg->payload, payload_len);
            break;

        case MSG_DESCRIPTOR:
            forward_to_hmi(MSG_DESCRIPTOR, msg->payload, payload_len);
            break;

        case MSG_DESCRIPTOR_REQ:
            // L'HMI chiede il descriptor di un nodo — lo forwardiamo al nodo
            {
                uint8_t target_id = msg->payload[0];
                NodeInfo* n = registry.find_by_logical(target_id);
                if (n) mesh.send_to(n->mesh_node_id, target_id,
                                    MSG_DESCRIPTOR_REQ, nullptr, 0);
            }
            break;

        case MSG_STATUS_REQ:
            // Dump registry verso HMI — invia tutti i nodi attivi
            for (const auto& n : registry.nodes_) {
                if (!n.active) continue;
                NodeEventPayload ev{};
                ev.node_id   = n.logical_id;
                ev.node_type = n.node_type;
                strlcpy(ev.name, n.name, sizeof(ev.name));
                forward_to_hmi(MSG_REGISTRY_DUMP, (const uint8_t*)&ev, sizeof(ev));
            }
            break;

        case MSG_COMMAND:
            // Forwarding comando HMI → nodo destinatario
            {
                NodeInfo* n = registry.find_by_logical(msg->dst_id);
                if (n) mesh.send_to(n->mesh_node_id, msg->dst_id,
                                    MSG_COMMAND, msg->payload, payload_len);
            }
            break;

        default:
            // Messaggi non gestiti dal ROOT (es. STEP_OPEN) → forward all'HMI
            forward_to_hmi(msg->msg_type, msg->payload, payload_len);
            break;
    }
}

// ── Cambio stato nodo (WARNING/OFFLINE/LOST) ─────────────────────────────────
static void on_node_status_changed(const NodeInfo& n, uint8_t old_status) {
    const uint8_t status_to_msg[] = { 0, MSG_NODE_WARNING, MSG_NODE_OFFLINE, MSG_NODE_LOST };
    if (n.status == 0 || n.status > 3) return;

    NodeEventPayload ev{};
    ev.node_id   = n.logical_id;
    ev.node_type = n.node_type;
    strlcpy(ev.name, n.name, sizeof(ev.name));
    forward_to_hmi(status_to_msg[n.status], (const uint8_t*)&ev, sizeof(ev));
}

// ── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led_buf, 1);
    FastLED.setBrightness(30);

    pinMode(PIN_OPT1_KEY_ON, INPUT_PULLUP);
    analogReadResolution(12);

    registry.set_on_status_changed(on_node_status_changed);

    mesh.set_on_message(on_message);
    mesh.begin();

    led.set(LedState::INITIALIZING);
    read_vbat();
}

void loop() {
    mesh.update();
    uint32_t now = millis();

    read_key_on();

    if (now - last_vbat_ms >= VBAT_READ_MS) {
        last_vbat_ms = now;
        read_vbat();
    }

    if (now - last_registry_tick_ms >= 1000) {
        last_registry_tick_ms = now;
        registry.tick(now);
    }

    led.update();
}
