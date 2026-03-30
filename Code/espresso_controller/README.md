# Espresso Controller — Arduino Mega 2560
3-group commercial espresso machine firmware

---

## File structure

```
espresso_controller/
├── espresso_controller.ino   ← Main sketch (open this in Arduino IDE)
└── include/
    ├── config.h              ← ALL pin assignments and tuning constants
    ├── temperature.h         ← DS18B20 1-Wire async temperature sensing
    ├── heater.h              ← Boiler heater PID via SSR
    ├── pressure.h            ← Pump pressure control via AC dimmer
    ├── waterlevel.h          ← Boiler autofill and reservoir monitoring
    ├── group.h               ← Per-group brew state machine (x3)
    └── serial_cmd.h          ← Serial command interface and EEPROM save
```

---

## Required libraries

Install all via Arduino IDE → Tools → Manage Libraries:

| Library              | Author           | Version tested |
|----------------------|------------------|----------------|
| PID                  | Brett Beauregard | 1.2.1          |
| OneWire              | Paul Stoffregen  | 2.3.7          |
| DallasTemperature    | Miles Burton     | 3.9.0          |

---

## Pin assignments (Mega 2560)

| Function                | Pin  | Type            |
|-------------------------|------|-----------------|
| Heater SSR              | 4    | Digital OUT     |
| Pump AC dimmer          | 5    | Digital OUT     |
| Zero-cross detect       | 19   | Interrupt IN    |
| 1-Wire temp sensors     | 8    | Digital I/O     |
| Pressure transducer     | A0   | Analog IN       |
| Boiler level probe      | A1   | Analog IN       |
| Reservoir float switch  | A2   | Digital IN      |
| Autofill solenoid       | 22   | Digital OUT     |
| Steam solenoid          | 23   | Digital OUT     |
| Group 1 solenoid        | 24   | Digital OUT     |
| Group 2 solenoid        | 25   | Digital OUT     |
| Group 3 solenoid        | 26   | Digital OUT     |
| Group 1 brew button     | 27   | Digital IN      |
| Group 2 brew button     | 28   | Digital IN      |
| Group 3 brew button     | 29   | Digital IN      |
| Group 1 flow meter      | 2    | Interrupt IN    |
| Group 2 flow meter      | 3    | Interrupt IN    |
| Group 3 flow meter      | 18   | Interrupt IN    |

All solenoid/SSR outputs switch a 5V signal — use an external relay board
or SSR driver (ULN2803A or similar) for mains-rated switching.

---

## First-time setup

### Step 1 — Discover 1-Wire sensor addresses

Before uploading the main firmware, run the standard OneWire address scanner
sketch (File → Examples → DallasTemperature → oneWireSearch).

Wire all 4 DS18B20 sensors to pin 8 with a 4.7kΩ pull-up to 5V.
Copy the 8-byte addresses into `config.h`:

```cpp
uint8_t BOILER_SENSOR_ADDR[8] = { 0x28, 0xAB, ... };
uint8_t GROUP1_SENSOR_ADDR[8] = { 0x28, 0xCD, ... };
// etc.
```

### Step 2 — Calibrate pressure transducer

Your transducer datasheet will give a voltage-to-pressure curve.
Edit `_readPressureBar()` in `pressure.h` to match your range:

```cpp
// Example: 0.5V–4.5V transducer, 0–16 bar
float voltage = (raw / 1023.0f) * 5.0f;
return (voltage - 0.5f) * (16.0f / 4.0f);
```

### Step 3 — Calibrate flow meters

Fill a measuring cup and count pulses for a known volume.
Update `FLOW_PULSES_PER_ML` in `config.h`.

### Step 4 — PID autotune (optional but recommended)

Start with the default Kp/Ki/Kd values. Once the machine is wired and
heating, monitor via serial (STATUS command) and adjust if the temperature
overshoots or is sluggish. Brett Beauregard's PID Autotune library can
automate this process.

---

## Serial commands (115200 baud)

Connect via Arduino IDE Serial Monitor or any terminal.

```
STATUS           — full system state
TEMP?            — all temperatures
PRESSURE?        — current pressure
SET TEMP 93.0    — set boiler temperature
SET BAR 9.0      — set pressure setpoint
SET ML 1 36.0    — set group 1 shot volume to 36 mL
SET KP 30.0      — update heater PID Kp
SET KI 0.8       — update heater PID Ki
SET KD 120.0     — update heater PID Kd
SAVE             — persist setpoints to EEPROM
```

---

## Safety notes

- All mains-voltage switching MUST be done via properly rated SSRs or
  relays. The Arduino outputs 5V signal only — never connect mains directly.
- The ZCD circuit requires isolation. Use an MOC3021 optocoupler or a
  commercial ZCD module (e.g. Robodyn AC Light Dimmer module).
- The pressure transducer must be food-safe and rated for steam temperatures
  if installed on the steam circuit.
- Test all solenoid logic at low voltage before connecting to the machine.
