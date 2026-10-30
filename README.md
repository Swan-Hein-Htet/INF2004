# INF2004 autonomous robotic car

For future troubleshooting after the archived build, start with
[the recovered context and handover](docs/RECOVERY_CONTEXT.md). It identifies
surviving evidence, original reference text, and gaps in the available chat history.

C11 firmware for Raspberry Pi Pico W on Cytron Robo Pico, using the supplied
micro T-Kernel 3.0 template and its Pico C SDK / CYW43 / lwIP integration.
The original kernel, BSP, build tree and five subsystem folders are retained.
Tutorial LED/producer/consumer tasks have been replaced with car firmware.

**This is compiled and host-tested firmware, not hardware-qualified firmware.**
The supplied documents leave encoder scale, dimensions, motor polarity, sensor
calibration, mounting, servo speed and obstacle depth unmeasured. Motors are
therefore inhibited by default. Do not set `b_calibrated` until the measurements
below are complete. No claim of zero bugs or physical accuracy is made.

## Build, test and flash

Prerequisites: GNU Make, host C compiler, Python 3, Arm GNU Toolchain including
newlib and libm. On a conventional Debian development machine:

```sh
sudo apt-get install build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi
./tests/run.sh
python3 tests/test_barcode_oracle.py
python3 tests/test_protocol.py
./tools/build.sh offline
./tools/build.sh diagnostics
```

In this prepared cloud workspace, use:

```sh
export PATH=/workspace/toolchain/root/usr/bin:$PATH
export PICO_SDK_PATH=/workspace/toolchain/pico-sdk
```

SDK 2.2.0 is pinned at `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`.
To prepare a different machine for the network build:

```sh
git clone --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git ../pico-sdk
git -C ../pico-sdk submodule update --init lib/cyw43-driver lib/lwip lib/tinyusb
export PICO_SDK_PATH="$(cd ../pico-sdk && pwd)"
cp config/wifi_secrets.example.h config/wifi_secrets.h
# Edit the ignored wifi_secrets.h locally, then:
./tools/build.sh network
```

SSID, password, broker IP and client ID are local configuration; never commit
real credentials. The cloud copy contains only the example placeholders. Broker
connectivity and real WiFi were not exercised here. MQTT is plaintext on port
1883 and must be used only on a trusted isolated lab network. Do not expose its
command topic to untrusted publishers. Use a unique client ID per car.

Outputs in `build_make/`:

| Profile | UF2 |
| --- | --- |
| offline | `mtk3pico_smp0_uart.uf2` |
| diagnostics | `mtk3pico_smp0_uart_test.uf2` |
| network | `mtk3pico_smp0_uart_wifi_lwip_mqtt.uf2` |

Hold BOOTSEL while plugging in USB, then copy the chosen UF2 to RPI-RP2.
UART console is GP0 TX / GP1 RX, 115200 baud, 3.3 V logic. Use a USB/UART
adapter with its RX on GP0 and common ground; never use RS-232 voltages.
All build helpers use single-core mode; SMP builds are rejected explicitly.
Default `make` now selects single-core, UART, radio off. Diagnostic builds
**always inhibit motors** and print raw ADC readings, encoder counters, faults
and missed sample releases every 200 ms. TEST=1 is a calibration logger, not a
claim that on-target self-tests ran. UF2s supplied here retain the calibration
inhibit and cannot drive the car until rebuilt with measured configuration.

For SG90 twitching without UART access, use the [no-serial servo troubleshooting guide](docs/SERVO_TROUBLESHOOTING.md).
`tools/build.sh servo-off` and `servo-hold` produce isolated images that hold
wheel outputs low and omit application sensor initialization. The carrier GP7
status LED shows their mode; leave GP7's Grove signal disconnected. Full
diagnostics also provides startup blink codes. See the [startup test report](testing/servo-startup-2026-10-30/REPORT.md)
for the watchdog/init and PWM-order fixes and their verification limits.

## Layout and ownership

| Folder | Responsibilities |
| --- | --- |
| `app_program/subsystem1` | mission, barcode command lifetime, binary protocol |
| `app_program/subsystem2` | motor/encoder/ADC/I2C/servo adapter, PID, motion API |
| `app_program/subsystem3` | line estimate, streaming and pure Code 39 decoder |
| `app_program/subsystem4` | accelerometer pitch, bias and hump integration |
| `app_program/subsystem5` | ultrasonic scan envelope and clearance estimates |
| `app_program/app_main.c` | RTOS tasks, state snapshots, safety, startup |
| `app_program/robot_config.c` | measured chassis and calibration parameters |
| `tests` | sanitizer regressions, independent barcode and telemetry checks |
| `tools/decode_telemetry.py` | standalone binary telemetry decoder |

