# DomoC — PCB Universale ESP32-S3 v2.0

---

## Filosofia del PCB v2.0

Il PCB è **sempre completamente popolato**. Non esistono sezioni DNP. Ogni scheda è identica; il
ruolo di ogni IO fisico viene assegnato dal file `node_config.json` che il firmware legge al boot.

| Risorsa hardware | Quantità | Configurazione |
| --- | :---: | --- |
| H-bridge relay (3 relay ognuno) | **2** | Ruolo assegnato da JSON |
| Relay SPDT generale | **2** | Ruolo assegnato da JSON |
| Optoisolatore PC817 (ingresso 12V isolato) | **4** | Ruolo assegnato da JSON |
| Partitore ADC (0–16.5 V → 0–3.3 V) | **2** | Ruolo assegnato da JSON |
| Bus 1-Wire (DS18B20 compatibile) | **1** | Dispositivi scoperti per indirizzo |
| LED RGB WS2812B | 1 | Stato sistema (fisso) |
| I2C espansione (pull-up opzionali) | 1 | Bus libero per sensori aggiuntivi |
| UART debug | 1 | TX/RX su header 3 pin |

---

## Nodi supportati — stesso PCB, diverso JSON

| Nodo | HB1 | HB2 | REL1 | REL2 | OPT1 | OPT2 | OPT3 | OPT4 | ADC1 | ADC2 | 1-Wire |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **ROOT** | — | — | — | — | key_on | — | — | — | vbat_eng | — | temp_amb |
| **STEP** | motor | — | — | — | fc_closed | fc_open | — | — | — | — | temp_ext |
| **GREY_WATER** | motor | — | camera | — | — | — | — | — | — | — | temp_amb |
| **FRESH_WATER** | — | — | valve_nc | — | — | — | — | — | — | — | temp_amb |
| **GARAGE** | motor | — | lights | — | door | — | key_on | — | vbat_svc | vbat_eng | temp_amb |

> Ogni cella indica il `role` del campo corrispondente nel JSON. `—` = `"unused"`.

---

## Schema a blocchi PCB

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                      PCB UNIVERSALE DomoC v2.0                           │
│                                                                          │
│  ┌─────────────┐   ┌──────────────────────────────────────────────────┐  │
│  │ ALIMENTAZ.  │   │             ESP32-S3-MINI-1                      │  │
│  │ 12V→3.3V   │   │                                                  │  │
│  │ MP2307DN   │   │  GPIO1   ADC_DIV1   GPIO2  ADC_DIV2              │  │
│  │ 800mA      │   │  GPIO3   OPT1_OUT   GPIO4  OPT2_OUT              │  │
│  └─────────────┘   │  GPIO5   OPT3_OUT   GPIO6  OPT4_OUT              │  │
│                    │  GPIO8   SDA (I2C)  GPIO9  SCL (I2C)             │  │
│  ┌─────────────┐   │  GPIO10  OW_DATA (1-Wire)                        │  │
│  │  H-BRIDGE 1 │◄──│  GPIO11  HB1_DIR_A  GPIO12 HB1_DIR_B             │  │
│  │  K1 DIR_A  │   │  GPIO13  HB1_EN                                  │  │
│  │  K2 DIR_B  │   │                                                  │  │
│  │  K3 ENABLE │   │  GPIO14  HB2_DIR_A  GPIO15 HB2_DIR_B             │  │
│  │  ►MOT1_A/B │   │  GPIO16  HB2_EN                                  │  │
│  └─────────────┘   │                                                  │  │
│                    │  GPIO17  REL1       GPIO18 REL2                  │  │
│  ┌─────────────┐   │  GPIO21  LED_DATA (WS2812B)                      │  │
│  │  H-BRIDGE 2 │◄──│  GPIO43  UART_TX   GPIO44 UART_RX                │  │
│  │  K4 DIR_A  │   │  GPIO0   BOOT (btn) EN    RESET (btn)            │  │
│  │  K5 DIR_B  │   └──────────────────────────────────────────────────┘  │
│  │  K6 ENABLE │                                                         │
│  │  ►MOT2_A/B │   ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  └─────────────┘   │  REL1    │  │  REL2    │  │  I2C EXP │            │
│                    │  SPDT    │  │  SPDT    │  │ JST-PH4  │            │
│  ┌─────────────┐   │  K7      │  │  K8      │  │ SDA/SCL  │            │
│  │ OPT1..OPT4 │   └──────────┘  └──────────┘  └──────────┘            │
│  │ 4× PC817   │                                                         │
│  │ 12V isolati│   ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  └─────────────┘   │ ADC DIV1 │  │ ADC DIV2 │  │  1-WIRE  │            │
│                    │ 100k/27k │  │ 100k/27k │  │  4.7k pu │            │
│                    └──────────┘  └──────────┘  └──────────┘            │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Blocco 1 — Alimentazione (sempre popolato)

