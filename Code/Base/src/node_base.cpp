// DomoC — NodeBase: implementazione della messaggistica comune.

#include "node_base.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "nvs_flash.h"

#include <cstring>

namespace domoc {

namespace {
constexpr const char* TAG = "node_base";

constexpr uint32_t STANDALONE_TIMEOUT_MS = 30000; // mesh assente → STANDALONE
constexpr uint32_t HEARTBEAT_BASE_MS     = 5000;  // battito ogni 5s (+ jitter)
constexpr uint32_t HOUSEKEEPING_TICK_MS  = 1000;
constexpr int      MESH_RECV_TIMEOUT_MS  = 1000;

constexpr uint32_t RX_STACK = 3072;
constexpr uint32_t TX_STACK = 3072;
constexpr uint32_t HK_STACK = 2560;
constexpr UBaseType_t MESH_TASK_PRIO = 5;
constexpr UBaseType_t HK_TASK_PRIO   = 4;
constexpr UBaseType_t TX_QUEUE_DEPTH = 8;
} // namespace

NodeBase::NodeBase(uint8_t node_id, NodeType type, const char* name)
    : node_id_(node_id), node_type_(type) {
    std::strncpy(name_, name, sizeof(name_) - 1);
    name_[sizeof(name_) - 1] = '\0';
}

NodeBase::~NodeBase() {
    running_ = false;
    if (rx_task_) vTaskDelete(rx_task_);
    if (tx_task_) vTaskDelete(tx_task_);
    if (hk_task_) vTaskDelete(hk_task_);
    if (tx_queue_) vQueueDelete(tx_queue_);
}

uint32_t NodeBase::now_ms() const {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint32_t NodeBase::heartbeat_jitter_ms() const {
    return esp_random() % 2000; // 0–2s, evita burst simultanei tra nodi
}

esp_err_t NodeBase::begin(const MeshConfig& cfg) {
    cfg_ = cfg;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = mesh_start(cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mesh_start fallito: %s", esp_err_to_name(err));
        return err;
    }

    tx_queue_ = xQueueCreate(TX_QUEUE_DEPTH, sizeof(TxItem));
    if (!tx_queue_) return ESP_ERR_NO_MEM;

    running_           = true;
    last_connected_ms_ = now_ms();
    next_heartbeat_ms_ = now_ms() + HEARTBEAT_BASE_MS + heartbeat_jitter_ms();

    xTaskCreate(rx_trampoline, "mesh_rx", RX_STACK, this, MESH_TASK_PRIO, &rx_task_);
    xTaskCreate(tx_trampoline, "mesh_tx", TX_STACK, this, MESH_TASK_PRIO, &tx_task_);
    xTaskCreate(housekeeping_trampoline, "hk", HK_STACK, this, HK_TASK_PRIO, &hk_task_);

    ESP_LOGI(TAG, "nodo '%s' (id=0x%02X) avviato", name_, node_id_);
    return ESP_OK;
}

esp_err_t NodeBase::mesh_start(const MeshConfig& cfg) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Vincolo energetico (CLAUDE.md): modem sleep + TX power ridotta.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_wifi_set_max_tx_power(cfg.tx_power_dbm * 4); // unita = 0.25 dBm

    ESP_ERROR_CHECK(esp_mesh_init());

    mesh_cfg_t mesh_cfg = MESH_INIT_CONFIG_DEFAULT();
    std::memcpy(mesh_cfg.mesh_id.addr, cfg.mesh_id, 6);
    mesh_cfg.channel = cfg.channel;
    mesh_cfg.mesh_ap.max_connection = 6;
    std::memcpy(mesh_cfg.mesh_ap.password, cfg.password,
                std::strlen(cfg.password));
    mesh_cfg.mesh_ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_mesh_set_config(&mesh_cfg));

    // Il MASTER e root fissa; gli altri nodi non partecipano mai all'election.
    esp_mesh_set_max_layer(cfg.max_layer);
    esp_mesh_fix_root(true);
    if (cfg.is_root) {
        esp_mesh_set_type(MESH_ROOT);
        esp_mesh_set_self_organized(false, false);
    }

