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

## Funzioni applicative aggiuntive

Oltre alle funzioni infrastrutturali, il MASTER gestisce direttamente:

- **Accensione/spegnimento luce esterna**: tramite uscita digitale o relay, il MASTER può comandare la luce esterna del camper su richiesta mesh o HMI.
- **Lettura livello acque chiare**: legge la tensione analogica proporzionale al livello del serbatoio acque chiare (tramite ADC) e pubblica il dato sulla mesh.
- **Rilevamento segnale acque grigie piene**: monitora un ingresso digitale collegato al sensore di livello acque grigie pieno e pubblica un alert sulla mesh.

Queste funzioni permettono di centralizzare alcune logiche di base e semplificare il cablaggio, mantenendo la mesh aggiornata su stati critici e comandi di servizio.

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
- **Non necessita di connessione fisica** dopo l'installazione — tutto via OTA da HMI


### Alimentazione — doppia sorgente

L'HMI può essere alimentato a batteria o tramite USB/12V. Non è necessario che sia sempre operativo.

```
[Bus 12V camper] ──→ [Buck 12V→5V, 2A] ──→ ┐
                                             ├──→ [Diodo OR / PowerMux] ──→ [LDO 5V→3.3V] ──→ ESP32-S3
[USB-C 5V esterno] ──────────────────────────┘
                          ↑
                    selezione automatica
                    (priorità 12V se presente)
```

- **Sorgente primaria**: bus 12V del camper → buck converter (es. LM2596 o MP2307) → 5V → LDO 3.3V
- **Sorgente secondaria**: USB-C 5V (alimentatore esterno, powerbank, PC) → direttamente al PowerMux
- **Selezione automatica**: circuito PowerMux (es. TPS2116 o due diodi Schottky con resistenza di priorità) — il 12V ha priorità; se assente, subentra USB-C senza interruzione
- **Capacità di hold-up**: condensatore bulk (470–1000 µF) sul rail 3.3V per assorbire micro-interruzioni durante switch di sorgente
- **Nessuna batteria interna**: non necessaria — USB-C è sempre disponibile durante manutenzione

> **Nota firmware**: al boot il master legge un GPIO collegato al segnale di presenza 12V (tramite partitore + optoisolatore). Se il 12V è assente, visualizza sul display un banner `⚡ Alimentazione USB-C` e disabilita i comandi verso gli attuatori (sicurezza: non comandare gradino/porta se il bus di potenza è assente).

### Display

- **Dimensioni consigliate**: 3.5"–4.3" (480×320 o 800×480)
- **Interfaccia**: SPI (ILI9488 o ST7796) oppure RGB parallel (per display 800×480 con ESP32-S3)
- **Touch**: capacitivo (FT5336 o GT911, I2C) — preferibile al resistivo per usabilità in ambiente camper
- **Backlight**: PWM dimmerabile via LEDC (ESP32-S3) — si spegne dopo 60s di inattività

### Input fisico

- **Encoder rotativo** con pulsante integrato (navigazione menu senza touch — utile con guanti o in condizioni di vibrazione)
- **Pulsante fisico dedicato** per conferma/azione rapida (es. chiudi tutto al momento della partenza)

### Connettività

| Interfaccia | Uso |
|---|---|
| Wi-Fi 802.11 (ESP-Mesh) | Comunicazione con tutti i nodi |
| USB-C (USB OTG) | Alimentazione secondaria + flash/debug |
| UART (pin header) | Debug seriale in campo |
| MicroSD (SPI) | Log eventi persistente, storage OTA firmware |

---

## Funzioni software

### 1. Ruolo mesh — nodo foglia

- Si connette come nodo normale alla mesh (mai root, mai dispatcher)
- Riceve lo stato aggiornato dei nodi dal ROOT
- Visualizza lo stato e gli alert
- Permette l'invio di comandi manuali ai nodi funzione

ogni secondo (timer task):

### 2. Visualizzazione stato e alert

Riceve e mostra lo stato dei nodi e gli alert inoltrati dal ROOT. Non prende decisioni autonome.

### 3. Invio comandi manuali

L'utente può inviare comandi manuali ai nodi funzione tramite l'interfaccia HMI. Nessuna logica di supervisione o automazione.

**Flusso tipico KEY_ON (chiave inserita) — tutto automatico senza HMI**:

