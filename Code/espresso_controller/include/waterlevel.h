#pragma once
#include <Arduino.h>
#include "config.h"

// =============================================================================
// waterlevel.h — Boiler autofill and reservoir low-water detection
//
// Boiler level:    Electrode probe on analog pin. Rising voltage = more water.
//                  Hysteresis band prevents relay chatter around the threshold.
// Reservoir level: Simple float switch on digital pin. LOW = sufficient water.
//                  LOW-water state inhibits autofill to protect the pump.
// =============================================================================

class WaterLevel {
public:
    WaterLevel()
        : _filling(false)
        , _reservoirLow(false)
        , _lastRun(0)
    {}

    // -------------------------------------------------------------------------
    // init() — call once in setup()
    // -------------------------------------------------------------------------
    void init() {
        pinMode(PIN_AUTOFILL,         OUTPUT);
        pinMode(PIN_LEVEL_RESERVOIR,  INPUT_PULLUP);
        digitalWrite(PIN_AUTOFILL, LOW);
    }

    // -------------------------------------------------------------------------
    // update() — call every loop tick; manages fill cycle
    // -------------------------------------------------------------------------
    void update() {
        uint32_t now = millis();
        if ((now - _lastRun) < INTERVAL_WATER_LEVEL) return;
        _lastRun = now;

        _reservoirLow = (digitalRead(PIN_LEVEL_RESERVOIR) == RESERVOIR_LOW_PIN_STATE);

        int boilerLevel = analogRead(PIN_LEVEL_BOILER);

        if (_filling) {
            // Stop filling when level reaches OK threshold
            if (boilerLevel >= BOILER_LEVEL_OK_THRESHOLD || _reservoirLow) {
                _setAutofill(false);
            }
        } else {
            // Start filling when level drops below low threshold
            if (boilerLevel < BOILER_LEVEL_LOW_THRESHOLD && !_reservoirLow) {
                _setAutofill(true);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Getters for telemetry
    // -------------------------------------------------------------------------
    bool isFilling()       const { return _filling; }
    bool isReservoirLow()  const { return _reservoirLow; }
    int  getRawBoilerLevel() const { return analogRead(PIN_LEVEL_BOILER); }

private:
    bool     _filling;
    bool     _reservoirLow;
    uint32_t _lastRun;

    void _setAutofill(bool on) {
        _filling = on;
        digitalWrite(PIN_AUTOFILL, on ? HIGH : LOW);
    }
};
