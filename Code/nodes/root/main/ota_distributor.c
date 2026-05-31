#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ota_distributor.h"
#include "node_registry.h"
#include "mesh_tx_task.h"
#include "mesh_protocol.h"

static const char *TAG = "OTA_DIST";

// Il firmware da distribuire viene letto dalla partizione SPIFFS "config"
// (caricato dall'HMI in precedenza via HTTP o UART) oppure tenuto in RAM
// per sessioni di piccole dimensioni. Per ora lo stub legge da un buffer
// che verrà riempito da un meccanismo futuro (es. HTTP upload da HMI).

#define OTA_ACK_TIMEOUT_MS  3000
#define OTA_MAX_RETRIES     3

// ── Stato sessione OTA ────────────────────────────────────────────────────
typedef struct {
    uint16_t node_id;
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
} ota_session_t;

static QueueHandle_t    s_ota_queue;
static EventGroupHandle_t s_ota_events;

#define OTA_ACK_BIT     BIT0
#define OTA_DONE_BIT    BIT1
#define OTA_ERROR_BIT   BIT2

static uint16_t s_active_node_id   = 0;
static uint16_t s_last_acked_chunk = UINT16_MAX;

// ── Inizializzazione ──────────────────────────────────────────────────────
// Viene chiamato implicitamente al primo utilizzo (lazy init in task)

// ── API chiamate da mesh_rx_task ──────────────────────────────────────────
void ota_distributor_start(uint16_t target_node_id,
                           uint32_t fw_size, uint32_t fw_crc32,
                           uint8_t  fw_major, uint8_t fw_minor, uint8_t fw_patch)
{
    if (!s_ota_queue) {
        s_ota_queue  = xQueueCreate(4, sizeof(ota_session_t));
        s_ota_events = xEventGroupCreate();
    }

    ota_session_t session = {
        .node_id  = target_node_id,
        .fw_size  = fw_size,
        .fw_crc32 = fw_crc32,
        .fw_major = fw_major,
        .fw_minor = fw_minor,
        .fw_patch = fw_patch,
    };

    if (xQueueSend(s_ota_queue, &session, pdMS_TO_TICKS(100)) != pdTRUE)
        ESP_LOGW(TAG, "OTA queue piena — sessione scartata");
    else
        ESP_LOGI(TAG, "OTA richiesta per nodo 0x%04X (%u byte)", target_node_id, fw_size);
}

void ota_distributor_on_chunk_ack(uint16_t node_id, uint16_t chunk_index)
{
    if (node_id != s_active_node_id) return;
    s_last_acked_chunk = chunk_index;
    if (s_ota_events)
        xEventGroupSetBits(s_ota_events, OTA_ACK_BIT);
}

void ota_distributor_on_complete(uint16_t node_id)
{
    if (node_id != s_active_node_id) return;
    ESP_LOGI(TAG, "OTA completata su nodo 0x%04X", node_id);
    node_registry_set_status(node_id, NODE_STATUS_ONLINE);
    mesh_tx_forward_to_hmi(MSG_OTA_COMPLETE, node_id, NULL, 0);
    if (s_ota_events)
        xEventGroupSetBits(s_ota_events, OTA_DONE_BIT);
}

void ota_distributor_on_error(uint16_t node_id)
{
    if (node_id != s_active_node_id) return;
    ESP_LOGE(TAG, "OTA errore su nodo 0x%04X", node_id);
    mesh_tx_forward_to_hmi(MSG_OTA_ERROR, node_id, NULL, 0);
    if (s_ota_events)
        xEventGroupSetBits(s_ota_events, OTA_ERROR_BIT);
}

// ── Distribuzione di una sessione ─────────────────────────────────────────
static void run_ota_session(const ota_session_t *session)
{
    node_info_t *node = node_registry_get_by_id(session->node_id);
    if (!node) {
        ESP_LOGE(TAG, "Nodo 0x%04X non trovato in registry", session->node_id);
        return;
    }

    s_active_node_id = session->node_id;
    node_registry_set_status(session->node_id, NODE_STATUS_UPDATING);
    ESP_LOGI(TAG, "Avvio OTA su %s (0x%04X) — %u byte v%d.%d.%d",
             node->name, session->node_id, session->fw_size,
             session->fw_major, session->fw_minor, session->fw_patch);

    // Invia MSG_OTA_START al nodo
    mesh_msg_t start_msg = {
        .version      = PROTOCOL_VERSION,
        .msg_type     = MSG_OTA_START,
        .node_id      = NODE_ID_ROOT,
        .target_id    = session->node_id,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .seq_num      = mesh_next_seq(),
    };
    ota_start_payload_t sp = {
        .target_node_id = session->node_id,
        .fw_size        = session->fw_size,
        .fw_crc32       = session->fw_crc32,
        .fw_major       = session->fw_major,
        .fw_minor       = session->fw_minor,
        .fw_patch       = session->fw_patch,
    };
    memcpy(start_msg.payload, &sp, sizeof(sp));
    start_msg.payload_len = sizeof(sp);
    start_msg.crc16 = mesh_crc16((uint8_t *)&start_msg,
                                  sizeof(start_msg) - sizeof(start_msg.crc16));
    mesh_tx_enqueue(&start_msg, node->mac);

    // TODO: leggere il firmware da SPIFFS e inviare i chunk
    // La struttura di invio chunk è:
    //   for ogni chunk:
    //     costruisci MSG_OTA_CHUNK con index, len, crc, data
    //     invia con mesh_tx_enqueue
    //     attendi OTA_ACK_BIT con timeout OTA_ACK_TIMEOUT_MS
    //     se timeout: ritrasmetti, max OTA_MAX_RETRIES tentativi
    //     se 3 fallimenti: abort, notifica HMI
    //
    // Poi attendi OTA_DONE_BIT o OTA_ERROR_BIT per conferma reboot

    ESP_LOGW(TAG, "Distribuzione chunk NON IMPLEMENTATA — stub");
    // Implementazione completa quando disponibile il meccanismo di storage firmware

    s_active_node_id = 0;
}

// ── Task OTA ──────────────────────────────────────────────────────────────
void ota_distributor_task(void *pvParam)
{
    s_ota_queue  = xQueueCreate(4, sizeof(ota_session_t));
    s_ota_events = xEventGroupCreate();

    ESP_LOGI(TAG, "ota_distributor_task avviato");

    ota_session_t session;
    while (1) {
        if (xQueueReceive(s_ota_queue, &session, portMAX_DELAY) == pdTRUE) {
            xEventGroupClearBits(s_ota_events, OTA_ACK_BIT | OTA_DONE_BIT | OTA_ERROR_BIT);
            run_ota_session(&session);
        }
    }
}