```
[KEY_ON nodo]  ──MSG_ALERT broadcast──→  [STEP nodo]   → legge evento, valuta stato
                                                         se gradino aperto → chiude autonomamente
                                                         invia MSG_ALERT "STEP: chiusura automatica"
                        └──────────────────→  [GREY_WATER] → legge evento, valuta stato
                                                         se valvola aperta → chiude autonomamente
                        └──────────────────→  [HMI]     → riceve gli alert
                                                         mostra notifica sul display
                                                         non invia nessun comando
```

> **Nota attuatori valvole**:
> - **Valvola acque grigie**: inversione polarità tramite H-bridge, come il gradino. Serve per aprire/chiudere.
> - **Valvola acque chiare**: NC, si apre solo se alimentata. Senza alimentazione torna chiusa automaticamente.

**L'HMI invia comandi SOLO su richiesta dell’utente**:

| Azione utente sul display | Comando inviato |
|---|---|
| Tocca pulsante "APRI gradino" | `MSG_COMMAND` → `STEP` ACTION_OPEN |
| Tocca pulsante "APRI valvola grigie" | `MSG_COMMAND` → `GREY_WATER` ACTION_OPEN |
| Tocca pulsante "CHIUDI valvola grigie" | `MSG_COMMAND` → `GREY_WATER` ACTION_CLOSE |
| Tocca pulsante "APRI valvola chiare" | `MSG_COMMAND` → `FRESH_WATER` ACTION_OPEN |
| Tocca pulsante "CHIUDI valvola chiare" | `MSG_COMMAND` → `FRESH_WATER` ACTION_CLOSE |
| Tocca pulsante "Chiudi tutto" | `MSG_COMMAND` broadcast a tutti gli attuatori |
| Avvia OTA da menu | `MSG_OTA_START` → nodo selezionato |

> **Caso limite**: l’utente può sempre sovrascrivere manualmente l’azione autonoma di un nodo tramite display. Es. riaprire il gradino dopo che si è chiuso automaticamente.

### 4. Interfaccia utente — schermate

#### Schermata 1 — Dashboard principale

```
┌──────────────────────────────────────────────┐
│  DomoC          🔋12.8V  ⚙  🔌12V  14:32    │
├──────────────┬───────────────────────────────┤
│ 🟢 STEP      │ 🟢 GREY_WATER                │
│  Chiuso      │  Chiusa (inversione polarità) │
├──────────────┼───────────────────────────────┤
│ 🟢 FRESH     │ 🟢 FRONT_DOOR                │
│  Chiusa (NC, si apre solo se alimentata) │  Chiusa                       │
├──────────────┼───────────────────────────────┤
│ 🟢 T.BUNK    │ 🟢 T.LOFT    │ 🟢 T.KITCHEN │
│  19.2°C      │  21.0°C      │  20.5°C       │
├──────────────┴───────────────────────────────┤
│ 🌡 Esterno: 12.4°C  💧 68%     [LOG] [CFG]  │
└──────────────────────────────────────────────┘
```

- Icona colorata per ogni nodo: 🟢 online / 🟡 warning / 🔴 offline
- Indicatore sorgente alimentazione: `🔌12V` oppure `🔌USB`
- Tensione batteria servizio sempre visibile in header

#### Schermata 2 — Dettaglio nodo

- Stato corrente (con icona)
- Ultimo valore / timestamp
- Pulsanti azione (es. APRI / CHIUDI per attuatori)
- Statistiche: RSSI, uptime, hop count, versione firmware
- Pulsante "Forza refresh stato"

#### Schermata 3 — Allarmi

- Lista eventi con timestamp (ultimi 50)
- Filtro per tipo (allarmi, disconnessioni, comandi, OTA)
- Colori: rosso = critico, giallo = warning, grigio = info

#### Schermata 4 — Topologia mesh

- Rappresentazione testuale ad albero della rete
- Per ogni nodo: hop count, RSSI verso parent, stato
- Utile per diagnostica di copertura

#### Schermata 5 — Configurazione

- Soglie allarmi batteria
- Timeout backlight display
- Nomi personalizzati dei nodi
- Intervallo heartbeat globale
- Avvio aggiornamento OTA nodi

---

## Task FreeRTOS

