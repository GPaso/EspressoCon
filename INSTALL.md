# Installation & Setup Guide

This document covers everything needed to go from parts to a running machine:
wiring, library setup, firmware upload, and step-by-step commissioning.

---

## Table of contents

1. [Required libraries](#1-required-libraries)
2. [Wiring](#2-wiring)
3. [Upload the firmware](#3-upload-the-firmware)
4. [Commissioning](#4-commissioning)
   - [4.1 Discover 1-Wire sensor addresses](#41-discover-1-wire-sensor-addresses)
   - [4.2 Calibrate the pressure transducer](#42-calibrate-the-pressure-transducer)
   - [4.3 Calibrate flow meters](#43-calibrate-flow-meters)
   - [4.4 Set water level thresholds](#44-set-water-level-thresholds)
   - [4.5 PID tuning](#45-pid-tuning)
5. [Serial command interface](#5-serial-command-interface)
6. [Pin reference](#6-pin-reference)
7. [Safety](#7-safety)

---

## 1. Required libraries

Open **Arduino IDE → Tools → Manage Libraries** and install:

| Library | Author | Min version |
|---------|--------|-------------|
| `PID` | Brett Beauregard | 1.2.1 |
| `OneWire` | Paul Stoffregen | 2.3.7 |
| `DallasTemperature` | Miles Burton | 3.9.0 |

---

## 2. Wiring

The full schematic is in `schematic/espresso_controller.kicad_sch` (KiCad 10).
A PDF reference copy is included in the repository root.

### Pin assignments at a glance

**Outputs**

| Pin | Function |
|-----|----------|
| D4 | Heater SSR control (time-proportional PWM) |
| D5 | Pump AC dimmer control (phase-angle) |
| D22 | Autofill solenoid SV5 via relay module |
| D23 | Steam solenoid SV4 via relay module |
| D24 | Group 1 solenoid SV1 via relay module |
| D25 | Group 2 solenoid SV2 via relay module |
| D26 | Group 3 solenoid SV3 via relay module |

**Inputs**

| Pin | Function |
|-----|----------|
| D2 | Group 1 flow meter — INT0 (hardware interrupt) |
| D3 | Group 2 flow meter — INT1 (hardware interrupt) |
| D8 | 1-Wire bus — all 4 DS18B20 sensors + 4.7 kΩ pull-up to 5V |
| D18 | Group 3 flow meter — INT5 (hardware interrupt) |
| D19 | Zero-cross detect from Robodyn dimmer — INT4 |
| D27 | Group 1 brew button (INPUT_PULLUP, active LOW) |
| D28 | Group 2 brew button (INPUT_PULLUP, active LOW) |
| D29 | Group 3 brew button (INPUT_PULLUP, active LOW) |
| A0 | Pressure transducer 0–5V |
| A1 | Boiler level electrode probe (analog) |
| A2 | Reservoir float switch (digital read) |

### Power supply wiring

```
MAINS AC (L/N/PE)
  └─[F1 5A]─► Meanwell LRS-50-24 ─► +24V bus ─► Relay module ─► Solenoids
                                  └─► LM2596 buck ─► +5V ─► Arduino + sensors
              Robodyn AC dimmer ─► Rotary pump
              Fotek SSR-40DA ─► Heater element
```

Mount the SSR on heatsink HS1 before powering. It will overheat and fail
without it at normal duty cycles.

### 1-Wire bus

All four DS18B20 sensors share a single wire on D8. Connect a **4.7 kΩ**
resistor between D8 and +5V. Sensor power can be parasitic (DATA + GND only)
or external (+5V / DATA / GND). External power is more reliable for 4 sensors.

---

## 3. Upload the firmware

1. Connect the Mega 2560 to your computer via USB.
2. In Arduino IDE: **Tools → Board → Arduino AVR Boards → Arduino Mega 2560**.
3. Select the correct port under **Tools → Port**.
4. Open `espresso_controller/espresso_controller.ino`.
5. Click **Upload** (Ctrl+U).

Open **Serial Monitor** at **115 200 baud** after upload. You should see:

```
Espresso controller v1.0 — type STATUS for help
Boot complete.
```

---

## 4. Commissioning

Work through these steps in order before connecting to the machine.

### 4.1 Discover 1-Wire sensor addresses

Each DS18B20 has a unique 8-byte ROM address that must be set in `config.h`
before the firmware can read the correct sensor for each location.

**Step 1** — Wire all 4 sensors to D8 with the 4.7 kΩ pull-up in place.

**Step 2** — Upload the address scanner sketch:
**File → Examples → DallasTemperature → oneWireSearch**

**Step 3** — Open Serial Monitor at 9600 baud. Each sensor prints its address:
```
ROM = 28 AB 12 34 56 78 90 CD
```

**Step 4** — Label each sensor physically (boiler, group 1, group 2, group 3),
then copy the addresses into `include/config.h`:

```cpp
uint8_t BOILER_SENSOR_ADDR[8] = { 0x28, 0xAB, 0x12, 0x34, 0x56, 0x78, 0x90, 0xCD };
uint8_t GROUP1_SENSOR_ADDR[8] = { 0x28, ... };
uint8_t GROUP2_SENSOR_ADDR[8] = { 0x28, ... };
uint8_t GROUP3_SENSOR_ADDR[8] = { 0x28, ... };
```

**Step 5** — Re-upload the main firmware.

---

### 4.2 Calibrate the pressure transducer

The default conversion in `pressure.h` assumes a 0–5V / 0–12 bar linear
transducer. Check your specific transducer datasheet and edit
`_readPressureBar()` accordingly:

```cpp
// Example: 0.5–4.5V output range, 0–16 bar
float voltage = (analogRead(PIN_PRESSURE_SENSOR) / 1023.0f) * 5.0f;
return (voltage - 0.5f) * (16.0f / 4.0f);

// Example: 0–5V output range, 0–12 bar (default)
return (analogRead(PIN_PRESSURE_SENSOR) / 1023.0f) * 12.0f;
```

Verify calibration by running `PRESSURE?` in the serial monitor while checking
against a known-good pressure gauge on the boiler.

---

### 4.3 Calibrate flow meters

The default value (`5.5 pulses/mL`) is typical for the YF-S201 but varies
between units.

**To calibrate:**

1. Run water through a group into a measuring jug.
2. Count pulses by adding a temporary `Serial.println(pulses1)` to `loop()`,
   or use the `STATUS` command and note the mL reading against the actual volume.
3. Calculate: `pulses_per_ml = total_pulses / measured_ml`
4. Update in `config.h`:

```cpp
#define FLOW_PULSES_PER_ML  5.5f   // adjust to your measured value
```

Repeat for each of the three flow meters — they may differ slightly.

---

### 4.4 Set water level thresholds

With the boiler connected, read raw A1 values in two states:

```
STATUS     ← run this with boiler empty, note raw level value
STATUS     ← run this with boiler full, note raw level value
```

Set thresholds in `config.h` so the band sits comfortably between the two readings:

```cpp
#define BOILER_LEVEL_LOW_THRESHOLD   400   // start filling below this
#define BOILER_LEVEL_OK_THRESHOLD    600   // stop filling above this
```

The hysteresis band between the two values prevents relay chatter.

---

### 4.5 PID tuning

Default values are conservative starting points for a single-boiler HX machine:

```cpp
#define HEATER_KP    30.0f
#define HEATER_KI     0.8f
#define HEATER_KD   120.0f
#define HEATER_WINDOW_MS  5000
```

Monitor temperature with `TEMP?` while heating up. Adjust at runtime
without re-uploading:

```
SET KP 25.0
SET KI 0.5
SET KD 150.0
SAVE
```

**Symptoms and fixes:**

| Symptom | Adjustment |
|---------|-----------|
| Temperature overshoots on heat-up | Reduce KP or increase KD |
| Slow to reach setpoint | Increase KP or KI |
| Oscillates around setpoint | Reduce KI, increase KD |
| Steady-state offset (never quite hits target) | Increase KI |

For automated tuning, install Brett Beauregard's
[PID_AutoTune library](https://github.com/br3ttb/Arduino-PID-AutoTune-Library).

---

## 5. Serial command interface

Connect via Serial Monitor or any terminal at **115 200 baud, newline termination**.

| Command | Description |
|---------|-------------|
| `STATUS` | Full system state |
| `TEMP?` | All temperature readings |
| `PRESSURE?` | Current boiler pressure in bar |
| `SET TEMP 93.0` | Boiler temperature setpoint (°C, 70–100) |
| `SET BAR 9.0` | Pressure setpoint (bar, 1–12) |
| `SET ML 1 36.0` | Shot volume for group 1 in mL |
| `SET KP 30.0` | Heater PID proportional gain |
| `SET KI 0.8` | Heater PID integral gain |
| `SET KD 120.0` | Heater PID derivative gain |
| `SAVE` | Persist all setpoints and PID tunings to EEPROM |

Example `STATUS` output:

```
--- STATUS ---
Boiler temp :  92.8 C  (set  93.0 C)
Pressure    :   8.97 bar (set  9.00 bar)
Heater out  :  42.0%
Reservoir   : OK
Autofill    : OFF
Group 1     : IDLE      0.0 /  36.0 mL
Group 2     : BREWING  18.4 /  36.0 mL
Group 3     : IDLE      0.0 /  36.0 mL
```

---

## 6. Pin reference

See [`config.h`](espresso_controller/include/config.h) for the canonical
pin definitions. All assignments can be changed there without touching
any other file.

---

## 7. Safety

> ⚠️ **This project involves mains voltage (230V AC). Incorrect wiring can
> cause electrocution, fire, or equipment damage. Only proceed if you are
> competent with mains electrical work.**

- The Arduino outputs **5V signal only**. Mains is never connected directly to Mega pins.
- The Robodyn dimmer provides built-in mains isolation for the ZCD line.
- The SSR **must** be mounted on heatsink HS1 — it will overheat without it.
- The pressure transducer must be **food-safe** and rated for steam temperatures.
- Route mains wiring in a separate physical channel from signal wiring, using
  **0.75 mm² silicone-insulated cable** with strain relief at all terminals.
- Install fuse F1 (5A) inline on the mains live feed before the PSU.
- Mount all electronics in an **IP65-rated enclosure**.
- Test all solenoid and relay logic at bench voltage before connecting to the machine.
