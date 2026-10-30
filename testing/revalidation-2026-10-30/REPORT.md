# Code review and automated revalidation — 30 October 2026

Reviewed the recovered code against the uploaded brief/datasheets and ran the
checks below. Starting repository revision:
`9db68169862026e1c67198f2d67785a2c1389c5f`.
The final results apply to the source changes and rebuilt images in this commit;
their hashes are recorded in `manifest.json`.

**The automated checks pass after two navigation fixes. Physical operation is
not qualified.** No Pico/robot, AP, broker or timing instrumentation was attached.
The unchanged calibration configuration deliberately prevents driving.

## Inputs checked

The uploaded archives were byte-identical to the original supplied packages,
as verified against `docs/recovered-context/original-attachment-inventory.json`:

| Input | SHA-256 |
| --- | --- |
| Brief Package.zip | `85bc9d3af4ae36b11d4df4e004e40e70383c6fbb55bd66a6855fc74e7ad76023` |
| Datasheets.zip | `f30d0ceed4dfe87b305ea5c32b8ee45f26d03311df91b51a5fbcf707e045cd30` |

Used the surviving text extractions, source code, original interface documents
and the recent PicoCar diagrams. The project brief requires line following,
barcode navigation, hump measurement, obstacle profiling/bypass/recovery and
WiFi telemetry, implemented in C with micro T-Kernel on the Pico platform.

Hardware review cross-checked GPIO roles with WIRING.md, HC-SR04 trigger/echo
constraints, LSM303DLHC register configuration (0x57 at 0x20: 100 Hz, axes on;
0x88 at 0x23: block-data-update, high-resolution, ±2 g), the 0x19 accelerometer
address, and the configured 125 MHz system clock used for the servo divider.
This was source/datasheet inspection; no bus or waveform measurements were made.

## Bugs reproduced and fixed

1. **False junction after separate black pulses.** Three isolated both-black
   observations, each separated by normal line readings, accumulated into a
   junction and consumed a pending A turn. The new regression failed on the
   original code (`new-regressions-before-fix.log`). An incomplete black run now
   resets its observation count. Returning to FOLLOW also resets old junction
   state, so observations from an earlier manoeuvre cannot qualify a new turn.
2. **Normal line loss could not resume when the line returned.** The controller
   applied the bypass crossing detector to ordinary line loss, leaving RECOVER
   active even when normal line readings returned. It could eventually fault,
   or apply a stale bypass side when it accepted a crossing. The new regression
   failed on the original code (`line-recovery-before-fix.log`). An explicit
   recovery-purpose flag now separates ordinary reacquisition from bypass
   reacquisition. Ordinary recovery returns to FOLLOW without a turn; bypass
   recovery retains its 12–26 mm crossing filter and original-heading turn.

The 80 mm / 8 s recovery budgets are checked before accepting reacquisition.
Tests cover both budget expirations, including when a line is visible again.
Pending commands remain frozen during recovery. Calibration, wiring assignments,
motor inhibit, STOP behaviour and telemetry format were not relaxed.

## Executed checks

| Check | Result | Evidence / scope |
| --- | --- | --- |
| C11 host regressions, ASan + UBSan, warnings as errors | PASS | `host-tests.log`; nine test functions, ten invocations because bypass runs for each side |
| Code 39 A–Z at 23 widths, both orientations | PASS | Pure decoder and streaming reader; malformed/trailing-noise cases also checked |
| Independent Code 39 fixtures with jitter | PASS | 7,280 forward/reverse symbols; independent Python test invokes the pure C decoder |
| Unstructured barcode noise | PASS | 10,000 deterministic random frames rejected |
| Telemetry C encoder versus Python decoder | PASS | 2,000 randomized packets; bad version/length rejection |
| Junction noise and genuine consecutive observations | PASS | New regression retains a pending turn until a real qualifying run |
| Ordinary line reacquisition | PASS | No previous bypass side, previous left side, previous right side; pending budget preserved; distance/time expiry enforced |
| Full bypass model | PASS | Both left and right route sequences, original heading restored, pending command frozen, narrow false-line rejection |
| Safety/navigation boundaries | PASS | STOP in all eight mission modes, RESET requires later START, <80 mm proximity stop, turn timeout over uint32 clock wrap |
| GCC `-fanalyzer` with warnings as errors | PASS | Six algorithm translation units compiled individually; `static-analysis.log` |
| Offline ARM ELF + UF2 build | PASS | `offline-build.log`; no compiler warnings/errors |
| Diagnostic ARM ELF + UF2 build | PASS | `diagnostics-build.log`; no compiler warnings/errors |
| Network ARM ELF + UF2 build | PASS | `network-build.log`; compile/link only, no AP/broker connection test |
| UF2 structure | PASS | `artifact-checks.log`; all blocks, magic values, unique indices/counts, RP2040 family ID, 256-byte payloads and flash address bounds |

