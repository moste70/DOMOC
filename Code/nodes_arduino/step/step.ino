// DomoC — Nodo STEP (Arduino + painlessMesh)
//
// Librerie richieste (Library Manager):
//   - painlessMesh   (https://github.com/gmag11/painlessMesh)
//   - Adafruit SHT31 (+ Adafruit BusIO)
//   - FastLED
//
// Board: ESP32S3 Dev Module — Flash: 8MB, Partition: Huge APP (OTA possibile)

#include <painlessMesh.h>
#include <FastLED.h>
#include "config.h"
#include "motor.h"
#include "sensors.h"

// ── Stato macchina ───────────────────────────────────────────────────────────
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

static StepState state = STATE_INITIALIZING;
static uint8_t   my_id = NODE_ID_STEP;   // fisso per questo nodo
static uint8_t   seq   = 0;
static bool      registered = false;
static bool      key_on     = false;

// Timing
static uint32_t motor_start_ms  = 0;
static uint32_t last_heartbeat  = 0;
static uint32_t heartbeat_interval = HEARTBEAT_MS;
static uint32_t last_sht31_ms   = 0;
static uint32_t last_mesh_rx_ms = 0;   // usato per standalone detection
static uint32_t last_fc_trigger_ms = 0;

// Sensori
static float temperature = 0.0f;
static float humidity    = 0.0f;

// Mesh
static painlessMesh mesh;
static uint32_t root_node_id = 0;   // ID mesh (non logico) del ROOT

// LED
static CRGB led[1];

// ── Strutture protocollo binario ─────────────────────────────────────────────
// Devono rimanere identiche a mesh_protocol.hpp per compatibilità con ROOT/HMI

#pragma pack(push, 1)
struct MeshMsg {
    uint8_t msg_type;
    uint8_t src_id;
    uint8_t dst_id;
    uint8_t seq_num;
    uint8_t payload[200];
};

struct StepStatusPayload {
    uint8_t  state;          // offset 0
    uint8_t  fc_closed;      // offset 1
    uint8_t  fc_open;        // offset 2
    uint8_t  error_code;     // offset 3
    uint16_t last_move_ms;   // offset 4
    uint8_t  _pad;           // offset 6
    float    temperature;    // offset 7
    float    humidity;       // offset 11
};                           // 15 byte

struct RegPayload {
    char    name[16];
    uint8_t node_type;
    uint8_t reconnect;
    uint8_t mac[6];
};

struct CmdPayload {
    uint8_t action_code;
    uint8_t param;
};
#pragma pack(pop)

// ── CRC8 (CRC-8/MAXIM) ───────────────────────────────────────────────────────
static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc ^ b) & 1) crc = (crc >> 1) ^ 0x8C;
            else                crc >>= 1;
            b >>= 1;
        }
    }
    return crc;
}

// ── LED stato ────────────────────────────────────────────────────────────────
static void set_led_for_state(StepState s) {
    static uint32_t blink_ms = 0;
    static bool     blink_on = false;
    uint32_t now = millis();
    bool do_blink = false;
    uint32_t period = 500;

    switch (s) {
        case STATE_INITIALIZING: do_blink = true;  led[0] = CRGB::White;  break;
        case STATE_CLOSED:       led[0] = CRGB::Green;                     break;
        case STATE_OPEN:         led[0] = CRGB::Blue;                      break;
        case STATE_OPENING:      do_blink = true;  led[0] = CRGB::Blue;   break;
        case STATE_CLOSING:      do_blink = true;  led[0] = CRGB::Orange; break;
        case STATE_AUTO_CLOSING: do_blink = true;  period = 200;
                                 led[0] = CRGB::Red;                       break;
        case STATE_ERROR:        led[0] = CRGB::Red;                       break;
        case STATE_STANDALONE:   led[0] = CRGB::White;                     break;
    }

    if (do_blink) {
        if (now - blink_ms >= period) {
            blink_ms = now;
            blink_on = !blink_on;
        }
        if (!blink_on) led[0] = CRGB::Black;
    }

    FastLED.show();
}

// ── Invio messaggi mesh ──────────────────────────────────────────────────────
static void send_msg(MeshMsg &msg, size_t payload_len) {
    size_t total = 4 + payload_len;
    uint8_t buf[total + 1];
    memcpy(buf, &msg, total);
    buf[total] = crc8(buf, total);

    // painlessMesh lavora con stringhe — serializziamo in base64-like hex
    // Usiamo String con dati raw in base16 per semplicità; l'HMI e il ROOT
    // devono fare lo stesso. Alternativa: usare payload JSON con campo "hex".
    String hex;
    hex.reserve(total * 2 + 2);
    for (size_t i = 0; i < total + 1; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
        hex += tmp;
    }

    if (msg.dst_id == NODE_ID_BROADCAST) {
        mesh.sendBroadcast(hex);
    } else if (root_node_id != 0) {
        mesh.sendSingle(root_node_id, hex);
    }
}

