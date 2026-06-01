#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// config.h — Nodo STEP
//
// Costanti hardware e timing per il gradino motorizzato.
// Questo file è l'unico punto dove si toccano i GPIO e i parametri di tempo:
// il resto del codice non contiene numeri magici.
//
// Hardware: ESP32-S3-MINI-1 su PCB Universale v3.0
// Il PCB ha due H-bridge relay (HB1, HB2) e due optoisolatori (OPT1, OPT2).
// Il nodo STEP usa solo HB1 (motore gradino) e OPT1/OPT2 (finecorsa).
// ═══════════════════════════════════════════════════════════════════════════════

// ── GPIO finecorsa ───────────────────────────────────────────────────────────
//
// Entrambi i finecorsa sono switch NC (normally closed) collegati agli
// optoisolatori del PCB. L'ingresso è in pull-up hardware.
//
// Comportamento NC con pull-up:
//   - Finecorsa a riposo (switch chiuso) → pin LOW
//   - Finecorsa attivato (switch aperto, scaletta in posizione) → pin HIGH
//
// Esempio: quando la scaletta raggiunge la posizione CHIUSA, il finecorsa
// meccanico si apre → il pull-up porta il pin a HIGH → digitalRead() = HIGH.
#define PIN_FC_CLOSED   3   // OPT1 — finecorsa chiuso (NC, pull-up: HIGH = raggiunto)
#define PIN_FC_OPEN     4   // OPT2 — finecorsa aperto  (NC, pull-up: HIGH = raggiunto)

// ── GPIO H-bridge (HB1) ──────────────────────────────────────────────────────
//
// L'H-bridge usa 3 relay SPDT pilotati da NPN:
//   K_DIR_A e K_DIR_B selezionano la direzione di rotazione del motore.
//   K_ENABLE abilita il rail 12V_SW verso il motore.
//
// REGOLA DI SICUREZZA: K_ENABLE deve essere OFF prima di cambiare K_DIR_A/B.
// Se entrambi DIR_A e DIR_B fossero HIGH con ENABLE=ON si avrebbe un
// cortocircuito sul ponte. DomocMotor gestisce questa sequenza.
//
// Tabella stati:
//   DIR_A=HIGH, DIR_B=LOW,  EN=HIGH → MOT_A=+12V, MOT_B=GND → APRI
//   DIR_A=LOW,  DIR_B=HIGH, EN=HIGH → MOT_A=GND,  MOT_B=+12V → CHIUDI
//   DIR_A=LOW,  DIR_B=LOW,  EN=LOW  → motore scollegato (standby sicuro)
#define PIN_HB_DIR_A    11  // HB1_DIR_A — direzione A
#define PIN_HB_DIR_B    12  // HB1_DIR_B — direzione B
#define PIN_HB_EN       13  // HB1_EN    — abilitazione rail 12V_SW

// ── GPIO I2C (sensore SHT31) ─────────────────────────────────────────────────
//
// Il bus I2C ha pull-up fissi da 4.7 kΩ sul PCB.
// SHT31 è all'indirizzo 0x44 (pin ADDR a GND).
// Misura temperatura e umidità esterna, pubblicata nell'heartbeat.
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9

// ── GPIO LED stato ───────────────────────────────────────────────────────────
//
// LED WS2812B-2020 RGB. Pilotato da DomocLed che mappa ogni LedState
// su un colore e un pattern di lampeggio predefinito.
// Permette di capire lo stato del nodo senza debug seriale.
#define PIN_LED         21  // LED_DATA — WS2812B

// ── Timing ──────────────────────────────────────────────────────────────────
//
// MOTOR_TIMEOUT_MS: tempo massimo che il motore può girare prima che
//   il firmware consideri il movimento fallito (finecorsa non raggiunto)
//   e transiti in STATE_ERROR. Dimensionato per coprire l'escursione
//   completa del gradino con margine del 50%.
//
// FC_DEBOUNCE_MS: finestra di debounce per i finecorsa. I relay e i
//   contatti meccanici producono rimbalzi (bounce) nell'ordine dei ms.
//   50 ms è sufficiente per filtrarli senza aggiungere latenza percepibile.
//
// SHT31_INTERVAL_MS: il SHT31 ha un tempo di conversione minimo di ~15 ms,
//   ma per un sensore ambientale 60 s è più che sufficiente e riduce
//   il carico sul bus I2C.
#define MOTOR_TIMEOUT_MS    10000   // 10 s — timeout movimento motore
#define FC_DEBOUNCE_MS      50      // 50 ms — debounce contatti finecorsa
#define SHT31_INTERVAL_MS   60000   // 60 s — intervallo lettura temperatura/umidità
