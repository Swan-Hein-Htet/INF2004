# Flash-and-try autonomous demo

Use **PicoCar-autonomous-DEMO.uf2**. This is a running autonomous application,
not servo-HOLD or a motor-inhibited diagnostic image. No calibration wizard or
serial terminal is required. GP20 starts a mission; GP21 stops it. It does not
start moving merely because it was flashed.

The owner explicitly requested plausible values to get a trial running quickly.
These numbers are **unmeasured estimates**, not facts extracted from the PDFs:

- Wheel travel: **0.50 mm per encoder count**. This is a placeholder for the
  supplied hall-encoder motors, not the rejected 20-slot / 5.1mm assumption.
- Left-to-right wheel spacing: **140 mm**.
- Overall width: **180 mm**; overall length: **240 mm**.
- Maximum planned obstacle front-to-back depth: **150 mm**.
- Cruise target: **100 mm/s**, which depends on the assumed encoder scale.
- All three IR channels: black **3500**, white **500**, black gives higher ADC.
- Motor signs and encoder signs: **+1 for both sides**.

Motor PWM is capped at **30%** in this demo, including turns and bypasses.
That caps electrical drive, not physical speed. The car may stall at this cap.
All existing runtime sensor, encoder, timing, line-loss and E-stop checks remain
enabled. The configuration gate is intentionally enabled using the estimates;
the `b_calibrated` flag in this demo does **not** claim real calibration.
Distance, turn angle, clearance and hump height will be wrong if the estimates
are wrong. Treat this as a supervised trial, not a qualified full-course result.

## A. Put the file on the Pico

1. Download **PicoCar-autonomous-DEMO.uf2** to your computer. No build is needed.
2. Turn the car's battery/motor power **OFF**.
3. Put the car on a stable support so **both drive wheels are off the surface**.
4. Hold **BOOTSEL on the Pico itself** while connecting its USB cable to the
   computer. Release BOOTSEL when **RPI-RP2** appears.
5. Copy **PicoCar-autonomous-DEMO.uf2** onto RPI-RP2. Wait for that drive to
   disappear. The Pico has flashed and restarted.
6. Disconnect USB. With power still off, prepare the battery connection as in
   the existing wiring guide. Keep both wheels raised.

## B. First check: wheel direction

1. Leave the car level with the wheels raised. Put a broad box or wall about
   **50 cm in front of the ultrasonic sensor**. It needs an echo; the current
   firmware may fault in an empty area with nothing in range.
2. Put black course material under **both line sensors** for this brief raised-
   wheel direction check, keeping it at the usual sensing height. Keep hands
   clear of wheels. Do not touch or restrain the servo. With the assumed IR
   polarity correct, both sensors see the same colour and request straight travel.
3. Turn the battery power on. Keep the whole car still for **3 seconds**.
4. Watch the **GP7 LED** on the carrier board. A regular slow blink means the
   supervisor is in IDLE. It is not proof of completed sensor calibration.
5. Press the carrier's **GP20 button** for about **half a second**, then release.
6. Briefly check both wheels rotate in the direction that would push the car
   forward. Press **GP21** to stop. A brief successful spin is enough for this
   first test. Both-black can also stop the car as an end marker after about
   60 mm of estimated travel; that is expected in this raised-wheel setup.
   A line/range fault can stop the trial if the stationary setup
   no longer resembles a valid line or the sonar sees no echo.
7. If either wheel goes backward, one wheel does not move, or it spins erratically,
   turn power **OFF**. Do not put it on the floor. Report which wheel and what
   the GP7 LED does. The motor/encoder sign guesses need correcting.

If the wheels do nothing, use the blink-code list below. Repeatedly pressing
START does not clear a sensor/encoder/timing fault. Fix the cause, power off,
then restart and wait three seconds.

## C. Second check: a short line

1. Turn power **OFF** before taking the car off its support.
2. Use a flat white surface with a short black course line. Use your supplied
   line width. Mount/position the sensors so at least one line sensor sees the
   line when the car is centred. This firmware treats both-white as lost.
