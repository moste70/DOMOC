#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mesh.h"
#include "esp_log.h"

#include "mesh_rx_task.h"
#include "mesh_tx_task.h"
#include "node_registry.h"
#include "heartbeat_monitor.h"
#include "ota_distributor.h"
#include "mesh_protocol.h"

static const char *TAG = "MESH_RX";

// ── Handler per ogni tipo di messaggio ────────────────────────────────────

static void handle_register(const mesh_msg_t *msg, const uint8_t src_mac[6])
{
    if (msg->payload_len < sizeof(reg_payload_t)) {
        ESP_LOGW(TAG, "MSG_REGISTER payload troppo corto");
        return;
    }
    const reg_payload_t *reg = (const reg_payload_t *)msg->payload;
    uint16_t assigned_id = node_registry_register(reg, src_mac);
    if (assigned_id == 0) return;

    // Aggiorna MAC dell'HMI se è un HMI
    if (reg->node_type == NODE_TYPE_HMI) {
        mesh_tx_set_hmi_mac(src_mac);
        // Manda subito il dump completo all'HMI
        mesh_tx_send_registry_dump(src_mac);
    }

    // Conferma registrazione al nodo
    mesh_tx_send_register_ack(assigned_id, src_mac);

    // Notifica HMI del nuovo nodo
    mesh_tx_forward_to_hmi(MSG_NODE_JOINED, assigned_id,
                           msg->payload, msg->payload_len);
}

static void handle_heartbeat(const mesh_msg_t *msg)
{
    // In IDF 6.0 non esiste esp_mesh_get_parent_rssi — usiamo 0 come placeholder
    // Il RSSI reale è disponibile via Wi-Fi scan o dai dati del nodo mittente
    node_registry_update_heartbeat(msg->node_id, 0,
                                   msg->payload, msg->payload_len);

    // Forwarding stato all'HMI (se connesso)
    mesh_tx_forward_to_hmi(MSG_STATUS_RESP, msg->node_id,
                            msg->payload, msg->payload_len);
}

static void handle_alert(const mesh_msg_t *msg)
{
    ESP_LOGI(TAG, "ALERT da nodo 0x%04X", msg->node_id);

    // Forwarding inalterato all'HMI
    if (mesh_tx_hmi_connected()) {
        mesh_tx_forward_to_hmi(MSG_ALERT, msg->node_id,
                                msg->payload, msg->payload_len);
    }
}

static void handle_command(const mesh_msg_t *msg, const uint8_t src_mac[6])
{
    // I comandi arrivano dall'HMI e devono essere instradati al nodo target
    node_info_t *target = node_registry_get_by_id(msg->target_id);
    if (!target) {
        ESP_LOGW(TAG, "Comando per nodo sconosciuto 0x%04X", msg->target_id);
        return;
    }

    // Instrada verso il nodo destinatario usando il MAC fisico
    mesh_tx_enqueue(msg, target->mac);
    ESP_LOGD(TAG, "Comando ACTION=0x%02X instradato verso %s",
             msg->payload[0], target->name);
}

static void handle_status_req(const mesh_msg_t *msg, const uint8_t src_mac[6])
{
    // L'HMI richiede un dump completo della registry
    if (node_registry_is_hmi(msg->node_id) || msg->node_id == 0) {
        mesh_tx_send_registry_dump(src_mac);
    }
}

static void handle_ota_start(const mesh_msg_t *msg)
{
    if (msg->payload_len < sizeof(ota_start_payload_t)) return;
    const ota_start_payload_t *ota = (const ota_start_payload_t *)msg->payload;
    ota_distributor_start(ota->target_node_id, ota->fw_size,
                          ota->fw_crc32, ota->fw_major,
                          ota->fw_minor, ota->fw_patch);
}

static void handle_ota_ack(const mesh_msg_t *msg)
{
    ota_distributor_on_chunk_ack(msg->node_id, msg->seq_num);
}

// ── Task principale ───────────────────────────────────────────────────────
void mesh_rx_task(void *pvParam)
{
    static uint8_t rx_buf[sizeof(mesh_msg_t)];
    mesh_addr_t    from;
    mesh_data_t    data = {
        .data  = rx_buf,
        .size  = sizeof(rx_buf),
        .proto = MESH_PROTO_BIN,
    };
    int flag = 0;

    ESP_LOGI(TAG, "mesh_rx_task avviato");

    while (1) {
        data.size = sizeof(rx_buf);
        esp_err_t err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_mesh_recv errore: %s", esp_err_to_name(err));
            continue;
        }
        if (data.size < sizeof(mesh_msg_t)) continue;

        const mesh_msg_t *msg = (const mesh_msg_t *)rx_buf;

        // Valida versione e CRC
        if (msg->version != PROTOCOL_VERSION) {
            ESP_LOGW(TAG, "Versione protocollo non supportata: 0x%02X", msg->version);
            continue;
        }
        if (!mesh_msg_valid(msg)) {
            ESP_LOGW(TAG, "CRC16 errato da nodo 0x%04X", msg->node_id);
            continue;
        }

        // Smista per tipo
        switch (msg->msg_type) {
        case MSG_REGISTER:
            handle_register(msg, from.addr);
            break;
        case MSG_HEARTBEAT:
            handle_heartbeat(msg);
            break;
        case MSG_ALERT:
            handle_alert(msg);
            break;
        case MSG_COMMAND:
            handle_command(msg, from.addr);
            break;
        case MSG_STATUS_REQ:
            handle_status_req(msg, from.addr);
            break;
        case MSG_OTA_START:
            handle_ota_start(msg);
            break;
        case MSG_OTA_ACK:
            handle_ota_ack(msg);
            break;
        case MSG_OTA_COMPLETE:
            ota_distributor_on_complete(msg->node_id);
            break;
        case MSG_OTA_ERROR:
            ota_distributor_on_error(msg->node_id);
            break;
        default:
            ESP_LOGD(TAG, "msg_type 0x%02X non gestito", msg->msg_type);
            break;
        }
    }
}
