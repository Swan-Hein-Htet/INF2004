# Wiring: Pico W + Cytron Robo Pico

Power off before wiring. **GP means the signal label**, for example GP26.
Use that label on the board or connector; there is no Pico position-number
column in this guide. **Grove** is the connector name printed on the Robo Pico.
The Robo Pico is the carrier board holding the smaller Raspberry Pi Pico W.

The tables below specify a wiring arrangement matching the existing firmware.
Grove connections require a breakout cable or correctly repinned harness where
the module has a different connector. Locate each contact by the Robo Pico
pinout/silkscreen and continuity-check it with power off. **Do not match cable
colours or assume the module's pin order matches a Grove socket.**

## Module identification: what the supplied files actually confirm

| Device | Model / module number | Identification limit |
| --- | --- | --- |
| Controller | Raspberry Pi Pico W + Cytron **ROBO-PICO** | These tables use the supplied ROBO-PICO datasheet. |
| Ultrasonic distance module | **HC-SR04**, nominal 5 V | Not HC-SR04P. Use its labelled VCC, TRIG, ECHO, GND pins. |
| IMU module | **OKY3231-2 / GY-511**, containing **LSM303DLHC** | OKY3231-2 is the product module number in the supplied sheet. Check the installed board matches. |
| Three IR sensors: left line, right line, barcode | Installed model **unconfirmed** | Supplied references include a **Waveshare Infrared Reflective Sensor, ST188 + LM393**, and a **TCRT5000** component sheet. They do not prove which assembled boards are installed. LM393 alone is a comparator chip number, not a complete module number. |
| Scanner servo | Installed model **unconfirmed** | SG90 is mentioned as a design assumption in the tribunal, but no supplied servo datasheet confirms the fitted model or pin order. |
| Left and right encoder gearmotors | Installed model **unconfirmed** | Review describes six-wire A/B encoder motors, without an exact part number, numbered connector pinout or confirmed logic voltage. |
| Optional UART adapter | Model **unconfirmed**; must use 3.3 V logic | Use labelled RX, TX and GND. |

Do not assign numbered module pins to an unidentified board. The tables use
module **pin labels**. For the documented Waveshare IR board specifically, its
manual numbers the pins **1 DOUT, 2 AOUT, 3 GND, 4 VCC**. Those numbers apply
only if your board matches that module; they are not a generic TCRT5000 pinout.

## Where each connection is on the Robo Pico

All seven Grove sockets below are on the **Robo Pico carrier**, not the Pico W
itself. Each also provides +3V3 and GND; these are power contacts, not GPIOs.

| Robo Pico connector | Signal contacts | Use in this wiring arrangement |
| --- | --- | --- |
| Grove 1 | GP0, GP1 | Optional UART adapter |
| Grove 2 | GP2, GP3 | HC-SR04 TRIG and divided ECHO |
| Grove 3 | GP4, GP5 | GY-511 SDA and SCL |
| Grove 4 | GP16, GP17 | Left encoder A and right encoder A, split harness |
| Grove 5 | GP6, GP26 | Left IR analogue output on GP26 only; GP6 left disconnected |
| Grove 6 | GP26, GP27 | Right IR analogue output on GP27 only; this socket's GP26 contact left disconnected |
| Grove 7 | GP7, GP28 | Barcode analogue output on GP28 only; GP7 left disconnected |
| Servo port 1 | Signal GP12, V+, GND | Scanner servo |
| Servo port 2 | Signal GP13, V+, GND | Signal contact used for left encoder B; **not a second servo** |
| Pico W GP19 header/breakout connection | GP19 | Right encoder B; no Grove socket carries GP19 |
| Motor terminals M1 / M2 | Motor power outputs | Left / right motor leads; not sensor or GPIO sockets |
| Onboard buttons | GP20, GP21 | START/RESET and E-STOP; already wired on carrier |

**Grove 5 GP26 and Grove 6 GP26 are the same electrical signal.** They cannot
receive two independent sensor outputs. Use the individual contacts specified
below and leave unused signal wires disconnected at the module end. A sensor's
digital output must not accidentally connect to another sensor's analogue line.
The Maker Port shares GP2/GP3 with Grove 2; do not attach another device there
while these signals are used for the HC-SR04.