static String hex_encode(const uint8_t* data, size_t len) {
    String s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", data[i]);
        s += tmp;
    }
    return s;
}

// ── Registrazione al ROOT ────────────────────────────────────────────────────
static void send_register(bool reconnect) {
    MeshMsg msg{};
    msg.msg_type = MSG_REGISTER;
    msg.src_id   = my_id;
    msg.dst_id   = NODE_ID_ROOT;
    msg.seq_num  = seq++;

    RegPayload reg{};
    strlcpy(reg.name, "STEP", sizeof(reg.name));
    reg.node_type = 0x02;
    reg.reconnect = reconnect ? 1 : 0;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    memcpy(reg.mac, mac, 6);

    memcpy(msg.payload, &reg, sizeof(reg));
    send_msg(msg, sizeof(reg));
}

// ── Heartbeat + stato ────────────────────────────────────────────────────────
static void send_status() {
    MeshMsg msg{};
    msg.msg_type = MSG_HEARTBEAT;
    msg.src_id   = my_id;
    msg.dst_id   = NODE_ID_ROOT;
    msg.seq_num  = seq++;

    StepStatusPayload s{};
    s.state       = (uint8_t)state;
    s.fc_closed   = !digitalRead(PIN_FC_CLOSED) ? 1 : 0;
    s.fc_open     = !digitalRead(PIN_FC_OPEN)   ? 1 : 0;
    s.error_code  = (state == STATE_ERROR) ? 1 : 0;
    s.temperature = temperature;
    s.humidity    = humidity;

    memcpy(msg.payload, &s, sizeof(s));
    send_msg(msg, sizeof(s));
}

// ── Transizioni di stato ──────────────────────────────────────────────────────
static void enter_state(StepState next) {
    state = next;

    if (next == STATE_OPEN) {
        // broadcast: scaletta aperta
        MeshMsg msg{};
        msg.msg_type = MSG_STEP_OPEN;
        msg.src_id   = my_id;
        msg.dst_id   = NODE_ID_BROADCAST;
        msg.seq_num  = seq++;
        uint8_t payload[3] = { 0, key_on ? 1u : 0u, 0 };
        memcpy(msg.payload, payload, 3);
        send_msg(msg, 3);
    }

    send_status();
}

// ── Finecorsa (polling con debounce) ─────────────────────────────────────────
static bool fc_closed_prev = false;
static bool fc_open_prev   = false;

static void check_endstops() {
    bool fc_closed = !digitalRead(PIN_FC_CLOSED);
    bool fc_open   = !digitalRead(PIN_FC_OPEN);
    uint32_t now   = millis();

    // Fronte attivo FC_CLOSED
    if (fc_closed && !fc_closed_prev) {
        if (now - last_fc_trigger_ms > FC_DEBOUNCE_MS) {
            last_fc_trigger_ms = now;
            if (state == STATE_CLOSING || state == STATE_AUTO_CLOSING) {
                motor_stop();
                enter_state(STATE_CLOSED);
            }
        }
    }

    // Fronte attivo FC_OPEN
    if (fc_open && !fc_open_prev) {
        if (now - last_fc_trigger_ms > FC_DEBOUNCE_MS) {
            last_fc_trigger_ms = now;
            if (state == STATE_OPENING) {
                motor_stop();
                enter_state(STATE_OPEN);
            }
        }
    }

    fc_closed_prev = fc_closed;
    fc_open_prev   = fc_open;
}

// ── Timeout motore ────────────────────────────────────────────────────────────
static void check_motor_timeout() {
    if (!motor_running()) return;
    if (millis() - motor_start_ms > MOTOR_TIMEOUT_MS) {
        motor_stop();
        enter_state(STATE_ERROR);
    }
}

// ── Parsing messaggi in arrivo ───────────────────────────────────────────────
static bool hex_decode(const String &hex, uint8_t* out, size_t &out_len) {
    size_t n = hex.length();
    if (n % 2 != 0) return false;
    out_len = n / 2;
    for (size_t i = 0; i < out_len; i++) {
        char tmp[3] = { hex[i*2], hex[i*2+1], 0 };
        out[i] = (uint8_t)strtol(tmp, nullptr, 16);
    }
    return true;
}

