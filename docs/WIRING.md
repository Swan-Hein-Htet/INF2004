# Wiring: Pico W + Cytron Robo Pico

Use **GPIO numbers**, not Pico physical pin numbers. Power off before wiring.
This map follows the supplied Robo Pico datasheet pages 3–4 and motor truth table,
with the encoder correction in the tutor review and PostTri1. The definitive
power/current limits remain those of your actual board and components.

| Signal | GPIO | Pico physical pin | Connection |
| --- | --- | --- | --- |
| UART console TX | GP0 | 1 | 3.3 V USB/UART adapter RX |
| UART console RX | GP1 | 2 | optional adapter TX |
| Ultrasonic TRIG | GP2 | 4 | HC-SR04 TRIG |
| Ultrasonic ECHO | GP3 | 5 | HC-SR04 ECHO **through divider below** |
| IMU SDA | GP4 | 6 | GY-511 LSM303DLHC SDA, I2C0 |
| IMU SCL | GP5 | 7 | GY-511 LSM303DLHC SCL, I2C0 |
| Left motor input A/B | GP8/GP9 | 11/12 | already connected to onboard M1 bridge |
| Right motor input A/B | GP10/GP11 | 14/15 | already connected to onboard M2 bridge |
| Scanner servo signal | GP12 | 16 | servo port 1 signal |
| Left encoder B | GP13 | 17 | encoder phase B, **not a second servo** |
| Left encoder A | GP16 | 21 | encoder A; Grove 4 signal |
| Right encoder A | GP17 | 22 | encoder A; other Grove 4 signal |
| Right encoder B | GP19 | 25 | encoder phase B via GPIO breakout |
| START/RESET button | GP20 | 26 | onboard active-low button; code enables pull-up |
| E-STOP button | GP21 | 27 | onboard active-low button; code enables pull-up |
| Left line analogue | GP26/ADC0 | 31 | IR sensor AO |
| Right line analogue | GP27/ADC1 | 32 | IR sensor AO |
| Barcode analogue | GP28/ADC2 | 34 | third IR AO; VBAT sense jumper **open** |
| Logic supply | 3V3 OUT | 36 | 3.3 V sensors/encoder logic, subject to current limits |
| Ground | GND | 3,8,13,18,23,28,33,38 | common logic/sensor/power ground |

GP6, GP7, GP14, GP15, GP18 are unallocated. GP22's buzzer and GP18's RGB LED
are not driven by this firmware. Never use GP23/24/25/29: Pico W radio owns them.
Do not confuse Pico W GP25 with the external GP19 encoder connector.

## Motor and encoder wiring

Connect the left motor's two power leads to **M1**, right motor leads to **M2**.
Do not connect motor power to a Pico GPIO or 3V3 output. The six-pin motor encoder
connector's exact pin order and voltage rating are not present in the supplied
datasheets. Identify M+/M−, encoder VCC/GND/A/B from the actual motor marking or
vendor pinout before applying power; **wire colour is not a reliable pinout**.
Only encoder **logic** A/B connect to the four GPIOs above. Confirm encoder supply
and output amplitude. Use proper level shifting if powered from 5 V; a bare
5 V push-pull encoder output must never reach a Pico input. Open-collector
outputs may use pull-ups to 3.3 V if their datasheet permits it.

Grove 4 carries GP16/GP17. It can carry the two A signals using a correctly
split harness; it does not by itself carry both motors' A/B pairs. Run B via
separate GPIO leads to GP13/GP19. Grove 5 shares GP6/GP26, Grove 6 GP26/GP27,
and Grove 7 GP7/GP28: do not attach an encoder harness to these ports.

GP13 is also servo port 2's signal pin. If using that port's V+ and GND for
HC-SR04 power, leave its signal pin connected **only to left encoder B**.
Do not plug in a second servo or route GP13 to HC-SR04 TRIG/ECHO.

## HC-SR04 divider and power

Use a regulated nominal 5 V sensor supply verified under motor load. The HC-SR04
is the 5 V part, not the 3.3 V HC-SR04P. Servo-port V+ follows board input and
can sag; USB-only motor/servo supply may be roughly VUSB −0.4 V. Check it with a
meter before assuming it provides 5 V. If using an external regulated sensor
supply, share ground and avoid back-feeding the board's power rails.

```text
HC-SR04 ECHO --- 10 kΩ ---+--- GP3
                        |
                       15 kΩ
                        |
                       GND
```

This gives 3.0 V at 5.0 V ECHO and 3.3 V at 5.5 V. Measure the actual voltage
before attaching GP3. This intentionally replaces the review's 10 kΩ/20 kΩ
proposal, which approaches/exceeds nominal 3.3 V at the upper supply range.
TRIG is a 3.3 V Pico output; verify your module accepts it as logic high.
Put local decoupling near the sensor and keep echo wires away from motor leads.

## IR sensors and IMU

Use analogue output **AO**, not the adjustable digital comparator output, for all
three IR sensors. Supply modules at 3.3 V only if their module specification
allows it; verify full AO range stays between ground and 3.3 V. If a module
requires 5 V, add a suitable analogue level interface and recalibrate. A raw
TCRT5000 is not a ready-to-wire module: it requires an LED current-limiting
resistor and a phototransistor bias circuit; follow its datasheet. Observe the
Robo Pico's total 3V3 Grove-output budget (300 mA in supplied table).

Keep the GP28 VBAT solder jumper open. Do not connect a second voltage source to
ADC2. Place the barcode sensor at the barcode strip's specified lateral position,
allowing for the brief's 20 mm gap between line and barcode. The review's
“20 mm from line centre” is ambiguous relative to the 18 mm line and illustrated
gap; use the actual printed track geometry, not that inferred centre offset.

GY-511: VCC to an appropriate 3.3 V-compatible supply, GND common, SDA GP4,
SCL GP5. Pull-ups must terminate at 3.3 V. The code expects the LSM303DLHC
accelerometer address 0x19, mounted X forward / Y left / Z up. Magnetometer pins
and interrupt outputs are unused. Do not substitute LSM303D (different register
map) or MPU-6050 without changing and testing the driver.

## Servo and power distribution

Servo signal to GP12, V+ to a supply within the **actual servo's** rating, ground
common. The supplied files do not identify a servo model or its stall current.
Check its datasheet; 1000–2000 µs is a conservative starting pulse range, not a
proof of mechanical travel. Use strain relief and avoid hard-stop current.

Robo Pico input is rated 3.6–6 V in the supplied sheet; this does not mean every
attached sensor accepts 6 V. Motor and servo voltage follows the input. Respect
its 1 A continuous / 1.5 A short peak per motor-channel limits and size the supply
for both motor stall currents plus servo current. Do not feed an unregulated
multi-cell battery directly into this board. Confirm polarity and ensure the
battery protection and independent power switch remain accessible.

Physical prototype inspection, load-voltage tests and polarity tests remain
required. This document cannot determine the pin order of an unidentified motor
or guarantee that a particular harness matches its colour scheme.