Run time task priorities (smaller is higher): initial task 1; safety 2;
line/barcode 5; terrain 6; wheel control 7; scanner 9; vehicle supervisor 10;
CYW43/MQTT owner 12. USB's inherited priority is 3, but timed builds use UART.
No second application network task accesses lwIP. WiFi loss is noncritical.
Six car tasks plus the existing network owner form the seven-task network
profile. Offline builds omit the network task. Diagnostic logging uses the
initial task additionally, so its timing must not be used for qualification.

Kernel tick is 1 ms. A 1 ms cyclic handler only wakes T2. T2 detects queued
wakeups and delayed sampling, discards timing confidence through a latched
fault, and never catches up by fabricating samples. Safety and PID run at
20 ms; terrain and supervisor at 10 ms, using absolute timer deadlines rather
than accumulating work time into relative sleeps. Actual jitter, WCET, CPU
load and mutex hold time still need logic-analyser measurements.

The state snapshot is protected by a priority-inheritance mutex; network
formatting, maths and publishing happen after copying and releasing it.
Encoder counts and echo capture are free-running ISR-owned aligned words.
The fault mask and command bits use interrupt-masked read/modify/write helpers.
These rules are valid on core 0 only. The GPIO interrupt has one dispatcher
for both encoders, echo and E-stop. It uses the kernel vector API rather than
installing a conflicting Pico SDK vector/callback. The SDK remains the template's
radio/network backend; peripheral register access follows the supplied BSP.

## Calibration and first use

See [WIRING.md](docs/WIRING.md) before connecting power.

1. Flash diagnostics with wheels raised and motor supply disconnected. Check
   every GPIO and supply against the visual wiring guide. Record the physical IMU
   marking. This driver supports **LSM303DLHC accelerometer at 0x19**, not
   LSM303D or MPU-6050. A device responding at another address is not compatible.
2. Sweep each IR sensor over white and black at its final height. Record
   `black`, `white`, and `b_black_low` separately for all three channels in
   `robot_config.c`. Span must be at least 200 ADC counts. A 40-count barcode
   hysteresis band rejects comparator chatter. No automatic threshold adaptation
   runs during a barcode. Mount so a centred line is observable by at least one
   line sensor; both-white is treated as lost, not silently as centred.
3. Roll the car over a measured distance at least 1 m in both directions, five
   trials. Use signed differences of wrapping encoder counts. Set
   `wheel_mm_per_edge = measured_mm / abs(count_delta)`. Measure both wheels;
   the current common scale requires matched effective diameters. Do not reuse
   the rejected 20-slot / 5.1 mm assumption. Verify forward counts increase,
   correcting `encoder_sign` separately per wheel.
4. Measure track width between wheel contact centres, body width including wheel
   protrusions, and overall body length. Populate all three dimensions in mm.
   Set `obstacle_depth_mm` to the **measured maximum depth permitted on your
   course**, with margin. A frontal scan cannot measure hidden depth. Objects
   beyond this bound invalidate the bypass plan; do not run on such a course.
5. Verify motor signs with wheels raised using an instrumented bench build; the
   diagnostic image intentionally cannot drive motors. Both wheel targets are
   signed. Calibrate turn scale through measured 90°/180° turns and tune PID at
   low speed before ground trials. Initial gains (P=.002, I=.003, D=0) and .8
   maximum duty are conservative starting values, not tuned results. PID uses
   measured 20 ms encoder differences; confirm resolution is sufficient before
   tuning. There is no claimed 100 ms speed-window calibration.
6. Mount IMU X forward, Y left, Z up. With power on and stationary in IDLE, allow
   at least 2 s for 200 accepted bias samples. The sensor runs 100 Hz, ±2 g,
   high-resolution, block-data-update; each left-justified signed sample is
   divided by 16000 to obtain g. Pitch is gated on 0.9–1.1 g norm and low-pass
   filtered. This is not the Week 6 nonexistent gyro complementary filter or a
   44 Hz gyro filter. Height integrates sin(pitch) × centre travel, not vertical
   acceleration. Impacts above 2.5 g norm latch a fault. No magnetometer heading
   is used; turn rate is kinematic wheel differential. Confirm mounting signs
   by pushing uphill before using height results.
