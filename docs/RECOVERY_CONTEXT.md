# Recovered context for troubleshooting

Prepared 30 October 2026 from the surviving workspace and the visible recovery
conversation. This is a reconstructed handover, not a recovered transcript of
the original build chat. The original prompts, intermediate discussion, and
unrecorded decisions are unavailable to the assistant in this session.

## Start here in a future session

> This is the recovered INF2004 autonomous robotic car project for Raspberry Pi
> Pico W on Cytron Robo Pico, using micro T-Kernel 3.0. Read
> docs/RECOVERY_CONTEXT.md, README.md, docs/WIRING.md, docs/VALIDATION.md and the
> relevant subsystem before troubleshooting. Read docs/recovered-context for
> original requirements and review evidence. Distinguish recorded test results
> from new measurements. Motors are deliberately inhibited until calibration;
> do not bypass that condition to make the robot move. Hardware timing,
> calibration and WiFi/MQTT operation have not been qualified. Preserve the
> existing five subsystem layout and explain any departure from the documented
> design. Ask for the actual symptom, firmware profile, logs and hardware
> measurements needed to investigate it.

## What survived and where

| Evidence | Location |
| --- | --- |
| Architecture, algorithms, calibration, protocol and deviations | [README.md](../README.md) |
| Electrical connections and component assumptions | [WIRING.md](WIRING.md) |
| Historical verification results and unexecuted bench checks | [VALIDATION.md](VALIDATION.md) |
| Subsystem ownership | [TEAM.md](../TEAM.md) |
| Requirements, review and datasheet extracted text | [reference-text](recovered-context/reference-text) |
| Original uploaded filenames, sizes and SHA-256 hashes | [attachment inventory](recovered-context/original-attachment-inventory.json) |
| Original packaged project and three build logs | [recovered-artifacts](../recovered-artifacts) |
| Host regression code and fixtures | [tests](../tests) |
| Evidence templates and subsystem test logs | [testing](../testing) |

Reference text was copied from the surviving workspace. PDF/DOCX extraction may
omit diagrams, tables, formatting or equations; consult the original document
before relying on a detail that depends on those. `tribunal.txt` is a supplied
architecture/tribunal document, not the archived Codex chat. The original binary
attachments themselves are not included in this context addition; the inventory
identifies them for matching against the user's originals. Original local copies
were found under `/workspace/attachments` during recovery.

The original firmware ZIP is retained unchanged. It predates this handover and
the reference-text addition. Download the recovery branch ZIP for the latest
complete repository contents.

## Visible recovery conversation

The user said they accidentally archived the Codex build and requested the files
again. The restored workspace contained `/workspace/INF2004`,
`INF2004-firmware.zip`, build logs, original attachments and extracted references.
The original archive contained 655 entries and passed its ZIP integrity check.

Workspace links did not download in the user's browser. A subsequent sandbox
link also failed with "could not download artifact". The user then explicitly
authorized uploading the recovered files to GitHub: "upload and go ahead, write
everything there". The project was committed and pushed to
`Swan-Hein-Htet/INF2004`, branch `recovered-build-2026-10-30`. The initial recovery
commit is `1d413c962af20035b5a0d0b805d50c367dfa6780`, containing 659 files:
source/documentation, three UF2 images, the original firmware ZIP and build logs.
The remote branch hash was verified against the local commit after pushing.
The user then requested original chat context for future troubleshooting, leading
to this reconstruction. No original build transcript has been retrieved.

## Technical state at recovery

- Hardware: Raspberry Pi Pico W, Cytron Robo Pico, quadrature wheel encoders,
  three analogue IR inputs, HC-SR04 plus scanner servo, and GY-511 LSM303DLHC
  accelerometer at address 0x19. LSM303D and MPU-6050 are not drop-in substitutes.
- Core 0 only. Kernel tick 1 ms. One GPIO interrupt dispatcher handles encoders,
  E-stop and ultrasonic echo. The network task alone owns CYW43/lwIP/MQTT.