```text
        F1 PTC 500mA
12V ───►──────────────┬─────────────────────────────► 12V_SW
        TVS D1 15V    │                   (relay K1–K8, optoisolatori)
        (SMBJ15A)     │
                      ├─── C1 470µF/25V  C2 100nF
                      └──► [Buck MP2307DN]
                                │  12V → 3.3V @ 800mA
                                └──► 3.3V_RAIL ──► ESP32-S3 + logica
```

| Ref | Componente | Valore / Part | Note |
| --- | --- | --- | --- |
| F1 | PTC polyfuse | 500mA hold, 1A trip | Protezione ingresso 12V |
| D1 | TVS unipolare | SMBJ15A (15V, 600W peak) | Picchi bus camper |
| C1 | Elettrolitico | 470µF / 25V, 105°C | Bulk 12V |
| C2 | MLCC | 100nF / 25V | HF bypass 12V |
| U1 | Buck | MP2307DN (SOIC-8) | 12V→3.3V, 800mA |
| L1 | Inductor | 22µH, 1.5A | Buck output |
| C3 | Elettrolitico | 100µF / 10V | Buck output |
| C4 | MLCC | 100nF / 10V | HF bypass 3.3V |

---

## Blocco 2 — ESP32-S3 (sempre popolato)

**Modulo:** ESP32-S3-MINI-1 (o -1U con antenna esterna)

| Ref | Componente | Valore | Note |
| --- | --- | --- | --- |
| SW1 | Pulsante SMD | BOOT (GPIO0) | Pull-up 10k — accessibile |
| SW2 | Pulsante SMD | RESET (EN) | RC 100nF debounce |
| C10–C14 | MLCC | 100nF / 10V × 5 | Decoupling VDD |
| C15 | MLCC | 10µF / 10V | Bulk vicino modulo |
| J_UART | Header 2.54mm 3p | TX / RX / GND | Debug e flash |

---

## Blocco 3 — H-Bridge 1 (sempre popolato)

Controlla un motore DC bidirezionale tramite 3 relay SPDT in configurazione H-bridge.

```text
GPIO11 (HB1_DIR_A) ──► R_B1 1kΩ ──► Q1 NPN ──► K1 bobina 12V
                                                  K1 contatto SPDT:
                                                  COM=MOT1_A  NO=+12V_SW  NC=GND

GPIO12 (HB1_DIR_B) ──► R_B2 1kΩ ──► Q2 NPN ──► K2 bobina 12V
                                                  K2 contatto SPDT:
                                                  COM=MOT1_B  NO=GND  NC=+12V_SW

GPIO13 (HB1_EN)    ──► R_B3 1kΩ ──► Q3 NPN ──► K3 bobina 12V
                                                  K3 contatto SPST NO:
                                                  in serie su 12V_SW prima di K1/K2
```

**Logica controllo:**

| K1 | K2 | K3 (enable) | Risultato |
| --- | --- | --- | --- |
| OFF | OFF | OFF | Motore scollegato (standby sicuro) |
| ON | OFF | ON | MOT1_A=+12V, MOT1_B=GND → **APRI** |
| OFF | ON | ON | MOT1_A=GND, MOT1_B=+12V → **CHIUDI** |
| ON | ON | ON | ⚠ Cortocircuito — **vietato in firmware** |

> K3=OFF prima di ogni cambio K1/K2. Il motore è fisicamente scollegato in standby.

**Connettore:** morsettiera 5.08mm 2 vie — `MOT1_A`, `MOT1_B`

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| K1, K2 | Relay SPDT | HRS1H-S-DC12V 10A |
| K3 | Relay SPST NO | HRS1H-S-DC12V |
| Q1–Q3 | NPN | BC547B (SOT-23) |
| R_B1–3 | Resistore | 1kΩ, 0402 |
| D_FW1–3 | Freewheeling | 1N4148 (SOD-323) |
| J_MOT1 | Morsettiera | 5.08mm 2 vie |

