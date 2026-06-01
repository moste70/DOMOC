// DomoC — Protocollo mesh (formato sul filo)
//
// Header C-puro: funziona in Arduino, ESP-IDF e nei test desktop.
// Il layout binario di ogni struct è il contratto tra nodi — non modificare
// gli offset senza aggiornare anche il firmware dell'HMI.
//
// Riferimento: Document/messaggi_mesh.md

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ── Costanti mesh ────────────────────────────────────────────────────────────
#define DOMOC_MESH_PREFIX    "DomoC01"
#define DOMOC_MESH_PASSWORD  "domoc2024"
#define DOMOC_MESH_PORT      5555

// ── ID logici nodi ───────────────────────────────────────────────────────────
#define NODE_ID_MASTER        0x01
#define NODE_ID_STEP          0x02
#define NODE_ID_GREY_WATER    0x03
#define NODE_ID_FRESH_WATER   0x04
#define NODE_ID_THERMO_BUNK   0x05
#define NODE_ID_THERMO_LOFT   0x06
#define NODE_ID_THERMO_KITCHEN 0x07
#define NODE_ID_REAR_CAM      0x08
#define NODE_ID_FRONT_DOOR    0x09
#define NODE_ID_CAM_EXT       0x0A
#define NODE_ID_HMI           0x0B
#define NODE_ID_BROADCAST     0xFF

// ── Tipi di messaggio ────────────────────────────────────────────────────────
#define MSG_COMMAND           0x01
#define MSG_STATUS            0x02
#define MSG_REGISTER          0x05
#define MSG_HEARTBEAT         0x06
#define MSG_DESCRIPTOR        0x07
#define MSG_DESCRIPTOR_REQ    0x08
#define MSG_REGISTER_ACK      0x09
#define MSG_STATUS_REQ        0x0A
#define MSG_STATUS_RESP       0x0B
#define MSG_REGISTRY_DUMP     0x0C
#define MSG_NODE_JOINED       0x0D
#define MSG_NODE_WARNING      0x0E
#define MSG_NODE_OFFLINE      0x0F
#define MSG_NODE_LOST         0x10
#define MSG_KEY_ON            0x11
#define MSG_KEY_OFF           0x12
#define MSG_STEP_OPEN         0x13
#define MSG_OTA_START         0x20
#define MSG_OTA_CHUNK         0x21
#define MSG_OTA_END           0x22
#define MSG_OTA_ACK           0x23

// ── Codici azione ────────────────────────────────────────────────────────────
#define ACTION_OPEN           0x01
#define ACTION_CLOSE          0x02
#define ACTION_CAM_ON         0x03
#define ACTION_CAM_OFF        0x04
#define ACTION_LIGHT_ON       0x05
#define ACTION_LIGHT_OFF      0x06
#define ACTION_TEMP_UP        0x07
#define ACTION_TEMP_DN        0x08
#define ACTION_GET_STATUS     0x0F

// ── Codici errore ────────────────────────────────────────────────────────────
#define ERR_NONE              0x00
#define ERR_TIMEOUT           0x01
#define ERR_ENDSTOP           0x02
#define ERR_OVERCURRENT       0x03
#define ERR_SENSOR            0x04
#define ERR_MESH              0x05
#define ERR_NVS               0x06
#define ERR_OTA               0x07

// ── Timing ───────────────────────────────────────────────────────────────────
#define DOMOC_HEARTBEAT_MS         5000
#define DOMOC_HEARTBEAT_JITTER_MS  2000
#define DOMOC_STANDALONE_TIMEOUT_MS 30000
#define DOMOC_NODE_WARNING_MS      15000
#define DOMOC_NODE_OFFLINE_MS      30000
#define DOMOC_NODE_LOST_MS         120000

// ── Strutture frame ──────────────────────────────────────────────────────────
#define DOMOC_PAYLOAD_MAX  200

#pragma pack(push, 1)

typedef struct {
    uint8_t msg_type;
    uint8_t src_id;
    uint8_t dst_id;
    uint8_t seq_num;
    uint8_t payload[DOMOC_PAYLOAD_MAX];
} DomocMsg;

// Payload: registrazione nodo → ROOT
typedef struct {
    char    name[16];
    uint8_t node_type;
    uint8_t reconnect;    // 1 = rientro dopo reboot
    uint8_t mac[6];
} RegPayload;             // 24 byte

// Payload: conferma registrazione ROOT → nodo
typedef struct {
    uint8_t assigned_id;  // eco dell'ID logico statico
} RegAckPayload;          // 1 byte

// Payload: comando HMI → nodo
typedef struct {
    uint8_t action_code;
    uint8_t param;        // setpoint ×10 o altro parametro opzionale
} CmdPayload;             // 2 byte

// Payload: evento KEY_ON / KEY_OFF
typedef struct {
    float    vbat_engine;
    uint32_t timestamp_s;
} KeyPayload;             // 8 byte

// Payload: nodo joined/warning/offline/lost
typedef struct {
    uint8_t node_id;
    uint8_t node_type;
    char    name[16];
} NodeEventPayload;       // 18 byte

#pragma pack(pop)

// ── CRC8 (CRC-8/MAXIM) ───────────────────────────────────────────────────────
static inline uint8_t domoc_crc8(const uint8_t* data, size_t len) {
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