LeakSanitizer is disabled by the existing runner because this host uses ptrace;
AddressSanitizer and UndefinedBehaviorSanitizer remain enabled. Static analysis
covers the six host-testable algorithm units, not the inherited kernel or the
hardware-dependent RTOS/board/network integration. Successful builds establish
compile/link compatibility, not real-time or physical correctness.

## Reproduce

From the repository root, with native GCC, Python 3 and Arm GCC installed:

```sh
./tools/check.sh
export PICO_SDK_PATH=/path/to/pico-sdk
./tools/build.sh offline
./tools/build.sh diagnostics
# Create local config/wifi_secrets.h from the example as described in README.md.
./tools/build.sh network
```

Recorded tools: native GCC 14.2.0; Arm GCC 14.2.1; Pico SDK 2.2.0 at
`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`. This cloud workspace used
`/workspace/toolchain/root/usr/bin` in PATH and
`PICO_SDK_PATH=/workspace/toolchain/pico-sdk`. Firmware profiles share an object
directory and were built sequentially; the existing profile guard discards
objects on each profile change.

To repeat the static analysis, compile each of mission.c, protocol.c, motion.c,
sensing.c, terrain.c and obstacle.c individually with native GCC using
`-std=c11 -O0 -Wall -Wextra -Wconversion -Wshadow -Werror -fanalyzer
-Iapp_program -c`, directing object files to a temporary/build directory.

## Remaining qualification gaps

- Default geometry and encoder scale are zero and `b_calibrated` remains false.
  Diagnostic firmware also deliberately inhibits motors. Measure and calibrate
  using README.md before attempting autonomous movement.
- Actual motor model, KH2.0-6P versus the slide's PH2.0-6P connector, module
  voltage/current limits, harness orientation, GP28 jumper, supplies and ECHO
  divider need physical verification. A slide drawing does not prove these.
- No evidence yet establishes 1 kHz sampling jitter, task/ISR execution times,
  mutex duration, live stack/heap headroom or motor shutdown latency. Host STOP
  tests check requested wheel speeds, not PWM/electrical response.
- Physical barcode success rate, IR lighting/noise tolerance, odometry/turn
  accuracy and hump-height accuracy remain unmeasured. The 100 ms barcode quiet
  windows must be checked against the real printed track and travel speed.
- The conservative scan and bypass are expected to exceed the <5 s design
  target. Front sonar cannot certify hidden obstacle depth or the full swept
  corridor, nor uniquely identify identical line branches.
- WiFi association, MQTT reconnect/command framing in the live network stack,
  actual broker telemetry and long-duration operation were not exercised.

Follow the full [bench checklist](../../docs/VALIDATION.md) and attach measured
evidence. Passing these automated checks is not completion of the physical
mission requirements or a claim of zero remaining bugs.

## Downloads

Use current source files and UF2 images in `build_make/` on the recovery branch.
The original ZIP under `recovered-artifacts/` is retained unchanged for history
and **does not contain these fixes**. The GitHub recovery-branch ZIP contains
the updated project, images, this report and raw test/build logs.
