#pragma once
#include <Arduino.h>
#include <PID_v1.h>
#include "config.h"

// =============================================================================
// heater.h — Boiler heater PID control via SSR (time-proportional window)
//
// Uses a time-proportional window rather than fast PWM because SSRs driving
// resistive heater elements work best at mains frequency (not kHz PWM).
// The window size (HEATER_WINDOW_MS) is the period; PID output 0–5000 maps
// to on-time 0–5000 ms within that window.
// =============================================================================

class Heater {
public:
    Heater()
        : _input(0.0)
        , _output(0.0)
        , _setpoint(BOILER_TEMP_SETPOINT)
        , _pid(&_input, &_output, &_setpoint,
               HEATER_KP, HEATER_KI, HEATER_KD, DIRECT)
        , _windowStart(0)
        , _lastRun(0)
    {}

    // -------------------------------------------------------------------------
    // init() — call once in setup()
    // -------------------------------------------------------------------------
    void init() {
        pinMode(PIN_HEATER_SSR, OUTPUT);
        digitalWrite(PIN_HEATER_SSR, LOW);
        _pid.SetOutputLimits(0, HEATER_WINDOW_MS);
        _pid.SetSampleTime(INTERVAL_HEATER_PID);
        _pid.SetMode(AUTOMATIC);
        _windowStart = millis();
    }

    // -------------------------------------------------------------------------
    // update(temp) — call every loop tick with latest boiler temperature
    // -------------------------------------------------------------------------
    void update(float boilerTemp) {
        uint32_t now = millis();
        if ((now - _lastRun) < INTERVAL_HEATER_PID) return;
        _lastRun = now;

        _input = boilerTemp;
        _pid.Compute();

        // Time-proportional SSR drive within the window
        if ((now - _windowStart) >= HEATER_WINDOW_MS) {
            _windowStart += HEATER_WINDOW_MS;
        }
        bool heaterOn = ((now - _windowStart) < (uint32_t)_output);
        digitalWrite(PIN_HEATER_SSR, heaterOn ? HIGH : LOW);
    }

    // -------------------------------------------------------------------------
    // setSetpoint() — change brew temperature target at runtime
    // -------------------------------------------------------------------------
    void setSetpoint(float temp) { _setpoint = temp; }
    float getSetpoint()    const { return _setpoint; }
    float getOutput()      const { return _output; }

    // -------------------------------------------------------------------------
    // setTunings() — update PID gains at runtime (e.g. from serial command)
    // -------------------------------------------------------------------------
    void setTunings(float kp, float ki, float kd) {
        _pid.SetTunings(kp, ki, kd);
    }

private:
    double  _input, _output, _setpoint;
    PID     _pid;
    uint32_t _windowStart;
    uint32_t _lastRun;
};
