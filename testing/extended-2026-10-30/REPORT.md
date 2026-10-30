# Extended code tests — 30 October 2026

Starting revision: `fbb3099521fe08259fb85139941c7f874d32e7c3`.

The expanded checks pass after three fixes. Source files, runners and current
firmware hashes are recorded in `manifest.json`. Raw logs and pre-fix assertion
failures are saved beside this report. Build-log trailing whitespace is normalized.

## Additional coverage

| Check | Executed result |
| --- | --- |
| Exhaustive 16-bit motion requests | All 65,536 values for each of two caller owners: **131,072 combinations**, checked for acceptance/rejection against command, argument and ownership rules; accepted jobs stepped at their initial heading and checked for bounded finite targets |
| Command bytes | All **256** values accepted as fresh commands, immediate duplicates and different commands with the same sequence rejected; four sequence wraps checked |
| Invalid command lengths | **7,936** byte/length combinations rejected without changing parser state; null payload also rejected |
| Telemetry buffer capacities | **65** capacities checked with guard bytes and reserved-bit checks |
| Invalid odometry | **48** NaN/+infinity/−infinity distance/yaw cases across all eight modes; START/RESET/END cannot override the fault |
| Zero-speed motion request | Both targets remain zero for **25** heading offsets from −180° to +180° |
| Critical fault versus END | All **10** critical bits in each of **8** modes: **80** combinations; a normal END with only the noncritical network bit still completes |
| Randomized sensor stream | **1,000,000** deterministic ticks through line estimation and barcode streaming; output/state bounds checked with ASan/UBSan |
| Scan profiles | **10,000** randomized profiles checked for valid result bounds, then **10,000** injected invalid-range cases rejected |
| Randomized navigation | **100,000** deterministic mixed-input steps; finite/bounded targets and zero targets for stopped/faulted states checked |
| Independent streaming Code 39 oracle | **7,280** jittered A–Z frames replayed in both directions; **1,456** line-departure, trailing-mark, incomplete-frame and insufficient-leading-quiet cases rejected |
| MQTT production callbacks | Actual `mqtt_utk.c` compiled into a host harness; subscription, retained online/offline status configuration, 200 ms timer requests, publish errors, reconnect deadlines and uint32 clock wrap checked |
| MQTT command framing | All **256** command bytes passed through actual incoming/data callbacks; duplicates, wrong topics, multi-byte packets, empty fragments and unfinished callback sequences checked |
| Decoder malformed fields | Each of the **six** reserved bits and all **five** invalid barcode values rejected, in addition to existing bad length/version tests |
| Existing regressions | Original navigation, sensors, motion, safety, 7,280-symbol pure barcode oracle, 10,000 noise rejections and 2,000 independent telemetry comparisons still pass |
| GCC static analyzer | Six algorithm units and the MQTT adapter compiled with `-fanalyzer` and warnings as errors; adapter analysis uses host stub headers |
| ARM cross-builds | Offline, diagnostics and network ELF/UF2 builds pass with zero compiler warnings/errors; see corresponding build logs |
| UF2 verification | Each generated image checked for magic values, block counts/indices, payload size, flash bounds, unique addresses and RP2040 family ID; see `artifact-checks.log` |

`all-tests.log` is the canonical final automated-suite output. Randomized tests
use fixed seeds for reproducibility. The million streaming ticks test state and
memory bounds; it does not claim a measured physical barcode success rate or
that every possible noise input is rejected. The exhaustive request test is
exhaustive over the request/owner combinations, not every runtime trajectory.

## Three reproduced bugs and fixes

### Non-finite navigation odometry

Navigation did not explicitly reject NaN or infinity in distance/yaw.
`before-invalid_odometry.log` records the failing regression on the starting
revision. Navigation now requests `ROBOT_FAULT_ENCODER`, enters FAULT, zeros
targets, consumes transient START/RESET/END flags and returns before calculating
motion. This makes malformed numeric inputs fail closed; it does not replace
physical encoder calibration or prove that every possible corrupted state is detected.

### Zero speed could apply heading correction

A `MOTION_SPEED` request with argument zero left the library job active. If yaw
then differed from its captured heading, `motion_step` produced opposite signed
wheel targets from heading correction. `before-zero_speed.log` records the
failure. Zero-speed jobs now complete with zero targets. This affects the
reusable motion API; the integrated autonomous mission loop uses its own goals.

### END could hide a critical fault state

A simultaneous END and critical fault first entered FAULT and then overwrote
the mode with FINISHED. Wheel targets remained zero, but the reported mode was
wrong. `before-critical_end.log` records the failure. END now respects critical
fault priority, and its request is consumed without masking FAULT.

The previous junction-noise and line-recovery fixes remain covered by the
original suite. Wiring, calibration values and the default motor inhibit were
not changed by this pass.

## Reproduce

Run from the repository root:

```sh
./tools/check.sh
```

The native runner now builds `test_robot`, `test_extended` and `test_mqtt` with
AddressSanitizer, UndefinedBehaviorSanitizer, conversion/shadow checks and
warnings as errors. LeakSanitizer remains disabled for the ptrace host.
The Python streaming oracle builds a temporary shared C bridge; its expected
letters come from the independent golden fixtures, not the C pattern table.
The C sanitizer stress suite separately exercises the streaming implementation.

To run an extended group individually after building:

```sh
ASAN_OPTIONS=detect_leaks=0 tests/build/test_extended motion
ASAN_OPTIONS=detect_leaks=0 tests/build/test_extended invalid_odometry
ASAN_OPTIONS=detect_leaks=0 tests/build/test_extended zero_speed
ASAN_OPTIONS=detect_leaks=0 tests/build/test_extended critical_end
```

Other group names are `commands`, `sensors` and `navigation`.
Baseline group exit statuses are in `baseline-results.json`; negative exit
status −6 indicates the expected assertion abort before the fixes.

For firmware, set PATH to the Arm compiler and PICO_SDK_PATH as described in
README.md, then run `tools/build.sh offline`, `diagnostics` and `network`
sequentially. Native GCC is 14.2.0; Arm GCC is 14.2.1; Pico SDK is pinned at
`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`. Network cross-compilation uses the
local configuration header and does not establish connectivity.

## Limits

The MQTT harness replaces lwIP calls, timers and robot callbacks with controlled
host stubs. No sockets, WiFi association, actual broker, real lwIP parsing or
network latency are exercised. It checks the application adapter logic only;
the production network profile separately builds against the real SDK/lwIP headers.

No robot, Pico or measurement instruments were attached. Host navigation tests
check requested wheel speeds; they do not measure PWM or stopping distance.
RTOS scheduling, ADC/I2C waveforms, encoder voltage/polarity, motor signs, power
under load, sampling jitter, stack headroom and physical mission reliability
remain unqualified. Follow the [bench checklist](../../docs/VALIDATION.md).

The conservative bypass's <5 s target and physical accuracy/clearance targets
remain unproven. Calibration stays deliberately incomplete and motors inhibited.
Use the current `build_make/` UF2s, or rebuild current sources. The historical
ZIP under `recovered-artifacts/` predates all these fixes.
