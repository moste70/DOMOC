#pragma once

// Inizializza il task di persistenza (crea semaforo/flag interni).
void nvs_persist_init(void);

// Task che scrive la registry su NVS ogni volta che è stata modificata.
// Utilizza node_registry_is_dirty() per evitare scritture inutili.
void nvs_persist_task(void *pvParam);
