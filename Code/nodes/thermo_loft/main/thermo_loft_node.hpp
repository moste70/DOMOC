#pragma once
#include "node_base.hpp"

namespace domoc {

struct __attribute__((packed)) ThermoLoftStatus {
    uint8_t valve_open;   // offset 0
    uint8_t error_code;   // offset 1
    float   temperature;  // offset 2–5
    float   setpoint;     // offset 6–9
};

// THERMO_LOFT — termostato mansarda, valvola aria calda.
// Stub: logica implementata in Fase 5.
class ThermoLoftNode : public NodeBase {
public:
    ThermoLoftNode();

protected:
    const NodeDescriptor& descriptor() const override;
    size_t build_status_payload(uint8_t* out, size_t max) override;
    void on_command(const CmdPayload& cmd, uint8_t seq) override;

private:
    float   setpoint_    = 20.0f;
    float   temperature_ = 0.0f;
    bool    valve_open_  = false;
    uint8_t error_code_  = 0;

    static const NodeDescriptor kDesc;
};

} // namespace domoc
