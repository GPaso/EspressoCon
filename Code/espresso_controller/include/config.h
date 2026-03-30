#pragma once

// =============================================================================
// config.h — Pin assignments, setpoints, and tuning constants
// Edit this file to match your wiring and machine characteristics.
// =============================================================================

// -----------------------------------------------------------------------------
// PIN ASSIGNMENTS
// -----------------------------------------------------------------------------

// --- Heater SSR (PWM out, zero-cross synchronized) ---
#define PIN_HEATER_SSR        4

// --- Pump AC dimmer (PWM out, phase-angle via ZCD) ---
#define PIN_PUMP_DIMMER       5

// --- Zero-cross detect (hardware interrupt) ---
#define PIN_ZCD               19   // INT4 on Mega

// --- 1-Wire bus (all temp sensors share this pin) ---
#define PIN_ONE_WIRE          8

// --- Dallas DS18B20 sensor addresses (64-bit ROM, fill after first scan) ---
//     Run examples/oneWireSearch to discover addresses on your bus.
uint8_t BOILER_SENSOR_ADDR[8]  = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t GROUP1_SENSOR_ADDR[8]  = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
uint8_t GROUP2_SENSOR_ADDR[8]  = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
uint8_t GROUP3_SENSOR_ADDR[8]  = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };

// --- Pressure transducer (analog in, 0-5V → 0-1.2 MPa) ---
#define PIN_PRESSURE_SENSOR   A0

// --- Water level probes ---
#define PIN_LEVEL_BOILER      A1   // analog electrode probe
#define PIN_LEVEL_RESERVOIR   A2   // digital float switch

// --- Autofill solenoid ---
#define PIN_AUTOFILL          22

// --- Steam solenoid valve ---
#define PIN_STEAM_VALVE       23

// --- Group solenoid valves ---
#define PIN_GROUP1_SOLENOID   24
#define PIN_GROUP2_SOLENOID   25
#define PIN_GROUP3_SOLENOID   26

// --- Group brew buttons (active LOW with internal pullup) ---
#define PIN_GROUP1_BUTTON     27
#define PIN_GROUP2_BUTTON     28
#define PIN_GROUP3_BUTTON     29

// --- Group flow meters (hardware interrupt pins) ---
#define PIN_GROUP1_FLOW       2    // INT0
#define PIN_GROUP2_FLOW       3    // INT1
#define PIN_GROUP3_FLOW       18   // INT5

// -----------------------------------------------------------------------------
// BOILER SETPOINTS
// -----------------------------------------------------------------------------

#define BOILER_TEMP_SETPOINT      93.0f   // °C brew temperature
#define STEAM_TEMP_SETPOINT      130.0f   // °C steam temperature
#define BOILER_PRESSURE_SETPOINT   9.0f   // bar (9 bar is espresso standard)

// -----------------------------------------------------------------------------
// WATER LEVEL THRESHOLDS
// -----------------------------------------------------------------------------

#define BOILER_LEVEL_LOW_THRESHOLD   400   // analog raw value (0–1023)
#define BOILER_LEVEL_OK_THRESHOLD    600   // stop filling above this
#define RESERVOIR_LOW_PIN_STATE     HIGH   // float switch logic level when low

// -----------------------------------------------------------------------------
// HEATER PID TUNING  (Kp, Ki, Kd)
// Start with conservative values; run autotune after basic bringup.
// -----------------------------------------------------------------------------

#define HEATER_KP    30.0f
#define HEATER_KI     0.8f
#define HEATER_KD   120.0f

// Window size for SSR time-proportional control (ms)
#define HEATER_WINDOW_MS  5000

// -----------------------------------------------------------------------------
// PRESSURE CONTROL TUNING
// Simple proportional correction — pump phase angle 0–100%
// -----------------------------------------------------------------------------

#define PRESSURE_KP   15.0f   // % phase-angle per bar of error
#define PRESSURE_MIN   20.0f  // % minimum pump power (keeps pump primed)
#define PRESSURE_MAX  100.0f  // % maximum pump power

// -----------------------------------------------------------------------------
// FLOW METER CALIBRATION
// -----------------------------------------------------------------------------

// Pulses per millilitre for your specific flow meter (YF-S201 ≈ 5.5 pulses/mL)
#define FLOW_PULSES_PER_ML  5.5f

// Default shot target volume in mL (can be overridden per group via serial)
#define DEFAULT_SHOT_ML     36.0f

// -----------------------------------------------------------------------------
// SCHEDULER INTERVALS (milliseconds)
// -----------------------------------------------------------------------------

#define INTERVAL_HEATER_PID    200
#define INTERVAL_PRESSURE      100
#define INTERVAL_WATER_LEVEL   500
#define INTERVAL_TEMP_REQUEST   750  // DS18B20 conversion time

// -----------------------------------------------------------------------------
// EEPROM ADDRESSES  (2 bytes per float via int16 scaled storage)
// -----------------------------------------------------------------------------

#define EEPROM_ADDR_KP          0
#define EEPROM_ADDR_KI          2
#define EEPROM_ADDR_KD          4
#define EEPROM_ADDR_BREW_TEMP   6
#define EEPROM_ADDR_SHOT_ML     8