7. Centre the servo at 90°, with 30° looking right and 150° looking left. Current
   pulses are 1000–2000 µs at 50 Hz. Check physical limits first. Confirm actual
   settling times exceed the programmed 40 ms + 3 ms/degree budget; increase it
   if necessary. Validate ultrasonic distances at 100, 200, 400 and 1000 mm with
   motors powered and loaded. Keep a known target in range: **no echo is unknown,
   not clear space**, and repeated no-echo readings stop the car.
8. Only after these checks set `b_calibrated = true`, rebuild offline, flash, and
   start with an unobstructed test area. GP20 held for 50 ms requests START;
   releasing and pressing it again in a STOP fault requests RESET. GP21 is
   E-stop. Reset returns to IDLE, never starts motion automatically. Use the
   hardware supply switch as an independent emergency cut-off during trials.

No battery-voltage input is available while GP28 reads barcodes. Use a properly
protected battery and verify voltage under motor/servo load; this firmware does
not claim brownout prediction or battery monitoring.

## Barcode and line behaviour

Both the pure 29-element decoder and the 1 kHz streaming reader accept framed
`*A*` through `*Z*`, in either direction. Each character has nine alternating
bar/space elements with exactly three wide elements. Guards, character gaps,
width populations, quiet zones and saturation are checked. Widths are uint8_t
milliseconds, with a 254 ms run timeout; saturation rejects a frame. Nominal
wide:narrow ratios accepted are 2:1–4:1, with within-group noise limits.
The entire symbol is never inferred from a prefix. A trailing mark before the
quiet-zone deadline invalidates the pending frame. A line departure aborts
capture. Quantisation is 1 ms; noisy samples are not interpolated.

The streaming reader requires at least 100 ms white before and after a symbol.
At 350 mm/s this is 35 mm, deliberately more conservative than the 10 mm
minimum in Week 6. Verify the printed course provides it; otherwise tune the
quiet-zone criterion using measured encoder travel, not an arbitrary shorter
constant. A fast or accelerating pass that violates the width tests is rejected.
Cruise is clamped to 200 mm/s; terrain excursions reduce it to 150 mm/s.

A/B/C latch left/right/straight for the next junction, expiring after 500 mm of
line-follow travel. D requests an immediate encoder U-turn. E–Z are decoded
and reported, but have no invented navigation meanings. Pending distance is
suspended throughout scanning/bypass/recovery/turns; barcode bars seen while off
route never replace it. The tribunal's later appendix contradicts its accepted
freeze/resume decision; freeze/resume is implemented and regression-tested.

A junction is at least three consecutive both-black observations. To distinguish
it from a long end marker, its pending turn is applied **after leaving the
short black run**. More than 60 mm continuously both-black means FINISHED,
including when a turn is pending. This resolves the otherwise impossible demand
to distinguish a junction from an end marker before observing its length. Check
sensor-to-axle offset on the actual course: the post-marker turning point may
need a measured offset, which is not supplied. Junction qualification requires
three consecutive both-black observations; separated noise pulses do not accumulate.
Short line losses creep forward for at most 80 mm / 8 s before faulting. Seeing
the line again within that budget resumes following without a bypass-restoration
turn. Bypass recovery separately retains its crossing-width and heading checks.

## Obstacle behaviour and limitations

An obstacle under 200 mm stops line following. A 13-point scan over 30°–150°
first samples the five coarse angles (30,60,90,120,150), then fills the intervening
10° points. Servo settling depends on movement size. Pings have at least 70 ms
spacing, above the HC-SR04's >60 ms recommendation. Any invalid scan point makes
the profile unusable; normal ranging faults after three consecutive failures.

Reported width applies a 15° beam correction. Planning adds beam uncertainty
back plus body half-width and 75 mm margin. Side-clearance fields are the
minimum measured flank ray ranges, **not certified swept-area clearance**.
The plan chooses the more open flank and rejects insufficient clearance.
It turns out 90°, crosses laterally, turns parallel, advances past the configured
maximum obstacle depth plus body length, turns back, and approaches the original
line. Recovery begins 60 mm before the predicted crossing. Only a 12–26 mm
both-black crossing is accepted as the nominal 18 mm original line; 3/9 mm
barcode bars are rejected. It then turns to the original heading. Every phase
has a bounded timeout. An obstacle closer than 80 mm causes immediate stop,
including during a turn; safety takes precedence over the tribunal's instruction
to finish a turn. Scanner and mission never write PWM directly.

