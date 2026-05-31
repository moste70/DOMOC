# DomoC — Component "Base": libreria comune ereditata da tutti i nodi.
#
# Uso da un nodo concreto (es. Code/nodes/step/):
#   set(EXTRA_COMPONENT_DIRS "../../Base")
# nel CMakeLists.txt di progetto, poi REQUIRES domoc_base nel component main.

idf_component_register(
    SRCS
        "src/node_base.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        mesh
        esp_timer
        esp_mac
)
