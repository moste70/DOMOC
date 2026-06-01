#include "registry.h"
#include "config.h"

void NodeRegistry::update_or_add(uint32_t mesh_id, uint8_t logical_id,
                                  uint8_t node_type, const char* name) {
    NodeInfo* n = find_by_logical(logical_id);
    if (!n) {
        // Cerca uno slot libero
        for (auto& slot : nodes_) {
            if (!slot.active) { n = &slot; break; }
        }
    }
    if (!n) return;  // registry pieno

    n->mesh_node_id = mesh_id;
    n->logical_id   = logical_id;
    n->node_type    = node_type;
    strlcpy(n->name, name, sizeof(n->name));
    n->last_hb_ms   = millis();
    n->status       = 0;  // online
    n->active       = true;
}

NodeInfo* NodeRegistry::find_by_logical(uint8_t logical_id) {
    for (auto& n : nodes_)
        if (n.active && n.logical_id == logical_id) return &n;
    return nullptr;
}

NodeInfo* NodeRegistry::find_by_mesh(uint32_t mesh_id) {
    for (auto& n : nodes_)
        if (n.active && n.mesh_node_id == mesh_id) return &n;
    return nullptr;
}

uint8_t NodeRegistry::hmi_mesh_id(uint32_t& out_id) const {
    for (const auto& n : nodes_) {
        if (n.active && n.logical_id == NODE_ID_HMI) {
            out_id = n.mesh_node_id;
            return 1;
        }
    }
    return 0;
}

void NodeRegistry::tick(uint32_t now) {
    for (auto& n : nodes_) {
        if (!n.active) continue;
        uint32_t elapsed = now - n.last_hb_ms;
        uint8_t  old = n.status;

        if      (elapsed > DOMOC_NODE_LOST_MS)    n.status = 3;
        else if (elapsed > DOMOC_NODE_OFFLINE_MS) n.status = 2;
        else if (elapsed > DOMOC_NODE_WARNING_MS) n.status = 1;
        else                                       n.status = 0;

        if (n.status != old && cb_) cb_(n, old);
    }
}
