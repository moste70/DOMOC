# DOMOC — Bill of Materials (BOM)

---

## Microcontrollori (MCU)

**Totale nodi del sistema: 11**

| MCU | Nodo | Quantità | Funzione | Note |
|---|---|---|---|---|
| **ESP32-C3** | ROOT | 1 | Root fisso mesh + sensori chiave e batteria motore | Always-on 12V |
| **ESP32-C3** | STEP | 1 | Gradino motorizzato + SHT31 temperatura/umidità | H-bridge DRV8833 |
| **ESP32-C3** | GARAGE (GREY_WATER) | 1 | Valvola acque grigie + monitor batteria servizio (critico) | H-bridge DRV8833 |
| **ESP32-C3** | FRESH_WATER | 1 | Valvola acque chiare (NC) | Relay NC |
| **ESP32-C3** | THERMO_BUNK | 1 | Termostato letto + comando valvola aria | DS18B20 + PWM |
| **ESP32-C3** | THERMO_LOFT | 1 | Termostato mansarda + comando valvola aria | DS18B20 + PWM |
| **ESP32-C3** | THERMO_KITCHEN | 1 | Termostato cucina + comando valvola aria | DS18B20 + PWM |
| **ESP32-C3** | FRONT_DOOR | 1 | Porta ingresso motorizzata | H-bridge DRV8833 |
| **Subtotale ESP32-C3** | — | **8** | Sensori + attuatori distribuiti | — |
| | | | | |
| **ESP32-S3** | HMI | 1 | Display touch 3.5-4.3" + batteria LiPo | PSRAM 8MB, USB-C |
| **Subtotale ESP32-S3** | — | **1** | Interfaccia utente portatile | — |
| | | | | |
| **ESP32-CAM** | REAR | 1 | Telecamera retromarcia stream MJPEG | OV2640 2MP |
| **ESP32-CAM** | CAM_EXT | 1 | Telecamere esterne sicurezza (motion detect) | OV2640 2MP |
| **Subtotale ESP32-CAM** | — | **2** | Video streaming | — |

**TOTALE MICROCONTROLLORI: 11**

---

## Driver e Protezione

