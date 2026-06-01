#include "step_node.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

namespace domoc {

static const char* TAG = "step";

// ── Descriptor statico (auto-descrizione per l'HMI) ─────────────────────────
// Offset nel payload StepStatus: state=0, temperature=7, humidity=11.
const NodeDescriptor StepNode::kDesc = {
    .node_icon      = ICON_STEP,
    .action_count   = 3,
    .property_count = 3,
    ._pad           = 0,
    .actions = {
        { ACTION_OPEN,       ICON_ACT_OPEN,  CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "APRI"   },
        { ACTION_CLOSE,      ICON_ACT_CLOSE, CTRL_BUTTON, 0, 0, FLAG_KEY_BLOCKED, "CHIUDI" },
        { ACTION_GET_STATUS, ICON_ACT_INFO,  CTRL_BUTTON, 0, 0, 0,                "INFO"   },
        {},
    },
    .properties = {
        { PROP_STATE,       0,  PAYLOAD_UINT8,   WIDGET_LABEL,       0,   0,    "",   "%s"   },
        { PROP_TEMPERATURE, 7,  PAYLOAD_FLOAT32, WIDGET_THERMOMETER, 150, 400,  "\xC2\xB0\x43", "%.1f" },
        { PROP_HUMIDITY,    11, PAYLOAD_FLOAT32, WIDGET_PROGRESS,    0,   1000, "%",  "%.0f" },
        {},
    },
};

StepNode::StepNode() : NodeBase(NODE_ID_STEP, NodeType::STEP, "STEP") {}

// ── Inizializzazione hardware ────────────────────────────────────────────────

void StepNode::gpio_init() {
    gpio_config_t out_cfg = {};
    out_cfg.pin_bit_mask = (1ULL << GPIO_HB_DIR_A) |
                           (1ULL << GPIO_HB_DIR_B) |
                           (1ULL << GPIO_HB_EN);
    out_cfg.mode         = GPIO_MODE_OUTPUT;
    out_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    out_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&out_cfg);

    // Stato sicuro: motore scollegato.
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);
    gpio_set_level(GPIO_HB_DIR_B, 0);

    // Finecorsa: input pull-up, no interrupt (polling in fc_loop).
    gpio_config_t in_cfg = {};
    in_cfg.pin_bit_mask  = (1ULL << GPIO_FC_CLOSED) | (1ULL << GPIO_FC_OPEN);
    in_cfg.mode          = GPIO_MODE_INPUT;
    in_cfg.pull_up_en    = GPIO_PULLUP_ENABLE;
    in_cfg.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    in_cfg.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&in_cfg);
}

void StepNode::i2c_init() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.clk_source              = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port                = I2C_NUM_0;
    bus_cfg.scl_io_num              = GPIO_SCL;
    bus_cfg.sda_io_num              = GPIO_SDA;
    bus_cfg.glitch_ignore_cnt       = 7;
    bus_cfg.flags.enable_internal_pullup = false; // pull-up 4.7kΩ fissi sul PCB

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length      = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address       = SHT31_I2C_ADDR;
    dev_cfg.scl_speed_hz         = 400000;

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &sht31_dev_));
}

// ── Task avvio ───────────────────────────────────────────────────────────────

void StepNode::start_tasks() {
    gpio_init();
    i2c_init();

    fc_queue_ = xQueueCreate(4, sizeof(uint8_t));

    // Determina stato iniziale dai finecorsa fisici.
    bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0; // active-LOW
    bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;
    if (fc_c)      transition_to(STATE_CLOSED);
    else if (fc_o) transition_to(STATE_OPEN);

    xTaskCreate(fc_trampoline,     "fc_task",     2048, this, 7, &fc_task_);
    xTaskCreate(ctrl_trampoline,   "ctrl_task",   3072, this, 6, &ctrl_task_);
    xTaskCreate(sensor_trampoline, "sensor_task", 3072, this, 3, &sensor_task_);
}

