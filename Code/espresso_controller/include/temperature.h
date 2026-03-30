#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// =============================================================================
// temperature.h — DS18B20 1-Wire bus management
//
// All 4 sensors (boiler + 3 groups) share a single wire on PIN_ONE_WIRE.
// Non-blocking async conversion: requestTemperatures() is called on a timer,
// and readings are collected after the conversion period (750 ms for 12-bit).
//
// The async pattern is critical — blocking on conversion would stall the
// pressure and heater loops during the 750 ms wait.
// =============================================================================

class Temperature {
public:
    Temperature()
        : _oneWire(PIN_ONE_WIRE)
        , _sensors(&_oneWire)
        , _boilerTemp(0.0f)
        , _lastRequest(0)
        , _conversionPending(false)
    {
        for (int i = 0; i < 3; i++) _groupTemp[i] = 0.0f;
    }

    // -------------------------------------------------------------------------
    // init() — call once in setup(); scan bus and start first conversion
    // -------------------------------------------------------------------------
    void init() {
        _sensors.begin();
        _sensors.setWaitForConversion(false);  // async — non-blocking
        _sensors.setResolution(12);            // 12-bit = 0.0625°C resolution
        _requestConversion();
    }

    // -------------------------------------------------------------------------
    // update() — call every loop tick; collects readings after conversion time
    // -------------------------------------------------------------------------
    void update() {
        uint32_t now = millis();

        // Collect reading after conversion time has elapsed
        if (_conversionPending &&
            (now - _lastRequest) >= INTERVAL_TEMP_REQUEST) {
            _collectReadings();
            _conversionPending = false;
        }

        // Schedule next conversion
        if (!_conversionPending &&
            (now - _lastRequest) >= INTERVAL_HEATER_PID) {
            _requestConversion();
        }
    }

    // -------------------------------------------------------------------------
    // Getters — return last successfully collected readings
    // -------------------------------------------------------------------------
    float getBoilerTemp()          const { return _boilerTemp; }
    float getGroupTemp(uint8_t g)  const {
        if (g < 3) return _groupTemp[g];
        return 0.0f;
    }

private:
    OneWire          _oneWire;
    DallasTemperature _sensors;

    float    _boilerTemp;
    float    _groupTemp[3];
    uint32_t _lastRequest;
    bool     _conversionPending;

    void _requestConversion() {
        _sensors.requestTemperatures();
        _lastRequest = millis();
        _conversionPending = true;
    }

    void _collectReadings() {
        float t;

        t = _sensors.getTempC(BOILER_SENSOR_ADDR);
        if (t != DEVICE_DISCONNECTED_C) _boilerTemp = t;

        t = _sensors.getTempC(GROUP1_SENSOR_ADDR);
        if (t != DEVICE_DISCONNECTED_C) _groupTemp[0] = t;

        t = _sensors.getTempC(GROUP2_SENSOR_ADDR);
        if (t != DEVICE_DISCONNECTED_C) _groupTemp[1] = t;

        t = _sensors.getTempC(GROUP3_SENSOR_ADDR);
        if (t != DEVICE_DISCONNECTED_C) _groupTemp[2] = t;
    }
};
