# DomoC — Processo OTA (Over-The-Air Update)

---

## Panoramica

Il sistema DomoC supporta l'aggiornamento firmware di tutti i nodi via rete mesh ESP-Mesh, senza accesso fisico ai dispositivi. Il processo è coordinato dal ROOT (che gestisce il trasporto) e avviato dall'HMI (che fornisce il file firmware e monitora l'avanzamento).

**Principi fondamentali:**
- Ogni nodo ha partizioni flash dual-bank (`ota_0` / `ota_1`) — il rollback è sempre possibile
- Il ROOT è solo il trasportatore: non valida il firmware, non decide quali nodi aggiornare
- L'HMI è l'interfaccia utente per l'avvio OTA — carica il file, seleziona il nodo, monitora
- Il rollback è automatico: se il nodo aggiornato non invia heartbeat entro 60s dal reboot, il bootloader ripristina il firmware precedente

---

## Partizioni flash

Ogni nodo funzione ha la seguente partition table (partizioni OTA):

```
# partitions.csv
# Name,     Type, SubType,  Offset,   Size
nvs,        data, nvs,      0x9000,   0x5000
otadata,    data, ota,      0xe000,   0x2000
ota_0,      app,  ota_0,    0x10000,  0x180000  # 1.5 MB per firmware
ota_1,      app,  ota_1,    0x190000, 0x180000  # 1.5 MB per firmware
config,     data, spiffs,   0x310000, 0x10000   # node_config.json
```

- Il bootloader alterna tra `ota_0` e `ota_1` a ogni OTA completata
- `otadata` registra quale partizione è attiva e lo stato di validità
- `config` (SPIFFS) non viene mai toccata dall'OTA — la configurazione sopravvive all'aggiornamento

---

## Messaggi mesh OTA

| Codice | Nome | Direzione | Descrizione |
|---|---|---|---|
| `0x20` | `MSG_OTA_START` | HMI → ROOT → Nodo | Avvia sessione OTA: versione, dimensione, CRC32 totale |
| `0x21` | `MSG_OTA_CHUNK` | ROOT → Nodo | Blocco firmware: chunk_id, dati (max 200 byte), CRC8 chunk |
| `0x22` | `MSG_OTA_END` | ROOT → Nodo | Fine trasferimento: verifica CRC32 totale, avvia reboot |
| `0x23` | `MSG_OTA_ACK` | Nodo → ROOT | Conferma chunk ricevuto / esito OTA / errore |

### Payload MSG_OTA_START

```c
typedef struct __attribute__((packed)) {
    uint8_t  target_node_id;   // nodo da aggiornare
    uint32_t fw_size;          // dimensione totale firmware (byte)
    uint32_t fw_crc32;         // CRC32 dell'intero firmware
    uint8_t  fw_version[4];    // major.minor.patch.build
    char     fw_name[16];      // nome firmware es. "step_v1.2.0"
} ota_start_payload_t;         // 28 byte
```

### Payload MSG_OTA_CHUNK

```c
typedef struct __attribute__((packed)) {
    uint16_t chunk_id;         // indice del chunk (0-based)
    uint16_t chunk_size;       // dimensione dati in questo chunk (1–200 byte)
    uint8_t  chunk_crc8;       // CRC8 dei soli dati
    uint8_t  data[200];        // dati firmware
} ota_chunk_payload_t;         // 205 byte — supera MSG_PAYLOAD_MAX, usare chunk_size ≤ 195
```

> Nota: con `MSG_PAYLOAD_MAX = 200` byte header incluso (4B), i dati per chunk sono max **195 byte**. Il firmware ROOT deve suddividere in chunk da 195 byte.

### Payload MSG_OTA_ACK

```c
typedef struct __attribute__((packed)) {
    uint16_t chunk_id;         // chunk confermato (0xFFFF = fine OTA)
    uint8_t  status;           // OTA_ACK_OK, OTA_ACK_RETRY, OTA_ACK_ABORT
    uint8_t  error_code;       // dettaglio errore (0 = nessuno)
} ota_ack_payload_t;           // 4 byte
```

---

## Flusso completo OTA

### Sequenza messaggi

```
HMI              ROOT              Nodo Target
 │                │                    │
 │ MSG_OTA_START  │                    │
 │───────────────►│                    │
 │                │  MSG_OTA_START     │
 │                │───────────────────►│
 │                │                   │ esp_ota_begin()
 │                │                   │ → prepara partizione inattiva
 │                │  MSG_OTA_ACK (OK)  │
 │                │◄───────────────────│
 │                │                   │
 │                │  MSG_OTA_CHUNK [0] │
 │                │───────────────────►│ esp_ota_write(data)
 │                │  MSG_OTA_ACK [0]   │
 │                │◄───────────────────│
 │                │  MSG_OTA_CHUNK [1] │
 │                │───────────────────►│ ...
 │                │  MSG_OTA_ACK [1]   │
 │                │◄───────────────────│
 │                │       ...          │
 │                │  MSG_OTA_END       │
 │                │───────────────────►│ esp_ota_end()
 │                │                   │ verifica CRC32
 │                │  MSG_OTA_ACK (END) │
 │                │◄───────────────────│ esp_restart()
 │  Progresso OTA │                   │
 │◄───────────────│                   │ ← reboot, avvio nuovo firmware
 │                │                   │
 │                │  MSG_HEARTBEAT     │ ← entro 60s
 │                │◄───────────────────│
 │  OTA completata│                   │ → rollback annullato
```

### Implementazione nodo (ricevitore OTA)

```c
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *ota_partition = NULL;

void handle_ota_start(ota_start_payload_t *payload) {
    ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        send_ota_ack(0, OTA_ACK_ABORT, OTA_ERR_NO_PARTITION);
        return;
    }
    esp_err_t err = esp_ota_begin(ota_partition, payload->fw_size, &ota_handle);
    if (err != ESP_OK) {
        send_ota_ack(0, OTA_ACK_ABORT, OTA_ERR_BEGIN_FAIL);
        return;
    }
    expected_crc32 = payload->fw_crc32;
    total_chunks = (payload->fw_size + 194) / 195;  // ceil(fw_size / 195)
    received_chunks = 0;
    send_ota_ack(0, OTA_ACK_OK, 0);
}

void handle_ota_chunk(ota_chunk_payload_t *payload) {
    uint8_t calc_crc = crc8(payload->data, payload->chunk_size);
    if (calc_crc != payload->chunk_crc8) {
        send_ota_ack(payload->chunk_id, OTA_ACK_RETRY, OTA_ERR_CRC_MISMATCH);
        return;
    }
    esp_ota_write(ota_handle, payload->data, payload->chunk_size);
    received_chunks++;
    send_ota_ack(payload->chunk_id, OTA_ACK_OK, 0);
}

void handle_ota_end(void) {
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        send_ota_ack(0xFFFF, OTA_ACK_ABORT, OTA_ERR_END_FAIL);
        return;
    }
    esp_ota_set_boot_partition(ota_partition);
    send_ota_ack(0xFFFF, OTA_ACK_OK, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();  // riavvio con nuovo firmware
}
```

### Implementazione ROOT (distributore OTA)

```c
void ota_distributor_task(void *pvParam) {
    ota_session_t *session;
    xQueueReceive(ota_queue, &session, portMAX_DELAY);

    // Invia OTA_START al nodo target
    send_ota_start(session->target_id, session);

    // Aspetta ACK con timeout
    if (!wait_for_ota_ack(session->target_id, 5000)) {
        notify_hmi_ota_error(session, OTA_ERR_TIMEOUT);
        return;
    }

    // Invia tutti i chunk
    uint32_t offset = 0;
    uint16_t chunk_id = 0;

    while (offset < session->fw_size) {
        uint16_t chunk_size = MIN(195, session->fw_size - offset);
        uint8_t *chunk_data = session->fw_data + offset;

        for (int retry = 0; retry < 3; retry++) {
            send_ota_chunk(session->target_id, chunk_id, chunk_data, chunk_size);

            ota_ack_t ack;
            if (wait_for_ota_ack_chunk(session->target_id, chunk_id, &ack, 3000)) {
                if (ack.status == OTA_ACK_OK) break;
                if (ack.status == OTA_ACK_ABORT) goto ota_failed;
                // OTA_ACK_RETRY: ritrasmetti il chunk
            }
        }

        offset += chunk_size;
        chunk_id++;
        notify_hmi_ota_progress(session, offset);  // aggiorna barra progresso HMI
    }

    // Fine trasferimento
    send_ota_end(session->target_id);
    if (!wait_for_ota_ack(session->target_id, 5000))
        goto ota_failed;

    // Aspetta heartbeat entro 60s (conferma rollback non necessario)
    if (!wait_for_node_heartbeat(session->target_id, 60000)) {
        notify_hmi_ota_rollback(session);
        return;
    }

    notify_hmi_ota_success(session);
    return;

ota_failed:
    notify_hmi_ota_error(session, OTA_ERR_TRANSFER);
}
```

---

## Rollback automatico

Il bootloader ESP-IDF gestisce il rollback automatico tramite il meccanismo `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`:

```c
// Nel nuovo firmware, subito dopo il boot, il nodo deve chiamare:
esp_ota_mark_app_valid_cancel_rollback();
// Questo viene fatto solo dopo che il nodo ha completato
// l'inizializzazione e inviato almeno un MSG_HEARTBEAT al ROOT.
```

Se il nodo non chiama `esp_ota_mark_app_valid_cancel_rollback()` prima del watchdog timeout (tipicamente 60s), il bootloader al prossimo reboot ripristina automaticamente il firmware precedente.

**Sequenza rollback:**

```
Nodo si riavvia con nuovo firmware
         │
         ▼
Boot + inizializzazione (max 60s)
         │
    Successo?
    /         \
  Sì           No (crash, watchdog, hang)
   │             │
   ▼             ▼
mark_valid()   Watchdog reset
send heartbeat       │
                     ▼
              Boot con firmware precedente
              (ota_0 ↔ ota_1 swap automatico)
                     │
                     ▼
              Nodo online con firmware stabile
              ROOT notifica HMI: OTA rollback
```

---

## Aggiornamento OTA dal punto di vista HMI

1. L'utente seleziona il nodo target nel menu configurazione HMI
2. L'HMI carica il file firmware dalla MicroSD o lo riceve via USB-C
3. L'HMI verifica la compatibilità del firmware (node_type nel header)
4. L'HMI invia `MSG_OTA_START` al ROOT con i metadati del firmware
5. Il ROOT avvia la distribuzione chunk-by-chunk
6. L'HMI mostra una barra di progresso aggiornata dal ROOT (ogni chunk confermato)
7. Al termine, l'HMI mostra il risultato: successo, rollback, o errore con codice

---

## Gestione degli errori

| Errore | Codice | Causa | Azione ROOT |
|---|---|---|---|
| `OTA_ERR_TIMEOUT` | 1 | Nodo non risponde al OTA_START | Abort, notifica HMI |
| `OTA_ERR_CRC_MISMATCH` | 2 | Chunk corrotto in transit | Ritrasmissione chunk (max 3 tentativi) |
| `OTA_ERR_NO_PARTITION` | 3 | Partizione OTA non trovata | Abort, notifica HMI |
| `OTA_ERR_BEGIN_FAIL` | 4 | `esp_ota_begin()` fallito | Abort, notifica HMI |
| `OTA_ERR_END_FAIL` | 5 | CRC32 totale non valido | Abort, nodo resta sul firmware precedente |
| `OTA_ERR_ROLLBACK` | 6 | Heartbeat non ricevuto entro 60s | Nodo torna automaticamente al firmware precedente |
| `OTA_ERR_TRANSFER` | 7 | Troppi retry su un chunk | Abort |

---

## Limitazioni e note operative

- **Un nodo alla volta**: il ROOT gestisce una sola sessione OTA contemporaneamente. L'HMI deve attendere il completamento prima di avviare un'altra sessione
- **Mesh attiva durante OTA**: gli altri nodi continuano a operare normalmente — il traffico OTA usa la mesh come canale dati separato
- **Dimensione firmware**: con chunk da 195 byte, un firmware da 512KB richiede ~2700 chunk. A 100ms per chunk (trasmissione + ACK), l'OTA completa in ~4.5 minuti
- **node_config.json preserved**: la partizione SPIFFS `config` non viene toccata dall'OTA — la configurazione hardware sopravvive all'aggiornamento
- **Aggiornare ROOT**: il ROOT deve essere aggiornato tramite cavo UART (JTAG o seriale) — non può auto-aggiornarsi via mesh perché è il coordinatore del processo OTA
