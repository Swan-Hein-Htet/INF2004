# Functional bring-up support — 30 October 2026

Starting revision: `d6ee75f0685f72c79c18aa508070039dc56e04e9`.
The owner reports a steady servo with two GP7 blinks and visually responsive IR
and tilt sensors. Numerical calibration readings, chassis/encoder measurements,
battery voltage and physical mission results have not been supplied.

The uploaded Week 6 review matches the original SHA-256. Page 19 marks the
encoder scale assumption open and blocking PID tuning; page 17 leaves body
dimensions unmeasured. These statements cannot populate a calibrated full-run
configuration. The old pin/IMU design is also superseded by later evidence.

## Delivered support

- Added `tools/build.sh diagnostics-usb`, enabling the existing USB CDC console
  with TEST=1 and radio off. Its single UF2 can be flashed through BOOTSEL.
  This enables a direct Pico USB serial interface; physical enumeration on the
  owner's computer/car has not been tested here.
- Diagnostic snapshots now include raw accelerometer milligravity values, latest
  read validity, bias readiness, pitch, front range/validity, line classification,
  signed line error, last accepted barcode, accepted barcode count and rejection
  state. All snapshots use the existing state mutex; formatting occurs after unlock.
- Diagnostic output is lowered to task priority 11, below sampling/control tasks.
  This reduces priority interference from formatting/UART output; no physical
  jitter or throughput guarantee is claimed. Failure to change priority latches
  TIMING and turns motors/servo off.
- Calibration and the default motor inhibit remain unchanged. Isolated servo
  tests still omit sensor/mission startup. No guessed physical constants were enabled.
- [The functional-run guide](../../docs/FUNCTIONAL_RUN.md) distinguishes PDF
  assumptions, data collection, calibrated build/start and physical acceptance tests.

## Verification

`tools/check.sh` passes: existing algorithm/exhaustive/randomized tests,
independent barcode/telemetry checks and real application/board startup host tests.
The host harness also checks the added diagnostic field names and priority 11,
including priority-change failure with servo off. It uses mock RTOS/MMIO and
does not validate actual USB enumeration, sensor data, scheduling or car motion.

Six profiles build with no compiler warnings/errors: UART diagnostics, USB
diagnostics, offline, network, servo-off and servo-hold. Logs, UF2 structural
checks and hashes are recorded here. The existing startup/servo corrections
remain covered. USB diagnostics is a calibration/inspection profile, not a
physical timing qualification or a completed integrated run.

The diagnostic CONFIG fault can hold the mission in FAULT and prevent its
IDLE-only IMU bias calibration. Consequently IMU_READY/PITCH are not a substitute
for raw sensor checks in this image. The calibrated normal build must verify
bias readiness while stationary before starting. This limitation is explicitly
documented in the user guide.