GP19 is a **direct GPIO connection**, outside the Grove system. Use an accessible
GP19-labelled Pico header/breakout contact verified against the board pinout.
If no contact is accessible with the Pico installed, a suitable breakout/header
is needed; do not substitute a different Grove pin. There is no need to wire
the other modules directly to Pico W header pins when using the carrier
connectors listed here.

## Module pin → GP signal → board connector

### HC-SR04 ultrasonic module

| Module pin | GP / power destination | Where to connect | Wiring detail |
| --- | --- | --- | --- |
| TRIG | GP2 | Robo Pico **Grove 2**, GP2 signal contact | Direct signal connection. |
| ECHO | GP3, through divider | Robo Pico **Grove 2**, GP3 signal contact | Connect ECHO through 10 kΩ to GP3; connect 15 kΩ from GP3 to GND. See diagram below. |
| VCC | Verified regulated nominal 5 V | Separate 5 V supply; alternatively Robo Pico **servo port 2 V+** only if verified suitable under load | **Do not use Grove +3V3** for this 5 V module. Servo V+ is not a fixed 5 V regulator output. |
| GND | GND | Robo Pico Grove 2 GND / common ground | Also connect the 5 V supply ground if using an external supply. |

Only the two HC-SR04 signals and ground use Grove 2. Its +3V3 contact is unused
for this module. Servo port 2's signal remains reserved for encoder B, even if
its power contacts supply the ultrasonic module.

### OKY3231-2 / GY-511 LSM303DLHC module

| Module pin label | GP / power destination | Where to connect | Wiring detail |
| --- | --- | --- | --- |
| SDA | GP4 | Robo Pico **Grove 3**, GP4 signal contact | I2C0 data. |
| SCL | GP5 | Robo Pico **Grove 3**, GP5 signal contact | I2C0 clock. |
| VCC (verify supply label on fitted board) | +3V3 | Robo Pico **Grove 3 +3V3** | Supplied module sheet permits 3–5 V; use 3.3 V and ensure bus pull-ups go to 3.3 V. |
| GND | GND | Robo Pico **Grove 3 GND** | Common ground. |
| Interrupt / data-ready pins, if present | None | Leave unconnected | Firmware polls the accelerometer; it does not use these pins. |

### Three IR modules: use analogue output

The installed IR module model must be confirmed from its marking. For the
Waveshare ST188/LM393 board documented above, **AOUT = pin 2, GND = pin 3,
VCC = pin 4; pin 1 DOUT stays disconnected**. On other boards use the verified
analogue output label AO/AOUT and their own pinout.

| Module / role | Module pin | GP / power destination | Where to connect |
| --- | --- | --- | --- |
| Left line IR | AO / AOUT | GP26 / ADC0 | Robo Pico **Grove 5**, GP26 signal contact |
| Left line IR | VCC | +3V3, only for a 3.3 V-compatible module | Robo Pico **Grove 5 +3V3** |
| Left line IR | GND | GND | Robo Pico **Grove 5 GND** |
| Right line IR | AO / AOUT | GP27 / ADC1 | Robo Pico **Grove 6**, GP27 signal contact |
| Right line IR | VCC | +3V3, only for a 3.3 V-compatible module | Robo Pico **Grove 6 +3V3** |
| Right line IR | GND | GND | Robo Pico **Grove 6 GND** |
| Barcode IR | AO / AOUT | GP28 / ADC2 | Robo Pico **Grove 7**, GP28 signal contact; **open GP28 VBAT sense jumper** |
| Barcode IR | VCC | +3V3, only for a 3.3 V-compatible module | Robo Pico **Grove 7 +3V3** |
| Barcode IR | GND | GND | Robo Pico **Grove 7 GND** |
| All three IR modules | DO / DOUT, if present | None | Leave disconnected; this firmware reads analogue outputs |

This arrangement uses one analogue signal per sensor. It does **not** connect
a four-pin IR module straight to a four-pin Grove cable: their pin functions
and order must be adapted. Keep GP6 (Grove 5), GP26 (Grove 6) and GP7 (Grove 7)
unused by those modules. GP26 is already driven by the left sensor via Grove 5.

### Left and right encoder motors

