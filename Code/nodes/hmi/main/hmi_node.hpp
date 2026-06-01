#pragma once
#include "node_base.hpp"

namespace domoc {

// HMI — Waveshare ESP32-S3-Knob-Touch-LCD-1.8.
// Stub: display QSPI ST77916, LVGL, encoder e touch sono implementati in Fase 3.
// Documentazione: Document/nodes/hmi.md
class HmiNode : public NodeBase {
public:
    HmiNode();

protected:
    const NodeDescriptor& descriptor() const override;
    size_t build_status_payload(uint8_t* out, size_t max) override;

private:
    static const NodeDescriptor kDesc;
};

} // namespace domoc
