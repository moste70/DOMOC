#include "step_node.hpp"
#include "esp_log.h"

static const char* TAG = "main";

extern "C" void app_main(void) {
    static domoc::StepNode node;

    domoc::MeshConfig cfg{};
    // cfg usa i default definiti in MeshConfig (mesh_id, channel, password).

    esp_err_t err = node.begin(cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "begin() fallito: %s", esp_err_to_name(err));
        return;
    }

    node.start_tasks();

    // app_main può terminare: i task FreeRTOS continuano in background.
}
