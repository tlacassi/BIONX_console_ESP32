# BionX ESP32 console

Part of my BionX e-bike retrofit: keep the motor, replace what drives the experience.
This repository is the firmware replacing the original console.
Hardware: [CustomBionxHw](https://github.com/tlacassi/CustomBionxHw).

## How it works

The motor, battery and console talk over a CAN bus (125 kbit/s). The ESP32 takes the
console's place: it starts the battery, reads telemetry and drives the motor.

- Adafruit HUZZAH32 (initial target: ESP32-C3)
- MCP2515 CAN controller
- Round GC9A01 display and rotary encoder
- C, ESP-IDF

## Status

In progress. Motor control and pedal sensor reading work on the bench, wheel off the ground.
**Not validated on the road**, readings not calibrated.

## Try the display (demo data, no CAN)

```sh
idf.py -B build-dashboard -D SDKCONFIG=build-dashboard/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.dashboard' build flash monitor
```

Wiring: [HARDWARE.md](HARDWARE.md). Bench results: [BENCH_FINDINGS.md](BENCH_FINDINGS.md).

## Credits and license

Protocol studied from [BionX Minimal Translator](https://github.com/stancecoke/BionX_Minimal_Translator),
[BionxConsole](https://github.com/inarighas/BionxConsole) and [BionXtool](https://github.com/buxtronix/BionXtool).
GPL-3.0.

[Detailed notes (in French)](docs/README.fr.md)
