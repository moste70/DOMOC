#include "fresh_water_node.hpp"
#include "esp_log.h"

extern "C" void app_main(void) {
    static domoc::FreshWaterNode node;
    domoc::MeshConfig cfg{};

    esp_err_t err = node.begin(cfg);
    if (err != ESP_OK) {
        ESP_LOGE("main", "begin() fallito: %s", esp_err_to_name(err));
        return;
    }
    node.start_tasks();
}
