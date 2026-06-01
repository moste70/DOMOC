// DomoC — DomocMotor
//
// Driver H-bridge relay (PCB Universale v3.0).
// Sicurezza: K_ENABLE viene sempre portato a OFF prima di cambiare DIR.
// Usato da: STEP, GREY_WATER, FRONT_DOOR.

#pragma once
#include <Arduino.h>

enum class MotorDir { OPEN, CLOSE };

class DomocMotor {
public:
    DomocMotor(uint8_t pin_dir_a, uint8_t pin_dir_b, uint8_t pin_enable)
        : pin_a_(pin_dir_a), pin_b_(pin_dir_b), pin_en_(pin_enable) {}

    void begin() {
        pinMode(pin_a_, OUTPUT);
        pinMode(pin_b_, OUTPUT);
        pinMode(pin_en_, OUTPUT);
        stop();
    }

    void start(MotorDir dir) {
        // Disabilita il rail 12V_SW prima di cambiare direzione
        digitalWrite(pin_en_, LOW);
        delayMicroseconds(500);

        if (dir == MotorDir::OPEN) {
            digitalWrite(pin_a_, HIGH);
            digitalWrite(pin_b_, LOW);
        } else {
            digitalWrite(pin_a_, LOW);
            digitalWrite(pin_b_, HIGH);
        }
        digitalWrite(pin_en_, HIGH);
        running_   = true;
        start_ms_  = millis();
    }

    void stop() {
        digitalWrite(pin_en_, LOW);
        digitalWrite(pin_a_, LOW);
        digitalWrite(pin_b_, LOW);
        running_ = false;
        last_move_ms_ = millis() - start_ms_;
    }

    bool     running()       const { return running_; }
    uint32_t start_ms()      const { return start_ms_; }
    uint16_t last_move_ms()  const { return (uint16_t)last_move_ms_; }

private:
    uint8_t  pin_a_, pin_b_, pin_en_;
    bool     running_     = false;
    uint32_t start_ms_    = 0;
    uint32_t last_move_ms_= 0;
};
