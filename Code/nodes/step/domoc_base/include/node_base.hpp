// DomoC — NodeBase: classe base per tutti i nodi della mesh
//
// Concentra tutta la logica di comunicazione comune: inizializzazione mesh,
// registrazione al ROOT, heartbeat periodico, ricezione/dispatch dei messaggi,
// coda di trasmissione, rilevamento della modalita STANDALONE.
//
// Un nodo concreto eredita da NodeBase, implementa i due metodi puri
// (descriptor() e build_status_payload()) e ridefinisce solo gli hook
// degli eventi che gli interessano. Tutta la macchina della messaggistica
// vive qui — i nodi figli pensano solo alla propria logica applicativa.
//
//   class StepNode : public domoc::NodeBase {
//   public:
//       StepNode() : NodeBase(NODE_ID_STEP, NodeType::STEP, "STEP") {}
//   protected:
//       const NodeDescriptor& descriptor() const override { return kDesc; }
//       size_t build_status_payload(uint8_t* out, size_t max) override { ... }
//       void on_command(const CmdPayload& cmd, uint8_t seq) override { ... }
//       void on_key_on(const KeyOnPayload&) override { ... }
//   };

#pragma once

#include "mesh_protocol.hpp"
#include "node_descriptor.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"

#include <cstdint>

namespace domoc {

// Parametri di rete — in futuro popolati da node_config.json.
struct MeshConfig {
    uint8_t mesh_id[6]   = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
    uint8_t channel      = 6;
    const char* password = "domoc_mesh_2024";
    uint8_t max_layer    = 4;
    int8_t  tx_power_dbm  = 10;
    bool    is_root      = false; // true solo per il nodo MASTER
};

class NodeBase {
public:
    NodeBase(uint8_t node_id, NodeType type, const char* name);
    virtual ~NodeBase();

    NodeBase(const NodeBase&)            = delete;
    NodeBase& operator=(const NodeBase&) = delete;

    // Inizializza NVS/Wi-Fi/mesh e avvia i task. Chiamato una volta da app_main
    // del nodo concreto. Da questo momento la messaggistica e autonoma.
    esp_err_t begin(const MeshConfig& cfg);

    uint8_t  node_id()         const { return node_id_; }
    bool     key_on_active()   const { return key_on_active_; }
    bool     is_standalone()   const { return standalone_; }
    bool     is_registered()   const { return registered_; }

protected:
    // ── Hook obbligatori ──────────────────────────────────────────────────
    // Descriptor statico del nodo (auto-descrizione per l'HMI).
    virtual const NodeDescriptor& descriptor() const = 0;
    // Riempie il payload di stato per heartbeat/MSG_STATUS. Ritorna i byte scritti.
    virtual size_t build_status_payload(uint8_t* out, size_t max) = 0;

    // ── Hook opzionali (default no-op) ────────────────────────────────────
    virtual void on_command(const CmdPayload& cmd, uint8_t seq) {}
    virtual void on_key_on(const KeyOnPayload& p)   {}
    virtual void on_key_off(const KeyOffPayload& p) {}
    virtual void on_step_open(const StepOpenPayload& p) {}
    // Messaggio non riconosciuto dal dispatch comune: il nodo puo gestirlo.
    virtual void on_message(const MeshMsg& msg, size_t payload_len) {}
    virtual void on_mesh_connected()    {}
    virtual void on_mesh_disconnected() {}
    virtual void on_standalone_enter()  {}
    virtual void on_standalone_exit()   {}

    // ── API per i nodi concreti ───────────────────────────────────────────
    // Accoda un messaggio verso un nodo specifico.
    esp_err_t send_to(uint8_t dst, MsgType type, const void* payload, size_t len);
    // Accoda un messaggio broadcast (dst = 0xFF, nessun ACK).
    esp_err_t broadcast(MsgType type, const void* payload, size_t len);
    // Costruisce e invia subito un MSG_STATUS con il payload corrente del nodo.
    void send_status_update();

    // Trasmette un frame gia serializzato (header+payload+CRC8). Default:
    // invia verso il ROOT, che fa da router. Il nodo MASTER ridefinisce
    // questo metodo per instradare i messaggi downstream verso i nodi.
    virtual esp_err_t transmit_frame(uint8_t dst_id, const uint8_t* frame, size_t len);

private:
    struct TxItem {
        uint8_t dst;
        uint8_t type;
        uint8_t len;
        uint8_t payload[MSG_PAYLOAD_MAX];
    };

    static void rx_trampoline(void* self);
    static void tx_trampoline(void* self);
    static void housekeeping_trampoline(void* self);

    void rx_loop();
    void tx_loop();
    void housekeeping_loop(); // heartbeat periodico + rilevamento standalone

    void dispatch(const MeshMsg& msg, size_t payload_len);
    void do_register(bool reconnect);
    void send_descriptor();
    void enter_standalone();
    void exit_standalone();

    uint8_t  next_seq() { return seq_++; }
    uint32_t now_ms() const;
    uint32_t heartbeat_jitter_ms() const; // 0–2000 ms casuali

    esp_err_t mesh_start(const MeshConfig& cfg);

    const uint8_t node_id_;
    const NodeType node_type_;
    char     name_[16];
    MeshConfig cfg_{};

    QueueHandle_t tx_queue_  = nullptr;
    TaskHandle_t  rx_task_   = nullptr;
    TaskHandle_t  tx_task_   = nullptr;
    TaskHandle_t  hk_task_   = nullptr;

    volatile bool running_       = false;
    volatile bool registered_    = false;
    volatile bool standalone_    = false;
    volatile bool key_on_active_ = false;

    uint8_t  seq_                 = 0;
    uint32_t last_connected_ms_   = 0;
    uint32_t last_heartbeat_ms_   = 0;
    uint32_t next_heartbeat_ms_   = 0;
};

} // namespace domoc
