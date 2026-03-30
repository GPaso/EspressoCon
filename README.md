# Espresso Controller (**WIP**)

Open-source firmware for up to 3-group commercial espresso machines, built on the Arduino Mega 2560.
Controls boiler temperature, pressure, water level, and independent per-group brew cycles — all running concurrently on a non-blocking scheduler.

---
 
## Features
 
- **3 independent brew groups** — each with its own solenoid, flow meter, and brew button; all three can run simultaneously
- **Boiler temperature PID** — time-proportional SSR control via DS18B20 1-Wire sensors, ~0.2 °C stability
- **Pump pressure control** — AC phase-angle dimmer with zero-cross detect synchronisation
- **Automatic boiler fill** — electrode level probe with hysteresis; inhibits fill when reservoir is low
- **Per-group flow metering** — shot stops automatically at target volume (mL) or after defined time (mS), configurable per group
- **Non-blocking scheduler** — all control loops run concurrently via `millis()` timing; no `delay()` anywhere
- **Runtime configuration** — full serial command interface at 115200 baud
- **EEPROM persistence** — PID tunings and setpoints survive power cycles
- **Modular codebase** — each subsystem is an independent header-only class; easy to extend
 
---
 
## Contributing
 
Pull requests are welcome. For major changes, please open an issue first to discuss what you'd like to change.
 
When contributing firmware changes:
- Keep all modules header-only (`.h` files only, no separate `.cpp`)
- No `delay()` anywhere in the main sketch or modules
- All new control loops must self-throttle with `millis()` intervals
- Test on a real machine or a bench simulation (resistive load + signal generator for ZCD) before opening a PR
 
---

## Safety

> ⚠️ This project involves **mains voltage (230V AC)**. The Arduino outputs 5V signal only — mains is never connected directly to Mega pins. All high-voltage switching is done via the SSR and Robodyn dimmer module. Read the safety section in [INSTALL.md](INSTALL.md) before building.

---
 
## License
 
GNU GPL — see [`LICENSE`](LICENSE) for details.
 
This project is not affiliated with or endorsed by any hardware manufacturer.
