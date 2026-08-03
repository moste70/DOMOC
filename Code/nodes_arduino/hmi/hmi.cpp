// DomoC — Nodo HMI (Arduino + painlessMesh)
//
// Controller portatile Waveshare ESP32-S3-Knob-Touch-LCD-1.8.
// Display QSPI ST77916 360x360, touch CST816, encoder via UART interno.
//
// STATO: stub — implementazione LVGL pianificata in Fase 3.
// Funzionalita' attive in questo stub:
//   - Connessione mesh, registrazione al ROOT
//   - Ricezione registry (MSG_REGISTRY_DUMP, MSG_NODE_JOINED)
//   - Ricezione status nodi (MSG_STATUS_RESP)
//   - Invio comandi (MSG_COMMAND) verso nodi
//
// Librerie: painlessMesh
// Board: ESP32S3 Dev Module (8MB PSRAM abilitata)

#include <Arduino.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "config.h"

struct NodeEntry {
    uint8_t      logical_id;
    uint8_t      node_type;
    char         name[17];
    NodeDescriptor descriptor;
    bool         has_descriptor;
    bool         active;
};

static NodeEntry  nodes[14]{};
static DomocMesh  mesh(NODE_ID_HMI, "HMI", 0x0B);

static NodeEntry* find_or_add(uint8_t logical_id) {
    for (auto& n : nodes) if (n.active && n.logical_id == logical_id) return &n;
    for (auto& n : nodes) if (!n.active) { n.active = true; n.logical_id = logical_id; return &n; }
    return nullptr;
}

static void on_message(const DomocMsg* msg, size_t payload_len) {
    switch (msg->msg_type) {

        case MSG_REGISTER_ACK:
            mesh.send_to_root(MSG_STATUS_REQ, nullptr, 0);
            break;

        case MSG_REGISTRY_DUMP:
        case MSG_NODE_JOINED: {
            const NodeEventPayload* ev = (const NodeEventPayload*)msg->payload;
            NodeEntry* n = find_or_add(ev->node_id);
            if (n) {
                n->node_type = ev->node_type;
                strlcpy(n->name, ev->name, sizeof(n->name));
            }
            Serial.printf("Nodo in rete: %s (id=0x%02X)\n", ev->name, ev->node_id);
            break;
        }

        case MSG_DESCRIPTOR: {
            NodeEntry* n = find_or_add(msg->src_id);
            if (n && payload_len >= sizeof(NodeDescriptor)) {
                memcpy(&n->descriptor, msg->payload, sizeof(NodeDescriptor));
                n->has_descriptor = true;
                Serial.printf("Descriptor ricevuto per nodo 0x%02X\n", msg->src_id);
            }
            break;
        }

        case MSG_STATUS_RESP:
            break;

        case MSG_NODE_WARNING:
        case MSG_NODE_OFFLINE:
        case MSG_NODE_LOST: {
            const NodeEventPayload* ev = (const NodeEventPayload*)msg->payload;
            Serial.printf("Nodo %s: %s\n", ev->name,
                msg->msg_type == MSG_NODE_WARNING ? "WARNING" :
                msg->msg_type == MSG_NODE_OFFLINE ? "OFFLINE" : "LOST");
            break;
        }

        case MSG_KEY_ON:
            Serial.println("KEY_ON: chiave inserita");
            break;

        case MSG_KEY_OFF:
            Serial.println("KEY_OFF");
            break;

        case MSG_STEP_OPEN:
            Serial.println("STEP: scaletta aperta");
            break;

        default: break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("HMI avvio...");

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due([]() {});
    mesh.begin();
}

void loop() {
    mesh.update();
}
