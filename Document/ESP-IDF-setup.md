# DOMOC — Guida Setup Ambiente ESP-IDF

## Indice
1. [Requisiti di sistema](#requisiti-di-sistema)
2. [Installazione ESP-IDF](#installazione-esp-idf)
3. [Configurazione VS Code + PlatformIO](#configurazione-vs-code--platformio)
4. [Librerie e dipendenze](#librerie-e-dipendenze)
5. [Struttura directory firmware](#struttura-directory-firmware)
6. [First build e verifica](#first-build-e-verifica)
7. [Configurazione per ogni MCU](#configurazione-per-ogni-mcu)

---

## Requisiti di sistema

### Hardware minimo
- PC/Mac/Linux con ~2 GB di RAM disponibile
- 500 MB spazio disco (ESP-IDF + toolchain)
- Porta USB per connessione seriale (UART debug)

### Software obbligatorio
| Componente | Versione minima | Uso |
|---|---|---|
| **Python** | 3.8+ | Build system, tools ESP-IDF |
| **CMake** | 3.16+ | Build configuration |
| **Git** | 2.25+ | Clone repository ESP-IDF |
| **gcc/clang** | Recente | Compilazione host tools |

### Verifiche preliminari
```bash
python3 --version    # Deve essere 3.8+
cmake --version      # Deve essere 3.16+
git --version        # Qualsiasi versione recente
```

---

## Installazione ESP-IDF

### Metodo 1: Installazione manuale (Linux/macOS)

**Passo 1: Clone repository**
```bash
mkdir -p ~/esp
cd ~/esp
git clone --branch v5.1 https://github.com/espressif/esp-idf.git
cd esp-idf
```

**Passo 2: Installa dipendenze Python**
```bash
python3 -m pip install --upgrade pip
python3 -m venv ~/esp/idf_venv
source ~/esp/idf_venv/bin/activate  # Linux/macOS
# Nel prompt (idf_venv)
pip install -r requirements.txt
```

**Passo 3: Installa toolchain**
```bash
./install.sh esp32c3,esp32s3,esp32
```

**Passo 4: Configura variabili d'ambiente**

Aggiungi al file `~/.bashrc` o `~/.zshrc`:
```bash
# ESP-IDF v5.1
export IDF_PATH="$HOME/esp/esp-idf"
export IDF_TOOLS_PATH="$HOME/.espressif"
alias get_idf='. $IDF_PATH/export.sh'
```

Ricaricare il terminale:
```bash
source ~/.bashrc
get_idf  # Attiva le variabili per la sessione corrente
```

### Metodo 2: Installazione Windows (Git Bash)

**Passo 1: Clone repository**
```bash
mkdir C:\esp
cd C:\esp
git clone --branch v5.1 https://github.com/espressif/esp-idf.git
cd esp-idf
```

**Passo 2: Esegui script installer (per Windows)**
```bash
./install.bat esp32c3 esp32s3 esp32
```

**Passo 3: Attiva environment**
```bash
./export.bat
```

### Metodo 3: Installazione con VS Code + Extension (consigliato)

1. Installa VS Code: https://code.visualstudio.com/
2. Installa extension **ESP-IDF Tools** dal marketplace
3. Apri Command Palette (`Ctrl+Shift+P`) → `ESP-IDF: Configure ESP-IDF Extension`
4. Segui wizard — scarica e configura automaticamente

---

## Configurazione VS Code + PlatformIO

### Opzione A: VS Code Extension ufficiale ESP-IDF

**Installazione:**
1. VS Code → Extensions → Cerca "ESP-IDF Tools"
2. Installa **ESP-IDF Tools** by Espressif Systems
3. Command Palette → `ESP-IDF: Configure ESP-IDF Extension`

**Shortcut utili:**
- `Ctrl+Shift+P` → `ESP-IDF: Build`
- `Ctrl+Shift+P` → `ESP-IDF: Flash Device`
- `Ctrl+Shift+P` → `ESP-IDF: Monitor` (seriale)

### Opzione B: PlatformIO (IDE visuale, consigliata)

**Installazione:**
1. VS Code → Extensions → Cerca "PlatformIO IDE"
2. Installa **PlatformIO IDE** by PlatformIO
3. Reload VS Code

**Configurazione per DOMOC:**

Crea file `platformio.ini` nella root del progetto:

```ini
[env]
framework = espidf
monitor_speed = 115200
monitor_raw = yes
upload_speed = 460800

; Configurazione debug UART
monitor_filters = esp32_exception_decoder

[env:step-c3]
platform = espressif32
board = esp32-c3-devkitm-1
build_flags = 
    -DNODE_NAME=\"STEP\"
    -DNODE_ID=0x0002

[env:fresh-water-c3]
platform = espressif32
board = esp32-c3-devkitm-1
build_flags = 
    -DNODE_NAME=\"FRESH_WATER\"
    -DNODE_ID=0x0004

[env:hmi-s3]
platform = espressif32
board = esp32-s3-devkitc-1
build_flags = 
    -DNODE_NAME=\"HMI\"
    -DNODE_ID=0x000B
```

**Shortcut utili:**
- `Ctrl+Alt+U` → Build & Upload
- `Ctrl+Alt+S` → Build
- `Ctrl+Alt+I` → Monitor seriale

---

## Librerie e dipendenze

### Librerie built-in ESP-IDF (già incluse)

| Libreria | Componente ESP-IDF | Uso in DOMOC |
|---|---|---|
| **ESP-Mesh** | `esp_mesh` | Comunicazione mesh tra nodi |
| **FreeRTOS** | `freertos` | Task, mutex, queue |
| **NVS Flash** | `nvs_flash` | Persistenza registry e configurazione |
| **SPIFFS** | `spiffs` | Filesystem JSON (node_config.json) |
| **ESP-TLS** | `esp-tls` | TLS/SSL (opzionale) |
| **Logging** | `esp_log` | Sistema log ESP_LOG_* |
| **GPIO/PWM** | `driver` | GPIO, LEDC PWM, ADC, I2C, 1-Wire |
| **OTA** | `esp_https_ota` | Over-The-Air updates |
| **Wi-Fi** | `esp_wifi` | Configurazione Wi-Fi/Mesh |
| **UART** | `driver` | Debug seriale |

### Librerie esterne (da scaricare)

#### 1. **cJSON** (parsing JSON)
```bash
cd Code/nodes/step
idf.py add-dependency "idf-component-dependencies"
idf.py add-dependency "cjson"
```

Oppure aggiungi a `idf_component.yml`:
```yaml
dependencies:
  cjson: "^1.7.15"
```

**Uso:**
```c
#include <cjson/cJSON.h>

cJSON *root = cJSON_Parse(json_string);
cJSON *node_name = cJSON_GetObjectItem(root, "node_name");
```

#### 2. **LVGL 8.3** (solo per HMI)

Aggiungi a `Code/nodes/hmi/idf_component.yml`:
```yaml
dependencies:
  lvgl: "^8.3"
  esp_lcd: "^1.0"
```

**Uso:**
```c
#include <lvgl.h>
#include <esp_lcd.h>

// Callback LVGL per display ST77916
lv_disp_drv_t disp_drv;
lv_disp_drv_init(&disp_drv);
```

#### 3. **ESP32 Camera** (per REAR_CAM e CAM_EXT)

Incluso in ESP-IDF. Aggiungi a `idf_component.yml`:
```yaml
dependencies:
  esp32_camera: "^1.0"
```

#### 4. **BLE (opzionale, per future espansioni)**
```yaml
dependencies:
  bluedroid: "^1.0"
```

### Dipendenze via `idf_component.yml` (file centrale)

Crea `Code/idf_component.yml` per dipendenze comuni:
```yaml
version: "1.0.0"
description: "DOMOC domotico system dependencies"

dependencies:
  # JSON parsing
  cjson: "^1.7.15"
  
  # Logging avanzato (opzionale)
  esp_serial_slave_link: "*"
  
  # Versione del progetto
  metadata:
    version: "1.0.0"
```

---

## Struttura directory firmware

### Layout raccomandato

```
DOMOC/
├── Document/                   # Documentazione
├── BOM.md / PART_LIST.md
├── CLAUDE.md
├── Code/
│   ├── CMakeLists.txt          # Root CMake (opzionale, per multi-nodo)
│   ├── idf_component.yml       # Dipendenze comuni
│   ├── shared/                 # Librerie condivise
│   │   ├── protocol/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── mesh_protocol.h    # Structs messaggi (mesh_msg_t)
│   │   │   ├── mesh_protocol.c
│   │   │   └── node_descriptor.h  # Descriptor nodi
│   │   ├── mesh_manager/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── mesh_manager.h     # Wrapper ESP-Mesh
│   │   │   └── mesh_manager.c
│   │   ├── nvs_store/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── nvs_store.h        # Astrazione NVS
│   │   │   └── nvs_store.c
│   │   └── json_config/
│   │       ├── CMakeLists.txt
│   │       ├── json_config.h      # Parser node_config.json
│   │       └── json_config.c
│   └── nodes/
│       ├── root/                # Firmware MASTER/ROOT
│       │   ├── CMakeLists.txt
│       │   ├── main/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── root_main.c
│       │   │   ├── registry.h
│       │   │   ├── ota_distributor.h
│       │   │   └── heartbeat_monitor.h
│       │   ├── partitions.csv
│       │   └── sdkconfig
│       ├── step/                # Firmware STEP
│       │   ├── CMakeLists.txt
│       │   ├── main/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── step_main.c
│       │   │   ├── motor_task.h
│       │   │   ├── sht31_sensor.h
│       │   │   └── endstop.h
│       │   ├── idf_component.yml
│       │   ├── partitions.csv
│       │   └── sdkconfig
│       ├── hmi/                 # Firmware HMI (Waveshare Knob)
│       │   ├── CMakeLists.txt
│       │   ├── main/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── hmi_main.c
│       │   │   ├── lvgl_ui.h
│       │   │   ├── display_driver.h
│       │   │   ├── touch_handler.h
│       │   │   └── encoder_task.h
│       │   ├── idf_component.yml
│       │   ├── partitions.csv
│       │   └── sdkconfig
│       ├── grey_water/          # Firmware GREY_WATER
│       ├── fresh_water/         # Firmware FRESH_WATER
│       ├── thermo_bunk/         # Firmware THERMO_BUNK
│       ├── rear_cam/            # Firmware REAR_CAM
│       └── ...
```

### CMakeLists.txt template (nodo)

```cmake
# CMakeLists.txt per nodo STEP

cmake_minimum_required(VERSION 3.16)
project(domoc_step_node)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Definizioni globali
add_definitions(-DNODE_NAME="STEP" -DNODE_ID=0x0002)

# Include path librerie condivise
include_directories(../../shared/protocol)
include_directories(../../shared/mesh_manager)
include_directories(../../shared/nvs_store)
include_directories(../../shared/json_config)

project_start()
```

---

## First build e verifica

### Passo 1: Preparazione

```bash
cd DOMOC/Code/nodes/step
get_idf  # Attiva variabili ambiente (se non già attive)
```

### Passo 2: Configura target MCU

```bash
idf.py set-target esp32c3
```

Verifica che `sdkconfig` riporta:
```
CONFIG_IDF_TARGET="esp32c3"
```

### Passo 3: Menuconfig (configurazione interattiva)

```bash
idf.py menuconfig
```

**Navigazione:**
- Arrow keys: movimento
- Enter: entra in menu
- Spacebar: toggle opzione
- `?`: help
- `Q`: esci

**Sezioni fondamentali da verificare:**

1. **Component config → ESP Mesh**
   - [x] Enable mesh
   
2. **Component config → Network → Wi-Fi**
   - [x] Enable Wi-Fi
   
3. **Component config → Bootloader**
   - Enable firmware rollback on boot ✓
   
4. **Partition Table**
   - Custom (per `partitions.csv` dual-bank)
   
5. **Serial flasher config**
   - Flash size: 4MB (per ESP32-C3)

Salva ed esci (premere `S` e poi `Enter`).

### Passo 4: Build

```bash
idf.py build
```

**Output atteso:**
```
[100%] Built target app_update
[100%] Built target esp32c3
Build complete. To flash, run this command:
  idf.py -p PORT flash

Or to build, flash and monitor, run:
  idf.py -p PORT flash monitor
```

### Passo 5: Identifica porta seriale

```bash
# Linux
ls /dev/ttyUSB*

# macOS
ls /dev/tty.usbserial*

# Windows
wmic logicaldisk get name  # Vedi porte COM
```

### Passo 6: Flash e monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Output atteso (nei log):**
```
I (0) cpu_start: Starting scheduler
I (500) mesh: [mesh] Mesh initialization successful
I (501) MAIN: STEP node booting...
```

Premi `Ctrl+]` per uscire da monitor.

---

## Configurazione per ogni MCU

### ESP32-C3 (STEP, FRESH_WATER, GREY_WATER, THERMO_*, ROOT, FRONT_DOOR)

**sdkconfig specifico:**
```
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESP32C3_DEFAULT_CPU_FREQ_240=y
CONFIG_FLASH_SIZE=4M
CONFIG_FLASH_SIZE_DETECT=n
CONFIG_FREERTOS_TICK_RATE_HZ=1000
```

**Partizioni (partitions.csv):**
```csv
# Name,   Type,  SubType, Offset,   Size,     Flags
nvs,      data,  nvs,     0x9000,   0x6000,
otadata,  data,  ota,     0xf000,   0x2000,
ota_0,    app,   ota_0,   0x20000,  0xE0000,
ota_1,    app,   ota_1,   0x100000, 0xE0000,
config,   data,  spiffs,  0x1E0000, 0x20000,
```

### ESP32-S3 (HMI - Waveshare Knob)

**sdkconfig specifico:**
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
CONFIG_ESP32S3_SPIRAM_SIZE_16MB=y    # PSRAM 8MB richiesto
CONFIG_ESP32S3_SPIRAM=y
CONFIG_SPIRAM_MALLOC_ALTS=y
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=16384
CONFIG_FLASH_SIZE=8M
CONFIG_FREERTOS_TICK_RATE_HZ=1000
CONFIG_LV_CONF_SKIP=n               # Per LVGL
CONFIG_LV_DISP_DEF_REFR_PERIOD=10   # Refresh 100 Hz
```

**Dipendenze LVGL:**
```yaml
# idf_component.yml
dependencies:
  lvgl: "^8.3"
  esp_lcd: "^1.1"
```

### ESP32-CAM (REAR_CAM, CAM_EXT)

**sdkconfig specifico:**
```
CONFIG_IDF_TARGET="esp32"
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_FLASH_SIZE=4M
CONFIG_CAMERA_SUPPORT=y
CONFIG_CAMERA_MODULE_OV2640=y
CONFIG_CAMERA_PIXEL_FORMAT_JPEG=y
```

**Nota:** ESP32-CAM trasmette video via **HTTP stream diretto**, non tramite mesh data.

---

## Troubleshooting

### Errore: "idf.py: command not found"

**Soluzione:**
```bash
source ~/esp/esp-idf/export.sh
# Oppure se usi alias:
get_idf
```

### Errore: "Python requirements not satisfied"

**Soluzione:**
```bash
cd ~/esp/esp-idf
python3 -m pip install -r requirements.txt
```

### Errore: "CMake version too old"

**Soluzione:**
```bash
pip install --upgrade cmake
cmake --version  # Verifica ≥ 3.16
```

### Errore: "SPIFFS mount failed"

**Soluzione:**
- Controlla partizioni in `partitions.csv` (config deve iniziare dopo ota_1)
- Verifica size: almeno 64 KB per DOMOC

### Errore: "Mesh initialization failed"

**Soluzione:**
- Verifica Wi-Fi abilitato in menuconfig
- Verifica ESP Mesh enabled
- Controlla se nodo ROOT è online

---

## Comandi utili

```bash
# Build senza flash
idf.py build

# Build + Flash
idf.py -p /dev/ttyUSB0 build flash

# Build + Flash + Monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Solo Monitor (senza flash)
idf.py -p /dev/ttyUSB0 monitor

# Pulisci build
idf.py fullclean

# Leggi log UART precedenti (con socat)
socat - /dev/ttyUSB0,b115200

# Cancella NVS (reset configurazione)
idf.py -p /dev/ttyUSB0 erase_flash

# OTA aggiornamento da linea di comando
curl -F "image=@build/step_node.bin" http://step.local:8080/update
```

---

## Prossimi step

1. **Clona questo template** per ogni nodo (step, hmi, grey_water, etc.)
2. **Modifica `CMakeLists.txt`** con NODE_NAME e NODE_ID specifici
3. **Copia `partitions.csv`** e **sdkconfig** nel nodo
4. **Aggiungi librerie** in `idf_component.yml` per quel nodo
5. **First build** seguendo la sezione [First build e verifica](#first-build-e-verifica)

Per domande specifiche su configurazione mesh o OTA, vedi:
- `Document/esp32-mesh-architecture.md`
- `Document/ota_process.md`