Module part numbers and connector pin numbers are unknown. Identify the actual
motor's M+/M−, encoder VCC, GND, A and B from its vendor pinout before wiring.

| Module / role | Module pin / lead | GP / power destination | Where to connect |
| --- | --- | --- | --- |
| Left motor | Two motor power leads M+/M− | Driver outputs M1A / M1B | Robo Pico **M1 motor terminals**; verify forward direction during calibration |
| Right motor | Two motor power leads M+/M− | Driver outputs M2A / M2B | Robo Pico **M2 motor terminals**; verify forward direction during calibration |
| Left encoder | A | GP16 | Robo Pico **Grove 4**, GP16 signal contact |
| Right encoder | A | GP17 | Robo Pico **Grove 4**, GP17 signal contact |
| Left encoder | B | GP13 | Robo Pico **servo port 2 signal** contact; do not use its V+ contact for this signal |
| Right encoder | B | GP19 | **Direct Pico W GP19 header/breakout**, not a Grove port |
| Both encoders | Encoder VCC | Supply required by actual encoder | +3V3 from Grove 4 only if the identified encoder permits it; otherwise use its specified supply and appropriate logic interface |
| Both encoders | Encoder GND | GND | Robo Pico **Grove 4 GND** / common ground |

Grove 4 needs a split harness: GP16 goes to the left motor's A pin and GP17 to
the right motor's A pin. Power and ground may be distributed only after checking
both encoders' ratings and the board's current budget. The remaining B wires go
to GP13 and GP19 as above. Do not connect a whole six-wire motor plug to Grove.

The carrier already connects its left motor driver inputs to **GP8/GP9** and
right driver inputs to **GP10/GP11**. Those are internal control connections;
do not run the motor power leads to the Pico GP8–GP11 header pins.

### Scanner servo, onboard buttons and optional UART adapter

| Module / role | Module pin / contact | GP / power destination | Where to connect |
| --- | --- | --- | --- |
| Scanner servo, model unconfirmed | Signal / PWM | GP12 | Robo Pico **servo port 1 signal** |
| Scanner servo | V+ | Supply within actual servo rating | Robo Pico **servo port 1 V+**, only if board input voltage meets servo rating |
| Scanner servo | GND | GND | Robo Pico **servo port 1 GND** |
| Onboard START/RESET button | Switch signal | GP20 | **Already wired on Robo Pico**; no external jumper |
| Onboard E-STOP button | Switch signal | GP21 | **Already wired on Robo Pico**; no external jumper |
| 3.3 V UART adapter | RX | GP0, Pico console TX | Robo Pico **Grove 1**, GP0 signal contact |
| 3.3 V UART adapter | TX, optional | GP1, Pico console RX | Robo Pico **Grove 1**, GP1 signal contact |
| 3.3 V UART adapter | GND | GND | Robo Pico **Grove 1 GND** |
| 3.3 V UART adapter | VCC / 5V / 3V3 power output | None | Leave disconnected when the Robo Pico has its own power |

Verify servo signal/V+/GND by its actual datasheet, not by wire colour. The
two onboard buttons are active-low and firmware enables pull-ups. UART is
115200 baud, 3.3 V logic; do not use an RS-232 voltage-level adapter.

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

## Sources for the connector and module labels

- [Robo Pico datasheet text, pages 3–4](recovered-context/reference-text/Robo_Pico.txt): Grove signal pairs, servo GPIOs, onboard buttons, driver inputs and power limits.
- [OKY3231-2 module sheet](recovered-context/reference-text/OKY3231-2.txt): GY-511 / LSM303DLHC identity and module supply range.
- [HC-SR04 sheet](recovered-context/reference-text/HCSR04.txt): module pin labels and 5 V supply requirement.
- [Waveshare IR module manual](recovered-context/reference-text/Infrared-Reflective-Sensor-UserManual.txt): ST188/LM393 identity, numbered DOUT/AOUT/GND/VCC pins and supply range; verify this is the installed module before using its numbered pinout.
- [Board adapter source](../app_program/subsystem2/board.c): actual firmware GPIO assignments, I2C address and ADC channels.

The electrical map is unchanged by this clarification. Exact installed IR,
servo and motor part numbers still need confirmation before this can serve as
a fully identified module-to-module assembly drawing.
