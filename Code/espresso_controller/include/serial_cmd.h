#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// =============================================================================
// serial_cmd.h — Serial command parser for runtime configuration & telemetry
//
// Commands (115200 baud, newline terminated):
//   STATUS         — print full system state
//   TEMP?          — print all temperatures
//   PRESSURE?      — print current pressure
//   SET TEMP <val> — set boiler temperature setpoint
//   SET BAR <val>  — set pressure setpoint
//   SET ML <g> <v> — set shot volume for group 1/2/3
//   SET KP <val>   — update heater PID Kp
//   SET KI <val>   — update heater PID Ki
//   SET KD <val>   — update heater PID Kd
//   SAVE           — persist current setpoints to EEPROM
// =============================================================================

// Forward declarations — resolved in main .ino
extern float  g_boilerTemp;
extern float  g_boilerBar;
extern float  g_groupTemp[3];
extern bool   g_groupBrewing[3];
extern float  g_groupMl[3];
extern bool   g_reservoirLow;
extern bool   g_filling;
extern float  g_heaterOutput;

// Setpoint write-back targets (also resolved in main .ino)
extern float  g_brewTempSetpoint;
extern float  g_pressureSetpoint;
extern float  g_shotMl[3];
extern float  g_heaterKp, g_heaterKi, g_heaterKd;

class SerialCmd {
public:
    SerialCmd() : _bufPos(0) {}

    void init() {
        Serial.begin(115200);
        Serial.println(F("Espresso controller v1.0 — type STATUS for help"));
    }

    // Call every loop tick — reads one byte at a time, non-blocking
    void update() {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                _buf[_bufPos] = '\0';
                if (_bufPos > 0) _parse(_buf);
                _bufPos = 0;
            } else if (_bufPos < (BUF_LEN - 1)) {
                _buf[_bufPos++] = c;
            }
        }
    }

private:
    static const uint8_t BUF_LEN = 48;
    char    _buf[BUF_LEN];
    uint8_t _bufPos;

    void _parse(char* line) {
        // Tokenize in-place
        char* cmd = strtok(line, " ");
        if (!cmd) return;

        if (strcmp_P(cmd, PSTR("STATUS")) == 0) {
            _printStatus();

        } else if (strcmp_P(cmd, PSTR("TEMP?")) == 0) {
            Serial.print(F("Boiler: "));   Serial.print(g_boilerTemp, 1);   Serial.println(F(" C"));
            for (int i = 0; i < 3; i++) {
                Serial.print(F("Group ")); Serial.print(i+1); Serial.print(F(": "));
                Serial.print(g_groupTemp[i], 1); Serial.println(F(" C"));
            }

        } else if (strcmp_P(cmd, PSTR("PRESSURE?")) == 0) {
            Serial.print(F("Pressure: ")); Serial.print(g_boilerBar, 2); Serial.println(F(" bar"));

        } else if (strcmp_P(cmd, PSTR("SET")) == 0) {
            char* key = strtok(nullptr, " ");
            char* val = strtok(nullptr, " ");
            if (!key || !val) { _err(); return; }
            _handleSet(key, val);

        } else if (strcmp_P(cmd, PSTR("SAVE")) == 0) {
            _saveEeprom();
            Serial.println(F("Saved to EEPROM"));

        } else {
            _err();
        }
    }

    void _handleSet(char* key, char* val) {
        float fval = atof(val);

        if (strcmp_P(key, PSTR("TEMP")) == 0) {
            if (fval < 70.0f || fval > 100.0f) { Serial.println(F("ERR: TEMP 70-100")); return; }
            g_brewTempSetpoint = fval;
            Serial.print(F("Brew temp -> ")); Serial.println(fval, 1);

        } else if (strcmp_P(key, PSTR("BAR")) == 0) {
            if (fval < 1.0f || fval > 12.0f) { Serial.println(F("ERR: BAR 1-12")); return; }
            g_pressureSetpoint = fval;
            Serial.print(F("Pressure -> ")); Serial.println(fval, 2);

        } else if (strcmp_P(key, PSTR("ML")) == 0) {
            // SET ML <group 1-3> <volume>
            int grp = atoi(val) - 1;
            char* vol = strtok(nullptr, " ");
            if (!vol || grp < 0 || grp > 2) { _err(); return; }
            g_shotMl[grp] = atof(vol);
            Serial.print(F("Group ")); Serial.print(grp+1);
            Serial.print(F(" shot -> ")); Serial.println(g_shotMl[grp], 1);

        } else if (strcmp_P(key, PSTR("KP")) == 0) {
            g_heaterKp = fval;
            Serial.print(F("Kp -> ")); Serial.println(fval, 3);

        } else if (strcmp_P(key, PSTR("KI")) == 0) {
            g_heaterKi = fval;
            Serial.print(F("Ki -> ")); Serial.println(fval, 3);

        } else if (strcmp_P(key, PSTR("KD")) == 0) {
            g_heaterKd = fval;
            Serial.print(F("Kd -> ")); Serial.println(fval, 3);

        } else {
            _err();
        }
    }

    void _printStatus() {
        Serial.println(F("--- STATUS ---"));
        Serial.print(F("Boiler temp : ")); Serial.print(g_boilerTemp, 1); Serial.print(F(" C  (set "));
        Serial.print(g_brewTempSetpoint, 1); Serial.println(F(" C)"));
        Serial.print(F("Pressure    : ")); Serial.print(g_boilerBar, 2); Serial.print(F(" bar (set "));
        Serial.print(g_pressureSetpoint, 2); Serial.println(F(" bar)"));
        Serial.print(F("Heater out  : ")); Serial.print(g_heaterOutput, 1); Serial.println(F("%"));
        Serial.print(F("Reservoir   : ")); Serial.println(g_reservoirLow ? F("LOW") : F("OK"));
        Serial.print(F("Autofill    : ")); Serial.println(g_filling     ? F("ON")  : F("OFF"));
        for (int i = 0; i < 3; i++) {
            Serial.print(F("Group ")); Serial.print(i+1);
            Serial.print(F("      : "));
            Serial.print(g_groupBrewing[i] ? F("BREWING ") : F("IDLE    "));
            Serial.print(g_groupMl[i], 1); Serial.print(F(" / "));
            Serial.print(g_shotMl[i], 1); Serial.println(F(" mL"));
        }
        Serial.println(F("SET TEMP/BAR/ML/KP/KI/KD | SAVE"));
    }

    void _saveEeprom() {
        _eepromWriteFloat(EEPROM_ADDR_KP,        g_heaterKp);
        _eepromWriteFloat(EEPROM_ADDR_KI,        g_heaterKi);
        _eepromWriteFloat(EEPROM_ADDR_KD,        g_heaterKd);
        _eepromWriteFloat(EEPROM_ADDR_BREW_TEMP, g_brewTempSetpoint);
        _eepromWriteFloat(EEPROM_ADDR_SHOT_ML,   g_shotMl[0]);
    }

    void _eepromWriteFloat(int addr, float val) {
        // Store as int16 scaled by 100 to fit in 2 bytes (range ±327.67)
        int16_t scaled = (int16_t)(val * 100.0f);
        EEPROM.put(addr, scaled);
    }

    void _err() {
        Serial.println(F("ERR: unknown command. TYPE STATUS"));
    }
};
