#include "thermo_loft_node.hpp"
#include <cstring>

namespace domoc {

const NodeDescriptor ThermoLoftNode::kDesc = {
    .node_icon      = ICON_THERMOMETER,
    .action_count   = 2,
    .property_count = 2,
    ._pad           = 0,
    .actions = {
        { ACTION_TEMP_UP, ICON_ACT_TEMP_UP, CTRL_STEPPER, 0, 0, 0, "+" },
        { ACTION_TEMP_DN, ICON_ACT_TEMP_DN, CTRL_STEPPER, 0, 0, 0, "-" },
        {}, {},
    },
    .properties = {
        { PROP_TEMPERATURE, 2, PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 150, 350, "\xC2\xB0\x43", "%.1f" },
        { PROP_SETPOINT,    6, PAYLOAD_FLOAT32, WIDGET_VALUE_UNIT,  150, 300, "\xC2\xB0\x43", "%.1f" },
        {}, {},
    },
};

ThermoLoftNode::ThermoLoftNode()
    : NodeBase(NODE_ID_THERMO_LOFT, NodeType::THERMO_LOFT, "THERMO_LOFT") {}

const NodeDescriptor& ThermoLoftNode::descriptor() const { return kDesc; }

size_t ThermoLoftNode::build_status_payload(uint8_t* out, size_t max) {
    if (max < sizeof(ThermoLoftStatus)) return 0;
    ThermoLoftStatus s{};
    s.valve_open  = valve_open_ ? 1 : 0;
    s.error_code  = error_code_;
    s.temperature = temperature_;
    s.setpoint    = setpoint_;
    std::memcpy(out, &s, sizeof(s));
    return sizeof(s);
}

void ThermoLoftNode::on_command(const CmdPayload& cmd, uint8_t /*seq*/) {
    switch (cmd.action_code) {
    case ACTION_TEMP_UP: setpoint_ += 0.5f; send_status_update(); break;
    case ACTION_TEMP_DN: setpoint_ -= 0.5f; send_status_update(); break;
    default: break;
    }
}

} // namespace domoc
