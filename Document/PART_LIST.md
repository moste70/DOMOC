# DOMOC — Part List (Lista Componenti Dettagliata)

**Data**: Aprile 2026  
**Progetto**: DOMOC — Sistema di controllo camper su rete mesh ESP32  
**Quantità totale nodi**: 11

---

## 1. MICROCONTROLLORI (MCU) — €40

| Descrizione | Modello | Quantità | Prezzo unitario | Totale | Note |
|---|---|---|---|---|---|
| Microcontrollore WiFi mesh | ESP32-C3-MINI-1 | 8 | €2.00 | €16.00 | Per nodi sensori/attuatori |
| Microcontrollore WiFi + PSRAM | ESP32-S3-WROOM-1-N16R8 | 1 | €6.00 | €6.00 | Per HMI (display + PSRAM 8MB) |
| Camera WiFi | ESP32-CAM OV2640 | 2 | €9.00 | €18.00 | Retromarcia + esterne |
| **SUBTOTALE MCU** | | **11** | | **€40.00** | |

---

## 2. RELAY E DRIVER H-BRIDGE — €30

Il PCB universale v3.0 usa H-bridge relay (3 relay SPDT per ponte) — nessun IC DRV8833.
Ogni scheda monta 8 relay SPDT (6 per i 2 H-bridge + 2 di uso generale) e 8 driver NPN.

| Descrizione | Modello | Qtà totale | Prezzo unitario | Totale | Note |
|---|---|---|---|---|---|
| Relay SPDT 12V 10A | HRS1H-S-DC12V (Panasonic) | 40 | €0.60 | €24.00 | 8×scheda × 5 schede PCB |
| NPN driver relay | BC547B SOT-23 | 40 | €0.06 | €2.40 | 8×scheda × 5 schede PCB |
| Diodo freewheeling | 1N4148 SOD-323 | 40 | €0.04 | €1.60 | 1 per relay |
| Resistore base 1kΩ 0402 | — | 40 | — | €0.50 | Kit |
| **SUBTOTALE RELAY** | | | | **€28.50** | |

---

## 3. GESTIONE BATTERIA (HMI) — €3

| Descrizione | Modello | Quantità | Prezzo unitario | Totale | Note |
|---|---|---|---|---|---|
| Caricabatteria LiPo + protezione | TP4056 + DW01-A (modulo) | 1 | €3.00 | €3.00 | Oppure BQ24075 integrato €8 |
| **SUBTOTALE BATTERIA** | | **1** | | **€3.00** | |

---

## 4. SENSORI — €12

| Descrizione | Modello | Quantità | Prezzo unitario | Totale | Nodi |
|---|---|---|---|---|---|
| Termometro 1-Wire impermeabile | DS18B20 (waterproof) | 3 | €1.00 | €3.00 | THERMO_BUNK/LOFT/KITCHEN |
| Sensore Temp/Umidità I2C IP65 | SHT31 con custodia | 1 | €4.00 | €4.00 | STEP (esterno) |

| Sensore corrente/tensione I2C (opz.) | INA219 | 1 | €2.50 | €2.50 | GARAGE monitor batteria |
| **SUBTOTALE SENSORI** | | | | **€9.80** | (microswitch eliminati - timeout software) |

---

## 5. PROTEZIONE E ISOLAMENTO — €6

Il PCB v3.0 monta 2 optoisolatori PC817-A per scheda (configurabili via JSON).
I relay K7/K8 (uso generale) sono già conteggiati nella sezione 2.

| Descrizione | Modello | Qtà totale | Prezzo unitario | Totale | Note |
|---|---|---|---|---|---|
| Optoisolatore 12V isolato | PC817-A (SMD) | 10 | €0.50 | €5.00 | 2×scheda × 5 schede PCB |
| Resistore R_ISO 10kΩ ¼W | — | 10 | — | €0.20 | LED optoisolatore |
| Resistore R_OUT 10kΩ 0402 | — | 10 | — | €0.10 | Pull-up uscita |
| **SUBTOTALE PROTEZIONE** | | | | **€5.30** | |