    return esp_mesh_start();
}

// ── Task trampolines ─────────────────────────────────────────────────────────
void NodeBase::rx_trampoline(void* self)          { static_cast<NodeBase*>(self)->rx_loop(); }
void NodeBase::tx_trampoline(void* self)          { static_cast<NodeBase*>(self)->tx_loop(); }
void NodeBase::housekeeping_trampoline(void* self) { static_cast<NodeBase*>(self)->housekeeping_loop(); }

// ── Ricezione e dispatch ─────────────────────────────────────────────────────
void NodeBase::rx_loop() {
    uint8_t buf[MSG_FRAME_MAX];
    mesh_addr_t from{};
    mesh_data_t data{};
    int flag = 0;

    while (running_) {
        data.data = buf;
        data.size = sizeof(buf);
        esp_err_t err = esp_mesh_recv(&from, &data, MESH_RECV_TIMEOUT_MS, &flag, nullptr, 0);
        if (err != ESP_OK || data.size < MSG_HEADER_LEN + 1) continue;

        // Ultimo byte = CRC8 sull'header+payload.
        size_t frame_len = data.size - 1;
        if (crc8(buf, frame_len) != buf[frame_len]) {
            ESP_LOGW(TAG, "CRC8 errato, frame scartato");
            continue;
        }

        MeshMsg msg{};
        size_t copy = frame_len < sizeof(MeshMsg) ? frame_len : sizeof(MeshMsg);
        std::memcpy(&msg, buf, copy);
        size_t payload_len = frame_len - MSG_HEADER_LEN;

        // Accetta solo messaggi a noi destinati o in broadcast.
        if (msg.dst_id != node_id_ && msg.dst_id != NODE_ID_BROADCAST &&
            !cfg_.is_root) {
            continue;
        }
        dispatch(msg, payload_len);
    }
}

void NodeBase::dispatch(const MeshMsg& msg, size_t payload_len) {
    switch (msg.msg_type) {
    case MSG_REGISTER_ACK: {
        if (payload_len >= sizeof(RegAckPayload)) {
            registered_ = true;
            send_descriptor();
            ESP_LOGI(TAG, "registrato al ROOT, invio descriptor");
        }
        break;
    }
    case MSG_COMMAND: {
        if (payload_len >= sizeof(CmdPayload)) {
            CmdPayload cmd{};
            std::memcpy(&cmd, msg.payload, sizeof(cmd));
            on_command(cmd, msg.seq_num);
        }
        break;
    }
    case MSG_KEY_ON: {
        key_on_active_ = true;
        KeyOnPayload p{};
        if (payload_len >= sizeof(p)) std::memcpy(&p, msg.payload, sizeof(p));
        on_key_on(p);
        break;
    }
    case MSG_KEY_OFF: {
        key_on_active_ = false;
        KeyOffPayload p{};
        if (payload_len >= sizeof(p)) std::memcpy(&p, msg.payload, sizeof(p));
        on_key_off(p);
        break;
    }
    case MSG_STEP_OPEN: {
        StepOpenPayload p{};
        if (payload_len >= sizeof(p)) std::memcpy(&p, msg.payload, sizeof(p));
        on_step_open(p);
        break;
    }
    case MSG_STATUS_REQ:
        send_status_update();
        break;
    default:
        on_message(msg, payload_len);
        break;
    }
}

