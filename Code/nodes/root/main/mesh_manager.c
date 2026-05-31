#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_log.h"
#include "esp_event.h"

#include "mesh_manager.h"
#include "mesh_protocol.h"

static const char *TAG = "MESH_MGR";

#define MESH_STARTED_BIT    BIT0
#define MESH_TIMEOUT_MS     30000

static EventGroupHandle_t s_mesh_events;
static bool               s_connected = false;

static void mesh_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    switch (event_id) {
    case MESH_EVENT_STARTED:
        ESP_LOGI(TAG, "Mesh avviata");
        xEventGroupSetBits(s_mesh_events, MESH_STARTED_BIT);
        s_connected = true;
        break;

    case MESH_EVENT_ROOT_FIXED:
        ESP_LOGI(TAG, "ROOT fissato, mesh operativa");
        break;

    case MESH_EVENT_PARENT_CONNECTED:
        // Il ROOT non ha parent — questo evento non dovrebbe accadere
        ESP_LOGW(TAG, "PARENT_CONNECTED su ROOT — configurazione errata?");
        break;

    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child = event_data;
        ESP_LOGI(TAG, "Nodo connesso: AID=%d", child->aid);
        break;
    }

    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child = event_data;
        ESP_LOGW(TAG, "Nodo disconnesso: AID=%d", child->aid);
        break;
    }

    case MESH_EVENT_STOPPED:
        s_connected = false;
        ESP_LOGW(TAG, "Mesh fermata");
        break;

    default:
        break;
    }
}

void mesh_manager_init(void)
{
    s_mesh_events = xEventGroupCreate();

    // Inizializza stack Wi-Fi
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Risparmio energetico: modem sleep compatibile con mesh
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // Potenza TX ridotta: < 5m nel camper, 10 dBm è più che sufficiente
    esp_wifi_set_max_tx_power(40);

    // Registra handler eventi mesh
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID,
                                               mesh_event_handler, NULL));

    // Configurazione mesh (mesh_cfg_t in IDF 6.0 — non più esp_mesh_cfg_t)
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    cfg.channel                   = MESH_CHANNEL;
    cfg.mesh_ap.max_connection    = MESH_MAX_CHILDREN;

    memcpy(cfg.mesh_id.addr, MESH_ID, sizeof(cfg.mesh_id.addr));
    strlcpy((char *)cfg.mesh_ap.password, MESH_PASSWORD,
            sizeof(cfg.mesh_ap.password));

    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_fix_root(true));
    // IDF 6.0: esp_mesh_set_self_organized richiede 2 argomenti (enable, select_parent)
    ESP_ERROR_CHECK(esp_mesh_set_self_organized(false, false));
    ESP_ERROR_CHECK(esp_mesh_set_type(MESH_ROOT));
    ESP_ERROR_CHECK(esp_mesh_start());

    // Attendi che la mesh sia pronta
    EventBits_t bits = xEventGroupWaitBits(s_mesh_events, MESH_STARTED_BIT,
                                            pdFALSE, pdTRUE,
                                            pdMS_TO_TICKS(MESH_TIMEOUT_MS));
    if (!(bits & MESH_STARTED_BIT)) {
        ESP_LOGE(TAG, "Timeout avvio mesh — riavvio");
        esp_restart();
    }
}

bool mesh_manager_is_connected(void)
{
    return s_connected;
}
