// DomoC — Node Descriptor (auto-descrizione per HMI)
//
// Ogni nodo dichiara staticamente icona, azioni e proprieta. L'HMI costruisce
// l'intera UI (carosello, widget, controlli) leggendo questi dati: non ha
// alcuna conoscenza hardcodata dei tipi di nodo.
//
// Il descriptor (140 byte) viaggia come payload di MSG_DESCRIPTOR.
// Riferimento: Document/comunicazione_nodi.md

#pragma once

#include <cstdint>

namespace domoc {

// Icone — Livello 0 (nodo) e Livello 1 (azioni).
enum HmiIcon : uint8_t {
    ICON_GENERIC      = 0x00,
    ICON_STEP         = 0x01,
    ICON_VALVE_GREY   = 0x02,
    ICON_VALVE_FRESH  = 0x03,
    ICON_DOOR         = 0x04,
    ICON_THERMOMETER  = 0x05,
    ICON_CAMERA       = 0x06,
    ICON_KEY          = 0x07,
    ICON_BATTERY      = 0x08,
    ICON_SENSOR       = 0x09,
    ICON_LIGHT        = 0x0A,
    ICON_ACT_OPEN     = 0x20,
    ICON_ACT_CLOSE    = 0x21,
    ICON_ACT_INFO     = 0x22,
    ICON_ACT_TEMP_UP  = 0x23,
    ICON_ACT_TEMP_DN  = 0x24,
    ICON_ACT_CAM_ON   = 0x25,
    ICON_ACT_CAM_OFF  = 0x26,
    ICON_ACT_LIGHT_ON = 0x27,
    ICON_ACT_LIGHT_OFF= 0x28,
    ICON_ACT_RESET    = 0x29,
    ICON_ACT_OTA      = 0x2A,
};

// Tipo del valore grezzo nel payload di stato.
enum PayloadType : uint8_t {
    PAYLOAD_UINT8   = 0,
    PAYLOAD_INT8    = 1,
    PAYLOAD_UINT16  = 2,
    PAYLOAD_INT16   = 3,
    PAYLOAD_FLOAT32 = 4,
    PAYLOAD_UINT32  = 5,
};

// ID proprieta — l'HMI lo usa per interpretare il dato.
enum PropertyId : uint8_t {
    PROP_STATE       = 0x01,
    PROP_TEMPERATURE = 0x02,
    PROP_HUMIDITY    = 0x03,
    PROP_BATTERY_V   = 0x04,
    PROP_SETPOINT    = 0x05,
    PROP_VALVE_ON    = 0x06,
    PROP_LIGHT_ON    = 0x07,
    PROP_DOOR_OPEN   = 0x08,
    PROP_CAM_ON      = 0x09,
};

// Widget LVGL da istanziare per visualizzare una proprieta.
// Fallback: tipo non riconosciuto → WIDGET_LABEL.
enum WidgetType : uint8_t {
    WIDGET_LABEL       = 0x00,
    WIDGET_VALUE_UNIT  = 0x01,
    WIDGET_GAUGE       = 0x02,
    WIDGET_PROGRESS    = 0x03,
    WIDGET_INDICATOR   = 0x04,
    WIDGET_THERMOMETER = 0x05,
    WIDGET_BATTERY     = 0x06,
};

// Tipo di controllo UI per inviare un'azione.
enum CtrlType : uint8_t {
    CTRL_BUTTON  = 0x00,
    CTRL_TOGGLE  = 0x01,
    CTRL_STEPPER = 0x02,
    CTRL_CONFIRM = 0x03,
    CTRL_SLIDER  = 0x04,
};

// Flag per ActionDescriptor::flags.
inline constexpr uint8_t FLAG_CONFIRM_REQUIRED = 0x01;
inline constexpr uint8_t FLAG_KEY_BLOCKED      = 0x02; // disabilitato con KEY_ON attivo

inline constexpr uint8_t NODE_DESC_MAX_ACTIONS    = 4;
inline constexpr uint8_t NODE_DESC_MAX_PROPERTIES = 4;

struct __attribute__((packed)) ActionDescriptor {  // 14 byte
    uint8_t action_code;
    uint8_t icon_id;
    uint8_t ctrl_type;
    uint8_t group_id;        // 0 = indipendente; >0 = raggruppata
    uint8_t linked_property; // PropertyId riflesso dal controllo (0 = nessuno)
    uint8_t flags;
    char    label[8];
};

struct __attribute__((packed)) PropertyDescriptor {  // 20 byte
    uint8_t property_id;
    uint8_t payload_offset;  // byte offset nel payload di stato
    uint8_t payload_type;
    uint8_t widget_type;
    int16_t range_min;       // valore minimo ×10 (es. 150 = 15.0)
    int16_t range_max;
    char    unit[4];         // "°C", "V", "%", ""
    char    fmt[8];          // "%.1f", "%d", "%s"
};

struct __attribute__((packed)) NodeDescriptor {  // 140 byte
    uint8_t           node_icon;
    uint8_t           action_count;
    uint8_t           property_count;
    uint8_t           _pad;
    ActionDescriptor   actions[NODE_DESC_MAX_ACTIONS];
    PropertyDescriptor properties[NODE_DESC_MAX_PROPERTIES];
};

static_assert(sizeof(NodeDescriptor) == 140, "NodeDescriptor deve essere 140 byte");

} // namespace domoc
