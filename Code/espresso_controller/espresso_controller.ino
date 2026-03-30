// =============================================================================
// espresso_controller.ino — Main sketch
//
// Hardware: Arduino Mega 2560
// Manages:  3 brew groups, boiler heater PID, pressure control, water level
//
// Required libraries (install via Arduino Library Manager):
//   - PID                by Brett Beauregard  (br3ttb/Arduino-PID-Library)
//   - OneWire            by Paul Stoffregen
//   - DallasTemperature  by Miles Burton
// =============================================================================

#include <Arduino.h>
#include <EEPROM.h>

#include "include/config.h"
#include "include/temperature.h"
#include "include/heater.h"
#include "include/pressure.h"
#include "include/waterlevel.h"
#include "include/group.h"
#include "include/serial_cmd.h"

// =============================================================================
// GLOBAL TELEMETRY — read by SerialCmd, written by control loops
// =============================================================================

float g_boilerTemp        = 0.0f;
float g_boilerBar         = 0.0f;
float g_groupTemp[3]      = { 0.0f, 0.0f, 0.0f };
bool  g_groupBrewing[3]   = { false, false, false };
float g_groupMl[3]        = { 0.0f, 0.0f, 0.0f };
bool  g_reservoirLow      = false;
bool  g_filling           = false;
float g_heaterOutput      = 0.0f;

// Setpoints — written by SerialCmd, applied to modules in loop()
float g_brewTempSetpoint   = BOILER_TEMP_SETPOINT;
float g_pressureSetpoint   = BOILER_PRESSURE_SETPOINT;
float g_shotMl[3]          = { DEFAULT_SHOT_ML, DEFAULT_SHOT_ML, DEFAULT_SHOT_ML };
float g_heaterKp           = HEATER_KP;
float g_heaterKi           = HEATER_KI;
float g_heaterKd           = HEATER_KD;

// =============================================================================
// FLOW METER PULSE COUNTERS — written by ISRs, read by Group instances
// =============================================================================

volatile uint32_t pulses1 = 0;
volatile uint32_t pulses2 = 0;
volatile uint32_t pulses3 = 0;

void ISR_flow1() { pulses1++; }
void ISR_flow2() { pulses2++; }
void ISR_flow3() { pulses3++; }

// =============================================================================
// MODULE INSTANCES
// =============================================================================

Temperature temperature;
Heater      heater;
Pressure    pressure;
WaterLevel  waterLevel;
SerialCmd   serialCmd;

Group group1(PIN_GROUP1_SOLENOID, PIN_GROUP1_BUTTON, pulses1);
Group group2(PIN_GROUP2_SOLENOID, PIN_GROUP2_BUTTON, pulses2);
Group group3(PIN_GROUP3_SOLENOID, PIN_GROUP3_BUTTON, pulses3);

// =============================================================================
// ZERO-CROSS DETECT ISR — delegates directly to pressure module
// Kept minimal: no branches, no function calls with stack overhead.
// =============================================================================

void ISR_zeroCross() {
    pressure.onZeroCross();
}

// =============================================================================
// EEPROM RESTORE — load persisted setpoints on boot
// =============================================================================

float _eepromReadFloat(int addr) {
    int16_t scaled;
    EEPROM.get(addr, scaled);
    // Sanity check — 0xFFFF means the address was never written
    if (scaled == (int16_t)0xFFFF) return 0.0f;
    return scaled / 100.0f;
}

void restoreEeprom() {
    float kp   = _eepromReadFloat(EEPROM_ADDR_KP);
    float ki   = _eepromReadFloat(EEPROM_ADDR_KI);
    float kd   = _eepromReadFloat(EEPROM_ADDR_KD);
    float temp = _eepromReadFloat(EEPROM_ADDR_BREW_TEMP);
    float ml   = _eepromReadFloat(EEPROM_ADDR_SHOT_ML);

    if (kp > 0.0f) { g_heaterKp = kp; g_heaterKi = ki; g_heaterKd = kd; }
    if (temp > 70.0f && temp < 100.0f) g_brewTempSetpoint = temp;
    if (ml > 5.0f && ml < 200.0f) {
        for (int i = 0; i < 3; i++) g_shotMl[i] = ml;
    }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    restoreEeprom();

    serialCmd.init();
    temperature.init();
    heater.init();
    pressure.init();
    waterLevel.init();

    // Steam valve — off by default
    pinMode(PIN_STEAM_VALVE, OUTPUT);
    digitalWrite(PIN_STEAM_VALVE, LOW);

    // Flow meter interrupts (RISING edge = one pulse)
    pinMode(PIN_GROUP1_FLOW, INPUT_PULLUP);
    pinMode(PIN_GROUP2_FLOW, INPUT_PULLUP);
    pinMode(PIN_GROUP3_FLOW, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_GROUP1_FLOW), ISR_flow1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_GROUP2_FLOW), ISR_flow2, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_GROUP3_FLOW), ISR_flow3, RISING);

    // Zero-cross detect interrupt
    pinMode(PIN_ZCD, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ZCD), ISR_zeroCross, RISING);

    // Group init
    group1.init();
    group2.init();
    group3.init();

    // Apply initial setpoints from EEPROM
    heater.setSetpoint(g_brewTempSetpoint);
    heater.setTunings(g_heaterKp, g_heaterKi, g_heaterKd);
    pressure.setSetpoint(g_pressureSetpoint);
    group1.setTargetMl(g_shotMl[0]);
    group2.setTargetMl(g_shotMl[1]);
    group3.setTargetMl(g_shotMl[2]);

    Serial.println(F("Boot complete."));
}

// =============================================================================
// MAIN LOOP — scheduler dispatches all modules non-blocking
// =============================================================================

void loop() {
    // --- 1. Collect sensor data ---
    temperature.update();

    // --- 2. Mirror latest temp readings into globals ---
    g_boilerTemp    = temperature.getBoilerTemp();
    g_groupTemp[0]  = temperature.getGroupTemp(0);
    g_groupTemp[1]  = temperature.getGroupTemp(1);
    g_groupTemp[2]  = temperature.getGroupTemp(2);

    // --- 3. Apply any runtime setpoint changes from serial ---
    heater.setSetpoint(g_brewTempSetpoint);
    heater.setTunings(g_heaterKp, g_heaterKi, g_heaterKd);
    pressure.setSetpoint(g_pressureSetpoint);
    group1.setTargetMl(g_shotMl[0]);
    group2.setTargetMl(g_shotMl[1]);
    group3.setTargetMl(g_shotMl[2]);

    // --- 4. Run control loops (each self-throttles via millis interval) ---
    heater.update(g_boilerTemp);
    pressure.update();
    waterLevel.update();

    // --- 5. Update telemetry globals ---
    g_boilerBar     = pressure.getPressureBar();
    g_heaterOutput  = heater.getOutput();
    g_reservoirLow  = waterLevel.isReservoirLow();
    g_filling       = waterLevel.isFilling();

    // --- 6. Run per-group state machines ---
    group1.update();
    group2.update();
    group3.update();

    g_groupBrewing[0] = group1.isBrewing();
    g_groupBrewing[1] = group2.isBrewing();
    g_groupBrewing[2] = group3.isBrewing();
    g_groupMl[0]      = group1.getShotMl();
    g_groupMl[1]      = group2.getShotMl();
    g_groupMl[2]      = group3.getShotMl();

    // --- 7. Handle serial commands ---
    serialCmd.update();
}
