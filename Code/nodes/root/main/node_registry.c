#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "node_registry.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "NODE_REG";

#define NVS_NAMESPACE   "domoc_reg"

// ── Storage in RAM ────────────────────────────────────────────────────────
static node_info_t  s_nodes[MAX_NODES];
static int          s_count     = 0;
static uint16_t     s_next_id   = 0x0002; // 0x0001 riservato al ROOT
static bool         s_dirty     = false;
static SemaphoreHandle_t s_mutex;

// ── Helpers interni ───────────────────────────────────────────────────────
static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static node_info_t *find_free_slot(void)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (!s_nodes[i].valid) return &s_nodes[i];
    }
    return NULL;
}

static node_info_t *find_by_mac_unlocked(const uint8_t mac[6])
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (s_nodes[i].valid && memcmp(s_nodes[i].mac, mac, 6) == 0)
            return &s_nodes[i];
    }
    return NULL;
}

// ── API pubblica ──────────────────────────────────────────────────────────
void node_registry_init(void)
{
    memset(s_nodes, 0, sizeof(s_nodes));
    s_mutex = xSemaphoreCreateMutex();
    node_registry_load_from_nvs();
    ESP_LOGI(TAG, "Registry inizializzata: %d nodi noti", s_count);
}

int node_registry_count(void)
{
    return s_count;
}

uint16_t node_registry_register(const reg_payload_t *reg, const uint8_t mac[6])
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    node_info_t *node = NULL;
    uint16_t assigned_id = 0;

    if (reg->reconnect) {
        // Nodo già noto: cerca per MAC
        node = find_by_mac_unlocked(mac);
        if (node) {
            assigned_id = node->node_id;
            node->status = NODE_STATUS_ONLINE;
            node->last_seen_ms = now_ms();
            ESP_LOGI(TAG, "Riconosciuto %s (ID=0x%04X) con reconnect", node->name, assigned_id);
        }
    }

    if (!node) {
        // Nuovo nodo o reconnect senza corrispondenza MAC
        node = find_by_mac_unlocked(mac); // controlla comunque per MAC duplicati
        if (!node) {
            node = find_free_slot();
            if (!node) {
                ESP_LOGE(TAG, "Registry piena — impossibile registrare nuovo nodo");
                xSemaphoreGive(s_mutex);
                return 0;
            }
            // Skip ID_ROOT e ID_HMI
            while (s_next_id == NODE_ID_ROOT || s_next_id == NODE_ID_HMI)
                s_next_id++;
            assigned_id = s_next_id++;
        } else {
            assigned_id = node->node_id;
        }

        node->valid      = true;
        node->node_id    = assigned_id;
        node->node_type  = (node_type_t)reg->node_type;
        node->status     = NODE_STATUS_ONLINE;
        node->last_seen_ms = now_ms();
        node->fw_major   = reg->fw_major;
        node->fw_minor   = reg->fw_minor;
        node->fw_patch   = reg->fw_patch;
        memcpy(node->mac, mac, 6);
        strlcpy(node->name, reg->name, NODE_NAME_MAX);

        s_count++;
        ESP_LOGI(TAG, "Nuovo nodo: %s tipo=%d ID=0x%04X", node->name, node->node_type, assigned_id);
    }

    s_dirty = true;
    xSemaphoreGive(s_mutex);
    return assigned_id;
}

node_info_t *node_registry_get_by_id(uint16_t node_id)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (s_nodes[i].valid && s_nodes[i].node_id == node_id)
            return &s_nodes[i];
    }
    return NULL;
}

node_info_t *node_registry_get_by_mac(const uint8_t mac[6])
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    node_info_t *node = find_by_mac_unlocked(mac);
    xSemaphoreGive(s_mutex);
    return node;
}

void node_registry_update_heartbeat(uint16_t node_id, int8_t rssi,
                                    const uint8_t *payload, uint8_t payload_len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    node_info_t *node = node_registry_get_by_id(node_id);
    if (node) {
        node->last_seen_ms = now_ms();
        node->rssi         = rssi;
        if (node->status == NODE_STATUS_WARNING || node->status == NODE_STATUS_OFFLINE)
            node->status = NODE_STATUS_ONLINE;

        uint8_t copy_len = payload_len < sizeof(node->payload_cache)
                           ? payload_len : sizeof(node->payload_cache);
        memcpy(node->payload_cache, payload, copy_len);
    }
    xSemaphoreGive(s_mutex);
}

