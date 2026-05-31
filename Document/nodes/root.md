# DomoC — Nodo ROOT (Root mesh dedicato)

---

## Descrizione

Il nodo ROOT è il **cuore infrastrutturale** della rete DomoC. È un ESP32-C3 piccolo, economico e senza display, fisso nel camper e sempre alimentato a 12V.

Responsabilità **esclusivamente infrastrutturali**:
1. **Root fissa della mesh**: garantisce che la rete ESP-Mesh sia sempre disponibile indipendentemente dall'HMI
2. **Node registry**: assegna gli ID logici ai nodi, mantiene lo stato aggiornato, persiste tutto su NVS
3. **Forwarding stato all'HMI**: quando l'HMI è connesso, gli inoltra gli aggiornamenti di stato
4. **Distribuzione OTA**: su richiesta dell'HMI, coordina gli aggiornamenti firmware ai nodi

> **Il ROOT non prende mai decisioni applicative**. Non invia comandi agli attuatori. Non valuta regole di allarme. Non ha logica di business. È un router intelligente con memoria persistente.

---

## Hardware

### Microcontrollore

- **ESP32-C3** — scelto per:
  - Consumo molto basso (~20 mA a pieno regime con mesh attiva)
  - Costo minimo (~3€)
  - Wi-Fi 802.11 b/g/n sufficiente per ESP-Mesh root
  - Nessun display necessario — single-core 160 MHz è abbondante

### Sensori integrati

1. **Rilevamento chiave accensione** — optoisolatore PC817
   - Ingresso: +12V dal contatto di accensione veicolo (tramite resistenza serie 10kΩ)
   - Output: GPIO interrupt, rilevamento fronte ON/OFF
   - Protezione: optoisolatore isola il 12V dal microcontrollore

2. **Monitor batteria motore (12V starter)**
   - Partitore ADC: 100kΩ / 27kΩ per scalare 0–16V → 0–3.3V ESP32
   - GPIO ADC dedicato
   - Lettura ogni 60s, alert se > 13.5V (motore acceso/alternatore carica)
   - Datasheet INA219 (I2C, opzionale): per misura di corrente più precisa

### Alimentazione

```
[Bus 12V camper] ──→ [Buck MP2307, 12V→3.3V, 500mA] ──→ ESP32-C3
```

- **Sorgente unica**: bus 12V del camper — il ROOT non ha alimentazione di backup
- **Motivazione**: il ROOT deve essere fisso e sempre alimentato; se il 12V cade, il camper è comunque fermo e la mesh non è necessaria
- **Protezione**: condensatore bulk 470 µF + TVS 15V sul bus 12V per assorbire spike
- **Consumo stimato**: 15–25 mA costanti — trascurabile sul budget energetico del camper

### Connettività

| Interfaccia | Uso |
|---|---|
| Wi-Fi (ESP-Mesh root) | Gestione rete mesh |
| UART (header pin) | Debug seriale, flash firmware |
| GPIO LED RGB | Stato sistema (opzionale ma consigliato) |

### GPIO (ESP32-C3)

| GPIO | Funzione |
|---|---|
| GPIO1 | Interrupt chiave accensione (dal PC817 optoisolatore) |
| GPIO2 | ADC batteria motore (VBATT_ENGINE / partitore) |
| GPIO18 | LED stato verde (mesh ok) |
| GPIO19 | LED stato rosso (errore / boot) |
| GPIO20/21 | UART TX/RX (debug) |

---

## Librerie ESP-IDF utilizzate

Il firmware ROOT è sviluppato con **ESP-IDF 5.x** su target **ESP32-C3**. Di seguito le librerie dichiarate come dipendenze in `main/CMakeLists.txt` (campo `REQUIRES`) e i relativi header inclusi nel codice.

### Dipendenze CMake (`REQUIRES`)

