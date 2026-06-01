// DomoC — DomocMesh
//
// Wrapper di painlessMesh che gestisce: registrazione, heartbeat con jitter,
// standalone detection, KEY_ON tracking, CRC8, codifica hex per il trasporto.
//
// Uso tipico (nodo funzione):
//   DomocMesh mesh(NODE_ID_STEP, "STEP", 0x02);
//   mesh.set_on_message([](const DomocMsg* m){ ... });
//   mesh.set_on_heartbeat_due([]{ mesh.send_heartbeat(...); });
//   mesh.begin();
//   // in loop(): mesh.update();
//
// Uso ROOT:
//   DomocMesh mesh(NODE_ID_MASTER, "MASTER", 0x01, /*is_root=*/true);
//   mesh.set_on_message([](const DomocMsg* m){ ... });
//   mesh.begin();

#pragma once

#include <Arduino.h>
#include <painlessMesh.h>
#include <functional>
#include "domoc_protocol.h"

class DomocMesh {
public:
    using MsgCallback = std::function<void(const DomocMsg* msg, size_t payload_len)>;
    using VoidCallback = std::function<void()>;

    DomocMesh(uint8_t my_id, const char* name, uint8_t node_type, bool is_root = false);

    void begin();
    void update();  // chiamare ogni loop()

    // Invio
    void send_heartbeat(const uint8_t* payload, size_t len);
    void send_to_root(uint8_t msg_type, const uint8_t* payload, size_t len);
    void broadcast(uint8_t msg_type, const uint8_t* payload, size_t len);
    // ROOT usa questo per rispondere a nodi specifici
    void send_to(uint32_t mesh_node_id, uint8_t dst_logical_id, uint8_t msg_type,
                 const uint8_t* payload, size_t len);

    // Stato
    bool registered()  const { return registered_; }
    bool standalone()  const { return standalone_; }
    bool key_on()      const { return key_on_; }
    uint32_t root_mesh_id() const { return root_mesh_id_; }
    painlessMesh& raw() { return mesh_; }  // accesso diretto per casi speciali (ROOT)

    // Callback
    void set_on_message(MsgCallback cb)          { msg_cb_ = cb; }
    void set_on_heartbeat_due(VoidCallback cb)   { heartbeat_cb_ = cb; }
    void set_on_standalone_enter(VoidCallback cb){ standalone_enter_cb_ = cb; }
    void set_on_standalone_exit(VoidCallback cb) { standalone_exit_cb_ = cb; }

    uint8_t next_seq() { return seq_++; }
    uint8_t my_id()    const { return my_id_; }

private:
    void send_frame(uint32_t to_mesh_id, uint8_t dst_logical_id, uint8_t msg_type,
                    const uint8_t* payload, size_t len, bool is_broadcast = false);
    void send_register(bool reconnect);
    void on_receive(uint32_t from, String& raw);
    void on_changed_connections();

    static String  bytes_to_hex(const uint8_t* data, size_t len);
    static bool    hex_to_bytes(const String& hex, uint8_t* out, size_t& out_len);

    Scheduler    scheduler_;
    painlessMesh mesh_;

    uint8_t my_id_;
    uint8_t node_type_;
    char    name_[17];
    bool    is_root_;

    uint8_t  seq_         = 0;
    bool     registered_  = false;
    bool     standalone_  = false;
    bool     key_on_      = false;
    uint32_t root_mesh_id_= 0;

    uint32_t last_heartbeat_ms_   = 0;
    uint32_t heartbeat_interval_  = DOMOC_HEARTBEAT_MS;
    uint32_t last_mesh_activity_  = 0;

    MsgCallback  msg_cb_;
    VoidCallback heartbeat_cb_;
    VoidCallback standalone_enter_cb_;
    VoidCallback standalone_exit_cb_;
};
