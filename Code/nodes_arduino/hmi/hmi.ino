// DomoC — Nodo HMI (Arduino + painlessMesh)
//
// Controller portatile Waveshare ESP32-S3-Knob-Touch-LCD-1.8.
// Display QSPI ST77916 360×360, touch CST816, encoder via UART interno.
//
// STATO: stub — implementazione LVGL pianificata in Fase 3.
// Funzionalità attive in questo stub:
//   - Connessione mesh, registrazione al ROOT
//   - Ricezione registry (MSG_REGISTRY_DUMP, MSG_NODE_JOINED)
//   - Ricezione status nodi (MSG_STATUS_RESP)
//   - Invio comandi (MSG_COMMAND) verso nodi
//
// La UI LVGL richiede: lvgl 8.3+, driver ST77916 QSPI, CST816 touch driver.
// Vedere Document/nodes/hmi.md per il design completo del carosello.
//
// Librerie: painlessMesh
// Board: ESP32S3 Dev Module (8MB PSRAM abilitata)

#include "../../arduino_lib/domoc/domoc_protocol.h"
#include "../../arduino_lib/domoc/domoc_descriptor.h"
#include "../../arduino_lib/domoc/DomocMesh.h"
#include "config.h"

// Stato registry locale (costruito dai messaggi del ROOT)
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
            // Registrazione confermata — richiedi dump del registry
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
            // TODO: aggiornare carosello LVGL
            Serial.printf("Nodo in rete: %s (id=0x%02X)\n", ev->name, ev->node_id);
            break;
        }

        case MSG_DESCRIPTOR: {
            // Il descriptor arriva con src_id = nodo che l'ha inviato
            NodeEntry* n = find_or_add(msg->src_id);
            if (n && payload_len >= sizeof(NodeDescriptor)) {
                memcpy(&n->descriptor, msg->payload, sizeof(NodeDescriptor));
                n->has_descriptor = true;
                Serial.printf("Descriptor ricevuto per nodo 0x%02X\n", msg->src_id);
            }
            // TODO: aggiornare scheda nodo nel carosello LVGL
            break;
        }

        case MSG_STATUS_RESP:
            // Status aggiornato di un nodo
            // TODO: aggiornare widget LVGL usando gli offset del descriptor
            break;

        case MSG_NODE_WARNING:
        case MSG_NODE_OFFLINE:
        case MSG_NODE_LOST: {
            const NodeEventPayload* ev = (const NodeEventPayload*)msg->payload;
            Serial.printf("Nodo %s: %s\n", ev->name,
                msg->msg_type == MSG_NODE_WARNING ? "WARNING" :
                msg->msg_type == MSG_NODE_OFFLINE ? "OFFLINE" : "LOST");
            // TODO: mostrare badge/icona di warning nel carosello
            break;
        }

        case MSG_KEY_ON:
            Serial.println("KEY_ON: chiave inserita");
            // TODO: mostrare badge chiave, disabilitare azioni FLAG_KEY_BLOCKED
            break;

        case MSG_KEY_OFF:
            Serial.println("KEY_OFF");
            // TODO: rimuovere badge chiave
            break;

        case MSG_STEP_OPEN:
            Serial.println("STEP: scaletta aperta");
            // TODO: mostrare icona scaletta aperta
            break;

        default: break;
    }
}

// Invia un comando a un nodo (da usare dalla UI LVGL o da seriale in debug)
void send_command(uint8_t target_logical_id, uint8_t action, uint8_t param = 0) {
    CmdPayload cmd{ action, param };
    // Il ROOT fa forwarding al nodo corretto usando il dst_id nel frame
    // Per ora usiamo il src_id dell'HMI e dst_id = target — il ROOT legge dst_id
    uint8_t buf[2] = { action, param };
    mesh.send_to_root(MSG_COMMAND, buf, 2);
    // TODO: il frame deve avere dst_id = target_logical_id, non ROOT.
    //       Attualmente DomocMesh::send_to_root imposta dst=NODE_ID_MASTER.
    //       Aggiungere DomocMesh::send_command(target_id, payload, len).
}

void setup() {
    Serial.begin(115200);
    Serial.println("HMI avvio...");

    // TODO: inizializzare display QSPI ST77916
    // TODO: inizializzare touch CST816
    // TODO: inizializzare LVGL e creare schermata carosello
    // TODO: inizializzare DRV2605 feedback aptico

    mesh.set_on_message(on_message);
    mesh.set_on_heartbeat_due([]() { /* HMI non invia heartbeat di stato */ });
    mesh.begin();
}

void loop() {
    mesh.update();
    // TODO: lv_timer_handler() per aggiornare LVGL
    // TODO: leggere eventi encoder via UART interno (ESP32-U4WDH)
    // TODO: leggere eventi touch CST816
}
