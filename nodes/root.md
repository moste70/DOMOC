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
| GPIO18 | LED stato verde (mesh ok) |
| GPIO19 | LED stato rosso (errore / boot) |
| GPIO20/21 | UART TX/RX (debug) |

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

### 2. Gestione registrazione nodi

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
    
    // Invia ACK con ID assegnato
    send_register_ack(msg->mac, assigned_id);
    
    // Notifica HMI del nuovo nodo (se connesso)
    forward_to_hmi(MSG_NODE_JOINED, assigned_id);
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
                case MSG_STATUS_REQ:
                    if (msg.node_id == NODE_ID_HMI)
                        send_registry_dump(NODE_ID_HMI); // dump completo alla connessione HMI
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

void node_registry_init(void) {
    // Al boot: ricarica tutti i nodi noti da NVS
    // I nodi si ri-connettono con reconnect=true e trovano già il loro ID
    nvs_iterator_t it = nvs_entry_find("registry", NULL, NVS_TYPE_BLOB);
    while (it != NULL) {
        node_nvs_entry_t entry;
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        nvs_get_blob(..., &entry, ...);
        registry_add_from_nvs(&entry);  // aggiunge con status = OFFLINE
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
