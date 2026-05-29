# DomoC — Nodo MASTER (Root mesh + Coordinatore)

---

## Descrizione

Il nodo MASTER è il **cuore infrastrutturale** della rete DomoC. È un ESP32-C3 senza display, fisso nel camper e sempre alimentato a 12V.

Responsabilità **esclusivamente infrastrutturali**:
1. **Root fissa della mesh**: garantisce che la rete ESP-Mesh sia sempre disponibile indipendentemente dall'HMI
2. **Node registry**: assegna gli ID logici ai nodi, mantiene lo stato aggiornato, persiste su NVS
3. **Forwarding stato all'HMI**: quando l'HMI è connesso, gli inoltra gli aggiornamenti di stato e gli alert
4. **Distribuzione OTA**: su richiesta dell'HMI, coordina gli aggiornamenti firmware ai nodi
5. **Routing comandi**: i comandi manuali inviati dall'HMI transitano attraverso il MASTER verso i nodi destinatari

> **Il MASTER non prende mai decisioni applicative**. Non invia comandi agli attuatori di propria iniziativa. Non valuta regole di allarme. Non ha logica di business. È un router intelligente con memoria persistente.
>
> **Node ID**: `0x0001` — riservato, fisso, mai riallocabile.

---

## Hardware

### Microcontrollore

- **ESP32-C3** — scelto per:
  - Consumo molto basso (~20 mA a pieno regime con mesh attiva)
  - Costo minimo (~3€)
  - Wi-Fi 802.11 b/g/n sufficiente per ESP-Mesh root
  - Nessun display necessario — single-core 160 MHz è abbondante per logica infrastrutturale

### Alimentazione

```
[Bus 12V camper] ──→ [Buck MP2307, 12V→3.3V, 500mA] ──→ ESP32-C3
```

- **Sorgente unica**: bus 12V del camper — il MASTER non ha alimentazione di backup
- **Motivazione**: il MASTER deve essere fisso e sempre alimentato; se il 12V cade, il camper è fermo e la mesh non è necessaria
- **Protezione**: condensatore bulk 470 µF + TVS 15V sul bus 12V per assorbire spike
- **Consumo stimato**: 15–25 mA costanti — trascurabile sul budget energetico del camper

### Connettività

| Interfaccia | Uso |
| --- | --- |
| Wi-Fi (ESP-Mesh root) | Gestione rete mesh — root fissa |
| UART (header pin) | Debug seriale, flash firmware |
| GPIO LED RGB | Stato sistema (opzionale ma consigliato) |

### GPIO (ESP32-C3)

| GPIO | Funzione |
|---|---|
| GPIO1 | Interrupt chiave accensione (optoisolatore PC817 — active-LOW) |
| GPIO2 | ADC batteria motore (partitore 100kΩ/27kΩ, range 0–16.5V) |
| GPIO18 | LED stato verde (mesh ok) |
| GPIO19 | LED stato rosso (errore / boot) |
| GPIO20/21 | UART TX/RX (debug) |

---

## Funzioni software

### 1. Root fissa della mesh

Il MASTER è impostato come root fissa con `esp_mesh_fix_root(true)`. Non partecipa mai alla root election. Se il MASTER si riavvia, i nodi restano in `STANDALONE_MODE` fino al suo ripristino, poi si ri-registrano con `reconnect=true`.

### 2. Node registry

Mantiene la tabella di tutti i nodi registrati (ID, MAC, nome, tipo, stato, ultimo heartbeat). La registry è persistita su NVS: al reboot il MASTER ricarica i nodi noti e li marca OFFLINE finché non inviano heartbeat.

```c
typedef struct {
    uint16_t  node_id;
    uint8_t   mac[6];
    char      name[16];
    uint8_t   node_type;      // SENSOR, ACTUATOR, HMI
    uint8_t   status;         // ONLINE / WARNING / OFFLINE / LOST
    uint32_t  last_seen_ms;
    int8_t    rssi;
    uint8_t   payload_cache[32];
} node_info_t;
```

### 3. Forwarding stato all'HMI

Ogni `MSG_HEARTBEAT` e `MSG_ALERT` ricevuto da un nodo funzione viene inoltrato all'HMI (se connesso) tramite `MSG_STATUS_RESP`. All'avvio dell'HMI, il MASTER invia un dump completo della registry.

### 4. Routing comandi HMI → nodi

I comandi inviati dall'HMI (`MSG_COMMAND`) arrivano al MASTER che li instrada verso il nodo destinatario. Il MASTER non valida né filtra il contenuto del comando — si limita al routing.

### 5. Distribuzione OTA

Su richiesta dell'HMI (`MSG_OTA_START`), il MASTER distribuisce il firmware ai nodi via `MSG_OTA_CHUNK` (chunk da 1 KB). Gestisce il flusso, la conferma di ogni chunk e il rollback automatico se il nodo non risponde entro 60s dal reboot.

