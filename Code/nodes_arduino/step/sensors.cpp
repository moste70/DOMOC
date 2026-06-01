#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ═══════════════════════════════════════════════════════════════════════════════
// sensors.cpp — Lettura SHT31 temperatura/umidità esterna
// ═══════════════════════════════════════════════════════════════════════════════

// Istanza statica del driver Adafruit SHT31.
// Unica nel translation unit — non condivisa con altri moduli.
static Adafruit_SHT31 sht31;

bool sensors_init() {
    // Inizializza il bus I2C con i pin definiti in config.h.
    // Wire.begin() deve essere chiamato prima di sht31.begin().
    // Il PCB ha pull-up fissi da 4.7 kΩ sul bus — non servono pull-up software.
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // sht31.begin(0x44) invia un soft-reset al sensore e verifica la risposta ACK.
    // Restituisce false se nessun dispositivo risponde all'indirizzo 0x44
    // (sensore non saldato, filo aperto, indirizzo errato).
    return sht31.begin(0x44);
}

bool sensors_read(float& temp_c, float& humidity_rh) {
    // La libreria Adafruit SHT31 invia il comando di misurazione e attende
    // ~15 ms (conversion time del sensore) prima di leggere il risultato.
    // In caso di errore I2C restituisce NaN — per questo si verifica con isnan().
    temp_c      = sht31.readTemperature();
    humidity_rh = sht31.readHumidity();

    // NaN indica timeout o sensore non raggiungibile.
    // Il chiamante (step.cpp) usa questo valore per segnalare DOMOC_ERR_SENSOR
    // nell'heartbeat e non aggiornare i valori in RAM con dati non validi.
    return !isnan(temp_c) && !isnan(humidity_rh);
}
