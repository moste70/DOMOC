#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
// motor.h — Driver H-bridge relay per il nodo STEP
//
// Astrazione del motore DC bidirezionale controllato tramite H-bridge a relay.
// Il driver garantisce la sequenza di sicurezza obbligatoria:
//   1. K_ENABLE=OFF  (stacca il 12V dal motore)
//   2. attesa 500 µs (i relay sono meccanici: serve tempo per aprire il contatto)
//   3. K_DIR_A / K_DIR_B → nuova direzione
//   4. K_ENABLE=ON   (applica il 12V con la direzione già impostata)
//
// Saltare il passo 1-2 prima di cambiare direzione causerebbe un cortocircuito
// istantaneo nell'H-bridge (entrambe le sponde del ponte a potenziale opposto
// per qualche ms mentre i relay commutano a tempi diversi).
// ═══════════════════════════════════════════════════════════════════════════════

// Direzione di movimento della scaletta.
// DIR_OPEN  = scaletta si estende verso il basso (esce dal vano).
// DIR_CLOSE = scaletta si ritrae verso l'alto (rientra nel vano).
enum MotorDir { DIR_OPEN, DIR_CLOSE };

// Inizializza i GPIO dell'H-bridge come OUTPUT e porta il motore in standby.
// Da chiamare in setup() prima di qualsiasi altro uso del motore.
void motor_init();

// Avvia il motore nella direzione indicata.
// Applica internamente la sequenza di sicurezza ENABLE-OFF → DIR → ENABLE-ON.
// Non ha effetto se il motore è già in movimento (evita doppi start).
void motor_start(MotorDir dir);

// Ferma il motore portando tutti i GPIO a LOW.
// Sequenza: ENABLE=LOW → DIR_A=LOW → DIR_B=LOW (stato standby sicuro).
// Chiamato sia dal firmware in risposta al finecorsa sia dal watchdog di timeout.
void motor_stop();

// Restituisce true se il motore è attivo (start senza stop successivo).
// Usato da check_motor_timeout() per sapere se avviare il conto del timeout.
bool motor_running();