// ── Trasmissione ─────────────────────────────────────────────────────────────
esp_err_t NodeBase::send_to(uint8_t dst, MsgType type, const void* payload, size_t len) {
    if (len > MSG_PAYLOAD_MAX) return ESP_ERR_INVALID_SIZE;
    if (!tx_queue_) return ESP_ERR_INVALID_STATE;

    TxItem item{};
    item.dst  = dst;
    item.type = type;
    item.len  = static_cast<uint8_t>(len);
    if (payload && len) std::memcpy(item.payload, payload, len);

    return xQueueSend(tx_queue_, &item, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t NodeBase::broadcast(MsgType type, const void* payload, size_t len) {
    return send_to(NODE_ID_BROADCAST, type, payload, len);
}

void NodeBase::tx_loop() {
    TxItem item;
    uint8_t frame[MSG_FRAME_MAX];

    while (running_) {
        if (xQueueReceive(tx_queue_, &item, portMAX_DELAY) != pdTRUE) continue;

        // Serializza: header(4) + payload + CRC8.
        frame[0] = item.type;
        frame[1] = node_id_;
        frame[2] = item.dst;
        frame[3] = next_seq();
        std::memcpy(frame + MSG_HEADER_LEN, item.payload, item.len);
        size_t frame_len = MSG_HEADER_LEN + item.len;
        frame[frame_len] = crc8(frame, frame_len);
        frame_len += 1;

        esp_err_t err = transmit_frame(item.dst, frame, frame_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "tx fallita (type=0x%02X dst=0x%02X): %s",
                     item.type, item.dst, esp_err_to_name(err));
        }
    }
}

esp_err_t NodeBase::transmit_frame(uint8_t /*dst_id*/, const uint8_t* frame, size_t len) {
    // Nodo funzione: invia upstream verso il ROOT (to == NULL), che instrada
    // verso il destinatario logico indicato nell'header. Il MASTER ridefinisce
    // questo metodo per l'instradamento downstream.
    mesh_data_t data{};
    data.data  = const_cast<uint8_t*>(frame);
    data.size  = len;
    data.proto = MESH_PROTO_BIN;
    data.tos   = MESH_TOS_P2P;
    return esp_mesh_send(nullptr, &data, MESH_DATA_P2P, nullptr, 0);
}

void NodeBase::send_status_update() {
    uint8_t payload[MSG_PAYLOAD_MAX];
    size_t len = build_status_payload(payload, sizeof(payload));
    send_to(NODE_ID_MASTER, MSG_STATUS, payload, len);
}

void NodeBase::send_descriptor() {
    const NodeDescriptor& d = descriptor();
    send_to(NODE_ID_MASTER, MSG_DESCRIPTOR, &d, sizeof(d));
}

void NodeBase::do_register(bool reconnect) {
    RegPayload reg{};
    std::strncpy(reg.name, name_, sizeof(reg.name) - 1);
    reg.node_type = static_cast<uint8_t>(node_type_);
    reg.reconnect = reconnect ? 1 : 0;
    esp_read_mac(reg.mac, ESP_MAC_WIFI_STA);
    send_to(NODE_ID_MASTER, MSG_REGISTER, &reg, sizeof(reg));
    ESP_LOGI(TAG, "MSG_REGISTER inviato (reconnect=%d)", reconnect);
}

// ── Heartbeat + rilevamento standalone ───────────────────────────────────────
void NodeBase::housekeeping_loop() {
    bool was_connected = false;

    while (running_) {
        bool connected = esp_mesh_is_connected();
        uint32_t t = now_ms();

        if (connected) {
            last_connected_ms_ = t;
            if (!was_connected) {
                on_mesh_connected();
                do_register(registered_); // reconnect=true se gia conosciuto
            }
            if (standalone_) exit_standalone();

            if (registered_ && t >= next_heartbeat_ms_) {
                send_status_update();
                last_heartbeat_ms_ = t;
                next_heartbeat_ms_ = t + HEARTBEAT_BASE_MS + heartbeat_jitter_ms();
            }
        } else {
            if (was_connected) on_mesh_disconnected();
            if (!standalone_ && (t - last_connected_ms_) > STANDALONE_TIMEOUT_MS) {
                enter_standalone();
            }
        }

        was_connected = connected;
        vTaskDelay(pdMS_TO_TICKS(HOUSEKEEPING_TICK_MS));
    }
}

void NodeBase::enter_standalone() {
    standalone_ = true;
    registered_ = false; // andra ri-registrato al ritorno della mesh
    ESP_LOGW(TAG, "mesh assente > %ums — STANDALONE", STANDALONE_TIMEOUT_MS);
    on_standalone_enter();
}

void NodeBase::exit_standalone() {
    standalone_ = false;
    ESP_LOGI(TAG, "mesh ripristinata — uscita da STANDALONE");
    on_standalone_exit();
}

} // namespace domoc
