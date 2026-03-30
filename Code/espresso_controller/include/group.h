#pragma once
#include <Arduino.h>
#include "config.h"

// =============================================================================
// group.h — Per-group state machine
// Each of the 3 groups runs an independent instance of this class.
// Handles: button debounce, solenoid control, flow counting, shot termination.
// =============================================================================

enum class GroupState : uint8_t {
    IDLE,
    BREWING,
    DONE
};

class Group {
public:
    // -------------------------------------------------------------------------
    // Constructor — assign hardware pins and interrupt pulse counter reference
    // -------------------------------------------------------------------------
    Group(uint8_t solenoidPin, uint8_t buttonPin, volatile uint32_t& pulseCounter)
        : _solenoidPin(solenoidPin)
        , _buttonPin(buttonPin)
        , _pulseCounter(pulseCounter)
        , _state(GroupState::IDLE)
        , _targetMl(DEFAULT_SHOT_ML)
        , _lastButtonState(HIGH)
        , _debounceTime(0)
        , _shotStartPulses(0)
    {}

    // -------------------------------------------------------------------------
    // init() — call once in setup()
    // -------------------------------------------------------------------------
    void init() {
        pinMode(_solenoidPin, OUTPUT);
        pinMode(_buttonPin,   INPUT_PULLUP);
        digitalWrite(_solenoidPin, LOW);
    }

    // -------------------------------------------------------------------------
    // update() — call every loop tick; drives the state machine
    // -------------------------------------------------------------------------
    void update() {
        switch (_state) {
            case GroupState::IDLE:    _handleIdle();    break;
            case GroupState::BREWING: _handleBrewing(); break;
            case GroupState::DONE:    _handleDone();    break;
        }
    }

    // -------------------------------------------------------------------------
    // setTargetMl() — override default shot volume (e.g. from serial command)
    // -------------------------------------------------------------------------
    void setTargetMl(float ml) {
        _targetMl = ml;
    }

    // -------------------------------------------------------------------------
    // Getters for telemetry / serial reporting
    // -------------------------------------------------------------------------
    GroupState getState()    const { return _state; }
    float      getShotMl()   const { return _pulsesSinceStart() / FLOW_PULSES_PER_ML; }
    bool       isBrewing()   const { return _state == GroupState::BREWING; }

private:
    // --- Hardware ---
    uint8_t             _solenoidPin;
    uint8_t             _buttonPin;
    volatile uint32_t&  _pulseCounter;   // shared with ISR — read atomically

    // --- State ---
    GroupState   _state;
    float        _targetMl;
    uint8_t      _lastButtonState;
    uint32_t     _debounceTime;
    uint32_t     _shotStartPulses;

    // -------------------------------------------------------------------------
    // _pulsesSinceStart() — safe atomic read of volatile counter delta
    // -------------------------------------------------------------------------
    uint32_t _pulsesSinceStart() const {
        uint32_t current;
        noInterrupts();
        current = _pulseCounter;
        interrupts();
        return current - _shotStartPulses;
    }

    // -------------------------------------------------------------------------
    // _buttonPressed() — debounced falling-edge detection (active LOW)
    // -------------------------------------------------------------------------
    bool _buttonPressed() {
        uint8_t reading = digitalRead(_buttonPin);
        if (reading != _lastButtonState) {
            _debounceTime = millis();
        }
        _lastButtonState = reading;
        if ((millis() - _debounceTime) > 50) {   // 50 ms debounce
            if (reading == LOW) return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // State handlers
    // -------------------------------------------------------------------------
    void _handleIdle() {
        if (_buttonPressed()) {
            noInterrupts();
            _shotStartPulses = _pulseCounter;
            interrupts();
            digitalWrite(_solenoidPin, HIGH);
            _state = GroupState::BREWING;
        }
    }

    void _handleBrewing() {
        float dispensed = _pulsesSinceStart() / FLOW_PULSES_PER_ML;
        if (dispensed >= _targetMl) {
            digitalWrite(_solenoidPin, LOW);
            _state = GroupState::DONE;
        }
    }

    void _handleDone() {
        // Brief pause before returning to IDLE prevents immediate re-trigger
        static uint32_t doneTime = 0;
        if (doneTime == 0) doneTime = millis();
        if ((millis() - doneTime) > 1000) {   // 1 second lockout
            doneTime = 0;
            _state = GroupState::IDLE;
        }
    }
};
