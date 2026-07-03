#pragma once

#include <Arduino.h>

enum class InputEvent : uint8_t {
    NONE = 0,
    ENC_CW,          // Encoder clockwise (scroll down)
    ENC_CCW,         // Encoder counter-clockwise (scroll up)
    ENC_PRESS,       // Encoder button press
    BTN_BACK,        // BACK button press
    BTN_CLR,         // CLR button press
    BTN_CLR_LONG,    // CLR button long-press (>1s)
};

class Input {
public:
    void begin();
    InputEvent poll();   // Returns next event (non-blocking)

private:
    // Encoder state
    volatile int8_t _encDelta = 0;
    uint8_t _encLastState = 0;

    // Button states
    struct ButtonState {
        uint8_t pin;
        bool lastReading;
        bool pressed;
        bool longFired;
        uint32_t pressTime;
        uint32_t debounceTime;
    };

    ButtonState _encBtn = {};
    ButtonState _backBtn = {};
    ButtonState _clrBtn = {};

    void _updateButton(ButtonState& btn);
    static void IRAM_ATTR _encISR();
    static Input* _instance;
};