void StepNode::ctrl_trampoline(void* s)   { static_cast<StepNode*>(s)->ctrl_loop(); }
void StepNode::fc_trampoline(void* s)     { static_cast<StepNode*>(s)->fc_loop(); }
void StepNode::sensor_trampoline(void* s) { static_cast<StepNode*>(s)->sensor_loop(); }

// ── Loop finecorsa (polling con debounce) ────────────────────────────────────

void StepNode::fc_loop() {
    bool prev_closed = false;
    bool prev_open   = false;

    while (tasks_running_) {
        bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0;
        bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;

        if (fc_c && !prev_closed) {
            vTaskDelay(pdMS_TO_TICKS(FC_DEBOUNCE_MS));
            if (gpio_get_level(GPIO_FC_CLOSED) == 0) {
                uint8_t evt = FC_EVT_CLOSED;
                xQueueSend(fc_queue_, &evt, 0);
            }
        }
        if (fc_o && !prev_open) {
            vTaskDelay(pdMS_TO_TICKS(FC_DEBOUNCE_MS));
            if (gpio_get_level(GPIO_FC_OPEN) == 0) {
                uint8_t evt = FC_EVT_OPEN;
                xQueueSend(fc_queue_, &evt, 0);
            }
        }
        prev_closed = fc_c;
        prev_open   = fc_o;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(nullptr);
}

// ── Loop controllo (macchina a stati) ────────────────────────────────────────

void StepNode::ctrl_loop() {
    uint8_t fc_evt;

    while (tasks_running_) {
        if (xQueueReceive(fc_queue_, &fc_evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            if (motor_start_ms_ > 0)
                last_move_ms_ = (uint16_t)(now - motor_start_ms_);
            motor_stop();
            motor_start_ms_ = 0;
            error_code_ = ERR_NONE;

            if (fc_evt == FC_EVT_CLOSED) {
                transition_to(STATE_CLOSED);
            } else {
                transition_to(STATE_OPEN);
                StepOpenPayload p{};
                p.open_reason   = 0;
                p.key_on_active = key_on_active() ? 1 : 0;
                p.temperature   = temperature_;
                broadcast(MSG_STEP_OPEN, &p, sizeof(p));
            }
            send_status_update();
            continue;
        }

        // Timeout motore: ferma il motore se il finecorsa non arriva entro N ms.
        if (state_ == STATE_OPENING || state_ == STATE_CLOSING ||
            state_ == STATE_AUTO_CLOSING) {
            uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - motor_start_ms_;
            if (motor_start_ms_ > 0 && elapsed > MOTOR_TIMEOUT_MS) {
                motor_stop();
                last_move_ms_   = (uint16_t)elapsed;
                motor_start_ms_ = 0;
                error_code_     = ERR_TIMEOUT;
                transition_to(STATE_ERROR);
                send_status_update();
                ESP_LOGE(TAG, "timeout motore — STATE_ERROR");
            }
        }
    }
    vTaskDelete(nullptr);
}

// ── Loop sensore SHT31 ───────────────────────────────────────────────────────

void StepNode::sensor_loop() {
    vTaskDelay(pdMS_TO_TICKS(500)); // attende stabilizzazione alimentazione

    while (tasks_running_) {
        float t, h;
        if (sht31_read(t, h) == ESP_OK) {
            temperature_ = t;
            humidity_    = h;
            ESP_LOGD(TAG, "SHT31: %.1f°C  %.0f%%RH", t, h);
        } else {
            ESP_LOGW(TAG, "SHT31 lettura fallita");
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
    vTaskDelete(nullptr);
}

// ── Driver SHT31 — nuova API I2C master (IDF 5.x) ───────────────────────────

esp_err_t StepNode::sht31_read(float& temp_c, float& hum_pct) {
    // Single-shot high repeatability: cmd 0x2C 0x06
    const uint8_t cmd[2] = {0x2C, 0x06};
    esp_err_t err = i2c_master_transmit(sht31_dev_, cmd, sizeof(cmd), 100);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20)); // misura ~15ms ad alta ripetibilità

    uint8_t data[6] = {};
    err = i2c_master_receive(sht31_dev_, data, sizeof(data), 100);
    if (err != ESP_OK) return err;

    uint16_t t_raw = ((uint16_t)data[0] << 8) | data[1];
    uint16_t h_raw = ((uint16_t)data[3] << 8) | data[4];
    temp_c  = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    hum_pct =          100.0f * (float)h_raw / 65535.0f;
    return ESP_OK;
}

// ── Controllo motore H-bridge ────────────────────────────────────────────────
// Sequenza obbligatoria: EN=OFF → cambia DIR → EN=ON (evita cortocircuito).

void StepNode::motor_open() {
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_B, 0);
    gpio_set_level(GPIO_HB_DIR_A, 1);
    gpio_set_level(GPIO_HB_EN,    1);
    motor_start_ms_ = (uint32_t)(esp_timer_get_time() / 1000);
}

void StepNode::motor_close() {
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);
    gpio_set_level(GPIO_HB_DIR_B, 1);
    gpio_set_level(GPIO_HB_EN,    1);
    motor_start_ms_ = (uint32_t)(esp_timer_get_time() / 1000);
}

