#!/usr/bin/env python3
"""Measure this car and personalize a hash-checked autonomous UF2; no local SDK."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import struct
import threading
import time

CONFIG_FORMAT = "<6f4b" + "HH?x" * 3 + "?x"
CONFIG_BYTES = struct.calcsize(CONFIG_FORMAT)
MAGIC = (0x0A324655, 0x9E5D5157, 0x0AB16F30)
ADC = re.compile(r"ADC=(\d+),(\d+),(\d+) ENC=(\d+),(\d+) FAULT=([0-9a-fA-F]+)")
BENCH = re.compile(r"BENCH wheel=([01]) before=(\d+),(\d+) after=(\d+),(\d+) FAULT=([0-9a-fA-F]+)")


def signed_delta(before, after):
    return ((after - before + 32768) % 65536) - 32768


def validate(config):
    bounds = {
        "wheel_mm_per_edge": (0.01, 20, False),
        "track_mm": (50, 500, False),
        "body_width_mm": (50, 500, False),
        "body_length_mm": (50, 600, False),
        "obstacle_depth_mm": (20, 1000, True),
        "cruise_mm_s": (100, 350, True),
    }
    for name, (low, high, inclusive) in bounds.items():
        value = config[name]
        if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value):
            raise ValueError(f"{name}: a finite measured number is required")
        if not ((low <= value <= high) if inclusive else (low < value < high)):
            raise ValueError(f"{name}: outside firmware limits {low}..{high}")
    for name in ("encoder_sign", "motor_sign"):
        if len(config[name]) != 2 or any(type(v) is not int or v not in (-1, 1) for v in config[name]):
            raise ValueError(f"{name}: requires two verified signs, each -1 or +1")
    if len(config["ir"]) != 3:
        raise ValueError("Three IR sensor calibrations are required")
    for channel in config["ir"]:
        black, white = channel["black"], channel["white"]
        if any(type(v) is not int or not 0 <= v <= 4095 for v in (black, white)):
            raise ValueError("IR endpoints must be integer ADC readings in 0..4095")
        if abs(white - black) < 200 or type(channel["b_black_low"]) is not bool or channel["b_black_low"] != (white > black):
            raise ValueError("IR span must be >=200 counts and polarity must match the readings")
    if config.get("measurements_verified") is not True:
        raise ValueError("Complete measurement and motor-direction verification first")


def pack_config(config):
    validate(config)
    values = [config[n] for n in ("wheel_mm_per_edge", "track_mm", "body_width_mm", "body_length_mm", "obstacle_depth_mm", "cruise_mm_s")]
    values += config["encoder_sign"] + config["motor_sign"]
    for channel in config["ir"]:
        values += [channel["black"], channel["white"], channel["b_black_low"]]
    return struct.pack(CONFIG_FORMAT, *values, True)


def patch_uf2(template, manifest, config):
    if hashlib.sha256(template).hexdigest() != manifest["sha256"]:
        raise ValueError("Template hash mismatch: use the files from the same release")
    if len(template) % 512 or not template:
        raise ValueError("Invalid UF2 length")
    address, size = manifest["address"], manifest["size"]
    if size != CONFIG_BYTES or not 0x10000000 <= address < 0x10200000 - size:
        raise ValueError("Invalid calibration address/layout")
    replacement = pack_config(config)
    original = bytes.fromhex(manifest["original_hex"])
    if len(original) != size:
        raise ValueError("Invalid original calibration object")
    result = bytearray(template)
    covered = [False] * size
    blocks = len(template) // 512
    seen = set()
    for offset in range(0, len(template), 512):
        m0, m1, flags, target, payload, block, total, family = struct.unpack_from("<8I", template, offset)
        if (m0, m1, struct.unpack_from("<I", template, offset + 508)[0]) != MAGIC:
            raise ValueError("Invalid UF2 magic")
        if flags != 0x2000 or family != 0xE48BFF56 or payload != 256 or total != blocks or block in seen or block >= blocks:
            raise ValueError("Unsupported, duplicate or incomplete RP2040 UF2 block")
        seen.add(block)
        start, end = max(target, address), min(target + payload, address + size)
        for location in range(start, end):
            index = location - address
            file_index = offset + 32 + location - target
            if covered[index] or template[file_index] != original[index]:
                raise ValueError("Calibration address overlaps or does not match the template")
            result[file_index] = replacement[index]
            covered[index] = True
    if not all(covered):
        raise ValueError("Calibration object is missing from the UF2")
    return bytes(result)


def derive_wheels(trials):
    """Trials contain signed reference distance and both integrated raw counts."""
    signs, scales = [], []
    for wheel in range(2):
        ratios = []
        for distance, counts in trials:
            if abs(counts[wheel]) < 10:
                raise ValueError("Too few encoder counts: check both encoder phases and supply")
            ratios.append(distance / counts[wheel])
        sign = 1 if ratios[0] > 0 else -1
        if any(value * sign <= 0 for value in ratios):
            raise ValueError("Encoder direction changed between trials; check wiring/slip")
        scale = statistics.median(abs(v) for v in ratios)
        if any(abs(abs(v) / scale - 1) > 0.05 for v in ratios):
            raise ValueError("Encoder trials disagree by >5%; repeat without slipping")
        signs.append(sign)
        scales.append(scale)
    if abs(scales[0] - scales[1]) / statistics.mean(scales) > 0.05:
        raise ValueError("Left/right wheel scales differ by >5%; firmware needs matched scales")
    return statistics.mean(scales), signs


def choose_port(preferred=None):
    import serial.tools.list_ports
    if preferred:
        return preferred
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        raise ValueError("No serial port found. Flash diagnostics-usb and use a USB DATA cable directly to the Pico")
    for number, port in enumerate(ports, 1):
        print(f"{number}: {port.device} — {port.description}")
    if len(ports) == 1:
        return ports[0].device
    number = int(input("Select the Pico USB port number: "))
    if not 1 <= number <= len(ports):
        raise ValueError("Invalid port selection")
    return ports[number - 1].device


def read_adc(connection, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        match = ADC.search(connection.readline().decode("ascii", errors="replace"))
        if match:
            values = [int(v) for v in match.groups()[:5]]
            fault = int(match[6], 16)
            if fault & (0x077F & ~0x0400):
                raise ValueError(f"Diagnostic fault {fault:04x}. Check wiring/power, face sonar toward a target 0.3–1m away, then reboot")
            if any(v > 4095 for v in values[:3]) or any(v > 65535 for v in values[3:]):
                raise ValueError("Invalid diagnostic readings")
            return values
    raise ValueError("No diagnostic readings. Check image, USB port, cable; close PuTTY/screen")


def capture_ir(connection, channel, surface):
    input(f"Put {('LEFT line', 'RIGHT line', 'BARCODE')[channel]} IR over {surface} at its installed height. Keep still; press Enter: ")
    connection.reset_input_buffer()
    readings = [read_adc(connection)[channel] for _ in range(10)]
    median = round(statistics.median(readings))
    print(f"  {surface}: median={median}, range={min(readings)}..{max(readings)}")
    return median, max(readings) - min(readings)


def capture_roll(connection, distance):
    input(f"Position car at the START mark for a {distance:+g} mm roll; press Enter when stationary: ")
    connection.reset_input_buffer()
    previous = read_adc(connection)[3:]
    totals = [0, 0]
    reverse = [0, 0]
    done = threading.Event()

    def finish():
        input("Roll straight by hand to the END mark, stop completely, then press Enter: ")
        done.set()

    threading.Thread(target=finish, daemon=True).start()
    started = time.monotonic()
    tail = 0
    while tail < 3:
        current = read_adc(connection)[3:]
        for wheel in range(2):
            delta = signed_delta(previous[wheel], current[wheel])
            totals[wheel] += delta
            reverse[wheel] += abs(delta)
        previous = current
        if done.is_set():
            tail += 1
        if time.monotonic() - started > 120:
            raise ValueError("Roll timed out; restart the wizard")
    if any(reverse[w] > abs(totals[w]) * 1.05 for w in range(2)):
        raise ValueError("Counts reversed during this straight roll; check encoder phases and repeat")
    print(f"  Integrated encoder counts: left={totals[0]}, right={totals[1]}")
    return [distance, totals]


def measured_number(prompt, low, high):
    while True:
        try:
            value = float(input(prompt))
            if math.isfinite(value) and low < value < high:
                return value
        except ValueError:
            pass
        print(f"Enter a measured value greater than {low} and less than {high}, in mm.")


def guided(port):
    import serial
    print("PicoCar guided calibration — measurements stay on your computer.")
    print("You need a ruler/tape, black/white course surfaces, and a marked 1000mm straight distance.")
    print("Motor supply OFF for IR/hand-roll tests; keep encoder 3.3V powered from Pico USB.")
    config = {"cruise_mm_s": 200.0, "ir": [], "motor_sign": [], "measurements_verified": False}
    config["track_mm"] = measured_number("Left-to-right tyre CONTACT CENTRE spacing: ", 50, 500)
    config["body_width_mm"] = measured_number("Overall width including wheels/protrusions: ", 50, 500)
    config["body_length_mm"] = measured_number("Overall front-to-back length including scanner: ", 50, 600)
    config["obstacle_depth_mm"] = measured_number("Maximum course obstacle FRONT-TO-BACK depth, including margin: ", 19.999, 1000.001)
    input("Flash PicoCar-calibrate.uf2 using BOOTSEL; leave Pico USB connected, close other serial apps. Press Enter: ")
    with serial.Serial(choose_port(port), 115200, timeout=0.25) as connection:
        for channel in range(3):
            white, noise_w = capture_ir(connection, channel, "WHITE")
            black, noise_b = capture_ir(connection, channel, "BLACK")
            if abs(white - black) < 200 or max(noise_w, noise_b) > abs(white - black) * 0.25:
                raise ValueError("IR readings overlap or fluctuate too much; check height, wiring, surface and ambient light")
            config["ir"].append({"black": black, "white": white, "b_black_low": white > black})
        print("Mark a straight 1000mm distance. Keep the car facing the SAME way throughout; no wheel slip.")
        trials = []
        for trial in range(3):
            print(f"Trial {trial + 1}/3: forward roll, then backward roll to the original mark.")
            trials.append(capture_roll(connection, 1000.0))
            trials.append(capture_roll(connection, -1000.0))
        config["wheel_mm_per_edge"], config["encoder_sign"] = derive_wheels(trials)
        config["roll_trials"] = trials
    print(f"Measured scale: {config['wheel_mm_per_edge']:.6g} mm/count; forward signs: {config['encoder_sign']}")
    print("Next: turn power OFF, RAISE WHEELS securely, flash PicoCar-motor-bench.uf2.")
    print("Reconnect motor supply, power on, keep wheels raised. Servo is disabled in this bench image.")
    input("Press Enter when the bench image is flashed and BOTH wheels are clear of the surface: ")
    print("Hold GP20 for each short pulse, then release for >=0.5 seconds. Left/right alternate.")
    print("Do this THREE times per wheel (six pulses). GP21 stops; reboot after any stop/fault.")
    observations = [[], []]
    with serial.Serial(choose_port(port), 115200, timeout=0.25) as connection:
        deadline = time.monotonic() + 180
        while min(map(len, observations)) < 3:
            if time.monotonic() > deadline:
                raise ValueError("Motor bench timed out; check power, press/release GP20, and restart")
            match = BENCH.search(connection.readline().decode("ascii", errors="replace"))
            if not match:
                continue
            wheel, b0, b1, a0, a1 = map(int, match.groups()[:5])
            if int(match[6], 16):
                raise ValueError("Motor bench reported a fault; investigate and reboot")
            delta = signed_delta((b0, b1)[wheel], (a0, a1)[wheel])
            other = wheel ^ 1
            if abs(signed_delta((b0, b1)[other], (a0, a1)[other])) > max(2, abs(delta) * 0.1):
                raise ValueError("The other encoder moved: motor/encoder sides may be swapped")
            if abs(delta) < 2:
                print("  Too few counts. Check motor power/encoder; hold GP20 for the complete pulse.")
                continue
            sign = 1 if delta * config["encoder_sign"][wheel] > 0 else -1
            observations[wheel].append(sign)
            print(f"  {('Left', 'Right')[wheel]}: {delta:+d} counts, motor sign {sign:+d}; samples={len(observations[wheel])}")
    if any(len(set(samples)) != 1 for samples in observations):
        raise ValueError("Motor directions were inconsistent; inspect wiring and repeat")
    config["motor_sign"] = [samples[0] for samples in observations]
    config["measurements_verified"] = True
    return config


def emit(config, release, destination):
    manifest = json.loads((release / "autonomous-template.json").read_text())
    image = patch_uf2((release / manifest["template"]).read_bytes(), manifest, config)
    destination.write_bytes(image)
    destination.with_suffix(".json").write_text(json.dumps(config, indent=2) + "\n")
    print(f"\nCreated {destination.resolve()}\nCalibration saved beside it as JSON.")
    print("Power OFF. Flash this autonomous UF2 via BOOTSEL. Reconnect motor power while OFF.")
    print("Power on, keep stationary >=2s for IMU calibration, then GP20 starts; GP21 stops.")
    print("Check a short straight line and 90-degree turn before attempting the full course.")
    print("This build runs autonomously without a computer; WiFi/MQTT requires a separately configured network build.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Pico USB port, e.g. COM5 or /dev/cu.usbmodem...")
    parser.add_argument("--config", type=Path, help="Regenerate from a previously measured calibration JSON")
    parser.add_argument("--release", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--output", type=Path, default=Path("PicoCar-autonomous.uf2"))
    args = parser.parse_args()
    try:
        config = json.loads(args.config.read_text()) if args.config else guided(args.port)
        emit(config, args.release, args.output)
    except ImportError:
        parser.exit(1, "Install pyserial first: python -m pip install pyserial\n")
    except (ValueError, OSError, KeyError, TypeError, EOFError, KeyboardInterrupt) as error:
        parser.exit(1, f"Calibration stopped: {error}\nNo new autonomous image was generated.\n")


if __name__ == "__main__":
    main()
