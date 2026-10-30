# Servo/startup investigation — 30 October 2026

Starting revision: `7a9f36cc28c93f83025cfe5b91258da68b02e949`.
No robot is attached. Battery/servo voltage, physical waveform, plug orientation
and real restart behaviour remain unmeasured. The software defects below are
established; their contribution to the owner's twitching is not confirmed.

## Reproduced defects

1. **Skipped safety task after initialization failure.** The real `usermain()`
   executed against a failing I2C backend skipped safety creation because of
   `b_ready && create`, then started the watchdog. `before-imu-fail.log` records
   the failing assertion that safety must exist. With no safety task to feed
   it, this creates a repeated reboot path, potentially reissuing servo centre.
   The host test does not emulate watchdog hardware or establish actual reset
   frequency on the car.
2. **PWM enabled before configuration.** The MMIO/BSP test checks state at the
   real `board_init()` PWM-enable call. `before-pwm.log` records the missing
   divider configuration. Any transient waveform was not physically measured.
3. **Unbounded public servo input.** `before-servo-range.log` records an
   out-of-range request producing a compare above 2000 µs. Application scan
   inputs were already 30–150°, so this hardening is not a demonstrated symptom cause.

## Changes

Safety-task creation is attempted independently of board initialization.
Watchdog startup requires that safety was created/started. If it cannot start,
the application stays disarmed with servo off rather than starting an unattended
watchdog. Other car tasks remain gated on successful initialization. Diagnostics
can report mutex failure without locking an invalid mutex.

Servo slice 6 is disabled, GP12 driven low, CSR reset, divider set to 125,
counter reset, TOP set to 19999 and compare to 1500. At configured 125 MHz this
is nominal 50 Hz / 1500 µs. Full startup enables servo output only after sensor
initialization succeeds and disables it on subsequent task-startup failure.

Isolated servo-off and servo-hold profiles bypass application sensor/mission
startup and hold GP8–GP11 low. They feed a watchdog and indicate their mode on
GP7. A bounded PWM-reset wait fails with servo off. Full diagnostics also reports
startup using GP7. The GP7 Grove contact must remain disconnected.

## Executed verification

The host harness compiles actual **app_main.c, board.c and robot_config.c**
against controlled RTOS/MMIO/BSP APIs with ASan/UBSan and warnings as errors.
It executes these scenarios, with results in `all-tests.log`:

- Healthy startup; safety exists, watchdog starts and one-blink status is observed.
- IMU/I2C failure; safety still exists and executing its real task through one
  iteration reloads the watchdog; servo low/off and three-blink status.
- Mutex creation failure; no invalid lock, safety still feeds, servo off, three blinks.
- Safety-task creation failure; watchdog not started, servo off, two blinks.
- Safety-task start failure; watchdog not started, servo off, two blinks.
- Terrain-task creation failure; existing safety feeds, servo off, two blinks.
- PWM configuration complete before enabling GP12.
- UINT16_MAX servo request clamped to a 2000 µs compare.
- Each isolation mode: no application task creation or I2C writes, motor compare
  words zero, watchdog reloads, correct GP12 policy and counted blink group.
- Each isolation mode with PWM-reset failure: bounded exit with servo off and
  three-blink indication.
- All prior algorithm, exhaustive/randomized, MQTT callback and independent
  barcode/telemetry tests pass through `tools/check.sh`.
- Five ARM profiles build without compiler warnings/errors: servo-off, servo-hold,
  full diagnostics, offline and network. Build logs are retained with trailing
  whitespace normalized.
- GCC `-fanalyzer` passes for the real application/board units in all three
  test modes, using the host stub headers; see `static-analysis.log`.
- UF2 magic/family/block/address validation and hashes are recorded in
  `artifact-checks.log` and `manifest.json`.

The startup model runs tasks one at a time. It checks control flow, configuration
ordering, output policy, GP7 writes and one real safety-task iteration. It does
not emulate preemption, waveforms, electrical load, USB boot ROM, battery sag,
servo mechanics or brownouts. An actual low/unstable supply or wiring problem
can still cause twitching after these changes. No physical fix is claimed.

## Reproduce

```sh
sh tests/run_startup.sh
./tools/check.sh
./tools/build.sh servo-off
./tools/build.sh servo-hold
./tools/build.sh diagnostics
./tools/build.sh offline
./tools/build.sh network
```

Tools: host GCC 14.2.0, Arm GCC 14.2.1 and Pico SDK commit
`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`. Calibration remains unchanged.
Follow [the no-serial procedure](../../docs/SERVO_TROUBLESHOOTING.md). The five
current UF2s include these changes; older ZIPs/manifests are historical snapshots.