---

## Blocco 4 — H-Bridge 2 (sempre popolato)

Identico al Blocco 3. Controlla un secondo motore DC indipendente.

```text
GPIO14 (HB2_DIR_A) ──► R_B4 1kΩ ──► Q4 NPN ──► K4 bobina 12V  (COM=MOT2_A)
GPIO15 (HB2_DIR_B) ──► R_B5 1kΩ ──► Q5 NPN ──► K5 bobina 12V  (COM=MOT2_B)
GPIO16 (HB2_EN)    ──► R_B6 1kΩ ──► Q6 NPN ──► K6 bobina 12V  (in serie 12V_SW)
```

Logica identica al Blocco 3. **Connettore:** morsettiera 5.08mm 2 vie — `MOT2_A`, `MOT2_B`

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| K4, K5 | Relay SPDT | HRS1H-S-DC12V 10A |
| K6 | Relay SPST NO | HRS1H-S-DC12V |
| Q4–Q6 | NPN | BC547B (SOT-23) |
| R_B4–6 | Resistore | 1kΩ, 0402 |
| D_FW4–6 | Freewheeling | 1N4148 (SOD-323) |
| J_MOT2 | Morsettiera | 5.08mm 2 vie |

---

## Blocco 5 — Relay 1 generale (sempre popolato)

SPDT indipendente, carico configurabile via JSON (`valve_nc`, `camera`, `lights`, …).

```text
GPIO17 (REL1) ──► R_B7 1kΩ ──► Q7 NPN ──► K7 bobina 12V
                                             K7 contatto SPDT:
                                             COM = 12V_SW
                                             NO  = REL1_OUT+ (morsettiera)
                                             NC  = NC
```

**Connettore:** morsettiera 5.08mm 2 vie — `REL1_OUT+`, `GND`

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| K7 | Relay SPDT | HRS1H-S-DC12V 10A |
| Q7 | NPN | BC547B (SOT-23) |
| R_B7 | Resistore | 1kΩ, 0402 |
| D_FW7 | Freewheeling | 1N4148 (SOD-323) |
| J_REL1 | Morsettiera | 5.08mm 2 vie |

---

## Blocco 6 — Relay 2 generale (sempre popolato)

Identico al Blocco 5.

```text
GPIO18 (REL2) ──► R_B8 1kΩ ──► Q8 NPN ──► K8 bobina 12V  (NO = REL2_OUT+)
```

**Connettore:** morsettiera 5.08mm 2 vie — `REL2_OUT+`, `GND`

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| K8 | Relay SPDT | HRS1H-S-DC12V 10A |
| Q8 | NPN | BC547B (SOT-23) |
| R_B8 | Resistore | 1kΩ, 0402 |
| D_FW8 | Freewheeling | 1N4148 (SOD-323) |
| J_REL2 | Morsettiera | 5.08mm 2 vie |

---

## Blocco 7 — Optoisolatori 1–4 (sempre popolato)

Quattro ingressi isolati **identici**, ognuno con PC817. Ogni ingresso accetta 12V
sul lato primario e produce un segnale logico 3.3V sul GPIO del ESP32-S3.

```text
12V_EXT ──► R_ISO 10kΩ (¼W) ──► LED PC817 ──► GND_ISO
                                      │
                               Fototransistor
                                      │
                        3.3V ──► R_OUT 10kΩ ──► GPIOx (OPTx_OUT)
                                               Collettore ──► GPIOx
                                               Emettitore ──► GND
```

**Logica:** 12V presente → GPIO = LOW (logica inversa). Firmware: `active = (gpio == 0)`

**Connettori:** 4 × morsettiera 5.08mm 2 vie isolate — `OPTx_12V+`, `OPTx_GND_ISO`

> Le masse isolate `GND_ISO` sono **separate** da `GND_PCB`. Collegarle al GND del
> cablaggio che porta il segnale 12V (non al GND del bus PCB) se il circuito lo richiede.

| Ref | Componente | Valore / Part | GPIO |
| --- | --- | --- | --- |
| U_OPT1 | PC817C (SMD) | CTR min 100% | GPIO3 |
| U_OPT2 | PC817C (SMD) | CTR min 100% | GPIO4 |
| U_OPT3 | PC817C (SMD) | CTR min 100% | GPIO5 |
| U_OPT4 | PC817C (SMD) | CTR min 100% | GPIO6 |
| R_ISO1–4 | Resistore | 10kΩ, ¼W | Corrente LED ~1 mA @ 12V |
| R_OUT1–4 | Resistore | 10kΩ, 0402 | Pull-up uscita fototransistor |
| J_OPT1–4 | Morsettiera | 5.08mm 2 vie × 4 | Input 12V isolati |