void StepNode::motor_stop() {
    gpio_set_level(GPIO_HB_EN,    0);
    gpio_set_level(GPIO_HB_DIR_A, 0);
    gpio_set_level(GPIO_HB_DIR_B, 0);
}

void StepNode::transition_to(StepState s) {
    state_ = s;
    ESP_LOGI(TAG, "stato: %d", (int)s);
}

// ── Hook NodeBase ────────────────────────────────────────────────────────────

const NodeDescriptor& StepNode::descriptor() const {
    return kDesc;
}

size_t StepNode::build_status_payload(uint8_t* out, size_t max) {
    if (max < sizeof(StepStatus)) return 0;
    StepStatus s{};
    s.state        = (uint8_t)state_;
    s.fc_closed    = gpio_get_level(GPIO_FC_CLOSED) == 0 ? 1 : 0;
    s.fc_open      = gpio_get_level(GPIO_FC_OPEN)   == 0 ? 1 : 0;
    s.error_code   = error_code_;
    s.last_move_ms = last_move_ms_;
    s.temperature  = temperature_;
    s.humidity     = humidity_;
    std::memcpy(out, &s, sizeof(s));
    return sizeof(s);
}

void StepNode::on_command(const CmdPayload& cmd, uint8_t /*seq*/) {
    switch (cmd.action_code) {
    case ACTION_OPEN:
        if (state_ == STATE_CLOSED) {
            transition_to(STATE_OPENING);
            motor_open();
        }
        break;
    case ACTION_CLOSE:
        if (state_ == STATE_OPEN) {
            transition_to(STATE_CLOSING);
            motor_close();
        }
        break;
    case ACTION_GET_STATUS:
        send_status_update();
        break;
    default:
        break;
    }
}

void StepNode::on_key_on(const KeyOnPayload& /*p*/) {
    if (state_ == STATE_OPEN || state_ == STATE_OPENING) {
        transition_to(STATE_AUTO_CLOSING);
        motor_close();
        ESP_LOGI(TAG, "KEY_ON: chiusura automatica gradino");
    }
}

void StepNode::on_standalone_enter() {
    if (state_ != STATE_ERROR)
        transition_to(STATE_STANDALONE);
}

void StepNode::on_standalone_exit() {
    bool fc_c = gpio_get_level(GPIO_FC_CLOSED) == 0;
    bool fc_o = gpio_get_level(GPIO_FC_OPEN)   == 0;
    if (fc_c)      transition_to(STATE_CLOSED);
    else if (fc_o) transition_to(STATE_OPEN);
    else           transition_to(STATE_INITIALIZING);
    send_status_update();
}

} // namespace domoc