- Subsystem 1 owns mission/protocol; 2 board/motion/PID; 3 line/barcode;
  4 terrain; 5 obstacle scan. T6 owns mission goals; T4 writes running PWM;
  safety and the E-stop interrupt can force PWM off.
- Barcode: framed Code 39 A–Z, both orientations. A/B/C select next-junction
  left/right/straight, D immediate U-turn; E–Z have no navigation commands.
  Pending junction commands expire after 500 mm of line-follow travel and freeze
  during bypass/turn/recovery. Long black marker takes priority over a turn.
- Telemetry version 2 has 202 meaningful bits in 26 bytes, serialized LSB-first,
  with six unused high bits zero. Use `tools/decode_telemetry.py`, not native
  struct layout or the older tribunal bit count.
- `app_program/robot_config.c` deliberately has zero geometry/encoder scale and
  `b_calibrated = false`. The supplied images cannot drive until calibrated and
  rebuilt. Diagnostic firmware intentionally inhibits motors as well.
- No robot, timing instrumentation, WiFi AP or broker was attached for the
  recorded validation. Do not treat host simulation or successful cross-builds
  as proof of physical behaviour.

## Build and reproduce

Prerequisites and SDK setup are in README.md. Recorded tools were Arm GNU GCC
14.2.1, native GCC 14 and Pico SDK 2.2.0, pinned to
`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`.

```sh
./tools/check.sh
./tools/build.sh offline
./tools/build.sh diagnostics
# After installing the SDK and creating local config/wifi_secrets.h:
./tools/build.sh network
```

The original cloud workspace used:

```sh
export PATH=/workspace/toolchain/root/usr/bin:$PATH
export PICO_SDK_PATH=/workspace/toolchain/pico-sdk
```

Those paths are specific to that workspace. SDK/toolchain binaries are not
bundled in the recovered repository. Keep real WiFi credentials in the ignored
`config/wifi_secrets.h`, using `config/wifi_secrets.example.h` as the template.

UF2 images in `build_make/` are `mtk3pico_smp0_uart.uf2` (offline),
`mtk3pico_smp0_uart_test.uf2` (diagnostics), and
`mtk3pico_smp0_uart_wifi_lwip_mqtt.uf2` (network, placeholder settings).
Hold BOOTSEL while connecting USB and copy the selected image to RPI-RP2.
The console uses GP0 TX / GP1 RX, 115200 baud, 3.3 V logic.

## Historical results and remaining limitations

VALIDATION.md records six host C11 sanitizer test groups passing, 7,280 jittered
barcode symbols passing, 10,000 noise frames rejected, 2,000 protocol comparisons
passing, and all three ARM profiles building without compiler warnings. These
are historical records, not tests rerun as part of this context recovery. The
three retained build logs show successful ELF/UF2 generation and section sizes.

Required future evidence includes encoder and chassis measurements, motor signs,
IR thresholds, actual IMU identity/orientation, power and voltage checks, timing
and stopping latency, physical barcode reliability, terrain height accuracy,
obstacle dimensions/clearance and AP/broker operation. Follow the full bench
checklist in VALIDATION.md.

Known constraints: the conservative bypass is expected to exceed the <5 s target;
front sonar cannot prove hidden depth or all side/rear clearance; no echo means
unknown rather than clear space; reacquisition cannot identify identical branches
with certainty. Battery monitoring is unavailable while GP28 reads barcodes.
MQTT is plaintext with no cryptographic command authentication. Recorded physical
accuracy, latency and timing targets remain unverified.

## First troubleshooting evidence to collect

1. Exact commit, firmware profile/image, host OS, compiler/SDK versions and full
   build output if compilation fails.
2. UART startup/diagnostic output, state and fault bits, and the action that
   triggered the symptom. See README.md for fault bit meanings and reset rules.
3. Actual wiring and component model numbers, measured supply/logic voltages,
   calibration values, wheel direction/counts and IR raw ranges for hardware issues.
4. For network issues: local configuration with secrets removed, broker logs,
   topics and raw packet hex for the independent decoder.
5. For timing/motion claims: measured traces or trial logs; keep them separate
   from host model results and update VALIDATION.md only with supporting evidence.

