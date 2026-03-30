#pragma once
#include <Arduino.h>
#include "config.h"

// =============================================================================
// pressure.h — Boiler pressure control via phase-angle pump dimmer
//
// A zero-cross detect (ZCD) interrupt fires at each AC mains zero crossing
// (~100 Hz at 50 Hz mains, ~120 Hz at 60 Hz mains). A timer delay after the
// ZCD event fires the TRIAC gate — the longer the delay, the less power is
// delivered to the pump.
//
// Phase angle (0–100%) maps inversely to gate delay:
//   100% power = fire immediately after ZCD (short delay)
//     0% power = never fire (pump off)
//
// The ZCD ISR is defined externally in the main .ino and calls
// Pressure::onZeroCross() so the class stays ISR-aware but testable.
// =============================================================================

class Pressure {
public:
    Pressure()
        : _currentBar(0.0f)
        , _setpoint(BOILER_PRESSURE_SETPOINT)
        , _pumpPower(PRESSURE_MIN)
        , _lastRun(0)
    {}

    // -------------------------------------------------------------------------
    // init() — call once in setup()
    // -------------------------------------------------------------------------
    void init() {
        pinMode(PIN_PUMP_DIMMER, OUTPUT);
        digitalWrite(PIN_PUMP_DIMMER, LOW);
        // Timer1 used for gate pulse timing — 1 µs resolution
        // Configured in main .ino alongside the ZCD interrupt
    }

    // -------------------------------------------------------------------------
    // update() — call every loop tick; applies proportional pressure correction
    // -------------------------------------------------------------------------
    void update() {
        uint32_t now = millis();
        if ((now - _lastRun) < INTERVAL_PRESSURE) return;
        _lastRun = now;

        _currentBar = _readPressureBar();
        float error  = _setpoint - _currentBar;

        // Proportional control: more error → more pump power correction
        _pumpPower += PRESSURE_KP * error;
        _pumpPower  = constrain(_pumpPower, PRESSURE_MIN, PRESSURE_MAX);
    }

    // -------------------------------------------------------------------------
    // onZeroCross() — called by ZCD ISR; schedules TRIAC gate pulse
    // Must be kept very short — no Serial, no malloc, no delay.
    // -------------------------------------------------------------------------
    void onZeroCross() {
        if (_pumpPower < 1.0f) {
            digitalWrite(PIN_PUMP_DIMMER, LOW);
            return;
        }
        // Delay inversely proportional to desired power:
        //   100% → 0 µs delay, 0% → 9000 µs delay (half-cycle at 50 Hz)
        uint16_t delayUs = (uint16_t)((100.0f - _pumpPower) * 90.0f);
        delayMicroseconds(delayUs);

        // Fire 100 µs gate pulse
        digitalWrite(PIN_PUMP_DIMMER, HIGH);
        delayMicroseconds(100);
        digitalWrite(PIN_PUMP_DIMMER, LOW);
    }

    // -------------------------------------------------------------------------
    // Getters
    // -------------------------------------------------------------------------
    float getPressureBar() const { return _currentBar; }
    float getPumpPower()   const { return _pumpPower; }
    void  setSetpoint(float bar) { _setpoint = bar; }

private:
    float    _currentBar;
    float    _setpoint;
    float    _pumpPower;
    uint32_t _lastRun;

    // -------------------------------------------------------------------------
    // _readPressureBar() — convert ADC reading to bar
    // Assumes a 0–5 V transducer on a 0–1.2 MPa (0–12 bar) range.
    // Adjust scale factor for your specific transducer datasheet.
    // -------------------------------------------------------------------------
    float _readPressureBar() {
        int raw = analogRead(PIN_PRESSURE_SENSOR);
        // 0–1023 ADC → 0–5 V → 0–12 bar linear
        return (raw / 1023.0f) * 12.0f;
    }
};
