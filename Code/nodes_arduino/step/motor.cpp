#include "motor.h"
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
// motor.cpp — Implementazione driver H-bridge relay
// ═══════════════════════════════════════════════════════════════════════════════

// Flag interno: true se il motore è stato avviato e non ancora fermato.
// Non riflette lo stato fisico del relay (non c'è feedback di corrente),
// ma è sufficiente per la logica di timeout e per evitare start multipli.
static bool _running = false;

void motor_init() {
    // Configura tutti e tre i GPIO come uscite digitali.
    // L'ordine non importa in setup, ma il motore deve partire in standby.
    pinMode(PIN_HB_DIR_A, OUTPUT);
    pinMode(PIN_HB_DIR_B, OUTPUT);
    pinMode(PIN_HB_EN,    OUTPUT);
    motor_stop();  // garantisce che EN=LOW, DIR_A=LOW, DIR_B=LOW all'avvio
}

void motor_start(MotorDir dir) {
    // ── Fase 1: disabilita il rail 12V PRIMA di toccare le direzioni ──────────
    // Se il motore stava già girando in una direzione e viene comandata l'inversa,
    // aprire prima K_ENABLE garantisce che non ci sia mai corrente nel ponte
    // mentre i relay di direzione commutano. I relay meccanici hanno tempi di
    // risposta di 5-15 ms e non commutano tutti nello stesso istante.
    digitalWrite(PIN_HB_EN, LOW);

    // ── Fase 2: attesa di sicurezza ───────────────────────────────────────────
    // 500 µs è ampiamente sufficiente per far aprire il contatto di K_ENABLE
    // prima di chiudere i relay di direzione. Un valore più basso potrebbe
    // non essere sufficiente per relay di qualità economica.
    delayMicroseconds(500);

    // ── Fase 3: imposta la direzione ─────────────────────────────────────────
    // DIR_OPEN:  DIR_A=HIGH, DIR_B=LOW  → MOT_A=+12V, MOT_B=GND
    // DIR_CLOSE: DIR_A=LOW,  DIR_B=HIGH → MOT_A=GND,  MOT_B=+12V
    if (dir == DIR_OPEN) {
        digitalWrite(PIN_HB_DIR_A, HIGH);
        digitalWrite(PIN_HB_DIR_B, LOW);
    } else {
        digitalWrite(PIN_HB_DIR_A, LOW);
        digitalWrite(PIN_HB_DIR_B, HIGH);
    }

    // ── Fase 4: abilita il 12V con direzione già impostata ───────────────────
    digitalWrite(PIN_HB_EN, HIGH);
    _running = true;
}

void motor_stop() {
    // Sequenza inversa rispetto a motor_start: prima stacca il 12V,
    // poi azzera le direzioni (porta l'H-bridge in standby sicuro).
    // Azzerare DIR_A/DIR_B con EN ancora alto causerebbe uno stato
    // transitorio non definito durante la commutazione dei relay.
    digitalWrite(PIN_HB_EN,    LOW);
    digitalWrite(PIN_HB_DIR_A, LOW);
    digitalWrite(PIN_HB_DIR_B, LOW);
    _running = false;
}

bool motor_running() { return _running; }
