#pragma once

// Soglie di timeout heartbeat (ms)
#define HB_TIMEOUT_WARNING_MS   15000   // 3 heartbeat mancati → WARNING
#define HB_TIMEOUT_OFFLINE_MS   30000   // 6 heartbeat mancati → OFFLINE
#define HB_TIMEOUT_LOST_MS     120000   // 24 heartbeat mancati → LOST

// Task che controlla periodicamente last_seen di ogni nodo.
// Aggiorna lo stato in registry e notifica l'HMI dei cambiamenti.
void heartbeat_monitor_task(void *pvParam);
