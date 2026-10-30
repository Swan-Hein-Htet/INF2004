# Assumed-value autonomous demo and guided calibration

The owner explicitly requested plausible values for a prompt autonomous trial.
`tools/build.sh assumed-demo` enables a separate `APP_ASSUMED_DEMO=1` build with
the exact unmeasured settings documented in `docs/DEMO_START.md`. Ordinary
profiles retain the inhibited factory configuration. The demo retains runtime
fault gating, a physical start button and E-stop, limits PID motor duty to 30%,
and indicates idle/activity/faults on GP7 without a serial console.

Validation completed:

- The Arm cross-build for `mtk3pico_smp0_uart_assumed_demo.uf2` succeeded.
- The configuration object was extracted from the actual UF2 using its ELF
  symbol address and checked: scale .50mm/count; track 140mm; body 180x240mm;
  depth 150mm; cruise target 100mm/s; signs all +1; black/white 3500/500; gate on.
- `tools/check.sh` passed: native sanitizer suites, MQTT checks, RTOS/MMIO
  startup tests, both independent barcode oracles, protocol checks, and the
  guided-calibration suite.
- Demo-specific native tests use the production status code and actual demo
  configuration. They verify the enabled gate, assumed parameters, actual 30%
  duty limit, all seven blink codes, idle blink, active LED and finished LED.
- Additional bench startup tests exercise production code with controlled
  MMIO/time: alternating single-wheel pulses, <=300ms plus polling latency,
  held-at-boot inhibition, release interlock, early release, E-stop and failed
  sensor initialization.
- The guided calibrator tests cover counter wrap, sign/scale derivation,
  inconsistent/insufficient counts, invalid/unverified settings, cross-block
  UF2 configuration replacement, malformed/foreign/hash-mismatched UF2s and
  acceptance of a packed object by the actual C configuration validator.

The guided package also contains an inhibited autonomous template and USB
calibration/bench images. It does not label guessed parameters as measured.
Its measurements produce a separate personalized image. The explicitly assumed
demo is delivered independently and needs no calibration wizard.

Physical USB enumeration, installed motor/encoder/IR polarity, wheel scale,
physical speed, steering, turns, barcode captures, terrain measurement, obstacle
clearance and stop latency remain unverified on the actual car. A 30% duty cap
does not establish a physical speed bound or adequate starting torque. No claim
of complete project qualification is made.
