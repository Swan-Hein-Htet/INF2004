#!/usr/bin/env python3
"""Package compiled diagnostic/bench/autonomous images with calibration wizard."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

from guided_calibration import CONFIG_FORMAT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / "build_make"
    output = root / "recovered-artifacts/PicoCar-autonomous-kit"
    output.mkdir(parents=True, exist_ok=True)
    template = (build / "mtk3pico_smp0_uart.uf2").read_bytes()
    symbols = subprocess.check_output([args.nm, "-S", str(build / "mtk3pico_smp0_uart.elf")], text=True)
    symbol = [line.split() for line in symbols.splitlines() if line.endswith(" g_robot_config")]
    if len(symbol) != 1:
        raise ValueError("Missing or duplicate configuration symbol")
    address, size = int(symbol[0][0], 16), int(symbol[0][1], 16)
    expected = struct.pack(CONFIG_FORMAT, *([0.0] * 5 + [200.0] + [1] * 4 + [0, 4095, True] * 3 + [False]))
    memory = {}
    for offset in range(0, len(template), 512):
        target, payload = struct.unpack_from("<II", template, offset + 12)
        for index in range(payload):
            memory[target + index] = template[offset + 32 + index]
    original = bytes(memory[address + index] for index in range(size))
    if original != expected:
        raise ValueError("Template is not the expected inhibited factory configuration")
    manifest = dict(template="PicoCar-autonomous-template.uf2", address=address, size=size,
                    sha256=hashlib.sha256(template).hexdigest(), original_hex=original.hex())
    (output / manifest["template"]).write_bytes(template)
    (output / "autonomous-template.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for source, destination in (
        (build / "mtk3pico_smp0_usb_cdc_test.uf2", "PicoCar-calibrate.uf2"),
        (build / "mtk3pico_smp0_usb_cdc_test_motor_bench.uf2", "PicoCar-motor-bench.uf2"),
        (root / "tools/guided_calibration.py", "guided_calibration.py"),
        (root / "docs/AUTONOMOUS_QUICKSTART.md", "START_HERE.md"),
    ):
        shutil.copyfile(source, output / destination)
    (output / "requirements.txt").write_text("pyserial>=3.5,<4\n")
    archive = output.with_suffix(".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for path in sorted(output.iterdir()):
            if path.is_file():
                package.write(path, output.name + "/" + path.name)
    print(archive)
    print("SHA256:", hashlib.sha256(archive.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