3. Start with no barcodes, junctions, humps or nearby obstacles. Keep a broad
   sonar target ahead, roughly **0.5–1 m** away, with at least **20 cm** clear
   distance during this short trial. Keep the car away from table edges.
4. Put the car at the start. Turn power on, leave it still for **3 seconds**,
   then press GP20 for half a second and release.
5. Let it travel only a short distance. Be ready to press GP21 or switch off.
6. It should move forward and steer to stay over the line. If it immediately
   leaves the line, circles, oscillates strongly or stops, end this trial.
   Report the movement and any blink count. IR polarity, thresholds, encoder
   signs or PID may need correcting; do not proceed to obstacle tests.

## D. Third check: barcode A and a junction

1. Do this only after forward movement and simple line following work.
2. Put the supplied complete Code 39 **`*A*`** barcode where the dedicated
   barcode IR sensor will pass over it. Include its start/stop patterns and
   clear white space before and after. Use the original printed scale.
3. Put the next junction shortly after the barcode, around **10–20 cm** away.
   This is a starting arrangement; the pending command expires after **500 mm
   of estimated travel**, so a bad scale can still make it expire too soon.
4. Power cycle, keep still for three seconds, then press GP20 to start.
5. Expected sequence: **read A → keep following → cross the short junction
   marking → turn left → resume following**. It does not turn as soon as A
   passes. The intended turn is 90°, but the assumed wheel scale/spacing may
   make the actual turn inaccurate.
6. Press GP21 after the turn. Passing a barcode by hand while the car is IDLE
   is not a complete autonomous test. The demo has no separate barcode-success
   LED, so no turn alone cannot distinguish a missed barcode from a missed
   junction. Use USB diagnostics if that distinction is needed.

Then try B for right, C for straight, and D for an immediate U-turn, one test at
a time. Confirm the turn scale before trying a narrow course or an obstacle.

## E. Hump and obstacle trials

Only proceed when the earlier trials work. Try one small measured hump at a
time. For the obstacle test, use a broad sonar-reflective box whose front-to-back
depth is no more than **150 mm**, leaving generous room on both sides. It should
stop below approximately **200 mm range**, scan, choose a side, bypass and seek
the line again. Watch from the battery switch and stop before any contact.
The estimated geometry cannot guarantee clearance or successful reacquisition.
The existing bypass is expected to exceed the project's five-second target.

## Read GP7 without serial

These meanings apply **only to this DEMO image**, not earlier test UF2s.

- Regular slow on/off: IDLE, waiting for a start/readiness conditions.
- Steady on: mission active, including following, turning or scanning.
- Off: finished, or no running supervisor; inspect power if unexpected.
- **1 short blink, pause, repeat:** IMU fault — check the LSM303DLHC wiring,
  power and supported device/address.
- **2 short blinks:** ultrasonic/range fault — check wiring and give it a broad
  target 0.3–1 m away. Reboot after fixing.
- **3 short blinks:** encoder plausibility fault — check encoder phase wiring,
  signs and the assumed travel scale.
- **4 short blinks:** stall — check motor battery, motor/encoder wiring and
  wheel obstruction. The 30% cap may also be insufficient to start the motor.
- **5 short blinks:** line lost — check sensor height, analogue A0 wiring,
  black/white polarity and alignment.
- **6 short blinks:** timing fault — power cycle; if it returns, capture the
  diagnostic output before further movement trials.
- **7 short blinks:** E-stop or another critical fault. If caused by GP21,
  release it, press GP20 once to reset, release GP20, then press it again to start.
  Reset works only if no other critical fault remains. Power cycling is the
  simpler first-trial reset.

If several faults are present, the first listed applicable code takes priority.
If sensor initialization fails before the supervisor starts, this image cannot
show runtime blink codes; use the diagnostic image to identify that failure.

Local autonomous behavior is included. WiFi/MQTT is not enabled in this image.
For measured settings instead, use [the guided package](AUTONOMOUS_QUICKSTART.md).
