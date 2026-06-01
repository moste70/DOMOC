#pragma once
#include "node_base.hpp"
#include "driver/gpio.h"

namespace domoc {

struct __attribute__((packed)) FreshWaterStatus {
    uint8_t valve_open;  // offset 0 — 1 se valvola aperta
    uint8_t error_code;  // offset 1
};

// FRESH_WATER — elettrovalvola acque chiare NC, relay singolo REL1 (GPIO17).
class FreshWaterNode : public NodeBase {
public:
    FreshWaterNode();
    void start_tasks();

protected:
    const NodeDescriptor& descriptor() const override;
    size_t build_status_payload(uint8_t* out, size_t max) override;
    void on_command(const CmdPayload& cmd, uint8_t seq) override;
    void on_key_on(const KeyOnPayload& p) override;

private:
    static constexpr gpio_num_t GPIO_REL1 = GPIO_NUM_17; // valvola NC

    void valve_open();
    void valve_close();
    void gpio_init();

    bool     valve_state_ = false;
    uint8_t  error_code_  = 0;

    static const NodeDescriptor kDesc;
};

} // namespace domoc
