#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "heartbeat_monitor.h"
#include "node_registry.h"
#include "mesh_tx_task.h"
#include "mesh_protocol.h"

static const char *TAG = "HB_MONITOR";

#define MONITOR_INTERVAL_MS  1000

void heartbeat_monitor_task(void *pvParam)
{
    ESP_LOGI(TAG, "heartbeat_monitor_task avviato");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

        // Itera su tutti i nodi validi
        // Nota: usa un buffer locale per evitare di tenere il mutex
        // troppo a lungo mentre si notifica l'HMI
        node_info_t snapshot[MAX_NODES];
        int count = node_registry_dump(snapshot, MAX_NODES);

        for (int i = 0; i < count; i++) {
            node_info_t *n = &snapshot[i];

            // L'HMI non viene monitorato: può connettersi e disconnettersi liberamente
            if (node_registry_is_hmi(n->node_id)) continue;
            // Nodi già LOST: non ripetiamo notifiche
            if (n->status == NODE_STATUS_LOST) continue;

            uint32_t delta = now - n->last_seen_ms;

            if (delta >= HB_TIMEOUT_LOST_MS && n->status != NODE_STATUS_LOST) {
                node_registry_mark_lost(n->node_id);
                mesh_tx_forward_to_hmi(MSG_NODE_LOST, n->node_id, NULL, 0);
                ESP_LOGW(TAG, "Nodo %s (0x%04X) LOST dopo %lus",
                         n->name, n->node_id, delta / 1000);
            }
            else if (delta >= HB_TIMEOUT_OFFLINE_MS && n->status != NODE_STATUS_OFFLINE) {
                node_registry_set_status(n->node_id, NODE_STATUS_OFFLINE);
                mesh_tx_forward_to_hmi(MSG_NODE_OFFLINE, n->node_id, NULL, 0);
                ESP_LOGW(TAG, "Nodo %s (0x%04X) OFFLINE dopo %lus",
                         n->name, n->node_id, delta / 1000);
            }
            else if (delta >= HB_TIMEOUT_WARNING_MS && n->status == NODE_STATUS_ONLINE) {
                node_registry_set_status(n->node_id, NODE_STATUS_WARNING);
                mesh_tx_forward_to_hmi(MSG_NODE_WARNING, n->node_id, NULL, 0);
                ESP_LOGI(TAG, "Nodo %s (0x%04X) WARNING — %lums senza heartbeat",
                         n->name, n->node_id, delta);
            }
        }
    }
}
