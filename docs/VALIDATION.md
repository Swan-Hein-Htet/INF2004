# Validation record

Latest revalidation: [30 October test report](../testing/revalidation-2026-10-30/REPORT.md).
That run adds navigation regression tests, fixes two issues, and rebuilds all
three profiles. Its image sizes supersede the original packaging sizes below.
The bench qualification checklist below remains unexecuted.

Performed in the cloud workspace on 30 October 2026. No robot, oscilloscope,
logic analyser, WiFi access point or MQTT broker was attached.

## Executed checks

| Check | Result |
| --- | --- |
| Host C11 regressions with ASan + UBSan | passed, six test groups |
| A–Z pure and streaming decoder, 23 narrow widths, both orientations | passed |
| Independent Code 39 oracle, 7,280 jittered symbols | passed |
| 10,000 deterministic unstructured noise frames | all rejected |
| 2,000 C encoder / independent Python decoder comparisons | passed |
| Full bypass goal sequence and original-heading restoration | passed in model |
| Pending-command suspension across bypass, false-line rejection | passed in model |
| Near-obstacle STOP and long-black-marker priority | passed in model |
| Offline ARM ELF + UF2 | built |
| Diagnostic ARM ELF + UF2 | built |
| Network ARM ELF + UF2 with placeholder settings | built |
| Compiler warnings in the final three build logs | none |

Compiler: Arm GNU GCC 14.2.1; native GCC 14; Pico C SDK 2.2.0; supplied
micro T-Kernel template. Application compilation enables `-Wall -Wextra
-Wshadow -Werror`; native algorithm checks additionally enable `-Wconversion`.
The entire inherited kernel is not compiled under new application warning rules.
LeakSanitizer is disabled because this host uses ptrace; ASan/UBSan remain active.

Recorded `arm-none-eabi-size` output before final packaging:

| Image | text | data | bss |
| --- | ---: | ---: | ---: |
| offline | 65,043 | 1,384 | 12,516 |
| diagnostics | 65,214 | 1,384 | 12,524 |
| network | 335,662 | 388 | 72,344 |

These are ELF section sizes, not measured live stack/heap high-water marks or
proof of whole-program RAM savings. The kernel allocates task stacks dynamically.
No pre-change car baseline was available; no artificial baseline tag was created.

## Required bench qualification (not executed)

1. Identify motor connector, encoder output voltage and real IMU variant.
   Measure all supply rails with motors/servo under peak load. Verify ECHO
   divider and GP28 jumper before connecting the Pico.
2. Sweep each ADC sensor on white/black. Check raw signals and hysteresis with
   motors active. Measure encoder counts per metre and all chassis dimensions.
3. Tune motor direction, PID and turn scale on a stand, then measure straight
   distance error over 2 m (≤5%) and 90° turns (±5°), including both directions.
4. Capture T2 sample timing for at least one minute and network/line operation
   for an hour. Verify 1 kHz ±5%, PID 20 ms ±2 ms, terrain 100 Hz ±2 Hz,
   safety 20 ms ±2 ms, ISR <20 µs, mutex hold <100 µs and core utilisation <60%.
   Record maxima, missed releases and stack/heap headroom, not only averages.
5. Trigger GP21 and MQTT STOP during every bypass/turn phase. Measure time from
   input/receipt to both motor outputs low; require <100 ms. Sample both PWM
   compares at least ten times after shutdown; all must remain zero. Test RESET
   with E-stop held, with another critical fault latched, and after safe release.
   Stop safety task to verify watchdog reset returns to disarmed IDLE/FAULT.
6. Run each of all 26 physical symbols at 100,150,200,250,300,350 mm/s in both
   directions. Require ≥95% correct over ≥20 passes per symbol, no false turns.
   Include acceleration, motor interference, print variation, extra marks,
   incomplete frames and inadequate quiet zones. Log every rejection.
7. Measure barcode-to-junction spacing, line-sensor axle offset and junction/end
   black lengths. Validate 500 mm pending budget, D immediate U-turn, C straight,
   long marker precedence, missed junction expiry and suspended bypass budget.
8. Push then drive over measured humps at 150 and 250 mm/s. Compare maximum peak
   to physical height (±5 mm); inspect pitch gating, wheel slip and bias drift.
9. Scan known objects at several ranges and headings; measure width error
   (±10%). Measure minimum swept-body clearance (≥50 mm), including sensor/body
   offsets. Test unequal width/depth, two obstacles, both blocked flanks,
   absorbent/angled targets and unexpected close obstacles mid-turn.
10. Validate reacquisition against every real barcode bar, junction branch and
    false line. Verify expected crossing location and width; do not assume width
    alone identifies a unique original branch. Measure complete bypass time.
    The current conservative plan is expected to exceed the <5 s target.
11. Configure a real broker/AP. Compare captured raw packets with the Python
    decoder and reject altered versions. Test one-byte command framing,
    fragmentation, duplicates, reconnects and loss of network during a run.
    Confirm the robot completes without network once its hardware is qualified.

Do not convert these unexecuted checks into “passed” entries without attaching
measurements. `testing/` and `videos/` from the template are available for evidence.
