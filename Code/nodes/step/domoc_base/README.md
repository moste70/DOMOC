# DomoC — Libreria Base

Component ESP-IDF condiviso: contiene il protocollo mesh e la classe `NodeBase`
da cui ereditano tutti i nodi del sistema. Tutta la logica di **comunicazione e
messaggistica** vive qui, una volta sola; i nodi concreti aggiungono solo la
propria logica applicativa.

## File

| File | Contenuto |
| --- | --- |
| `include/mesh_protocol.hpp` | Formato sul filo: `MeshMsg`, `MsgType`, payload, ID nodi, CRC8 |
| `include/node_descriptor.hpp` | `NodeDescriptor` (140 byte) + enum icone/widget/controlli |
| `include/node_base.hpp` | Classe base `NodeBase` |
| `src/node_base.cpp` | Implementazione: init mesh, task RX/TX, dispatch, heartbeat, standalone |

## Cosa gestisce già `NodeBase`

- Inizializzazione NVS / Wi-Fi / ESP-Mesh (modem sleep + TX power ridotta da CLAUDE.md)
- Registrazione al ROOT (`MSG_REGISTER`) e invio del descriptor su ACK
- Heartbeat periodico (5s ± jitter) con il payload di stato del nodo
- Task `mesh_rx` / `mesh_tx` + coda di trasmissione
- Serializzazione frame + CRC8 in TX, verifica CRC8 in RX
- Dispatch dei messaggi comuni verso gli hook virtuali
- Filtro per `dst_id` (accetta solo i messaggi propri o broadcast)
- Rilevamento `STANDALONE` (mesh assente > 30s) e ri-registrazione al ritorno

Da aggiungere in seguito (man mano che emergono): OTA receiver, parsing di
`node_config.json`, gestione LED RGB di stato, NVS store applicativo.

## Come si eredita

Un nodo concreto implementa i **due hook obbligatori** e ridefinisce solo gli
eventi che gli servono:

```cpp
#include "node_base.hpp"
using namespace domoc;

class StepNode : public NodeBase {
public:
    StepNode() : NodeBase(NODE_ID_STEP, NodeType::STEP, "STEP") {}

protected:
    const NodeDescriptor& descriptor() const override { return kDesc; }

    size_t build_status_payload(uint8_t* out, size_t max) override {
        StepStatus st = current_status();
        std::memcpy(out, &st, sizeof(st));
        return sizeof(st);
    }

    void on_command(const CmdPayload& cmd, uint8_t seq) override {
        if (cmd.action_code == ACTION_OPEN) open();
        else if (cmd.action_code == ACTION_CLOSE) close();
    }

    void on_key_on(const KeyOnPayload&) override {
        if (is_open()) start_auto_close(); // chiusura di sicurezza
    }

private:
    static const NodeDescriptor kDesc; // descriptor statico del nodo
};
```

```cpp
extern "C" void app_main(void) {
    static StepNode node;
    MeshConfig cfg;            // in futuro letta da node_config.json
    ESP_ERROR_CHECK(node.begin(cfg));
}
```

### Hook disponibili

| Hook | Obbligatorio | Quando |
| --- | :---: | --- |
| `descriptor()` | ✓ | Auto-descrizione del nodo per l'HMI |
| `build_status_payload()` | ✓ | Riempie il payload di heartbeat/`MSG_STATUS` |
| `on_command()` | — | `MSG_COMMAND` ricevuto dall'HMI |
| `on_key_on()` / `on_key_off()` | — | Broadcast di sicurezza chiave accensione |
| `on_step_open()` | — | Broadcast `MSG_STEP_OPEN` |
| `on_message()` | — | Messaggio non gestito dal dispatch comune |
| `on_mesh_connected()` / `on_mesh_disconnected()` | — | Cambi di connettività mesh |
| `on_standalone_enter()` / `on_standalone_exit()` | — | Transizioni modalità STANDALONE |

### API utilizzabili dal nodo

- `send_to(dst, type, payload, len)` — invia a un nodo specifico
- `broadcast(type, payload, len)` — invia a tutti (es. `MSG_STEP_OPEN`)
- `send_status_update()` — pubblica subito lo stato corrente
- `key_on_active()`, `is_standalone()`, `is_registered()` — interrogano lo stato base

## Note di design

- **`transmit_frame()` è virtuale**: i nodi funzione inviano upstream al ROOT
  (che fa da router); il nodo **MASTER** lo ridefinirà per instradare downstream.
- Le struct del protocollo sono `__packed__`: gli offset dei campi sono un
  contratto con l'HMI, che legge i payload tramite gli offset dei descriptor.
- La rilevazione mesh/standalone è basata su polling (`esp_mesh_is_connected()`)
  per aderire a `Document/standalone_mode.md`; si può passare agli eventi
  `esp_event` in seguito senza toccare l'interfaccia dei nodi.