| Componente | Funzione | Nodi | Quantità | Datasheet |
|---|---|---|---|---|
| **DRV8833** | H-bridge 2A doppio canale | STEP, GARAGE, FRONT_DOOR | 3 | [TI DRV8833](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |
| **TP4056** | Caricabatteria LiPo 1A | HMI | 1 | — |
| **DW01-A** | Protezione batteria LiPo | HMI | 1 | — |
| **BQ24075** | Alternativa all-in-one (TP4056+DW01+step-up) | HMI | 1 (alternativa) | [TI BQ24075](https://www.ti.com/lit/ds/symlink/bq24075.pdf) |
| **Relay NC 12V** | Valvola acque chiare | FRESH_WATER | 1 | — |
| **PC817** | Optoisolatore chiave accensione | ROOT | 1 | — |

---

## Sensori

| Componente | Funzione | Nodi | Quantità | Note |
|---|---|---|---|---|
| **DS18B20** | Termometro 1-Wire ±0.5°C | THERMO_BUNK, THERMO_LOFT, THERMO_KITCHEN | 3 | Impermeabile |
| **SHT31** | Temp/Umidità I2C ±0.3°C / ±2% RH | STEP | 1 | Custodia Gore-Tex IP65 |
| **Microswitch NC** | Finecorsa apertura | STEP, FRONT_DOOR | 2 | Standard NO/NC |
| **Microswitch NC** | Finecorsa chiusura | STEP, FRONT_DOOR | 2 | Standard NO/NC |
| **Reed magnetico** | Sensore posizione | STEP | 1 | Rilevamento stato gradino |
| **INA219** | Sensore tensione/corrente I2C (opzionale) | GARAGE (batteria servizio) | 1 (opzionale) | Precisione tensione/corrente |
| **Partitore ADC** | Scalaggio 12V→3.3V | ROOT, GARAGE | 2 | 100kΩ / 27kΩ |

---

## Componenti di alimentazione

| Componente | Funzione | Nodi | Quantità | Note |
|---|---|---|---|---|
| **Buck MP2307** | 12V → 3.3V, 500mA | Tutti i nodi C3 | 8 | Efficienza >90% |
| **Buck MP2307** | 12V → 5V, 1A | HMI (TP4056) | 1 | Per caricabatteria LiPo |
| **Boost/LDO** | 3.7V → 3.3V (LiPo) | HMI | 1 | Per batteria LiPo interna |
| **TVS 15V** | Protezione sovratensione | Tutti i nodi | 11 | SMD 0805 |
| **Condensatore 470µF** | Bulk capacitor 16V | Tutti i nodi | 11 | Protezione transitori |
| **Condensatore 100nF** | Decoupling | Tutti i nodi | 11 | C0G/NP0 |
| **Condensatore 10µF** | Decoupling display | HMI | 1 | Per driver display SPI |
| **Batteria LiPo** | Alimentazione HMI | HMI | 1 | 2000-3000 mAh, 3.7V |

---

## Attuatori

| Componente | Funzione | Nodi | Quantità | Note |
|---|---|---|---|---|
| **Motore DC 12V** | Gradino | STEP | 1 | Già presente sul camper |
| **Motore DC 12V** | Porta ingresso | FRONT_DOOR | 1 | Già presente sul camper |
| **Elettrovalvola 12V bipolare** | Acque grigie | GARAGE | 1 | Comando via inversione polarità (H-bridge) |
| **Elettrovalvola 12V NC** | Acque chiare | FRESH_WATER | 1 | Alimentazione ON/OFF |
| **Attuatore PWM 12V** | Valvola aria | THERMO_BUNK/LOFT/KITCHEN | 3 | Comando via PWM LEDC |

---

## Connettività e connettori

| Componente | Funzione | Quantità | Note |
|---|---|---|---|
| **Connettore JST-PH 2.0mm** | Alimentazione 12V | 11 | GND + 12V |
| **Connettore JST-GH 4.0mm** | I2C (SDA/SCL) | 5 | ROOT, GARAGE, STEP, THERMO_* |
| **Connettore JST-PH** | UART Debug | 11 | TX/RX per ogni nodo |
| **Connettore JST-GH 8-pin** | H-bridge DRV8833 | 3 | STEP, GARAGE, FRONT_DOOR |
| **Cavetto USB-C** | HMI flash + carica | 1 | Integrato su ESP32-S3 |
| **Connettore SPI** | Display + SD card (HMI) | 1 | Display touch |

---

## Costi stimati (marzo 2026)

| Categoria | Componenti | Costo/unità | Quantità | Totale |
|---|---|---|---|---|
| **MCU** | ESP32-C3 | €2 | 8 | €16 |
| | ESP32-S3 | €6 | 1 | €6 |
| | ESP32-CAM | €9 | 2 | €18 |
| **Driver** | DRV8833 | €2.50 | 3 | €7.50 |
| | TP4056 + DW01 | €3 | 1 | €3 |
| **Sensori** | DS18B20 | €1 | 3 | €3 |
| | SHT31 | €4 | 1 | €4 |
| | Microswitch | €0.50 | 4 | €2 |
| **Alimentazione** | Buck converter | €1.50 | 9 | €13.50 |
| **Passivi** | Resistenze, capacitori, TVS | — | — | ~€5 |
| **Connettori JST** | Vari | — | — | ~€3 |
| | | | **TOTALE** | **~€81** |

---

## Ordine consigliato

**Minimo operativo: 11 MCU (come sopra)**

**Con ricambi/test (+20%):**
- ESP32-C3: 8 + 2 = **10**
- ESP32-S3: 1 + 1 = **2**
- ESP32-CAM: 2 + 1 = **3**
- **TOTALE: 15 MCU**

**Costo con ricambi: ~€100-110**

---

## Note di acquisto

- **Lead time**: Ordinare 2-3 settimane prima di iniziare prototipazione (spedizione standard dalla Cina)
- **Fornitori**: AliExpress, Ebelt, Zetlab, Amazon EU
- **Quantità**: Meglio ordinare 20-30% in più per test e fallimenti di saldatura
- **Specifiche**: 
  - ESP32-C3 standard (NOT C3-12F)
  - ESP32-S3 con PSRAM 8MB
  - ESP32-CAM con OV2640 2MP
