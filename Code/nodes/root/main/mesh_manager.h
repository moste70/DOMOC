#pragma once

#include "esp_err.h"

#define MESH_CHANNEL        6
#define MESH_ID             "DomoC01"
#define MESH_PASSWORD       "domoc2025"
#define MESH_MAX_CHILDREN   10
#define MESH_MAX_LAYER      4

// Inizializza e avvia ESP-Mesh come root fisso.
// Blocca finché la mesh non è operativa (timeout 30s, poi riavvio).
void mesh_manager_init(void);

// true se la mesh è attiva e il ROOT è connesso come root
bool mesh_manager_is_connected(void);
