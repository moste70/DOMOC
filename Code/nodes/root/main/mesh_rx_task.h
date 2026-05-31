#pragma once

// Task principale di ricezione messaggi dalla mesh.
// Legge da esp_mesh_recv, valida CRC e smista per msg_type.
void mesh_rx_task(void *pvParam);
