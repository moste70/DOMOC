#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// sensors.h — Lettura SHT31 (temperatura e umidità esterna)
//
// Il SHT31 è collegato al bus I2C del PCB Universale (SDA=GPIO8, SCL=GPIO9).
// Indirizzo I2C: 0x44 (pin ADDR collegato a GND sul PCB).
// Misura temperature da -40 a +125 °C, umidità relativa 0-100 %RH.
//
// I dati vengono pubblicati nell'heartbeat mesh come parte di StepStatus
// e permettono all'HMI di monitorare le condizioni esterne del camper
// (utile per sapere se è necessario ritirare il gradino per gelo/pioggia).
// ═══════════════════════════════════════════════════════════════════════════════

// Inizializza il bus I2C e verifica che il SHT31 risponda.
// Restituisce true se il sensore è presente e operativo, false altrimenti.
// In caso di false, sensors_read() non deve essere chiamata (restituirebbe NaN).
bool sensors_init();

// Legge temperatura e umidità dal SHT31.
// I valori vengono scritti nelle variabili passate per riferimento.
// Restituisce true se la lettura è valida, false se il sensore ha restituito NaN
// (timeout I2C, sensore scollegato, condensazione sul sensore).
bool sensors_read(float& temp_c, float& humidity_rh);
