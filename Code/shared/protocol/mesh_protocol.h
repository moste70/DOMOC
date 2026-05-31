#pragma once

#include <stdint.h>
#include <stdbool.h>

// ── Versione protocollo ────────────────────────────────────────────────────
#define PROTOCOL_VERSION    0x01

// ── Indirizzi logici riservati ─────────────────────────────────────────────
#define NODE_ID_ROOT        0x0001
#define NODE_ID_HMI         0x000F
#define NODE_ID_BROADCAST   0xFFFF

// ── Limiti ────────────────────────────────────────────────────────────────
#define MAX_NODES           64
#define MSG_PAYLOAD_MAX     200
#define NODE_NAME_MAX       16
#define OTA_CHUNK_SIZE      1024

// ── Tipi di messaggio ─────────────────────────────────────────────────────
typedef enum {
    MSG_HEARTBEAT     = 0x01,   // nodo → ROOT: keepalive periodico
    MSG_STATUS        = 0x02,   // nodo → ROOT/HMI: stato corrente
    MSG_ALERT         = 0x03,   // nodo → broadcast: evento critico
    MSG_REGISTER      = 0x05,   // nodo → ROOT: registrazione al boot
    MSG_REGISTER_ACK  = 0x06,   // ROOT → nodo: conferma con node_id
    MSG_COMMAND       = 0x10,   // HMI → nodo: esegui azione
    MSG_COMMAND_ACK   = 0x11,   // nodo → HMI: conferma esecuzione
    MSG_STATUS_REQ    = 0x20,   // HMI → ROOT: richiesta dump registry
    MSG_STATUS_RESP   = 0x21,   // ROOT → HMI: risposta stato nodo
    MSG_NODE_JOINED   = 0x22,   // ROOT → HMI: nuovo nodo registrato
    MSG_NODE_WARNING  = 0x23,   // ROOT → HMI: nodo non risponde
    MSG_NODE_OFFLINE  = 0x24,   // ROOT → HMI: nodo offline
    MSG_NODE_LOST     = 0x25,   // ROOT → HMI: nodo non risponde da >120s
    MSG_KEY_ON        = 0x11,   // ROOT → broadcast: chiave inserita
    MSG_KEY_OFF       = 0x12,   // ROOT → broadcast: chiave tolta
    MSG_OTA_START     = 0x40,   // HMI → ROOT: avvia aggiornamento OTA
    MSG_OTA_CHUNK     = 0x41,   // ROOT → nodo: blocco firmware
    MSG_OTA_ACK       = 0x42,   // nodo → ROOT: conferma chunk
    MSG_OTA_END       = 0x43,   // ROOT → nodo: fine OTA, esegui reboot
    MSG_OTA_COMPLETE  = 0x44,   // nodo → ROOT: OTA applicata con successo
    MSG_OTA_ERROR     = 0x45,   // nodo → ROOT: OTA fallita
} msg_type_t;

// ── Tipi di nodo ──────────────────────────────────────────────────────────
typedef enum {
    NODE_TYPE_ROOT       = 0x01,
    NODE_TYPE_ACTUATOR   = 0x02,
    NODE_TYPE_SENSOR     = 0x03,
    NODE_TYPE_THERMO     = 0x04,
    NODE_TYPE_CAMERA     = 0x05,
    NODE_TYPE_HMI        = 0x06,
} node_type_t;

// ── Stati nodo ────────────────────────────────────────────────────────────
typedef enum {
    NODE_STATUS_ONLINE   = 0x01,
    NODE_STATUS_WARNING  = 0x02,
    NODE_STATUS_OFFLINE  = 0x03,
    NODE_STATUS_LOST     = 0x04,
    NODE_STATUS_UPDATING = 0x05,
    NODE_STATUS_ERROR    = 0x06,
} node_status_t;

// ── Azioni attuatori (payload MSG_COMMAND) ────────────────────────────────
typedef enum {
    ACTION_OPEN       = 0x01,
    ACTION_CLOSE      = 0x02,
    ACTION_TOGGLE     = 0x03,
    ACTION_GET_STATUS = 0x04,
    ACTION_SET_VALUE  = 0x05,
    ACTION_RESET      = 0x06,
    ACTION_CLOSE_ALL  = 0x07,   // broadcast da HMI: chiudi tutto
    ACTION_OTA_PREPARE = 0x08,
} action_t;

// ── Struttura messaggio principale ────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  version;                   // PROTOCOL_VERSION
    uint8_t  msg_type;                  // msg_type_t
    uint16_t node_id;                   // mittente
    uint16_t target_id;                 // destinatario (NODE_ID_BROADCAST = tutti)
    uint32_t timestamp_ms;              // uptime mittente in ms
    uint16_t seq_num;                   // sequenza per dedup/ACK
    uint8_t  payload_len;               // lunghezza payload effettiva
    uint8_t  payload[MSG_PAYLOAD_MAX];  // dati specifici per msg_type
    uint16_t crc16;                     // CRC su tutti i campi (escluso crc16 stesso)
} mesh_msg_t;

// ── Payload MSG_REGISTER ──────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  mac[6];
    uint8_t  node_type;                 // node_type_t
    char     name[NODE_NAME_MAX];
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    bool     reconnect;                 // true se ha già un node_id salvato in NVS
    uint16_t saved_node_id;            // valido solo se reconnect=true
} reg_payload_t;

// ── Payload MSG_REGISTER_ACK ──────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint16_t assigned_node_id;
    uint8_t  mesh_channel;
} reg_ack_payload_t;

// ── Payload MSG_HEARTBEAT ─────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  status;                    // node_status_t
    int8_t   rssi;
    uint32_t uptime_s;
    uint8_t  state_flags;               // bitfield stato specifico del nodo
    uint8_t  data[8];                   // dato sintetico (es. temperatura, tensione)
} heartbeat_payload_t;

// ── Payload MSG_COMMAND ───────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  action;                    // action_t
    uint8_t  param;                     // parametro opzionale
    uint16_t value;                     // valore opzionale (es. setpoint)
} cmd_payload_t;

// ── Payload MSG_ALERT ─────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  severity;                  // 0=info 1=warning 2=critical
    uint8_t  alert_code;
    uint8_t  new_state;
    char     message[24];
} alert_payload_t;

// ── Payload MSG_OTA_START ─────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint16_t target_node_id;
    uint32_t fw_size;                   // byte totali
    uint32_t fw_crc32;                  // CRC intero firmware
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
} ota_start_payload_t;

// ── Payload MSG_OTA_CHUNK ─────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint16_t chunk_index;
    uint16_t chunk_len;
    uint16_t chunk_crc16;
    uint8_t  data[OTA_CHUNK_SIZE];
} ota_chunk_payload_t;

// ── Utility ───────────────────────────────────────────────────────────────
uint16_t mesh_crc16(const uint8_t *data, uint32_t len);
bool     mesh_msg_valid(const mesh_msg_t *msg);
uint16_t mesh_next_seq(void);
