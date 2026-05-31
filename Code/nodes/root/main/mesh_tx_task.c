#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_mesh.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "mesh_tx_task.h"
#include "node_registry.h"
#include "mesh_protocol.h"

static const char *TAG = "MESH_TX";

typedef struct {
    mesh_msg_t msg;
    uint8_t    target_mac[6];
    bool       has_target;    // false = broadcast
} tx_item_t;

static QueueHandle_t s_tx_queue;
static uint8_t       s_hmi_mac[6];
static bool          s_hmi_known = false;
static uint16_t      s_seq       = 0;

// ── Init ──────────────────────────────────────────────────────────────────
void mesh_tx_queue_init(void)
{
    s_tx_queue = xQueueCreate(TX_QUEUE_DEPTH, sizeof(tx_item_t));
}

// ── Task ──────────────────────────────────────────────────────────────────
void mesh_tx_task(void *pvParam)
{
    tx_item_t item;
    mesh_addr_t dest;

    while (1) {
        if (xQueueReceive(s_tx_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        mesh_data_t data = {
            .data  = (uint8_t *)&item.msg,
            .size  = sizeof(mesh_msg_t),
            .proto = MESH_PROTO_BIN,
            .tos   = MESH_TOS_P2P,
        };

        if (item.has_target) {
            memcpy(dest.addr, item.target_mac, 6);
        } else {
            // Broadcast: usa addr tutti-zero (ESP-Mesh lo instrada a tutti)
            memset(dest.addr, 0, 6);
        }

        esp_err_t err = esp_mesh_send(&dest, &data,
                                      item.has_target ? MESH_DATA_P2P : 0,
                                      NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "mesh_send fallito: %s (msg_type=0x%02X)",
                     esp_err_to_name(err), item.msg.msg_type);
        }
    }
}

// ── API pubblica ──────────────────────────────────────────────────────────
bool mesh_tx_enqueue(const mesh_msg_t *msg, const uint8_t target_mac[6])
{
    tx_item_t item;
    item.msg = *msg;
    if (target_mac) {
        memcpy(item.target_mac, target_mac, 6);
        item.has_target = true;
    } else {
        item.has_target = false;
    }

    if (xQueueSend(s_tx_queue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "Coda TX piena — messaggio scartato");
        return false;
    }
    return true;
}

void mesh_tx_send_register_ack(uint16_t node_id, const uint8_t target_mac[6])
{
    mesh_msg_t msg = {
        .version     = PROTOCOL_VERSION,
        .msg_type    = MSG_REGISTER_ACK,
        .node_id     = NODE_ID_ROOT,
        .target_id   = node_id,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .seq_num     = ++s_seq,
    };

    reg_ack_payload_t ack = {
        .assigned_node_id = node_id,
        .mesh_channel     = 6,   // MESH_CHANNEL
    };
    memcpy(msg.payload, &ack, sizeof(ack));
    msg.payload_len = sizeof(ack);
    msg.crc16 = mesh_crc16((uint8_t *)&msg, sizeof(msg) - sizeof(msg.crc16));

    mesh_tx_enqueue(&msg, target_mac);
    ESP_LOGI(TAG, "REGISTER_ACK inviato a 0x%04X", node_id);
}

void mesh_tx_forward_to_hmi(msg_type_t type, uint16_t node_id,
                             const uint8_t *payload, uint8_t payload_len)
{
    if (!s_hmi_known) return;

    mesh_msg_t msg = {
        .version      = PROTOCOL_VERSION,
        .msg_type     = (uint8_t)type,
        .node_id      = node_id,
        .target_id    = NODE_ID_HMI,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .seq_num      = ++s_seq,
        .payload_len  = payload_len,
    };
    if (payload && payload_len)
        memcpy(msg.payload, payload, payload_len);
    msg.crc16 = mesh_crc16((uint8_t *)&msg, sizeof(msg) - sizeof(msg.crc16));

    mesh_tx_enqueue(&msg, s_hmi_mac);
}

void mesh_tx_send_registry_dump(const uint8_t hmi_mac[6])
{
    node_info_t nodes[MAX_NODES];
    int count = node_registry_dump(nodes, MAX_NODES);

    // Invia ogni nodo come MSG_STATUS_RESP separato
    for (int i = 0; i < count; i++) {
        mesh_msg_t msg = {
            .version      = PROTOCOL_VERSION,
            .msg_type     = MSG_STATUS_RESP,
            .node_id      = nodes[i].node_id,
            .target_id    = NODE_ID_HMI,
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
            .seq_num      = ++s_seq,
        };
        // Inserisce stato e payload_cache come body del messaggio
        memcpy(msg.payload, nodes[i].payload_cache, sizeof(nodes[i].payload_cache));
        msg.payload[sizeof(nodes[i].payload_cache)]     = (uint8_t)nodes[i].status;
        msg.payload_len = sizeof(nodes[i].payload_cache) + 1;
        msg.crc16 = mesh_crc16((uint8_t *)&msg, sizeof(msg) - sizeof(msg.crc16));

        mesh_tx_enqueue(&msg, hmi_mac);
        vTaskDelay(pdMS_TO_TICKS(10)); // evita burst su coda
    }
    ESP_LOGI(TAG, "Registry dump inviata all'HMI (%d nodi)", count);
}

void mesh_tx_set_hmi_mac(const uint8_t mac[6])
{
    memcpy(s_hmi_mac, mac, 6);
    s_hmi_known = true;
}

bool mesh_tx_hmi_connected(void)
{
    return s_hmi_known;
}