A single front sonar cannot prove rear, side or hidden corridor clearance, nor
identify an original line unambiguously among identical branches. The width
filter and expected crossing reduce that risk but do not eliminate it. There is
no claim of collision-free operation on arbitrary geometry. Reverse/retry is
provided by the motion primitive API but is not automatically selected: without
rear sensing, a failed scan/bypass safely stops. This is a deliberate departure
from FR18's optional reverse-and-reattempt branch.

The 13 pings alone require at least 0.91 s; the programmed settling allowance
adds roughly 1.8 s plus recentering. Travel of three legs at ≤200 mm/s and four
quarter-turns adds several seconds for normal chassis dimensions. **The <5 s
bypass target is not established and this conservative implementation will
usually exceed it.** Clearance and valid measurements were prioritised. ±10%
width, ≥50 mm clearance, ±5 mm hump, ±5° turns, <5% distance error, ±5% sampling,
95% physical barcode success and CPU/ISR/mutex budgets all require the tests
below. None is reported as passed from host simulation.

## Motion API

`subsystem2/motion.h` exposes `motion_pack`, `motion_begin`, and `motion_step`.
The 16-bit request contains command[2:0], argument[14:3], owner[15]. Commands
0–5 are forward mm, backward mm, left degrees, right degrees, set speed mm/s,
and stop. Angles >180°, speed >350, malformed commands and an inactive owner
are rejected. A zero-speed request completes with both wheel targets zero even
if heading changes. `motion_step` is nonblocking and writes signed wheel targets,
returning true on completion/timeout; its caller must distinguish a timeout by
elapsed time. It requires **signed centre position**, not absolute total travel,
for backwards motion. It belongs to the mission owner and is not a cross-task
PWM escape hatch. The integrated autonomous controller uses its own internal
encoder goals; it does not maintain the old two-producer motion queue.

## Faults and recovery

| Bit | Fault | Detection / recovery |
| --- | --- | --- |
| 0 | STOP | GP21 or MQTT STOP; release GP21, explicit RESET, then separate START |
| 1 | stall | target >50 mm/s with no encoder edges for >750 ms; inspect then reboot |
| 2 | encoder | speed >1000 mm/s or non-finite odometry; inspect wiring/scale then reboot |
| 3 | IMU | initialisation failure or three I2C read failures; inspect then reboot |
| 4 | impact | acceleration norm >2.5 g; inspect then reboot |
| 5 | range | invalid scan or three missed forward echoes; inspect then reboot |
| 6 | bypass | clearance/proximity/phase timeout failure; inspect then reboot |
| 7 | network | noncritical; automatically clears on successful MQTT publishing |
| 8 | timing | missed/stale line sample, task deadline or heartbeat fault; reboot |
| 9 | line | bounded recovery exhausted; inspect then reboot |
| 10 | config | uncalibrated/invalid configuration or diagnostic image; rebuild |

Safety runs above the enabled service tasks. It forces both PWM compare words
to zero. Every normal motor compare write masks interrupts and rechecks both the
arming latch and critical faults; the E-stop ISR latches STOP and forces off
immediately. Low/low on Robo Pico is **brake**, per its truth table, not coast.
A 500 ms hardware watchdog (including RP2040-E1 ×2 load correction) recovers if
safety itself hangs. A reboot always starts disarmed and requires START.
Software cannot replace an independent power disconnect. Other faults are
latched for investigation; RESET does not erase a still-unexplained hardware
fault. The initial 750 ms stall threshold must be reduced if mechanical risk
or stopping-distance measurements require it.
Critical faults also take priority over END: the reported mode stays FAULT,
rather than changing to FINISHED. Non-finite navigation distance/yaw requests
an encoder fault, clears transient control requests and keeps wheel targets zero.

## MQTT binary schema

Topics: `p5is20/car/telemetry` QoS0 at 200 ms; `p5is20/car/cmd` QoS1;
`p5is20/car/status` retained online/offline, with offline last will. The network
owner performs subscription, publishing and reconnects, outside the state lock.
A one-byte command has low 2 bits START=0, STOP=1, RESET=2, END=3; high 6 bits
are sequence, modulo 64. Immediate repeated sequence values are ignored.
The sender must advance the sequence and use one command publisher. This
six-bit scheme does not prevent delayed replays across sequence wrap or reboot.
The transport provides no cryptographic command authentication. STOP latency is
measured from the firmware's receipt, not from a dashboard click.

