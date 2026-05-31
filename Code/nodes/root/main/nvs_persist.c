#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "nvs_persist.h"
#include "node_registry.h"

static const char *TAG = "NVS_PERSIST";

// Scrive su NVS al massimo ogni 10s per limitare usura flash
#define NVS_FLUSH_INTERVAL_MS  10000

void nvs_persist_init(void)
{
    // Nulla da inizializzare — usa direttamente node_registry_is_dirty()
}

void nvs_persist_task(void *pvParam)
{
    ESP_LOGI(TAG, "nvs_persist_task avviato");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(NVS_FLUSH_INTERVAL_MS));

        if (node_registry_is_dirty()) {
            node_registry_save_to_nvs();
            ESP_LOGD(TAG, "Registry persistita su NVS");
        }
    }
}