| Componente ESP-IDF | Header principali | Uso nel firmware ROOT |
|---|---|---|
| `esp_wifi` | `esp_wifi.h` | Inizializzazione Wi-Fi, modem sleep, potenza TX |
| `esp_mesh` | `esp_mesh.h` | Stack ESP-Mesh: init, config, send/recv, eventi |
| `nvs_flash` | `nvs_flash.h`, `nvs.h` | Persistenza registry nodi e configurazione su flash NVS |
| `esp_netif` | `esp_netif.h` | Netif richiesto da ESP-Mesh prima dell'avvio Wi-Fi |
| `esp_event` | `esp_event.h` | Event loop per callback eventi mesh e Wi-Fi |
| `freertos` | `freertos/FreeRTOS.h`, `task.h`, `queue.h`, `semphr.h`, `event_groups.h` | Task, code messaggi TX, mutex registry, sincronizzazione OTA |
| `esp_timer` | `esp_timer.h` | Timestamp `now_ms()` per heartbeat monitor e payload |
| `app_update` | `esp_ota_ops.h` | Gestione partizioni OTA dual-bank per distribuzione firmware |
| `spiffs` | `esp_spiffs.h` | Partizione `config` per storage firmware OTA in attesa di distribuzione |
| `esp_log` | `esp_log.h` | Logging strutturato con livelli (TAG per ogni modulo) |

### Header interni al progetto

| File | Dipende da |
|---|---|
| `mesh_protocol.h` | `stdint.h`, `stdbool.h` — nessuna dipendenza IDF |
| `mesh_manager.h` | `esp_err.h`, `esp_mesh.h`, `esp_wifi.h`, `esp_event.h` |
| `node_registry.h` | `mesh_protocol.h`, `nvs.h`, `freertos/semphr.h` |
| `mesh_rx_task.h` | `esp_mesh.h`, `mesh_protocol.h` |
| `mesh_tx_task.h` | `esp_mesh.h`, `freertos/queue.h`, `mesh_protocol.h` |
| `heartbeat_monitor.h` | `esp_timer.h`, `node_registry.h` |
| `nvs_persist.h` | `nvs_flash.h`, `nvs.h` |
| `ota_distributor.h` | `freertos/queue.h`, `freertos/event_groups.h`, `esp_ota_ops.h` |

### Configurazioni chiave in `sdkconfig.defaults`

| Parametro | Valore | Effetto |
|---|---|---|
| `CONFIG_IDF_TARGET` | `esp32c3` | Target MCU |
| `CONFIG_FREERTOS_HZ` | `1000` | Tick RTOS a 1ms — necessario per timeout precisi |
| `CONFIG_ESP_TASK_WDT` | `y` | Watchdog task abilitato — rileva task bloccati |
| `CONFIG_ESP_TASK_WDT_TIMEOUT_S` | `10` | Reset se un task non fa `vTaskDelay` entro 10s |
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` | `y` | Rollback automatico OTA se il nodo non si avvia |
| `CONFIG_PARTITION_TABLE_CUSTOM` | `y` | Usa `partitions.csv` con layout OTA dual-bank |
| `CONFIG_LOG_DEFAULT_LEVEL_INFO` | `y` | Livello log default INFO (DEBUG disabilitato in produzione) |

### Schema partizioni flash (`partitions.csv`)

| Nome | Tipo | Offset | Dimensione | Uso |
|---|---|---|---|---|
| `nvs` | data/nvs | 0x9000 | 24 KB | Registry nodi, config mesh |
| `phy_init` | data/phy | 0xF000 | 4 KB | Calibrazione RF Wi-Fi |
| `ota_data` | data/ota | 0x10000 | 8 KB | Puntatore partizione OTA attiva |
| `ota_0` | app/ota_0 | 0x20000 | 896 KB | Firmware attivo (slot A) |
| `ota_1` | app/ota_1 | 0x100000 | 896 KB | Firmware nuovo (slot B) |
| `config` | data/spiffs | 0x1E0000 | 128 KB | Storage firmware per distribuzione OTA |

---

## Funzioni software

### 1. Inizializzazione come root fissa

```c
void app_main(void) {
    nvs_flash_init();
    wifi_init();
    
    esp_mesh_cfg_t cfg = {
        .channel          = MESH_CHANNEL,      // canale fisso (es. 6)
        .mesh_id          = MESH_ID,
        .mesh_ap.max_connection = 10,          // max figli diretti
        .mesh_ap.authmode = WIFI_AUTH_WPA2_PSK,
    };
    esp_mesh_init();
    esp_mesh_set_config(&cfg);
    esp_mesh_fix_root(true);   // ROOT non partecipa mai all'election
    esp_mesh_set_self_organized(false);
    esp_mesh_set_type(MESH_ROOT);
    esp_mesh_start();
    
    node_registry_init();  // carica registry da NVS
}
```

### 2. Gestione registrazione nodi e descriptor

```c
void handle_register(mesh_msg_t *msg) {
    reg_payload_t *reg = (reg_payload_t*)msg->payload;
    uint16_t assigned_id;

    if (reg->reconnect && nvs_has_node(msg->mac)) {
        // Nodo già conosciuto: conferma ID esistente senza riassegnare
        assigned_id = nvs_get_node_id(msg->mac);
        node_update_status(assigned_id, NODE_ONLINE);
    } else {
        // Nuovo nodo: assegna ID progressivo
        assigned_id = registry_assign_new_id(msg->mac, reg->name, reg->node_type);
        nvs_save_node(assigned_id, msg->mac, reg->name, reg->node_type);
    }

    send_register_ack(msg->mac, assigned_id);

    // Notifica HMI del nodo (se connesso) — descriptor arriverà subito dopo
    forward_to_hmi(MSG_NODE_JOINED, assigned_id);
}