static void on_mesh_message(uint32_t from, String &raw) {
    last_mesh_rx_ms = millis();

    uint8_t buf[210];
    size_t  buf_len = 0;
    if (!hex_decode(raw, buf, buf_len)) return;
    if (buf_len < 5) return;  // header 4 + crc 1 minimo

    // Verifica CRC
    uint8_t expected = crc8(buf, buf_len - 1);
    if (expected != buf[buf_len - 1]) return;

    MeshMsg *msg = (MeshMsg*)buf;

    // Filtra messaggi non destinati a noi
    if (msg->dst_id != my_id && msg->dst_id != NODE_ID_BROADCAST) return;

    switch (msg->msg_type) {

        case MSG_REGISTER_ACK:
            root_node_id = from;
            registered   = true;
            // Invia il descriptor dopo la registrazione
            // (omesso per brevità — struttura identica a node_descriptor.hpp)
            break;

        case MSG_KEY_ON: {
            key_on = true;
            if (state == STATE_OPEN || state == STATE_OPENING) {
                motor_stop();
                motor_start(DIR_CLOSE);
                motor_start_ms = millis();
                enter_state(STATE_AUTO_CLOSING);
            }
            break;
        }

        case MSG_KEY_OFF:
            key_on = false;
            break;

        case MSG_COMMAND: {
            // Blocca comandi con chiave inserita
            CmdPayload *cmd = (CmdPayload*)msg->payload;
            if (key_on && cmd->action_code != ACTION_GET_STATUS) break;

            if (cmd->action_code == ACTION_OPEN) {
                if (state == STATE_CLOSED) {
                    motor_start(DIR_OPEN);
                    motor_start_ms = millis();
                    enter_state(STATE_OPENING);
                }
            } else if (cmd->action_code == ACTION_CLOSE) {
                if (state == STATE_OPEN) {
                    motor_start(DIR_CLOSE);
                    motor_start_ms = millis();
                    enter_state(STATE_CLOSING);
                }
            } else if (cmd->action_code == ACTION_GET_STATUS) {
                send_status();
            }
            break;
        }
    }
}

// ── Callback painlessMesh ─────────────────────────────────────────────────────
static void on_node_time_adjusted(int32_t offset) {}

static void on_changed_connections() {
    // Quando la topologia cambia, cerca il ROOT (nodo root della mesh)
    // painlessMesh non espone direttamente quale nodo è root; usiamo un
    // approccio semplice: il ROOT si identifica rispondendo a MSG_REGISTER
    if (!registered) send_register(false);
}

// ── Standalone mode ───────────────────────────────────────────────────────────
static bool standalone = false;

static void check_standalone() {
    if (standalone) {
        if (mesh.getNodeList().size() > 0) {
            standalone = false;
            send_register(true);
            send_status();
        }
        return;
    }

    // Entra in standalone se non si ricevono messaggi da ROOT per 30s
    // e la mesh non ha altri nodi
    if (registered && millis() - last_mesh_rx_ms > MESH_TIMEOUT_MS) {
        if (mesh.getNodeList().size() == 0) {
            standalone = true;
            motor_stop();   // sicurezza: ferma eventuale movimento in corso
            enter_state(STATE_STANDALONE);
        }
    }
}

// ── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_LED, GRB>(led, 1);
    FastLED.setBrightness(40);

    motor_init();

    pinMode(PIN_FC_CLOSED, INPUT_PULLUP);
    pinMode(PIN_FC_OPEN,   INPUT_PULLUP);

    if (!sensors_init()) {
        Serial.println("SHT31 non trovato");
    }

    // Leggi lo stato iniziale dai finecorsa
    bool fc_c = !digitalRead(PIN_FC_CLOSED);
    bool fc_o = !digitalRead(PIN_FC_OPEN);
    if      (fc_c) state = STATE_CLOSED;
    else if (fc_o) state = STATE_OPEN;
    else           state = STATE_OPEN;   // posizione ignota → assume aperto per sicurezza

    mesh.setDebugMsgTypes(ERROR | STARTUP);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&on_mesh_message);
    mesh.onChangedConnections(&on_changed_connections);

    // Jitter casuale sull'heartbeat per evitare burst simultanei
    heartbeat_interval = HEARTBEAT_MS + random(0, HEARTBEAT_JITTER_MS);

    last_mesh_rx_ms = millis();
    send_register(false);
}

void loop() {
    mesh.update();

    uint32_t now = millis();

    check_endstops();
    check_motor_timeout();
    check_standalone();

    // Lettura sensore ogni 60s
    if (now - last_sht31_ms >= SHT31_INTERVAL_MS) {
        last_sht31_ms = now;
        sensors_read(temperature, humidity);
    }

    // Heartbeat periodico
    if (now - last_heartbeat >= heartbeat_interval) {
        last_heartbeat     = now;
        heartbeat_interval = HEARTBEAT_MS + random(0, HEARTBEAT_JITTER_MS);
        if (registered) send_status();
    }

    set_led_for_state(state);
}
