#pragma once

#include <stdint.h>
#include "mesh_protocol.h"

// Dimensione coda TX — aumentare se si usa OTA con molti nodi
#define TX_QUEUE_DEPTH  16

// Inizializza la coda TX. Chiamare prima di mesh_tx_task.
void mesh_tx_queue_init(void);

// Task principale TX. Invia i messaggi in coda verso la mesh.
void mesh_tx_task(void *pvParam);

// Accoda un messaggio per la trasmissione.
// target_mac: MAC ESP-Mesh del destinatario (NULL = broadcast).
// Restituisce true se accodato, false se la coda è piena.
bool mesh_tx_enqueue(const mesh_msg_t *msg, const uint8_t target_mac[6]);

// Shortcut: invia un ACK di registrazione al nodo appena registrato.
void mesh_tx_send_register_ack(uint16_t node_id, const uint8_t target_mac[6]);

// Shortcut: inoltra un aggiornamento di stato all'HMI (se connesso).
void mesh_tx_forward_to_hmi(msg_type_t type, uint16_t node_id,
                             const uint8_t *payload, uint8_t payload_len);

// Shortcut: invia un dump completo della registry all'HMI.
void mesh_tx_send_registry_dump(const uint8_t hmi_mac[6]);

// Registra il MAC dell'HMI (aggiornato ad ogni MSG_REGISTER dall'HMI).
void mesh_tx_set_hmi_mac(const uint8_t mac[6]);
bool mesh_tx_hmi_connected(void);