---

## Blocco 8 — Partitori ADC 1 e 2 (sempre popolato)

Due canali di misura tensione identici. Scala 0–16.5 V → 0–3.3 V su ADC1 dell'ESP32-S3.

```text
VBAT_IN1 ──► R_H1 100kΩ ──┬──► GPIO1 (ADC1_CH0)
                            R_L1 27kΩ
                            │
                           GND

VBAT_IN2 ──► R_H2 100kΩ ──┬──► GPIO2 (ADC1_CH1)
                            R_L2 27kΩ
                            │
                           GND

Formula: V_GPIO = V_IN × 27k / (100k + 27k) = V_IN × 0.2126
→ 12.6V → 2.68V  |  14.4V → 3.06V
```

**Connettori:** 2 × morsettiera 5.08mm 2 vie — `VBAT1+`, `GND` e `VBAT2+`, `GND`

| Ref | Componente | Valore | Note |
| --- | --- | --- | --- |
| R_H1, R_H2 | Resistore | 100kΩ, ¼W, 1% | |
| R_L1, R_L2 | Resistore | 27kΩ, ¼W, 1% | |
| C_ADC1, C_ADC2 | MLCC | 100nF / 10V, 0402 | Filtro anti-alias |
| J_ADC1, J_ADC2 | Morsettiera | 5.08mm 2 vie | |

---

## Blocco 9 — Bus 1-Wire (sempre popolato)

Un singolo bus Dallas 1-Wire su GPIO10. Supporta **più dispositivi in parallelo**
sullo stesso filo (DS18B20, DS2438, …) — ogni dispositivo è indirizzato per ROM a 64 bit.

```text
3.3V ──► C_OW 100nF ──► VCC sensori (pin VDD)
3.3V ──► R_PU 4.7kΩ ──► OW_DATA (GPIO10) ──► DATA sensori (pin DQ)
                                              GND sensori (pin GND)
                         J_OW: VCC / OW_DATA / GND  (header 2.54mm 3p)
```

> Il firmware usa la libreria `onewire_bus` (Espressif IDF component). Al boot esegue
> una ROM search per scoprire tutti i dispositivi presenti. Gli indirizzi vengono
> confrontati con quelli definiti nel JSON per assegnare i ruoli.

**Connettore:** header 2.54mm 3 pin — `VCC`, `OW_DATA`, `GND`

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| R_PU | Resistore pull-up | 4.7kΩ, 0402 |
| C_OW | MLCC | 100nF / 10V, 0402 |
| J_OW | Header 2.54mm | 3 pin |

---

## Blocco 10 — LED RGB stato (sempre popolato)

```text
GPIO21 ──► DATA ──► WS2812B-2020 ──► 3.3V
```

| Ref | Componente |
| --- | --- |
| LED1 | WS2812B-2020 (2×2mm) |
| C_WS | MLCC 100nF |

---

## Blocco 11 — I2C espansione (sempre popolato — pull-up opzionali)

GPIO8/GPIO9 portati su connettore JST-PH 4p per sensori I2C aggiuntivi (INA219, BH1750, …).
I resistori di pull-up R_PU_SDA e R_PU_SCL sono **sempre saldati** — se il sensore
esterno ha già i propri pull-up, tagliare le piazzole (jumper).

```text
3.3V ──► R_PU_SDA 4.7kΩ ──► SDA (GPIO8) ──► J_I2C pin 3
3.3V ──► R_PU_SCL 4.7kΩ ──► SCL (GPIO9) ──► J_I2C pin 4
                               GND ──► J_I2C pin 1
                             3.3V ──► J_I2C pin 2
```

| Ref | Componente | Valore / Part |
| --- | --- | --- |
| R_PU_SDA | Resistore | 4.7kΩ, 0402 |
| R_PU_SCL | Resistore | 4.7kΩ, 0402 |
| J_I2C | JST-PH 4p | GND / 3.3V / SDA / SCL |

---

## Blocco 12 — UART debug (sempre popolato)

Header maschio 2.54mm 3 pin: `TX / RX / GND`

---

## Mappa GPIO completa — ESP32-S3

