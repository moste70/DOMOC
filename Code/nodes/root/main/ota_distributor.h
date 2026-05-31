#pragma once

#include <stdint.h>

// Avvia una sessione OTA verso un nodo specifico.
// Chiamato da mesh_rx_task alla ricezione di MSG_OTA_START dall'HMI.
void ota_distributor_start(uint16_t target_node_id,
                           uint32_t fw_size, uint32_t fw_crc32,
                           uint8_t  fw_major, uint8_t fw_minor, uint8_t fw_patch);

// Callback chiamato da mesh_rx_task quando il nodo conferma un chunk.
void ota_distributor_on_chunk_ack(uint16_t node_id, uint16_t chunk_index);

// Callback: nodo ha applicato il firmware e si è riavviato correttamente.
void ota_distributor_on_complete(uint16_t node_id);

// Callback: il nodo ha segnalato un errore OTA.
void ota_distributor_on_error(uint16_t node_id);

// Task OTA: attende sessioni dalla coda interna e le esegue.
void ota_distributor_task(void *pvParam);
