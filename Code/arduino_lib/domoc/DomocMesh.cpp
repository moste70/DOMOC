#include "DomocMesh.h"
#include "esp_wifi.h"
#include "esp_mac.h"

DomocMesh::DomocMesh(uint8_t my_id, const char* name, uint8_t node_type, bool is_root)
    : my_id_(my_id), node_type_(node_type), is_root_(is_root) {
    strlcpy(name_, name, sizeof(name_));
}

void DomocMesh::begin() {
    if (is_root_) {
        mesh_.setRoot(true);
        mesh_.setContainsRoot(true);
    }
    mesh_.setDebugMsgTypes(ERROR | STARTUP);
    mesh_.init(DOMOC_MESH_PREFIX, DOMOC_MESH_PASSWORD, &scheduler_, DOMOC_MESH_PORT);
    mesh_.onReceive([this](uint32_t from, String& msg) { on_receive(from, msg); });
    mesh_.onChangedConnections([this]() { on_changed_connections(); });

    heartbeat_interval_ = DOMOC_HEARTBEAT_MS + random(0, DOMOC_HEARTBEAT_JITTER_MS);
    last_mesh_activity_ = millis();

    // Il ROOT non si registra — è lui il registro
    if (!is_root_) send_register(false);
}

void DomocMesh::update() {
    mesh_.update();
    uint32_t now = millis();

    // Heartbeat periodico (solo nodi funzione registrati)
    if (!is_root_ && registered_ && (now - last_heartbeat_ms_ >= heartbeat_interval_)) {
        last_heartbeat_ms_   = now;
        heartbeat_interval_  = DOMOC_HEARTBEAT_MS + random(0, DOMOC_HEARTBEAT_JITTER_MS);
        if (heartbeat_cb_) heartbeat_cb_();
    }

    // Standalone detection
    if (!is_root_) {
        bool mesh_empty = (mesh_.getNodeList().size() == 0);
        if (!standalone_ && mesh_empty && (now - last_mesh_activity_ > DOMOC_STANDALONE_TIMEOUT_MS)) {
            standalone_ = true;
            registered_ = false;
            if (standalone_enter_cb_) standalone_enter_cb_();
        } else if (standalone_ && !mesh_empty) {
            standalone_ = false;
            send_register(true);
            if (standalone_exit_cb_) standalone_exit_cb_();
        }
    }
}

// ── Invio ────────────────────────────────────────────────────────────────────

void DomocMesh::send_heartbeat(const uint8_t* payload, size_t len) {
    send_frame(root_mesh_id_, NODE_ID_MASTER, MSG_HEARTBEAT, payload, len);
}

void DomocMesh::send_to_root(uint8_t msg_type, const uint8_t* payload, size_t len) {
    send_frame(root_mesh_id_, NODE_ID_MASTER, msg_type, payload, len);
}

void DomocMesh::broadcast(uint8_t msg_type, const uint8_t* payload, size_t len) {
    send_frame(0, NODE_ID_BROADCAST, msg_type, payload, len, true);
}

void DomocMesh::send_to(uint32_t mesh_node_id, uint8_t dst_logical_id,
                         uint8_t msg_type, const uint8_t* payload, size_t len) {
    send_frame(mesh_node_id, dst_logical_id, msg_type, payload, len);
}

void DomocMesh::send_frame(uint32_t to_mesh_id, uint8_t dst_logical_id,
                            uint8_t msg_type, const uint8_t* payload, size_t len,
                            bool is_broadcast) {
    uint8_t buf[4 + DOMOC_PAYLOAD_MAX + 1];
    buf[0] = msg_type;
    buf[1] = my_id_;
    buf[2] = dst_logical_id;
    buf[3] = seq_++;
    size_t plen = (len > DOMOC_PAYLOAD_MAX) ? DOMOC_PAYLOAD_MAX : len;
    if (payload && plen) memcpy(buf + 4, payload, plen);
    size_t total = 4 + plen;
    buf[total] = domoc_crc8(buf, total);

    String hex = bytes_to_hex(buf, total + 1);
    if (is_broadcast)           mesh_.sendBroadcast(hex);
    else if (to_mesh_id != 0)   mesh_.sendSingle(to_mesh_id, hex);
}

void DomocMesh::send_register(bool reconnect) {
    RegPayload reg{};
    strlcpy(reg.name, name_, sizeof(reg.name));
    reg.node_type = node_type_;
    reg.reconnect = reconnect ? 1 : 0;
    esp_read_mac(reg.mac, ESP_MAC_WIFI_STA);
    // Broadcast la registrazione — il ROOT risponderà con REGISTER_ACK
    broadcast(MSG_REGISTER, (const uint8_t*)&reg, sizeof(reg));
}

// ── Ricezione ────────────────────────────────────────────────────────────────

void DomocMesh::on_receive(uint32_t from, String& raw) {
    last_mesh_activity_ = millis();

    uint8_t buf[4 + DOMOC_PAYLOAD_MAX + 2];
    size_t  len = 0;
    if (!hex_to_bytes(raw, buf, len)) return;
    if (len < 5) return;

    if (domoc_crc8(buf, len - 1) != buf[len - 1]) return;

    const DomocMsg* msg = (const DomocMsg*)buf;
    size_t payload_len  = len - 4 - 1;  // sottrare header (4) e CRC (1)

    // Filtra messaggi non destinati a noi (il ROOT riceve tutto)
    if (!is_root_ && msg->dst_id != my_id_ && msg->dst_id != NODE_ID_BROADCAST) return;

    // Aggiorna stato interno
    if (msg->msg_type == MSG_REGISTER_ACK) {
        root_mesh_id_ = from;
        registered_   = true;
    } else if (msg->msg_type == MSG_KEY_ON) {
        key_on_ = true;
    } else if (msg->msg_type == MSG_KEY_OFF) {
        key_on_ = false;
    }

    if (msg_cb_) msg_cb_(msg, payload_len);
}

void DomocMesh::on_changed_connections() {
    last_mesh_activity_ = millis();
    // Se non ancora registrati, ritenta (es. ROOT appena tornato disponibile)
    if (!is_root_ && !registered_ && !standalone_) send_register(false);
}

// ── Helpers hex ──────────────────────────────────────────────────────────────

String DomocMesh::bytes_to_hex(const uint8_t* data, size_t len) {
    String s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", data[i]);
        s += tmp;
    }
    return s;
}

bool DomocMesh::hex_to_bytes(const String& hex, uint8_t* out, size_t& out_len) {
    size_t n = hex.length();
    if (n % 2 != 0 || n == 0) return false;
    out_len = n / 2;
    for (size_t i = 0; i < out_len; i++) {
        char tmp[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        out[i] = (uint8_t)strtol(tmp, nullptr, 16);
    }
    return true;
}
