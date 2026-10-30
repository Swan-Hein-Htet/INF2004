# Make your autonomous image

This package builds **your** autonomous UF2 on your computer after measuring your
car. No C editing, compiler, Pico SDK, PuTTY or separate UART adapter is needed.
It uses the USB socket on the Pico itself. The car runs independently on battery
after the final flash; the computer is only needed for calibration.

The included autonomous **template** has movement inhibited. The wizard creates
`PicoCar-autonomous.uf2` with measured calibration and movement enabled. Do not
flash the template expecting movement. Calibration cannot be filled from the
design-review placeholders. The wizard does not claim to tune PID or qualify
physical performance; first trials are still required.

## 1. Download and prepare

Extract **all** files from `PicoCar-autonomous-kit.zip` into one folder. Keep the
two JSON/template files beside `guided_calibration.py`.

Have these ready:

- A ruler/tape and a straight **1000 mm** distance marked on the floor.
- The actual black and white course surfaces at the installed IR sensor height.
- Four dimensions in mm: left-to-right tyre contact-centre spacing, overall
  width including protrusions, overall front-to-back length including scanner,
  and maximum permitted obstacle front-to-back depth with margin.
- A USB **data** cable directly to the Pico, and a stable support that holds
  both driven wheels above the surface for the motor test.

Face the ultrasonic sensor toward a broad target about **0.3–1 m** away during
stationary calibration. Its current driver latches a range fault after repeated
missing echoes. Reboot after fixing any diagnostic fault. Keep the target ahead
of the scanner as you roll, so it remains within its supported range.

## 2. Start the wizard on the computer with the USB cable

Install Python 3 if necessary. Close PuTTY, screen and other serial applications.
Open a terminal in the extracted folder, then use the commands for your computer.

**Windows PowerShell** — run on Windows, rather than inside WSL, for COM access:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install pyserial
.\.venv\Scripts\python.exe guided_calibration.py
```

**macOS or native Ubuntu:**

```sh
python3 -m venv .venv
.venv/bin/python -m pip install pyserial
.venv/bin/python guided_calibration.py
```

On Ubuntu, if serial access is denied, add your login to `dialout` using
`sudo usermod -aG dialout "$USER"`, then log out and back in. If `venv` is missing,
install `python3-venv`. On macOS, the Pico normally appears as `/dev/cu.usbmodem…`.
On Windows, it appears as a COM port. The wizard lists available ports; select
the Pico, rather than the old USB-to-UART adapter. If you cannot identify it,
disconnect the Pico, compare the port list, then reconnect.

## 3. Follow the prompts

1. Enter the four dimensions. The initial cruise setting is **200 mm/s**.
2. Turn the **motor supply off** while retaining Pico USB and encoder 3.3 V power.
   Hold BOOTSEL while plugging in USB. Copy `PicoCar-calibrate.uf2` onto RPI-RP2.
   The drive disappears and the Pico starts USB diagnostics. This image never
   enables wheel movement. Press Enter in the wizard and select its USB port.
3. Put each requested IR sensor over white, then black, keeping its final mounting
   height. Press Enter when instructed; the wizard captures ten readings for each
   surface. Order: left line, right line, dedicated barcode sensor. It rejects
   insufficient contrast and excessive fluctuation.
4. Roll the car forward **1000 mm**, then backward **1000 mm**, keeping its nose
   facing the same way. Follow the start/end Enter prompts and stop completely
   before confirming. Repeat for three pairs. The wizard integrates wrapping
   encoder counts, learns forward encoder signs, compares repetitions and checks
   both wheel scales. Do not lift the wheels, slip, or rock the car during a roll.
5. The wizard closes the USB port. Turn power off, securely **raise both wheels**,
   then BOOTSEL-flash `PicoCar-motor-bench.uf2`. Reconnect motor power while off,
   power on, and select the newly enumerated USB port in the wizard.
6. Hold GP20 for one pulse, then release for **at least 0.5 seconds**. Each pulse
   is capped at **300 ms at 25% duty** and alternates left/right. A held button
   cannot repeatedly drive the wheels. Perform six successful pulses: three per
   wheel. The wizard uses the hand-roll signs to learn motor direction. GP21
   latches a stop; reboot after a stop. If the wheel does not move or the other
   encoder responds, inspect motor power and left/right wiring. Do not increase
   duty or place the car on the floor to force this test through.
7. On success, the wizard writes **`PicoCar-autonomous.uf2`** and a calibration
   JSON in the current folder. Save both. That UF2 contains the autonomous
   application and your calibration; it is the final image to flash.

The wizard stops on inconsistent measurements or faults. Previously generated
files are not deleted on failure: use only a file from a completed successful
session. An interrupted session must be restarted; partial measurements do not
enable an image. Keep the Pico and motor supply powered off while changing
wiring or repositioning supports.

## 4. Flash and start autonomy

1. Turn off motor power. Hold BOOTSEL while reconnecting Pico USB.
2. Copy the **generated `PicoCar-autonomous.uf2`** onto RPI-RP2. The template,
   calibration logger and motor-bench images are not the final image.
3. With power off, reconnect the motor supply. Start with wheels raised and a
   clear forward ultrasonic target. Power on and keep the car stationary for at
   least **2 seconds** so the IMU can establish its bias.
4. Turn power off before lowering the car. Put it on a short clear line so at
   least one line sensor can see black. Power on, leave it still for at least two
   seconds, then press GP20 for at least **50 ms** to START. Keep GP21 and the
   battery switch accessible. Use GP21 to stop immediately if steering is wrong.
5. Confirm forward travel and line following before adding a junction. Verify
   measured travel and 90° turn accuracy; PID/turn tuning may still be needed.
6. Pass a complete Code 39 **`*A*`** under the barcode sensor. It requests a
   **90° left turn at the next junction**, within 500 mm of following travel.
   It does not turn immediately when the barcode passes. White quiet space is
   required before and after the complete symbol. B requests right, C straight,
   and D an immediate U-turn.
7. Add the hump, then one bounded-depth obstacle, then the combined course only
   after the simpler trials are repeatable. Keep a valid forward range return;
   an open area with no echo can latch the existing range fault.

Calibration persists across power cycles because it is part of the final UF2.
Reflashing the stock template/logger replaces it. To regenerate a saved measured
configuration, use `guided_calibration.py --config PicoCar-autonomous.json` with
the same package. To recalibrate changed hardware, run the full wizard again.

This autonomous package uses UART output and no WiFi/MQTT. It requires no serial
connection while running. Network telemetry needs the separately configured
network build in the repository. USB enumeration, motor pulses and physical
course performance have not been tested on your car here. The existing obstacle
bypass is expected to exceed the design's five-second target; a successful
calibration does not establish that every project requirement has passed.