The tribunal's listed fields total **164 bits**, not 156; expanding its barcode
field from three to five bits requires 166. This implementation uses version 2,
**202 meaningful bits in 26 bytes**, adding both flank ranges and the sample
overrun count. Bits are serialized least-significant first across bytes, never
by sending a C struct. Signed fields use two's complement. Remaining six high
bits must be zero. See `tools/decode_telemetry.py` for the exact ordered widths
and a runnable decoder. All 24 fields have been checked against an independent
Python decoder. Whole-mm height, total distance and closest-range fields
saturate at 127, 32767 and 511 respectively; keep raw logs for larger values.
Current pitch and validity are internal; the wire schema does not claim them.

```sh
python3 tools/decode_telemetry.py HEX_PAYLOAD
```

## Software verification and remaining qualification

`tests/run.sh` builds with warnings as errors, conversion/shadow checks and
AddressSanitizer/UndefinedBehaviorSanitizer. LeakSanitizer alone is disabled
because the execution host uses ptrace; the car algorithms allocate no heap.
Regressions cover A–Z framing/streaming, reverse scans, partial/saturated frames,
trailing noise, line state, pending command expiry, long-marker priority,
full bypass goal sequencing, false-line rejection, STOP/reset, motion bounds,
PID saturation, terrain integration, invalid scans and packet bounds.

`test_barcode_oracle.py` uses checked-in golden fixtures generated by the
independent python-barcode 0.16.1 encoder. It checks 7,280 A–Z forward/reverse
symbols with ±10% element jitter and rejects 10,000 deterministic noise frames.
The oracle dependency is not needed to run the test. `test_protocol.py` compares
2,000 randomly varied C-encoded packets against the Python decoder.

All three firmware profiles have been cross-compiled to ELF and UF2 in this
workspace. Network compilation used placeholder settings; it is not a WiFi or
broker connectivity test. See [VALIDATION.md](docs/VALIDATION.md) for actual
results and mandatory bench evidence before autonomous use.
The [extended test report](testing/extended-2026-10-30/REPORT.md) records the latest
exhaustive/randomized tests, streaming barcode oracle, host MQTT callback tests,
three further fixes and rebuilt images. The [earlier revalidation report](testing/revalidation-2026-10-30/REPORT.md)
records the original two navigation fixes. The UF2s in `build_make/` include all these fixes; the ZIP in
`recovered-artifacts/` remains the original, older recovery snapshot.

## Design and coding decisions

Inputs: supplied project brief and barcode/line specification; Week 6 IS20 design;
tutor IS20 comments; Week 8 PostTri1 notes; BARR-C:2018; Robo Pico, HC-SR04,
TCRT5000, LSM303DLHC and LSM303D datasheets. Documents were used as requirements
and design evidence, not instructions to publish, tag, contact people or fabricate
measurements. The old tribunal repository URL was not used to replace this repo.

Changes from the design are explicit above: corrected wiring, actual non-gyro
IMU, 1 ms tick, higher-priority safety, deferred junction decision, all 26 letters,
larger truthful wire format, single mission owner instead of a racy two-sender
queue, measured-depth bound and safe stop instead of blind reverse. No baseline
RAM-saving figure or timing trace is invented; there was no runnable car baseline.

New C uses explicit-width storage, bools, guarded headers, private static helpers,
module prefixes, Allman braces, explicit comparisons and bounded hardware waits.
`.clang-format` captures the layout. Public robot-core functions intentionally
share the `robot_` namespace across subsystem files; matched local headers include
that common interface. BARR-C deviations to review: inherited T-Kernel/SDK/vendor
code follows its upstream conventions; mandatory `usermain`/newlib ABI names
retain prescribed names, including leading underscores for syscall hooks;
C11 static assertions and designated initializers are used; native floating-point
control/calibration state and snapshots are retained instead of pretending that
unmeasured ranges justify lossless minimum-bit packing. Packed wire fields and
motion requests use explicit shifts, not implementation-defined bit-fields.
Shared continuous control state is **not** fully minimum-bit packed as requested
by the tribunal curveball; further quantisation needs measured ranges and timing
validation. Strict whole-tree BARR certification and the tribunal RAM-reduction
assessment are therefore not claimed. Treat those as outstanding compliance work,
not as tests that have passed. Newlib stdio/heap syscalls fail explicitly; console
uses T-Monitor and networking uses the template's kernel memory adapter.

## Upstream attribution

micro T-Kernel 3.0 © Ken Sakamura / TRON Forum, distributed under T-License 2.2.
This RP2040 port derives from `tron-forum/mtk3_bsp`, branch `pico_rp2040`.
Copyright and licence notices in inherited source files are preserved. Pico SDK,
CYW43, lwIP and newlib retain their respective upstream licences; they are
external dependencies rather than project-authored code. The BARR-C document and
course attachments are not redistributed in the source archive.