The recovery itself changed packaging and documentation, not firmware behaviour.

## Subsequent wiring clarification

The owner later confirmed **ST188** IR sensors, an **SG90** servo and the motor
connector marking **KH2.0-6P**, then supplied
[PicoCar.pptx](recovered-context/picocar/PicoCar.pptx) as context for editing the
wiring guide. This supplemental deck and relevant embedded reference images
are preserved in `docs/recovered-context/picocar/`.

Its slide 2 instead labels the motor connector **PH2.0-6P** and assigns
1 encoder GND, 2 encoder A, 3 encoder B, 4 encoder supply, 5 motor +,
6 motor −. The KH/PH marking difference and actual hardware ratings remain
unverified. Slide 4 shows IR contacts VCC/GND/D0/A0; use A0 for this firmware.
Slide 5 distinguishes the IMU VIN input from its separate 3.3 pad. Slide 7
shows the Robo Pico carrier's GPIO breakout female headers, which provide
GP19 without soldering directly to the Pico W.

The deck's generic GPIO choices do not override firmware assignments, and its
HC-SR04 3.3 V supply annotation conflicts with the supplied 5 V module datasheet.
WIRING.md retains the 5 V supply and ECHO divider, and now explains these
differences. No firmware changes or new hardware qualification were performed.

## Subsequent code testing

The owner then supplied Brief Package.zip and Datasheets.zip and requested code
checking and testing. Both files matched the original inventory's SHA-256
hashes. Automated revalidation found and fixed two navigation issues: isolated
both-black pulses accumulating into a junction, and ordinary line reacquisition
remaining stuck in bypass-style recovery. Ordinary recovery now resumes line
following within its distance/time budget; bypass recovery retains its separate
crossing-width and heading-restoration behaviour.

See [the test report](../testing/revalidation-2026-10-30/REPORT.md) for executed
checks, logs, fresh firmware builds and limitations. This later step **does**
change firmware behaviour. Use the current `build_make/` UF2s or rebuild the
current sources; the original ZIP in `recovered-artifacts/` predates the fixes.

The owner subsequently requested visual wiring instructions. WIRING.md now
uses seven labelled diagrams instead of tables, with a printable A4-landscape
PDF at `docs/wiring/WIRING.pdf` and SVG/PNG sheets beside it. The newly uploaded
Datasheets.zip again matched the original hash. No firmware assignments changed.

The owner requested another testing pass. The [extended test report](../testing/extended-2026-10-30/REPORT.md)
records exhaustive motion/command tests, long randomized sensor/navigation runs,
an independent streaming barcode oracle, and tests of the production MQTT callback
unit using host stubs. Three further bugs were reproduced and fixed: non-finite
odometry was not rejected by navigation; a zero-speed motion-library request
could apply heading correction; END could mask a critical FAULT mode (while
wheel targets remained zero). All profiles were rebuilt. These host tests do not
establish physical performance, real RTOS integration or live network operation.

Further wiring clarification: GPIO breakout means the carrier's individual
GP-labelled female headers. Grove 4 has two different signal contacts, GP16 and
GP17, assigned to the two encoder A outputs. A new sheet 05B also shows the
no-harness option: leave Grove 4 empty and use GPIO header jumpers for GP16,
GP13, GP17 and GP19, with separate distribution of encoder power/common ground.
The visual guide PDF now contains eight pages. No firmware changes were needed.

The owner later reported continuous SG90 twitching on battery power, confirmed
the GP12 port location with a photo, and could not obtain UART output. The
[startup investigation](../testing/servo-startup-2026-10-30/REPORT.md) reproduced
a missing safety-task/watchdog-feed path on failed board initialization and
servo PWM being enabled before configuration. These paths were corrected;
servo-off/hold isolation images and GP7 startup/mode blink codes were added.
See [the no-serial procedure](SERVO_TROUBLESHOOTING.md). Battery voltage and the
actual physical cause remain unconfirmed; neither software tests nor these
images establish power quality or hardware qualification.
