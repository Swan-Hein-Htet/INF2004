# Barcode-track trial UF2

File: `mtk3pico_smp0_uart_track_trial.uf2` (133,632 bytes).
SHA-256: `95f13b5afc1eda84006b421210c9959fb81dacca9236a5b132e7c8f67fd10104`.

This motor-enabled Pico W / Cytron Robo Pico image uses estimated starting
values requested for an immediate test. They are not measurements:
0.5105 mm per encoder count (assumed 65 mm wheel / 400 counted A edges/rev),
140 mm wheel track, 170 x 230 mm body, 200 mm obstacle depth,
IR black=3200 / white=650 (black-high), encoder and motor signs +1/+1.
Nominal cruise is 100 mm/s; following targets are capped at 150 mm/s and
motor PWM magnitude at 25%. Actual speed and turn angles depend on the hardware.

1. Hold BOOTSEL while connecting USB and copy the UF2 to RPI-RP2.
2. Leave the car level and stationary for at least two seconds.
3. Put its sensors over the guide line and barcode strip; press GP20 to START.
4. GP21 is E-stop. Verify forward motor direction on a stand before a ground run.

The LSM303DLHC at I2C address 0x19 and the ultrasonic sensor must work.
Missing echoes, encoder, stall, impact and timing faults still stop the car.
This trial stops at obstacles under 200 mm instead of attempting a bypass.
A/B/C junction commands, D U-turns, E-Z reporting and end markers remain active.
This is an offline image: no WiFi credentials are needed.

Optional logs: GP0 TX to a 3.3 V UART adapter RX, common ground, 115200 8N1.
There is no USB serial console. Logs report barcodes and mode/fault changes.

Fresh ARM compilation completed without warnings. Existing host regressions,
independent barcode/protocol checks and trial-profile gate tests passed.
UF2 block format, RP2040 family, boot2 CRC, reset vector and all ELF load bytes
were verified. No physical robot test was performed.

This commit adds the prebuilt binary and its metadata only. It was built from
the modified working source in the chat workspace; it is not claimed to be
reproducible from the remote branch's existing source without those changes.
