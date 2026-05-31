#include <stddef.h>
#include "mesh_protocol.h"

// ── CRC-16/CCITT-FALSE ────────────────────────────────────────────────────
uint16_t mesh_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// ── Validazione messaggio ─────────────────────────────────────────────────
bool mesh_msg_valid(const mesh_msg_t *msg)
{
    uint16_t expected = mesh_crc16((const uint8_t *)msg,
                                    sizeof(mesh_msg_t) - sizeof(msg->crc16));
    return expected == msg->crc16;
}

// ── Sequenza monotona ─────────────────────────────────────────────────────
static uint16_t s_seq = 0;

uint16_t mesh_next_seq(void)
{
    return ++s_seq;
}