| GPIO | Segnale | Funzione | Blocco |
| --- | --- | --- | --- |
| GPIO0 | BOOT | Pulsante boot (strapping) | 2 |
| GPIO1 | ADC_DIV1 | ADC1_CH0 — partitore tensione 1 | 8 |
| GPIO2 | ADC_DIV2 | ADC1_CH1 — partitore tensione 2 | 8 |
| GPIO3 | OPT1_OUT | Uscita optoisolatore 1 | 7 |
| GPIO4 | OPT2_OUT | Uscita optoisolatore 2 | 7 |
| GPIO5 | OPT3_OUT | Uscita optoisolatore 3 | 7 |
| GPIO6 | OPT4_OUT | Uscita optoisolatore 4 | 7 |
| GPIO7 | — | Libero (espansione) | — |
| GPIO8 | SDA | I2C espansione | 11 |
| GPIO9 | SCL | I2C espansione | 11 |
| GPIO10 | OW_DATA | Bus 1-Wire | 9 |
| GPIO11 | HB1_DIR_A | H-bridge 1 — direzione A | 3 |
| GPIO12 | HB1_DIR_B | H-bridge 1 — direzione B | 3 |
| GPIO13 | HB1_EN | H-bridge 1 — enable | 3 |
| GPIO14 | HB2_DIR_A | H-bridge 2 — direzione A | 4 |
| GPIO15 | HB2_DIR_B | H-bridge 2 — direzione B | 4 |
| GPIO16 | HB2_EN | H-bridge 2 — enable | 4 |
| GPIO17 | REL1 | Relay 1 generale | 5 |
| GPIO18 | REL2 | Relay 2 generale | 6 |
| GPIO21 | LED_DATA | WS2812B stato | 10 |
| GPIO43 | UART_TX | Debug seriale TX | 12 |
| GPIO44 | UART_RX | Debug seriale RX | 12 |
| EN | RESET | Pulsante reset | 2 |

> GPIO19–GPIO20, GPIO26–GPIO42 liberi per espansioni future.
> **Non usare GPIO26–GPIO32** su moduli con PSRAM (bus SPI interno).
> **GPIO46** è strapping pin — lasciare floating o a 3.3V.

---

## Connettori esterni — riepilogo

| Ref | Tipo | Segnali |
| --- | --- | --- |
| J_PWR | XT30 o JST-VH 2p | 12V+, GND |
| J_MOT1 | Morsettiera 5.08mm 2p | MOT1_A, MOT1_B |
| J_MOT2 | Morsettiera 5.08mm 2p | MOT2_A, MOT2_B |
| J_REL1 | Morsettiera 5.08mm 2p | REL1_OUT+, GND |
| J_REL2 | Morsettiera 5.08mm 2p | REL2_OUT+, GND |
| J_OPT1 | Morsettiera 5.08mm 2p | OPT1_12V+, OPT1_GND_ISO |
| J_OPT2 | Morsettiera 5.08mm 2p | OPT2_12V+, OPT2_GND_ISO |
| J_OPT3 | Morsettiera 5.08mm 2p | OPT3_12V+, OPT3_GND_ISO |
| J_OPT4 | Morsettiera 5.08mm 2p | OPT4_12V+, OPT4_GND_ISO |
| J_ADC1 | Morsettiera 5.08mm 2p | VBAT1+, GND |
| J_ADC2 | Morsettiera 5.08mm 2p | VBAT2+, GND |
| J_OW | Header 2.54mm 3p | VCC, OW_DATA, GND |
| J_I2C | JST-PH 4p | GND, 3.3V, SDA, SCL |
| J_UART | Header 2.54mm 3p | TX, RX, GND |

---

## Configurazione IO via node_config.json

Il firmware legge il file `node_config.json` dalla flash NVS al boot e configura ogni
risorsa hardware in base al campo `"role"`. Un ruolo `"unused"` disabilita completamente
la risorsa (GPIO lasciato in input floating, nessun interrupt, nessuna pubblicazione).

### Ruoli disponibili per tipo IO

**H-bridge** (`hb1`, `hb2`):

| role | Descrizione |
| --- | --- |
| `"motor"` | Motore bidirezionale — apri/chiudi via MSG_COMMAND |
| `"unused"` | Disabilitato — GPIO in input, relay non pilotati |

**Relay** (`rel1`, `rel2`):

