#include "fresh_water_node.hpp"
#include "esp_log.h"
#include <cstring>

namespace domoc {

static const char* TAG = "fresh_water";

const NodeDescriptor FreshWaterNode::kDesc = {
    .node_icon      = ICON_VALVE_FRESH,
    .action_count   = 2,
    .property_count = 1,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,  ICON_ACT_OPEN,  CTRL_CONFIRM, 0, 0, FLAG_KEY_BLOCKED, "APRI"   },
        { ACTION_CLOSE, ICON_ACT_CLOSE, CTRL_BUTTON,  0, 0, 0,                "CHIUDI" },
        {}, {},
    },
    .properties = {
        { PROP_VALVE_ON, 0, PAYLOAD_UINT8, WIDGET_INDICATOR, 0, 1, "", "%d" },
        {}, {}, {},
    },
};

FreshWaterNode::FreshWaterNode()
    : NodeBase(NODE_ID_FRESH_WATER, NodeType::FRESH_WATER, "FRESH_WATER") {}

void FreshWaterNode::gpio_init() {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << GPIO_REL1;
    cfg.mode         = GPIO_MODE_OUTPUT;
    gpio_config(&cfg);
    gpio_set_level(GPIO_REL1, 0); // NC chiusa a riposo
}

void FreshWaterNode::start_tasks() {
    gpio_init();
}

void FreshWaterNode::valve_open() {
    gpio_set_level(GPIO_REL1, 1);
    valve_state_ = true;
    ESP_LOGI(TAG, "valvola aperta");
}

void FreshWaterNode::valve_close() {
    gpio_set_level(GPIO_REL1, 0);
    valve_state_ = false;
    ESP_LOGI(TAG, "valvola chiusa");
}

const NodeDescriptor& FreshWaterNode::descriptor() const { return kDesc; }

size_t FreshWaterNode::build_status_payload(uint8_t* out, size_t max) {
    if (max < sizeof(FreshWaterStatus)) return 0;
    FreshWaterStatus s{};
    s.valve_open = valve_state_ ? 1 : 0;
    s.error_code = error_code_;
    std::memcpy(out, &s, sizeof(s));
    return sizeof(s);
}

void FreshWaterNode::on_command(const CmdPayload& cmd, uint8_t /*seq*/) {
    switch (cmd.action_code) {
    case ACTION_OPEN:   valve_open();  send_status_update(); break;
    case ACTION_CLOSE:  valve_close(); send_status_update(); break;
    default: break;
    }
}

void FreshWaterNode::on_key_on(const KeyOnPayload& /*p*/) {
    // La valvola acque chiare non reagisce automaticamente a KEY_ON.
}

} // namespace domoc
