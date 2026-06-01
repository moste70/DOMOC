#include "motor.h"
#include "config.h"

static bool _running = false;

void motor_init() {
    pinMode(PIN_HB_DIR_A, OUTPUT);
    pinMode(PIN_HB_DIR_B, OUTPUT);
    pinMode(PIN_HB_EN,    OUTPUT);
    motor_stop();
}

void motor_start(MotorDir dir) {
    // K_ENABLE OFF prima di cambiare direzione — sicurezza H-bridge
    digitalWrite(PIN_HB_EN, LOW);
    delayMicroseconds(500);

    if (dir == DIR_OPEN) {
        digitalWrite(PIN_HB_DIR_A, HIGH);
        digitalWrite(PIN_HB_DIR_B, LOW);
    } else {
        digitalWrite(PIN_HB_DIR_A, LOW);
        digitalWrite(PIN_HB_DIR_B, HIGH);
    }

    digitalWrite(PIN_HB_EN, HIGH);
    _running = true;
}

void motor_stop() {
    // Disabilita prima il rail 12V_SW, poi le direzioni
    digitalWrite(PIN_HB_EN,    LOW);
    digitalWrite(PIN_HB_DIR_A, LOW);
    digitalWrite(PIN_HB_DIR_B, LOW);
    _running = false;
}

bool motor_running() { return _running; }