| role | Descrizione |
| --- | --- |
| `"camera"` | Alimentazione videocamera — ON su ACTION_CAM_ON |
| `"valve_nc"` | Elettrovalvola 12V NC — ON su ACTION_OPEN |
| `"lights"` | Luci — ON/OFF via ACTION_LIGHT_ON/OFF |
| `"generic_no"` | Uscita generica — ON/OFF via ACTION_OPEN/CLOSE |
| `"unused"` | Disabilitato |

**Optoisolatore** (`opt1`…`opt4`):

| role | Descrizione |
| --- | --- |
| `"key_on"` | Positivo sotto chiave — fronte ON→ broadcast MSG_KEY_ON, OFF→ MSG_KEY_OFF |
| `"fc_closed"` | Finecorsa chiuso — usato dall'H-bridge associato (`hb_id`) |
| `"fc_open"` | Finecorsa aperto — usato dall'H-bridge associato (`hb_id`) |
| `"door_sensor"` | Sensore porta/portellone — pubblica stato e opzionale alert |
| `"button"` | Pulsante esterno — invia evento MSG_COMMAND all'HMI |
| `"generic_di"` | Ingresso generico — stato pubblicato nel heartbeat |
| `"unused"` | Disabilitato |

**Partitore ADC** (`adc1`, `adc2`):

| role | Descrizione |
| --- | --- |
| `"vbat_engine"` | Batteria motore (starter) — alert se > 13.5V (alternatore acceso) |
| `"vbat_service"` | Batteria di servizio — alert se < 11.8V (scarica) |
| `"voltage_generic"` | Misura generica — pubblicata nel heartbeat senza alert |
| `"unused"` | Disabilitato |

**Dispositivi 1-Wire** (lista in `onewire.devices[]`):

| role | Descrizione |
| --- | --- |
| `"temp_ambient"` | Temperatura ambiente nodo — pubblicata nel heartbeat |
| `"temp_external"` | Sonda remota esterna (cavetto lungo) |
| `"temp_water"` | Temperatura impianto idrico |
| `"temp_engine"` | Temperatura vano motore / batterie |
| `"unused"` | Dispositivo ignorato |

---

### Struttura JSON completa

```json
{
  "node_name": "STEP",
  "node_type": "STEP",
  "mesh": {
    "channel": 6,
    "mesh_id": "DomoC01"
  },
  "hardware": {
    "hb1": {
      "role": "motor",
      "gpio_dir_a": 11,
      "gpio_dir_b": 12,
      "gpio_enable": 13,
      "motor_run_ms": 4000,
      "opt_fc_closed": "opt2",
      "opt_fc_open":   "opt1"
    },
    "hb2": {
      "role": "unused"
    },
    "rel1": {
      "role": "unused"
    },
    "rel2": {
      "role": "unused"
    },
    "opt1": {
      "role": "fc_open",
      "gpio": 3,
      "hb_id": "hb1"
    },
    "opt2": {
      "role": "fc_closed",
      "gpio": 4,
      "hb_id": "hb1"
    },
    "opt3": {
      "role": "unused",
      "gpio": 5
    },
    "opt4": {
      "role": "unused",
      "gpio": 6
    },
    "adc1": {
      "role": "unused",
      "gpio": 1
    },
    "adc2": {
      "role": "unused",
      "gpio": 2
    },
    "onewire": {
      "gpio": 10,
      "devices": [
        {
          "address": "28FF641D1C040000",
          "role": "temp_external"
        }
      ]
    }
  }
}
```

### Esempi per gli altri nodi

**ROOT:**

```json
{
  "node_name": "ROOT", "node_type": "ROOT",
  "hardware": {
    "hb1": { "role": "unused" },
    "hb2": { "role": "unused" },
    "rel1": { "role": "unused" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "key_on",  "gpio": 3 },
    "opt2": { "role": "unused",  "gpio": 4 },
    "opt3": { "role": "unused",  "gpio": 5 },
    "opt4": { "role": "unused",  "gpio": 6 },
    "adc1": { "role": "vbat_engine",  "gpio": 1 },
    "adc2": { "role": "unused",       "gpio": 2 },
    "onewire": { "gpio": 10, "devices": [
      { "address": "28AA110F00000000", "role": "temp_ambient" }
    ]}
  }
}
```

**GREY_WATER:**

