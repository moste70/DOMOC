#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "mesh_manager.h"
#include "node_registry.h"
#include "mesh_rx_task.h"
#include "mesh_tx_task.h"
#include "heartbeat_monitor.h"
#include "nvs_persist.h"
#include "ota_distributor.h"

static const char *TAG = "ROOT_MAIN";

#define FW_MAJOR    0
#define FW_MINOR    1
#define FW_PATCH    0

// ── Priorità e stack task ─────────────────────────────────────────────────
#define TASK_PRIO_MESH_RX       5
#define TASK_PRIO_MESH_TX       5
#define TASK_PRIO_HB_MONITOR    4
#define TASK_PRIO_NVS_PERSIST   2
#define TASK_PRIO_OTA           1

#define TASK_STACK_MESH_RX      4096
#define TASK_STACK_MESH_TX      4096
#define TASK_STACK_HB_MONITOR   2048
#define TASK_STACK_NVS_PERSIST  2048
#define TASK_STACK_OTA          8192

// ── LED di stato (GPIO18 = verde, GPIO19 = rosso) ─────────────────────────
#define GPIO_LED_GREEN  18
#define GPIO_LED_RED    19

static void led_init(void)
{
    // TODO: gpio_config per GPIO18 e GPIO19 output push-pull
}

static void led_set_mesh_ok(bool ok)
{
    // TODO: gpio_set_level(GPIO_LED_GREEN, ok ? 1 : 0)
    //        gpio_set_level(GPIO_LED_RED,   ok ? 0 : 1)
}

// ── Sequenza di inizializzazione ──────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "DomoC ROOT v%d.%d.%d — avvio", FW_MAJOR, FW_MINOR, FW_PATCH);

    // 1. Flash NVS — deve essere prima di tutto
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS: cancellazione e reinit");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Netif (richiesto da ESP-Mesh)
    esp_netif_init();

    // 3. LED di stato
    led_init();
    led_set_mesh_ok(false);

    // 4. Registry nodi — carica da NVS i nodi già noti (tutti OFFLINE al boot)
    node_registry_init();
    ESP_LOGI(TAG, "Registry: %d nodi caricati da NVS", node_registry_count());

    // 5. NVS persist task — scrittura asincrona
    nvs_persist_init();
    xTaskCreate(nvs_persist_task, "nvs_persist", TASK_STACK_NVS_PERSIST,
                NULL, TASK_PRIO_NVS_PERSIST, NULL);

    // 6. Coda TX — deve esistere prima dell'avvio mesh
    mesh_tx_queue_init();

    // 7. ESP-Mesh init come root fisso
    mesh_manager_init();     // blocca finché mesh non è pronta

    led_set_mesh_ok(true);
    ESP_LOGI(TAG, "Mesh pronta — avvio task");

    // 8. Task principali
    xTaskCreate(mesh_rx_task,        "mesh_rx",      TASK_STACK_MESH_RX,
                NULL, TASK_PRIO_MESH_RX, NULL);

    xTaskCreate(mesh_tx_task,        "mesh_tx",      TASK_STACK_MESH_TX,
                NULL, TASK_PRIO_MESH_TX, NULL);

    xTaskCreate(heartbeat_monitor_task, "hb_monitor", TASK_STACK_HB_MONITOR,
                NULL, TASK_PRIO_HB_MONITOR, NULL);

    xTaskCreate(ota_distributor_task, "ota",          TASK_STACK_OTA,
                NULL, TASK_PRIO_OTA, NULL);

    // app_main termina: FreeRTOS scheduler già in esecuzione, i task continuano
}
