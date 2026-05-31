#pragma once

#include "node_base.hpp"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace domoc {

enum StepState : uint8_t {
    STATE_INITIALIZING = 0,
    STATE_CLOSED       = 1,
    STATE_OPEN         = 2,
    STATE_OPENING      = 3,
    STATE_CLOSING      = 4,
    STATE_AUTO_CLOSING = 5,
    STATE_ERROR        = 6,
    STATE_STANDALONE   = 7,
};

struct __attribute__((packed)) StepStatus {
    uint8_t  state;         // offset 0
    uint8_t  fc_closed;     // offset 1 — 1 se finecorsa chiuso attivo
    uint8_t  fc_open;       // offset 2 — 1 se finecorsa aperto attivo
    uint8_t  error_code;    // offset 3
    uint16_t last_move_ms;  // offset 4–5 — durata ultimo movimento (diagnostica)
    uint8_t  _pad;          // offset 6
    float    temperature;   // offset 7–10 — °C da SHT31
    float    humidity;      // offset 11–14 — %RH da SHT31
};                          // 15 byte totali

class StepNode : public NodeBase {
public:
    StepNode();

    // Da chiamare dopo begin(): avvia i task applicativi del nodo.
    void start_tasks();

protected:
    const NodeDescriptor& descriptor() const override;
    size_t build_status_payload(uint8_t* out, size_t max) override;
    void on_command(const CmdPayload& cmd, uint8_t seq) override;
    void on_key_on(const KeyOnPayload& p) override;
    void on_standalone_enter() override;
    void on_standalone_exit() override;

private:
    // GPIO PCB v3.0
    static constexpr gpio_num_t GPIO_HB_DIR_A  = GPIO_NUM_11;
    static constexpr gpio_num_t GPIO_HB_DIR_B  = GPIO_NUM_12;
    static constexpr gpio_num_t GPIO_HB_EN     = GPIO_NUM_13;
    static constexpr gpio_num_t GPIO_FC_CLOSED = GPIO_NUM_3;   // active-LOW
    static constexpr gpio_num_t GPIO_FC_OPEN   = GPIO_NUM_4;   // active-LOW

    static constexpr gpio_num_t GPIO_SDA = GPIO_NUM_8;
    static constexpr gpio_num_t GPIO_SCL = GPIO_NUM_9;

    static constexpr uint32_t MOTOR_TIMEOUT_MS = 10000;
    static constexpr uint32_t SENSOR_PERIOD_MS = 60000;
    static constexpr uint32_t FC_DEBOUNCE_MS   = 50;
    static constexpr uint8_t  SHT31_I2C_ADDR   = 0x44;

    static constexpr uint8_t FC_EVT_CLOSED = 0;
    static constexpr uint8_t FC_EVT_OPEN   = 1;

    static void ctrl_trampoline(void* self);
    static void fc_trampoline(void* self);
    static void sensor_trampoline(void* self);
    static void IRAM_ATTR fc_isr(void* arg);

    void ctrl_loop();
    void fc_loop();
    void sensor_loop();

    void gpio_init();
    void i2c_init();
    void motor_open();
    void motor_close();
    void motor_stop();
    void transition_to(StepState s);
    esp_err_t sht31_read(float& temp_c, float& hum_pct);

    QueueHandle_t  fc_queue_    = nullptr;
    TaskHandle_t   ctrl_task_   = nullptr;
    TaskHandle_t   fc_task_     = nullptr;
    TaskHandle_t   sensor_task_ = nullptr;

    volatile StepState state_        = STATE_INITIALIZING;
    volatile bool      tasks_running_ = true;

    uint32_t motor_start_ms_ = 0;
    uint16_t last_move_ms_   = 0;
    uint8_t  error_code_     = 0;
    float    temperature_    = 0.0f;
    float    humidity_       = 0.0f;

    static const NodeDescriptor kDesc;
};

} // namespace domoc