| Task | Core | Priorità | Stack | Funzione |
| --- | --- | --- | --- | --- |
| `mesh_rx_task` | 0 | 5 | 4 KB | Ricezione messaggi mesh, dispatch alla coda interna |
| `mesh_tx_task` | 0 | 5 | 4 KB | Invio comandi **manuali** utente, gestione ACK e retry |
| `heartbeat_monitor_task` | 0 | 4 | 2 KB | Controllo last\_seen, aggiornamento stati nodi |
| `alert_display_task` | 0 | 4 | 3 KB | Ricezione MSG\_ALERT dalla mesh, aggiornamento UI, log SD, buzzer |
| `display_task` | 1 | 3 | 12 KB | Rendering LVGL, gestione touch/encoder, coda eventi UI |
| `command_processor_task` | 0 | 4 | 4 KB | Elaborazione comandi **manuali** da UI → coda mesh TX |
| `sd_logger_task` | 0 | 1 | 3 KB | Scrittura asincrona log eventi su microSD (ring buffer) |
| `ota_manager_task` | 0 | 2 | 8 KB | Distribuzione aggiornamenti OTA ai nodi (su richiesta utente) |
| `power_monitor_task` | 0 | 3 | 2 KB | Lettura sorgente alimentazione, gestione banner USB |

> `display_task` è assegnato al **Core 1** per non interferire con la mesh (Core 0). LVGL richiede un tick periodico ogni 5ms (`lv_timer_handler()`).
>
> `alarm_engine_task` **non esiste più**: la logica reattiva vive nei singoli nodi. Il master ha solo `alert_display_task` che è passivo — riceve, mostra, logga.

---

## Comportamento in assenza di alimentazione 12V

Quando il master viene alimentato solo via USB-C (es. durante manutenzione del camper):

1. Il banner `⚡ Alimentazione USB-C — modalità diagnostica` viene mostrato in header
2. I **comandi verso gli attuatori sono bloccati** (gradino, valvole, porta) — il bus di potenza 12V è assente e gli attuatori non funzionerebbero comunque
3. La **mesh rimane attiva**: i nodi ancora alimentati continuano a inviare heartbeat
4. Il **display e il log** funzionano normalmente — utile per diagnostica
5. Se il 12V torna, il banner scompare e i comandi vengono riabilitati automaticamente

---

## Considerazioni hardware pratiche

- **Dissipatore**: l'ESP32-S3 con display attivo può scaldarsi — prevedere pad termico o piccolo dissipatore passivo sul modulo
- **Protezione ESD**: proteggere i pin USB-C e GPIO esposti con TVS diode (es. USBLC6-2)
- **Fissaggio display**: in ambiente camper con vibrazioni, usare viti + cornice rigida — non solo biadesivo
- **Connettore di alimentazione 12V**: usare connettore con locking (es. XT30 o Anderson PowerPole) — evitare jack barrel che si sfilano
- **Buzzer**: aggiungere un buzzer piezoelettrico passivo per allarmi sonori (es. scaletta aperta a motore acceso)
- **LED di stato**: un LED RGB sul PCB indica lo stato generale anche a display spento (verde = tutto ok, rosso lampeggiante = allarme attivo)

---

## Monitoraggio diretto su MASTER

Oltre alle funzioni di coordinamento mesh, il MASTER può monitorare direttamente alcuni segnali fisici del camper tramite ingressi dedicati:

- **Livello acque chiare**: se disponibile dal pannello di fabbrica del camper, il MASTER può leggere la tensione analogica proporzionale al livello del serbatoio acque chiare (tramite ADC). Questo dato viene pubblicato periodicamente sulla mesh.

- **Allarme serbatoio acque scure pieno**: ingresso digitale collegato al segnale di allarme del serbatoio acque nere (tipicamente un galleggiante o sensore reed). Quando il serbatoio è pieno, il MASTER invia un alert sulla mesh e lo stato viene visualizzato sull'HMI.

> **Nota:** La lettura della tensione della batteria di servizio è ora affidata al nodo GREY_WATER, che pubblica il dato sulla mesh.

> Questi segnali permettono di integrare le informazioni vitali del camper direttamente nella rete mesh, senza dover aggiungere nodi dedicati per ogni sensore. Tutti i dati sono disponibili in tempo reale su HMI e possono essere usati per automazioni o notifiche.
