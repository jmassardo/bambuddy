#include "input.h"
#include "pins.h"
#include "config.h"

Input* Input::_instance = nullptr;

void Input::begin() {
    _instance = this;

    // Encoder pins
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    // Button pins
    pinMode(PIN_BTN_BACK, INPUT_PULLUP);
    pinMode(PIN_BTN_CLR, INPUT_PULLUP);

    // Read initial encoder state
    _encLastState = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);

    // Attach encoder interrupt on A pin (CHANGE for both edges)
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), _encISR, CHANGE);

    // Init button states
    _encBtn = {PIN_ENC_SW, true, false, false, 0, 0};
    _backBtn = {PIN_BTN_BACK, true, false, false, 0, 0};
    _clrBtn = {PIN_BTN_CLR, true, false, false, 0, 0};
}

void IRAM_ATTR Input::_encISR() {
    if (!_instance) return;

    uint8_t state = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    uint8_t combined = (_instance->_encLastState << 2) | state;

    // Gray code state transitions → direction
    // CW:  00→01→11→10  CCW: 00→10→11→01
    switch (combined) {
        case 0b0001: case 0b0111: case 0b1110: case 0b1000:
            _instance->_encDelta++;
            break;
        case 0b0010: case 0b1011: case 0b1101: case 0b0100:
            _instance->_encDelta--;
            break;
    }
    _instance->_encLastState = state;
}

InputEvent Input::poll() {
    // Check encoder rotation (accumulate 4 ticks per detent)
    int8_t delta = _encDelta;
    if (delta >= 4) {
        _encDelta -= 4;
        return InputEvent::ENC_CW;
    } else if (delta <= -4) {
        _encDelta += 4;
        return InputEvent::ENC_CCW;
    }

    // Update buttons
    _updateButton(_encBtn);
    _updateButton(_backBtn);
    _updateButton(_clrBtn);

    // Encoder button press
    if (_encBtn.pressed) {
        _encBtn.pressed = false;
        return InputEvent::ENC_PRESS;
    }

    // BACK button press
    if (_backBtn.pressed) {
        _backBtn.pressed = false;
        return InputEvent::BTN_BACK;
    }

    // CLR button — check for long press
    if (_clrBtn.longFired) {
        _clrBtn.longFired = false;
        return InputEvent::BTN_CLR_LONG;
    }
    if (_clrBtn.pressed) {
        _clrBtn.pressed = false;
        return InputEvent::BTN_CLR;
    }

    return InputEvent::NONE;
}

void Input::_updateButton(ButtonState& btn) {
    bool reading = digitalRead(btn.pin);
    uint32_t now = millis();

    if (reading != btn.lastReading) {
        btn.debounceTime = now;
    }

    if ((now - btn.debounceTime) > DEBOUNCE_MS) {
        // Stable reading — active LOW
        bool isPressed = !reading;

        if (isPressed && !btn.pressed && btn.pressTime == 0) {
            // Just pressed
            btn.pressTime = now;
        } else if (isPressed && btn.pressTime > 0 && !btn.longFired) {
            // Still held — check long press (1s)
            if ((now - btn.pressTime) > 1000) {
                btn.longFired = true;
            }
        } else if (!isPressed && btn.pressTime > 0) {
            // Released
            if (!btn.longFired) {
                btn.pressed = true;  // short press event
            }
            btn.pressTime = 0;
            btn.longFired = false;
        }
    }

    btn.lastReading = reading;
}
