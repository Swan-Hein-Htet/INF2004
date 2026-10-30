# Servo troubleshooting without UART

The owner reports an SG90 turning left and continuously twitching after flashing.
The photo identifies the GP12 servo-port column, but its pins are uncovered, so
actual plug orientation is not visible. Power is from an unspecified battery.
These test images separate application startup from power/wiring problems;
they do not establish the actual cause without observations on the car.

## Prepare

With power off, disconnect motor power at M1/M2. Keep SG90 orange on GP12 /
servo port 1 S, brown on common GND, and red on a verified suitable servo supply.
Servo V+ follows the carrier's input; it is not a fixed 5 V output. Typical SG90s
use nominal 5 V, subject to the fitted unit's rating. Battery voltage is still
unconfirmed; code cannot correct an unsuitable supply.

Leave **GP7 on Grove 7 unconnected**. Watch the small **GP7 GPIO status LED on
the Robo Pico carrier**, beside Grove 7. The barcode still uses GP28, with D0
disconnected. See [WIRING.md](WIRING.md).

## First: servo output OFF

Flash [mtk3pico_smp0_uart_test_servo_off.uf2](../build_make/mtk3pico_smp0_uart_test_servo_off.uf2)
with BOOTSEL and RPI-RP2. Observe for at least ten seconds after boot.

- GP7: **one short blink, pause, repeat**.
- GP12: held LOW; no servo pulses. The servo may stop actively holding position.
- GP8–GP11 wheel-driver inputs: held LOW.
- The application does not initialize IMU, ADC, sonar, networking or mission tasks.

A brief power-on movement can occur inside a servo independently of commands.
If continuous twitching persists here, the scanner/mission is not commanding it.
Inspect supply voltage/current, signal/ground wiring, connector orientation,
load/binding and the servo itself. Power-related Pico resets remain possible.

## Second: fixed centre HOLD

After verifying the supply is within the SG90's rating, flash
[mtk3pico_smp0_uart_test_servo_hold.uf2](../build_make/mtk3pico_smp0_uart_test_servo_hold.uf2).

- GP7: **two short blinks, pause, repeat**.
- GP12: fixed nominal **50 Hz, 1500 µs** centre pulse.
- Expect one movement toward centre followed by holding; no scan/turn commands.
- Application sensors are not initialized and wheel inputs remain LOW.

If this is steady but the old full image twitched, try the corrected full
diagnostics next. If it twitches here, check power sag, ground, wiring and
mechanical load/servo condition. The old IMU startup/scan path is not executed
in this image. Actual PWM timing still requires a scope/logic analyser if those
checks do not resolve it.

**Three blinks** in either isolated image means the PWM peripheral failed to
leave reset within the timeout; the servo remains off. No recognizable pattern
does not identify a cause: check image selection, board model, GP7 LED and power.
Both images feed their own watchdog while their main loop is running.

Electrical centre may not face the mounted sonar forward. Once stable, reposition
the horn/linkage with power off; do not force a powered shaft to another position.

## Third: updated full diagnostics

Flash [mtk3pico_smp0_uart_test.uf2](../build_make/mtk3pico_smp0_uart_test.uf2).
GP7 reports startup status, with a longer pause between blink groups:

- **One:** startup completed. This does not mean calibrated or runtime-fault-free.
- **Two:** task/cyclic startup failed; motors and servo stay off.
- **Three:** board/peripheral or mutex initialization failed; motors and servo
  stay off. This includes the IMU initialization failure path.

This indicator is not the full runtime fault mask. The default calibration
inhibit remains active. Report what happens with OFF and HOLD and the GP7 pattern.

## Corrected software paths

Board initialization failure previously skipped creating the safety task but
still started the watchdog. That creates an unattended watchdog/restart path
which can reissue servo centring on each reboot. Safety creation is now attempted
independently; the watchdog starts only if safety started. Startup failure keeps
the servo off, and an invalid mutex is not locked for diagnostic printing.

Servo PWM was enabled before its divider/centre compare were configured. It is
now configured while disabled, with GPIO low and counter reset, then enabled
only after peripheral initialization succeeds. Later task-startup failure also
turns it off. Out-of-range angle requests are clamped to 180° (2000 µs); this
is hardening, not evidence for the observed twitch, since normal scan angles
were already bounded.

```sh
./tools/build.sh servo-off
./tools/build.sh servo-hold
./tools/build.sh diagnostics
```

Isolation profiles require TEST=1, radio off and UART console; the helpers supply
those flags. They have separate names and include their mode in the object-cache
guard. See [the test report](../testing/servo-startup-2026-10-30/REPORT.md).
