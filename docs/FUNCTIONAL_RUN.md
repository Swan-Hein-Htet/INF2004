# From servo test to an integrated run

The [guided autonomous package](AUTONOMOUS_QUICKSTART.md) now automates IR capture,
wrapping encoder-count collection, direction checks and UF2 personalization.
Use that path if you want to avoid editing C and rebuilding locally. Chassis
dimensions and maximum permitted obstacle depth still need a ruler measurement.

The servo-HOLD image is identified by its filename and two-blink GP7 pattern.
It sends a fixed centre pulse; it does not run sensors or the mission. A steady
servo there is a useful partial result, not an integrated functional test.
Two blinks in a full diagnostic image instead means startup failure, so always
record which image is flashed.

## What the design review does and does not supply

The newly uploaded Week 6 Design Review IS20 (1).pdf is byte-identical to the
earlier review (SHA-256
`cb5cc07700c5bab0d26b0832c61c7eb75b5d15ed7bbd86fdf33cc0365b59ecdf`).
It supplies requirements, architecture and intended tests. It does not close
the calibration needed for a full-run image:

- PDF page 19, A2: 20 slots/revolution, 65 mm wheels and about 5.1 mm/edge are an
  **open assumption**, explicitly marked as blocking PID tuning. Page 13 says
  slot count and diameter must be measured. Later tribunal evidence rejects
  using that assumed encoder model for the actual six-wire encoder motors.
- PDF page 17: robot width and length remain `[measured robot width]` and
  `[measured robot length]`, still to be measured on the assembled car.
- The review gives a procedure for IR calibration, not the installed sensors'
  black/white ADC measurements. Actual motor/encoder signs and maximum permitted
  obstacle depth also require verification on the assembled car/course.
- The review's MPU-6050, older encoder GPIO mapping and 10 kΩ/20 kΩ ECHO divider
  have been superseded by the documented wiring/LSM303DLHC integration. Do not
  revert the current GPIO map to make it match the old drawing.

The repository's `app_program/robot_config.c` remains intentionally incomplete.
Provide the measured local file or measurements below; no guessed calibration
has been enabled by this update.

## Read data through the Pico USB socket

If the separate UART adapter could not be opened, flash
[mtk3pico_smp0_usb_cdc_test.uf2](../build_make/mtk3pico_smp0_usb_cdc_test.uf2)
using BOOTSEL/RPI-RP2. Then leave a USB **data** cable attached directly to the
Pico W. This image enables the existing USB CDC console; no UART adapter is needed.
Motors stay inhibited. Physical USB enumeration on this car is still unverified.

- Windows/WSL: use Windows Device Manager to find the newly appearing COM port,
  then PuTTY Serial, 115200, 8N1, no flow control. This is the Pico's USB device,
  not the old adapter COM port; its product string may include MTK3/Thread-Metric.
- macOS: run `ls /dev/cu.usbmodem*`, then
  `screen /dev/cu.usbmodemYOUR_DEVICE 115200` using the actual device name.
- Native Ubuntu: look for `/dev/ttyACM*` and use a serial terminal at 115200.

The two repeating output lines include:

```text
ADC=...,...,... ENC=...,... FAULT=... OVERRUN=... LINE=... ERROR=... LAST_BAR=... BAR_COUNT=... REJECT=...
IMU_MG=...,...,... IMU_OK=... IMU_READY=... PITCH_MRAD=... RANGE_MM=... RANGE_OK=...
```

ADC order is left line, right line, barcode. ENC order is left/right wheel.
`IMU_MG` is X/Y/Z acceleration in milligravity units (1000 is approximately 1 g).
`IMU_OK` indicates the latest read succeeded; old raw values are not fresh if it
is zero. `RANGE_OK` must be one before relying on `RANGE_MM`.

`IMU_READY` is algorithm bias readiness, not merely sensor connectivity. In
motor-inhibited diagnostics the CONFIG fault can put the mission into FAULT,
which prevents the IDLE-only bias calibration from completing. Use raw IMU
readings for initial orientation checks; verify bias readiness in the calibrated
operating image while stationary in IDLE. Raw tilt response alone does not
qualify hump-height measurement.

LINE classification and barcode thresholds depend on the configured IR values.
Collect raw black/white readings first. LAST_BAR persists; BAR_COUNT must
increase to establish a new accepted barcode. A stale letter is not a new decode.
One GP7 startup blink means initialization completed, not that every runtime
sensor passed. Two/three startup blinks are explained in
[servo troubleshooting](SERVO_TROUBLESHOOTING.md).

## Minimum measurements needed

```text
Left IR: white=     black=
Right IR: white=    black=
Barcode IR: white=  black=

Roll straight 2000 mm by hand, motor power disconnected:
Left encoder: before=     after=
Right encoder: before=    after=
Repeat five times in each direction; retain raw wrapping counts.

Track width between wheel contact centres=  mm
Overall body width, including wheels=       mm
Overall body length=                       mm
Maximum obstacle depth allowed on course=  mm
Forward motor direction / encoder signs verified=
```

Compute mm/edge from measured distance divided by the magnitude of the signed
wrapping count difference. The current common scale requires both wheels to
agree sufficiently. Complete the README motor-direction/PID bench checks before
enabling motion. Set `b_calibrated` only after actual measurements and checks;
the code's range validation is not proof those values were measured.

## First full run, after configuration

Run `./tools/check.sh`, then `./tools/build.sh offline` with the measured
configuration. Flash `mtk3pico_smp0_uart.uf2`. This is the autonomous image,
replacing diagnostics and isolated servo images. Reconnect motor power with the
board off after direction/power checks, then power on with wheels raised for
the initial readiness check. Keep the car stationary at least two seconds for
IMU bias. Address critical faults before a floor trial.

On a clear test area, use GP20 START; GP21 is E-stop. Start requires valid IMU
readiness and a valid forward range reading as well as valid calibration.
Reset after an E-stop returns to IDLE; a later START is separate. Other latched
faults require investigation/reboot as documented in README.md.

Test the mission incrementally, recording video, trial counts and measurements:

1. Straight line and a simple bend. Verify wheel direction, line tracking and
   measured travel/90° turn accuracy before adding obstacles.
2. Barcodes: A left, B right, C straight at the next junction; D immediate
   U-turn. Check expiry if no junction appears and long end-marker priority.
3. One measured hump, then several. Compare reported maximum peak with physical
   height. A plausible tilt indication alone does not verify the integration.
4. One obstacle: stop, scan, choose a flank, bypass, reacquire the original line
   and restore heading. Measure width estimate, swept-body clearance and elapsed
   time. The current conservative plan is expected to exceed the <5 s target.
5. GP21 stop during straight movement, turns and bypass, with an independent
   power switch available. Measure electrical/mechanical stop response; software
   model tests do not establish physical stop latency.
6. For telemetry, configure a reachable broker and 2.4 GHz WiFi locally, build
   `network`, and flash its `_wifi_lwip_mqtt.uf2`. Subscribe to the documented
   topics and verify version-2 binary packets with `tools/decode_telemetry.py`.
7. Combine the course only after the individual trials are repeatable.

The design's numerical acceptance targets remain tests to perform, not results
obtained from the PDF. See [the bench checklist](VALIDATION.md). No physical
trial or calibration measurement has been fabricated by the cloud build.