```json
{
  "node_name": "GREY_WATER", "node_type": "GREY_WATER",
  "hardware": {
    "hb1": { "role": "motor", "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
             "motor_run_ms": 3000, "opt_fc_closed": null, "opt_fc_open": null },
    "hb2": { "role": "unused" },
    "rel1": { "role": "camera" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "unused", "gpio": 3 },
    "opt2": { "role": "unused", "gpio": 4 },
    "opt3": { "role": "unused", "gpio": 5 },
    "opt4": { "role": "unused", "gpio": 6 },
    "adc1": { "role": "unused", "gpio": 1 },
    "adc2": { "role": "unused", "gpio": 2 },
    "onewire": { "gpio": 10, "devices": [
      { "address": "28BB220F00000000", "role": "temp_ambient" }
    ]}
  }
}
```

**GARAGE:**

```json
{
  "node_name": "GARAGE", "node_type": "GARAGE",
  "hardware": {
    "hb1": { "role": "motor", "gpio_dir_a": 11, "gpio_dir_b": 12, "gpio_enable": 13,
             "motor_run_ms": 5000, "opt_fc_closed": null, "opt_fc_open": "opt1" },
    "hb2": { "role": "unused" },
    "rel1": { "role": "lights" },
    "rel2": { "role": "unused" },
    "opt1": { "role": "door_sensor", "gpio": 3 },
    "opt2": { "role": "unused",      "gpio": 4 },
    "opt3": { "role": "key_on",      "gpio": 5 },
    "opt4": { "role": "unused",      "gpio": 6 },
    "adc1": { "role": "vbat_service", "gpio": 1 },
    "adc2": { "role": "vbat_engine",  "gpio": 2 },
    "onewire": { "gpio": 10, "devices": [
      { "address": "28CC330F00000000", "role": "temp_ambient" }
    ]}
  }
}
```

---

## BOM — componenti per scheda (fisso, indipendente dal nodo)

| Categoria | Componente | Qtà |
| --- | --- | --- |
| MCU | ESP32-S3-MINI-1 | 1 |
| Buck | MP2307DN SOIC-8 | 1 |
| TVS | SMBJ15A | 1 |
| PTC fuse | 500mA hold | 1 |
| Relay SPDT | HRS1H-S-DC12V 10A | **8** (K1–K8) |
| NPN driver | BC547B SOT-23 | **8** (Q1–Q8) |
| Optoisolatore | PC817C SMD | **4** (U_OPT1–4) |
| Resistore 1kΩ 0402 | Driver base relay | 8 |
| Resistore 10kΩ 0402 | Pull-up opto uscita + R_ISO | 4+4 |
| Resistore 100kΩ ¼W 1% | Partitore alto | 2 |
| Resistore 27kΩ ¼W 1% | Partitore basso | 2 |
| Resistore 4.7kΩ 0402 | Pull-up 1-Wire + I2C | 3 |
| Diodo 1N4148 SOD-323 | Freewheeling | 8 |
| Capacitor 470µF/25V | Bulk 12V | 1 |
| Capacitor 100µF/10V | Buck output | 1 |
| Capacitor 100nF 0402 | Bypass (×10) | 10 |
| Inductor 22µH 1.5A | Buck | 1 |
| LED WS2812B-2020 | Stato RGB | 1 |

**Costo stimato per scheda:** ~22–26 € (componenti singoli, LCSC/AliExpress)

---

## Note di progettazione PCB

### Dimensioni suggerite

- **100 × 70 mm** — spazio extra rispetto v1.0 per gli 8 relay e i 4 optoisolatori

### Stack-up

- **2 layer**: Top (SMD + relay), Bottom (piano GND massiccio)
- Keep-out zone antenna ESP32-S3

### Separazione masse

- **GND_3V3** / **GND_12V**: star ground unico vicino a C1
- **GND_ISO** (optoisolatori 1–4): pad separati, collegati al GND del circuito esterno
  via J_OPTx — **non connettere al GND_PCB sul PCB**

### Piste di potenza

- Relay K1–K8 e motori: larghezza minima **2 mm** (copper fill consigliato)
- 3.3V logica: 0.25 mm sufficiente
- Via thermal fill sotto buck U1

### Posizionamento

- Relay K1–K8: bordo PCB, morsettieri sul lato esterno
- Optoisolatori: vicino ai J_OPTx, lontani da ESP32-S3 (EMI)
- Buck U1: lontano dall'antenna Wi-Fi
- J_OW, J_I2C: lato opposto ai relay (sensori delicati lontani da carichi induttivi)
