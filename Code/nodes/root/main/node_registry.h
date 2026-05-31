#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mesh_protocol.h"

// ── Struttura dati di ogni nodo registrato ────────────────────────────────
typedef struct {
    uint16_t     node_id;
    uint8_t      mac[6];
    char         name[NODE_NAME_MAX];
    node_type_t  node_type;
    node_status_t status;
    uint32_t     last_seen_ms;      // uptime locale al momento dell'ultimo heartbeat
    int8_t       rssi;
    uint8_t      fw_major;
    uint8_t      fw_minor;
    uint8_t      fw_patch;
    uint8_t      payload_cache[32]; // ultimo heartbeat_payload_t ricevuto
    bool         valid;             // slot occupato
} node_info_t;

// ── Inizializzazione ──────────────────────────────────────────────────────

// Carica tutti i nodi da NVS. Tutti i nodi caricati vengono marcati OFFLINE
// e attendono il primo heartbeat per tornare ONLINE.
void node_registry_init(void);

// Numero di nodi validi in registry (inclusi OFFLINE)
int  node_registry_count(void);

// ── Registrazione nodi ────────────────────────────────────────────────────

// Registra o riconferma un nodo dalla payload di MSG_REGISTER.
// Se reconnect=true e il MAC è già noto, restituisce l'ID esistente.
// Se è un nodo nuovo, assegna un ID progressivo.
// Restituisce il node_id assegnato, 0 in caso di errore (registry piena).
uint16_t node_registry_register(const reg_payload_t *reg, const uint8_t mac[6]);

// ── Accesso e aggiornamento ───────────────────────────────────────────────

// Restituisce puntatore alla entry (NULL se non trovato).
// Il puntatore è valido fino alla prossima modifica della registry.
node_info_t *node_registry_get_by_id(uint16_t node_id);
node_info_t *node_registry_get_by_mac(const uint8_t mac[6]);

// Aggiorna last_seen, rssi e payload_cache da un heartbeat ricevuto.
// Porta lo stato a ONLINE se era WARNING/OFFLINE.
void node_registry_update_heartbeat(uint16_t node_id, int8_t rssi,
                                    const uint8_t *payload, uint8_t payload_len);

// Imposta esplicitamente lo stato di un nodo.
void node_registry_set_status(uint16_t node_id, node_status_t status);

// Marca il nodo come LOST (non risponde da >120s).
// Non rimuove da NVS: il nodo potrebbe tornare con reconnect=true.
void node_registry_mark_lost(uint16_t node_id);

// ── Dump per HMI ──────────────────────────────────────────────────────────

// Copia la registry completa in buf (max MAX_NODES entry).
// Restituisce il numero di entry copiate.
int  node_registry_dump(node_info_t *buf, int buf_size);

// ── Utilità ───────────────────────────────────────────────────────────────

// true se node_id è l'HMI (non monitorato per heartbeat)
bool node_registry_is_hmi(uint16_t node_id);

// Segnala che la registry è stata modificata → nvs_persist_task la salverà
void node_registry_mark_dirty(void);

// true se la registry ha modifiche non ancora persiste su NVS
bool node_registry_is_dirty(void);

// Serializza e deserializza su NVS (chiamate da nvs_persist)
void node_registry_save_to_nvs(void);
void node_registry_load_from_nvs(void);