---

## 6. ALIMENTAZIONE E REGOLAZIONE — €13.50

| Descrizione | Modello | Quantità | Prezzo unitario | Totale | Note |
|---|---|---|---|---|---|
| Step-down converter 12V→3.3V | MP2307 (modulo) 500mA | 8 | €1.50 | €12.00 | Tutti i nodi C3 |
| Step-down converter 12V→5V | MP2307 (modulo) 1A | 1 | €1.50 | €1.50 | TP4056 HMI |
| **SUBTOTALE ALIMENTAZIONE** | | | | **€13.50** | |

---

## 7. COMPONENTI PASSIVI (Resistenze, Capacitori, Diodi) — €5

| Descrizione | Valore/Tipo | Quantità | Prezzo totale | Note |
|---|---|---|---|---|
| Resistenza 100kΩ 1/4W | 100k 1% | 10 | €0.20 | Partitori ADC (2×scheda × 5 schede) |
| Resistenza 27kΩ 1/4W | 27k 1% | 10 | €0.15 | Partitori ADC (2×scheda × 5 schede) |
| Resistenza 10kΩ 1/4W | 10k 5% | 20 | €0.20 | Pull-up, protezione |
| Condensatore ceramico 100nF | 100nF 16V X7R | 15 | €0.30 | Decoupling tutti i nodi |
| Condensatore ceramico 10µF | 10µF 16V | 10 | €0.40 | Decoupling |
| Condensatore elettrolitico 470µF | 470µF 25V | 11 | €0.80 | Bulk capacitor protezione |
| Diodo TVS 15V | SMAJ15CA | 15 | €0.50 | Protezione sovratensione |
| Fusibile resettabile | PTC 500mA | 5 | €0.30 | Protezione cortocircuito |
| **SUBTOTALE PASSIVI** | | | **€3.75** | |
| **+ varie (jumper, solder, etc)** | | | **~€1.25** | |

---

## 8. CONNETTORI JST — €3

| Descrizione | Modello | Quantità | Prezzo totale | Uso |
|---|---|---|---|---|
| Connettore JST-PH 2.0mm 2-pin | JST-PH2 maschio/femmina | 20 coppie | €0.80 | Alimentazione 12V |
| Connettore JST-GH 4.0mm 4-pin | JST-GH4 maschio/femmina | 10 coppie | €1.00 | I2C (SDA/SCL) |
| Connettore JST-GH 8-pin | JST-GH8 maschio | 3 | €0.30 | H-bridge DRV8833 |
| Connettore JST-PH 2-pin UART | JST-PH maschio | 15 | €0.60 | Debug seriale |
| **SUBTOTALE CONNETTORI** | | | **€2.70** | |

---

## 10. BATTERIA HMI — €15-25

| Descrizione | Specifiche | Quantità | Prezzo | Note |
|---|---|---|---|---|
| Batteria LiPo 3.7V | 2000-3000 mAh | 1 | €15-25 | Autonomia 6-24h |
| Connettore JST-XH 2S | Per pack LiPo | 1 | Incluso | — |
| Protezione BMS (integrato) | Batteria con BMS | 1 | Incluso | Smart protection |

---

## 11. DISPLAY HMI (Opzionale) — €30-50

| Descrizione | Specifiche | Quantità | Prezzo | Note |
|---|---|---|---|---|
| Display touch | 3.5-4.3" SPI | 1 | €30-50 | ILI9488 o ST7796 |
| Touch controller | FT5336 o GT911 I2C | 1 | Incluso | — |
| Connettore SPI | Specifico display | 1 | Incluso | — |
| Cavo flat | Per connessione | 1 | Incluso | — |

---

## RIASSUNTO COSTI