// Il nodo invia MSG_DESCRIPTOR subito dopo aver ricevuto l'ACK
void handle_descriptor(mesh_msg_t *msg) {
    node_info_t *node = registry_find_by_id(msg->node_id);
    if (!node) return;

    // Copia descriptor nella registry in RAM
    memcpy(&node->descriptor, msg->payload, sizeof(node_descriptor_t));
    node->descriptor_valid = true;

    // Persiste descriptor su NVS (namespace "desc", chiave = node_id string)
    nvs_save_descriptor(node->node_id, &node->descriptor);

    // Forwarda all'HMI se connesso — HMI aggiorna carosello in tempo reale
    if (hmi_is_connected()) {
        forward_to_hmi_raw(msg);
    }
}
```

### 3. Heartbeat monitor e rilevamento offline

```c
void heartbeat_monitor_task(void *pvParam) {
    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        
        for (int i = 0; i < registry_count(); i++) {
            node_info_t *node = registry_get(i);
            if (node->node_type == HMI) continue;  // HMI è opzionale, skip
            
            uint32_t delta = now - node->last_seen_ms;
            
            if (delta > 15000 && node->status != NODE_WARNING) {
                node->status = NODE_WARNING;
                forward_to_hmi(MSG_NODE_WARNING, node->node_id);
            }
            if (delta > 30000 && node->status != NODE_OFFLINE) {
                node->status = NODE_OFFLINE;
                forward_to_hmi(MSG_NODE_OFFLINE, node->node_id);
                nvs_update_node_status(node->node_id, NODE_OFFLINE);
            }
            if (delta > 120000) {
                forward_to_hmi(MSG_NODE_LOST, node->node_id);
                registry_mark_lost(node->node_id);
                // Non rimuove da NVS: il nodo potrebbe tornare
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 4. Forwarding stato all'HMI

Il ROOT agisce da **aggregatore**: ogni messaggio di stato o alert ricevuto da un nodo funzione viene inoltrato all'HMI (se connesso):

```c
void mesh_rx_task(void *pvParam) {
    mesh_msg_t msg;
    while (1) {
        if (esp_mesh_recv(&msg) == ESP_OK) {
            switch (msg.msg_type) {
                case MSG_HEARTBEAT:
                    update_last_seen(msg.node_id);
                    forward_to_hmi(MSG_STATUS_RESP, msg.node_id); // forwarding stato
                    break;
                case MSG_REGISTER:
                    handle_register(&msg);
                    break;
                case MSG_ALERT:
                    forward_to_hmi_raw(&msg); // alert inoltrato inalterato
                    break;
                case MSG_COMMAND:
                    // Comandi dall'HMI verso nodi: ROOT fa da router
                    route_to_node(&msg);
                    break;
                case MSG_DESCRIPTOR:
                    handle_descriptor(&msg);
                    break;
                case MSG_DESCRIPTOR_REQ:
                    // HMI richiede descriptor di un nodo specifico
                    send_descriptor_to_hmi(msg.target_id);
                    break;
                case MSG_STATUS_REQ:
                    if (msg.node_id == NODE_ID_HMI)
                        send_registry_dump(NODE_ID_HMI); // dump completo + tutti i descriptor
                    break;
            }
        }
    }
}
```

### 5. Persistenza NVS

La node registry è salvata su NVS in modo incrementale:

```c
// Struttura NVS
// Namespace "registry": chiave = MAC string → valore = node_info_t serializzata
// Namespace "config":   parametri mesh (channel, mesh_id, soglie)

void nvs_save_node(uint16_t id, uint8_t *mac, char *name, uint8_t type) {
    char key[18];
    mac_to_str(mac, key);  // "AA:BB:CC:DD:EE:FF"

    node_nvs_entry_t entry = {
        .node_id   = id,
        .node_type = type,
    };
    strncpy(entry.name, name, 16);

    nvs_handle_t handle;
    nvs_open("registry", NVS_READWRITE, &handle);
    nvs_set_blob(handle, key, &entry, sizeof(entry));
    nvs_commit(handle);
    nvs_close(handle);
}

// Persiste il descriptor di un nodo (namespace "desc", chiave = node_id decimale)
void nvs_save_descriptor(uint16_t node_id, const node_descriptor_t *desc) {
    char key[8];
    snprintf(key, sizeof(key), "%u", node_id);

    nvs_handle_t handle;
    nvs_open("desc", NVS_READWRITE, &handle);
    nvs_set_blob(handle, key, desc, sizeof(node_descriptor_t));
    nvs_commit(handle);
    nvs_close(handle);
}

void node_registry_init(void) {
    // Al boot: ricarica tutti i nodi noti da NVS
    nvs_iterator_t it = nvs_entry_find("registry", NULL, NVS_TYPE_BLOB);
    while (it != NULL) {
        node_nvs_entry_t entry;
        nvs_entry_info(it, &(nvs_entry_info_t){});
        nvs_get_blob(handle, key, &entry, &sz);
        node_info_t *node = registry_add_from_nvs(&entry);  // status = OFFLINE

        // Ricarica descriptor se precedentemente salvato
        char dkey[8];
        snprintf(dkey, sizeof(dkey), "%u", entry.node_id);
        size_t desc_sz = sizeof(node_descriptor_t);
        nvs_handle_t dh;
        if (nvs_open("desc", NVS_READONLY, &dh) == ESP_OK) {
            if (nvs_get_blob(dh, dkey, &node->descriptor, &desc_sz) == ESP_OK)
                node->descriptor_valid = true;
            nvs_close(dh);
        }
        it = nvs_entry_next(it);
    }
}
```

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 4 KB | Ricezione messaggi, dispatch, routing verso nodi |
| `mesh_tx_task` | 5 | 4 KB | Invio REGISTER_ACK, forwarding HMI, OTA chunks |
| `heartbeat_monitor_task` | 4 | 2 KB | Rilevamento nodi offline, aggiornamento registry |
| `nvs_persist_task` | 2 | 2 KB | Scrittura asincrona registry su NVS flash |
| `ota_distributor_task` | 1 | 8 KB | Distribuzione OTA ai nodi su richiesta HMI |

---

## Comportamento alla perdita del ROOT

Se il ROOT si riavvia o perde alimentazione temporaneamente:

1. **I nodi funzione** continuano ad operare in `STANDALONE_MODE` con l'ultima configurazione
2. **La logica di sicurezza dei nodi** (es. STEP che si chiude su KEY_ON) funziona comunque perché i nodi si ascoltano direttamente via broadcast mesh
3. **ESP-Mesh** tenta il re-parenting (ROOT election temporanea), ma con `fix_root=true` su tutti i nodi non-ROOT, la rete si stabilizza correttamente appena il ROOT torna
4. **L'HMI** mostra `ROOT: OFFLINE` in header ma può continuare a visualizzare lo stato cached dei nodi
5. Al reboot del ROOT: ricarica la registry da NVS, i nodi si ri-registrano con `reconnect=true` e tutto riprende in pochi secondi

---

## Considerazioni hardware pratiche

- **Posizionamento**: montare il ROOT in una posizione centrale nel camper per minimizzare le distanze verso tutti i nodi — armadio elettrico o vano batterie sono location ideali
- **Ventilazione**: il ROOT genera pochissimo calore (< 100 mW) — nessun dissipatore necessario
- **Fissaggio**: usare viti o biadesivo rinforzato — non deve muoversi con le vibrazioni
- **Protezione**: custodia in ABS o contenitore DIN rail per installazione su quadro elettrico
- **LED di stato**: verde fisso = mesh ok + tutti i nodi online; verde lampeggiante = mesh ok + qualche nodo offline; rosso = errore mesh
- **UART esposto**: header a 3 pin (TX, RX, GND) accessibile per debug in campo senza smontare
- **Non necessita di connessione fisica** dopo l'installazione — tutto via OTA
