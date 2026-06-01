// DomoC — Node Descriptor (auto-descrizione per HMI)
//
// Il descriptor (140 byte fissi) viaggia come payload di MSG_DESCRIPTOR.
// L'HMI costruisce l'intera UI da questi dati senza conoscenza hardcodata dei nodi.
// Riferimento: Document/comunicazione_nodi.md

#pragma once
#include <stdint.h>

// ── Icone ────────────────────────────────────────────────────────────────────
#define ICON_GENERIC       0x00
#define ICON_STEP          0x01
#define ICON_VALVE_GREY    0x02
#define ICON_VALVE_FRESH   0x03
#define ICON_DOOR          0x04
#define ICON_THERMOMETER   0x05
#define ICON_CAMERA        0x06
#define ICON_KEY           0x07
#define ICON_BATTERY       0x08
#define ICON_SENSOR        0x09
#define ICON_LIGHT         0x0A
#define ICON_ACT_OPEN      0x20
#define ICON_ACT_CLOSE     0x21
#define ICON_ACT_INFO      0x22
#define ICON_ACT_TEMP_UP   0x23
#define ICON_ACT_TEMP_DN   0x24
#define ICON_ACT_CAM_ON    0x25
#define ICON_ACT_CAM_OFF   0x26
#define ICON_ACT_LIGHT_ON  0x27
#define ICON_ACT_LIGHT_OFF 0x28
#define ICON_ACT_RESET     0x29
#define ICON_ACT_OTA       0x2A

// ── Tipi payload ─────────────────────────────────────────────────────────────
#define PAYLOAD_UINT8      0
#define PAYLOAD_INT8       1
#define PAYLOAD_UINT16     2
#define PAYLOAD_INT16      3
#define PAYLOAD_FLOAT32    4
#define PAYLOAD_UINT32     5

// ── ID proprietà ─────────────────────────────────────────────────────────────
#define PROP_STATE         0x01
#define PROP_TEMPERATURE   0x02
#define PROP_HUMIDITY      0x03
#define PROP_BATTERY_V     0x04
#define PROP_SETPOINT      0x05
#define PROP_VALVE_ON      0x06
#define PROP_LIGHT_ON      0x07
#define PROP_DOOR_OPEN     0x08
#define PROP_CAM_ON        0x09

// ── Widget LVGL ──────────────────────────────────────────────────────────────
#define WIDGET_LABEL       0x00
#define WIDGET_VALUE_UNIT  0x01
#define WIDGET_GAUGE       0x02
#define WIDGET_PROGRESS    0x03
#define WIDGET_INDICATOR   0x04
#define WIDGET_THERMOMETER 0x05
#define WIDGET_BATTERY     0x06

// ── Tipo controllo UI ────────────────────────────────────────────────────────
#define CTRL_BUTTON        0x00
#define CTRL_TOGGLE        0x01
#define CTRL_STEPPER       0x02
#define CTRL_CONFIRM       0x03
#define CTRL_SLIDER        0x04

// ── Flag azioni ───────────────────────────────────────────────────────────────
#define FLAG_CONFIRM_REQUIRED 0x01
#define FLAG_KEY_BLOCKED      0x02  // disabilitato con KEY_ON attivo

// ── Strutture descriptor ─────────────────────────────────────────────────────
#define DESC_MAX_ACTIONS    4
#define DESC_MAX_PROPERTIES 4

#pragma pack(push, 1)

typedef struct {        // 14 byte
    uint8_t action_code;
    uint8_t icon_id;
    uint8_t ctrl_type;
    uint8_t group_id;
    uint8_t linked_property;
    uint8_t flags;
    char    label[8];
} ActionDesc;

typedef struct {        // 20 byte
    uint8_t property_id;
    uint8_t payload_offset;
    uint8_t payload_type;
    uint8_t widget_type;
    int16_t range_min;   // ×10
    int16_t range_max;
    char    unit[4];
    char    fmt[8];
} PropertyDesc;

typedef struct {        // 140 byte
    uint8_t      node_icon;
    uint8_t      action_count;
    uint8_t      property_count;
    uint8_t      _pad;
    ActionDesc   actions[DESC_MAX_ACTIONS];
    PropertyDesc properties[DESC_MAX_PROPERTIES];
} NodeDescriptor;

#pragma pack(pop)