void node_registry_set_status(uint16_t node_id, node_status_t status)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    node_info_t *node = node_registry_get_by_id(node_id);
    if (node) {
        node->status = status;
        s_dirty = true;
    }
    xSemaphoreGive(s_mutex);
}

void node_registry_mark_lost(uint16_t node_id)
{
    node_registry_set_status(node_id, NODE_STATUS_LOST);
    ESP_LOGW(TAG, "Nodo 0x%04X marcato LOST", node_id);
}

int node_registry_dump(node_info_t *buf, int buf_size)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int n = 0;
    for (int i = 0; i < MAX_NODES && n < buf_size; i++) {
        if (s_nodes[i].valid)
            buf[n++] = s_nodes[i];
    }
    xSemaphoreGive(s_mutex);
    return n;
}

bool node_registry_is_hmi(uint16_t node_id)
{
    node_info_t *node = node_registry_get_by_id(node_id);
    return node && node->node_type == NODE_TYPE_HMI;
}

void node_registry_mark_dirty(void) { s_dirty = true; }
bool node_registry_is_dirty(void)   { return s_dirty; }

// ── Persistenza NVS ───────────────────────────────────────────────────────
// Chiave NVS: stringa MAC "AABBCCDDEEFF" → valore: struct condensata

typedef struct {
    uint16_t    node_id;
    uint8_t     node_type;
    uint8_t     fw_major;
    uint8_t     fw_minor;
    uint8_t     fw_patch;
    char        name[NODE_NAME_MAX];
} nvs_node_entry_t;

static void mac_to_key(const uint8_t mac[6], char key[13])
{
    snprintf(key, 13, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void node_registry_save_to_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_NODES; i++) {
        if (!s_nodes[i].valid) continue;

        char key[13];
        mac_to_key(s_nodes[i].mac, key);

        nvs_node_entry_t entry = {
            .node_id   = s_nodes[i].node_id,
            .node_type = (uint8_t)s_nodes[i].node_type,
            .fw_major  = s_nodes[i].fw_major,
            .fw_minor  = s_nodes[i].fw_minor,
            .fw_patch  = s_nodes[i].fw_patch,
        };
        strlcpy(entry.name, s_nodes[i].name, NODE_NAME_MAX);

        nvs_set_blob(h, key, &entry, sizeof(entry));
    }
    // Salva anche il prossimo ID da assegnare
    nvs_set_u16(h, "next_id", s_next_id);
    nvs_commit(h);
    s_dirty = false;
    xSemaphoreGive(s_mutex);

    nvs_close(h);
    ESP_LOGD(TAG, "Registry salvata su NVS");
}

void node_registry_load_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "NVS registry non trovata — prima accensione");
        return;
    }

    nvs_get_u16(h, "next_id", &s_next_id);
    if (s_next_id < 0x0002) s_next_id = 0x0002;

    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(NVS_NAMESPACE, NULL, NVS_TYPE_BLOB, &it);
    while (err == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        nvs_node_entry_t entry;
        size_t entry_size = sizeof(entry);
        if (nvs_get_blob(h, info.key, &entry, &entry_size) == ESP_OK) {
            node_info_t *slot = find_free_slot();
            if (slot) {
                slot->valid     = true;
                slot->node_id   = entry.node_id;
                slot->node_type = (node_type_t)entry.node_type;
                slot->status    = NODE_STATUS_OFFLINE; // al boot tutti OFFLINE
                slot->fw_major  = entry.fw_major;
                slot->fw_minor  = entry.fw_minor;
                slot->fw_patch  = entry.fw_patch;
                strlcpy(slot->name, entry.name, NODE_NAME_MAX);
                // Converte chiave stringa MAC → bytes
                uint8_t *m = slot->mac;
                sscanf(info.key, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
                       &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
                s_count++;
            }
        }
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(h);
}
