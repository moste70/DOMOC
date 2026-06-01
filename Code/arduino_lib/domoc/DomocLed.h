// DomoC — DomocLed
//
// Helper per il LED WS2812B di stato. Chiama led_update() ogni loop().
// Dipende da FastLED (deve essere inizializzato nel .ino con FastLED.addLeds).

#pragma once
#include <FastLED.h>

// Stato LED comuni — i nodi possono aggiungere i propri
enum class LedState : uint8_t {
    OFF,
    INITIALIZING,   // bianco lampeggio lento
    OK_CLOSED,      // verde fisso
    OK_OPEN,        // blu fisso
    MOVING_OPEN,    // blu lampeggio
    MOVING_CLOSE,   // arancio lampeggio
    AUTO_CLOSING,   // rosso lampeggio veloce
    ERROR,          // rosso fisso
    STANDALONE,     // bianco fisso
    VALVE_OPEN,     // azzurro fisso
    VALVE_CLOSED,   // verde fisso (alias OK_CLOSED)
    THERMO_HEAT,    // arancio fisso (riscaldamento attivo)
    THERMO_IDLE,    // verde fisso
};

class DomocLed {
public:
    explicit DomocLed(CRGB* led) : led_(led) {}

    void set(LedState s)  { state_ = s; }
    LedState state() const { return state_; }

    // Chiamare ogni loop() — gestisce il lampeggio senza blocking
    void update() {
        uint32_t now    = millis();
        bool     blink  = false;
        uint32_t period = 500;
        CRGB     color  = CRGB::Black;

        switch (state_) {
            case LedState::OFF:           color = CRGB::Black;                            break;
            case LedState::INITIALIZING:  color = CRGB::White;   blink = true;            break;
            case LedState::OK_CLOSED:     color = CRGB::Green;                            break;
            case LedState::VALVE_CLOSED:  color = CRGB::Green;                            break;
            case LedState::THERMO_IDLE:   color = CRGB::Green;                            break;
            case LedState::OK_OPEN:       color = CRGB::Blue;                             break;
            case LedState::VALVE_OPEN:    color = CRGB(0, 150, 255);                      break;
            case LedState::MOVING_OPEN:   color = CRGB::Blue;    blink = true;            break;
            case LedState::MOVING_CLOSE:  color = CRGB::Orange;  blink = true;            break;
            case LedState::THERMO_HEAT:   color = CRGB::Orange;                           break;
            case LedState::AUTO_CLOSING:  color = CRGB::Red;     blink = true; period=200; break;
            case LedState::ERROR:         color = CRGB::Red;                              break;
            case LedState::STANDALONE:    color = CRGB::White;                            break;
        }

        if (blink) {
            if (now - blink_ms_ >= period) {
                blink_ms_ = now;
                blink_on_ = !blink_on_;
            }
            *led_ = blink_on_ ? color : CRGB::Black;
        } else {
            *led_ = color;
        }
        FastLED.show();
    }

private:
    CRGB*    led_;
    LedState state_   = LedState::INITIALIZING;
    uint32_t blink_ms_= 0;
    bool     blink_on_= false;
};