| Categoria | Costo |
|---|---|
| Microcontrollori | €40.00 |
| Relay e driver H-bridge | €28.50 |
| Gestione batteria HMI | €3.00 |
| Sensori | €9.80 |
| Protezione/Isolamento | €5.30 |
| Alimentazione | €13.50 |
| Componenti passivi | €5.00 |
| Connettori JST | €3.00 |
| **SUBTOTALE CIRCUITERIA** | **€105.10** |
| | |
| Batteria LiPo (opzionale) | €15-25 |
| Display touch (opzionale) | €30-50 |
| **TOTALE MINIMO** | **~€84** |
| **TOTALE COMPLETO** | **~€129-159** |

---

## ORDINI CONSIGLIATI

### Ordine 1: Microcontrollori + Driver
```
Fornitori: AliExpress, eBay
- 10× ESP32-C3 (ricambi)
- 2× ESP32-S3 (ricambi)
- 3× ESP32-CAM
- 3× DRV8833
Lead time: 2-3 settimane
```

### Ordine 2: Sensori e Componenti passivi
```
Fornitori: AliExpress, Ebelt (stocking EU)
- DS18B20 5× (impermeabile)
- SHT31 2× (custodia)
- Microswitch 10×
- Resistenze/Capacitori kit
- Connettori JST kit
Lead time: 1-2 settimane
```

### Ordine 3: Batteria + Display (Opzionale)
```
Fornitori: Amazon EU, DigiKey, Mouser
- Batteria LiPo 3000mAh 3S (1-2 pezzi)
- Display touch 3.5-4.3"
Lead time: 3-7 giorni (EU stock)
```

---

## CHECKLIST ACQUISTO

### Essenziale (Circuiteria base)
- [ ] MCU ESP32-C3×10
- [ ] MCU ESP32-S3×2
- [ ] MCU ESP32-CAM×3
- [ ] Relay HRS1H-S-DC12V×40 (8 per scheda PCB × 5 schede)
- [ ] BC547B SOT-23×40 (driver NPN)
- [ ] TP4056+DW01 oppure BQ24075
- [ ] DS18B20×5
- [ ] SHT31×2
- [ ] Resistenze kit 100k, 27k, 10k
- [ ] Capacitori kit (100nF, 10µF, 470µF)
- [ ] Buck converter MP2307×9
- [ ] Connettori JST kit
- [ ] PC817-A optoisolatore SMD×10 (2 per scheda × 5 schede)
- [ ] 1N4148 SOD-323×40 (freewheeling relay)

### Opzionale (Completamento HMI)
- [ ] Batteria LiPo 2000-3000mAh
- [ ] Display touch SPI 3.5-4.3"
- [ ] Cavo USB-C per HMI

### Strumenti (Non nel BOM)
- [ ] Saldatore 30-60W
- [ ] Stagno lead-free
- [ ] Flux pen
- [ ] Pompa dissaldante
- [ ] Multimetro

---

## NOTE DI ACQUISTO

**Qualità**: Evitare cloni. Preferire fornitori con feedback positivo (>95%)

**Quantità**: Ordinare 20-30% in più per test e possibili fallimenti

**Spedizione**: Consolidare ordini per ridurre costi di shipping (€5-10 per ordine)

**Tempo**: Ordinare **4 settimane prima** della prototipazione (worst case)

**Compatibilità**: Verificare versioni dei moduli (soprattutto TP4056, INA219)

---

## Fornitori consigliati

| Fornitore | Vantaggi | Svantaggio |
|---|---|---|
| **AliExpress** | Prezzo basso, buona selezione | Lead time 2-4 settimane |
| **Ebelt** (Italia) | Stocking EU, veloce | Prezzo 20-30% più alto |
| **Amazon EU** | Consegna 1-2 giorni | Prezzo premium |
| **DigiKey/Mouser** | Affidabilità, componentistica professionale | Prezzo alto |
