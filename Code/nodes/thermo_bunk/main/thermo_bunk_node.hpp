#pragma once
#include "node_base.hpp"

namespace domoc {

struct __attribute__((packed)) ThermoStatus {
    uint8_t  valve_open;    // offset 0 — 1 se valvola aria calda aperta
    uint8_t  error_code;    // offset 1
    float    temperature;   // offset 2–5 — °C letta dal sensore
    float    setpoint;      // offset 6–9 — °C setpoint attuale
};

// THERMO_BUNK — termostato letto a castello, valvola aria calda.
// Stub: logica PID e lettura sensore implementati in Fase 5.
class ThermoBunkNode : public NodeBase {
public:
    ThermoBunkNode();

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
