// DomoC — Protocollo mesh (formato sul filo)
//
// Strutture binarie scambiate tra i nodi sulla rete ESP-Mesh. Tutte le struct
// che viaggiano sul filo sono __packed__: l'offset di ogni campo e parte del
// contratto tra nodi e va mantenuto stabile (gli HMI leggono i payload usando
// gli offset dichiarati nei descriptor).
//
// Riferimento: Document/messaggi_mesh.md

#pragma once

#include <cstdint>
#include <cstddef>

namespace domoc {

// Dimensione massima del payload applicativo. L'header (4 byte) + payload entrano
// nel frame mesh; il CRC8 viene appeso in coda dal layer di trasporto, non e qui.
inline constexpr size_t MSG_PAYLOAD_MAX = 200;

// ── ID logici dei nodi ──────────────────────────────────────────────────────
// 0xFF = broadcast a tutti i nodi (nessun ACK richiesto).
enum NodeId : uint8_t {
    NODE_ID_MASTER        = 0x01,  // ROOT/MASTER della mesh
    NODE_ID_STEP          = 0x02,
    NODE_ID_GREY_WATER    = 0x03,
    NODE_ID_FRESH_WATER   = 0x04,
    NODE_ID_THERMO_BUNK   = 0x05,
    NODE_ID_THERMO_LOFT   = 0x06,
    NODE_ID_THERMO_KITCHEN= 0x07,
    NODE_ID_REAR_CAM      = 0x08,
    NODE_ID_FRONT_DOOR    = 0x09,
    NODE_ID_CAM_EXT       = 0x0A,
    NODE_ID_HMI           = 0x0B,
    NODE_ID_UNASSIGNED    = 0x00,
    NODE_ID_BROADCAST     = 0xFF,
};

// Tipo funzionale del nodo — trasmesso in MSG_REGISTER.
enum class NodeType : uint8_t {
    MASTER        = 0x01,
    STEP          = 0x02,
    GREY_WATER    = 0x03,
    FRESH_WATER   = 0x04,
    THERMO_BUNK   = 0x05,
    THERMO_LOFT   = 0x06,
    THERMO_KITCHEN= 0x07,
    REAR_CAM      = 0x08,
    FRONT_DOOR    = 0x09,
    CAM_EXT       = 0x0A,
    HMI           = 0x0B,
};

// ── Tipi di messaggio ───────────────────────────────────────────────────────
enum MsgType : uint8_t {
    MSG_COMMAND        = 0x01,  // HMI → nodo: azione manuale
    MSG_STATUS         = 0x02,  // nodo → ROOT/HMI: stato + errori
    MSG_ALERT          = 0x03,  // deprecato — usare messaggi specifici
    MSG_REGISTER       = 0x05,  // nodo → ROOT: registrazione al boot
    MSG_HEARTBEAT      = 0x06,  // nodo → ROOT: battito periodico (5s ± jitter)
    MSG_DESCRIPTOR     = 0x07,  // nodo → ROOT: auto-descrizione azioni/proprieta
    MSG_DESCRIPTOR_REQ = 0x08,  // HMI → ROOT: richiesta descriptor di un nodo
    MSG_REGISTER_ACK   = 0x09,  // ROOT → nodo: ID logico confermato
    MSG_STATUS_REQ     = 0x0A,  // HMI → ROOT: dump completo rete
    MSG_STATUS_RESP    = 0x0B,  // ROOT → HMI: forwarding heartbeat nodo
    MSG_REGISTRY_DUMP  = 0x0C,  // ROOT → HMI: dump registry + descriptor
    MSG_NODE_JOINED    = 0x0D,  // ROOT → HMI: nuovo nodo in rete
    MSG_NODE_WARNING   = 0x0E,  // ROOT → HMI: nodo silente da > 15s
    MSG_NODE_OFFLINE   = 0x0F,  // ROOT → HMI: nodo offline da > 30s
    MSG_NODE_LOST      = 0x10,  // ROOT → HMI: nodo perso da > 120s
    MSG_KEY_ON         = 0x11,  // ROOT → broadcast: chiave inserita
    MSG_KEY_OFF        = 0x12,  // ROOT → broadcast: chiave tolta
    MSG_STEP_OPEN      = 0x13,  // STEP → broadcast: scaletta aperta
    MSG_OTA_START      = 0x20,  // ROOT → nodo: avvio sessione OTA
    MSG_OTA_CHUNK      = 0x21,  // ROOT → nodo: blocco firmware
    MSG_OTA_END        = 0x22,  // ROOT → nodo: fine trasferimento
    MSG_OTA_ACK        = 0x23,  // nodo → ROOT: conferma chunk / esito OTA
};

// ── Header + frame ──────────────────────────────────────────────────────────
struct __attribute__((packed)) MeshMsg {
    uint8_t msg_type;                 // MsgType
    uint8_t src_id;                   // nodo mittente
    uint8_t dst_id;                   // nodo destinatario — 0xFF = broadcast
    uint8_t seq_num;                  // sequenza per ACK/dedup
    uint8_t payload[MSG_PAYLOAD_MAX];
};

inline constexpr size_t MSG_HEADER_LEN = 4;
inline constexpr size_t MSG_FRAME_MAX  = MSG_HEADER_LEN + MSG_PAYLOAD_MAX + 1; // +CRC8

// ── Payload comuni ──────────────────────────────────────────────────────────

struct __attribute__((packed)) CmdPayload {
    uint8_t action_code;   // ActionCode
    uint8_t param;         // parametro opzionale (es. setpoint ×10)
};

enum ActionCode : uint8_t {
    ACTION_OPEN       = 0x01,
    ACTION_CLOSE      = 0x02,
    ACTION_CAM_ON     = 0x03,
    ACTION_CAM_OFF    = 0x04,
    ACTION_LIGHT_ON   = 0x05,
    ACTION_LIGHT_OFF  = 0x06,
    ACTION_TEMP_UP    = 0x07,
    ACTION_TEMP_DN    = 0x08,
    ACTION_GET_STATUS = 0x0F,
};

struct __attribute__((packed)) RegPayload {
    char    name[16];      // es. "STEP", "GREY_WATER"
    uint8_t node_type;     // NodeType
    uint8_t reconnect;     // 1 = rientro dopo reboot (gia in registry)
    uint8_t mac[6];        // MAC fisico
};

struct __attribute__((packed)) RegAckPayload {
    uint8_t assigned_id;   // ID logico assegnato (1–254)
};

struct __attribute__((packed)) NodeEventPayload {
    uint8_t node_id;       // nodo coinvolto (JOINED/WARNING/OFFLINE/LOST)
};

struct __attribute__((packed)) KeyOnPayload {
    float    vbat_engine;  // tensione batteria motore al momento dell'evento (V)
    uint32_t timestamp_s;  // uptime ROOT in secondi
};
using KeyOffPayload = KeyOnPayload;

struct __attribute__((packed)) StepOpenPayload {
    uint8_t open_reason;   // 0=fc_open, 1=timeout motore, 2=comando manuale
    uint8_t key_on_active; // 1 se KEY_ON attivo in questo momento
    float   temperature;   // °C zona gradino
};

// Codici errore usati in error_code dei payload di stato.
enum ErrorCode : uint8_t {
    ERR_NONE        = 0x00,
    ERR_TIMEOUT     = 0x01,  // timeout motore senza finecorsa
    ERR_ENDSTOP     = 0x02,  // finecorsa mancante o entrambi attivi
    ERR_OVERCURRENT = 0x03,
    ERR_SENSOR      = 0x04,
    ERR_MESH        = 0x05,
    ERR_NVS         = 0x06,
    ERR_OTA         = 0x07,
};

// ── CRC8 (CRC-8/MAXIM) — integrita del frame ────────────────────────────────
// Appeso in coda al frame (header+payload) prima della trasmissione e verificato
// in ricezione. Non fa parte di MeshMsg.
inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        uint8_t inbyte = data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

} // namespace domoc
