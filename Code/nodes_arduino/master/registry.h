#pragma once
#include <Arduino.h>
#include "../../arduino_lib/domoc/domoc_protocol.h"

struct NodeInfo {
    uint32_t mesh_node_id;   // painlessMesh uint32 ID
    uint8_t  logical_id;     // DomoC 0x01–0x0B
    uint8_t  node_type;
    char     name[17];
    uint32_t last_hb_ms;     // millis() ultimo heartbeat
    uint8_t  status;         // 0=online 1=warning 2=offline 3=lost
    bool     active;
};

class NodeRegistry {
public:
    void update_or_add(uint32_t mesh_id, uint8_t logical_id,
                       uint8_t node_type, const char* name);
    NodeInfo* find_by_logical(uint8_t logical_id);
    NodeInfo* find_by_mesh(uint32_t mesh_id);
    uint8_t   hmi_mesh_id(uint32_t& out_id) const;

    // Chiama ogni loop() — aggiorna status, restituisce logical_id dei nodi cambiati stato
    void tick(uint32_t now);

    // Callback per cambio stato nodo
    using StatusChangedCb = std::function<void(const NodeInfo&, uint8_t old_status)>;
    void set_on_status_changed(StatusChangedCb cb) { cb_ = cb; }

    NodeInfo nodes_[REGISTRY_MAX_NODES]{};

private:
    StatusChangedCb cb_;
};