### Approfondimento: Funzione OTA (Over-The-Air Update)

L'OTA (Over-The-Air Update) è il meccanismo che permette di aggiornare il firmware di tutti i nodi della rete DomoC senza doverli collegare fisicamente via cavo. Il MASTER svolge il ruolo di coordinatore e distributore OTA per l'intera mesh.

**Come funziona l'OTA in DomoC:**

1. **Avvio OTA:**
   - L'utente, tramite l'HMI, seleziona il nodo (o i nodi) da aggiornare e carica il nuovo firmware.
   - L'HMI invia un comando `MSG_OTA_START` al MASTER, specificando il nodo target e le informazioni sul firmware (versione, dimensione, CRC).

2. **Distribuzione chunk:**
   - Il MASTER suddivide il firmware in blocchi (chunk) da 1 KB e li invia al nodo target tramite messaggi `MSG_OTA_CHUNK` sulla mesh.
   - Ogni chunk viene confermato dal nodo ricevente (ACK). In caso di errore o perdita, il MASTER ritrasmette solo i chunk mancanti.

3. **Completamento e reboot:**
   - Una volta ricevuti tutti i chunk, il nodo aggiorna la partizione OTA e invia un `MSG_OTA_END` al MASTER.
   - Il nodo si riavvia con il nuovo firmware. Se non invia heartbeat entro 60s, il MASTER considera l'OTA fallita e può forzare un rollback.

4. **Rollback automatico:**
   - Se il nodo aggiornato non risponde dopo il reboot, il bootloader ripristina automaticamente il firmware precedente (OTA dual-bank).

**Vantaggi della gestione OTA dal MASTER:**
- Aggiornamento centralizzato e sicuro di tutti i nodi, anche in punti difficili da raggiungere.
- Riduzione dei rischi di brick: il MASTER controlla la sequenza, la conferma di ogni chunk e la validità del firmware.
- Possibilità di aggiornare più nodi in parallelo o in sequenza, mantenendo la mesh operativa.
- Log e stato avanzamento visibili in tempo reale sull'HMI.

> **Nota:** Il MASTER non memorizza il firmware a lungo termine: funge solo da ponte tra l'HMI (che carica il file) e i nodi destinatari. Tutto il processo è tracciato e ogni errore viene notificato all'utente tramite l'HMI.

---

## Task FreeRTOS

| Task | Priorità | Stack | Funzione |
|---|---|---|---|
| `mesh_rx_task` | 5 | 4 KB | Ricezione messaggi mesh, dispatch, routing verso nodi |
| `mesh_tx_task` | 5 | 4 KB | Invio REGISTER_ACK, forwarding HMI, OTA chunks |
| `heartbeat_monitor_task` | 4 | 2 KB | Rilevamento nodi offline, aggiornamento registry |
| `nvs_persist_task` | 2 | 2 KB | Scrittura asincrona registry su NVS flash |
| `ota_distributor_task` | 1 | 8 KB | Distribuzione OTA ai nodi su richiesta HMI |

---

## Comportamento alla perdita del MASTER

Se il MASTER si riavvia o perde alimentazione temporaneamente:

1. **I nodi funzione** continuano ad operare in `STANDALONE_MODE` con l'ultima configurazione ricevuta
2. **La logica di sicurezza dei nodi** (es. STEP che si chiude su KEY_ON) funziona comunque — i nodi si ascoltano direttamente via broadcast mesh
3. **L'HMI** mostra `MASTER: OFFLINE` in header ma visualizza lo stato cached degli altri nodi
4. Al reboot del MASTER: ricarica la registry da NVS, i nodi si ri-registrano con `reconnect=true` e tutto riprende in pochi secondi

---

## Considerazioni hardware pratiche

- **Posizionamento**: montare il MASTER in posizione centrale nel camper (armadio elettrico o vano batterie) per minimizzare le distanze verso tutti i nodi
- **Ventilazione**: genera pochissimo calore (< 100 mW) — nessun dissipatore necessario
- **Fissaggio**: viti o biadesivo rinforzato — non deve muoversi con le vibrazioni del mezzo
- **Protezione**: custodia in ABS o contenitore DIN rail per installazione su quadro elettrico
- **LED di stato**: verde fisso = mesh ok + tutti i nodi online; verde lampeggiante = qualche nodo offline; rosso = errore mesh
- **UART esposto**: header a 3 pin (TX, RX, GND) accessibile per debug in campo
- **Non necessita di connessione fisica** dopo l’installazione — tutto via OTA da HMI

> La documentazione completa dell’HMI (display, alimentazione dual-source, LVGL, encoder, task FreeRTOS) si trova in `Document/nodes/hmi.md`.
