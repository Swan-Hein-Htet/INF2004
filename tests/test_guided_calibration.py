#!/usr/bin/env python3
"""Check calibration arithmetic and that UF2 personalization alters only config."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("calibration", ROOT / "tools/guided_calibration.py")
cal = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cal)


def measured_example():
    # Synthetic fixture; these are NOT the user's physical measurements.
    return dict(wheel_mm_per_edge=0.5, track_mm=140.0, body_width_mm=180.0,
                body_length_mm=240.0, obstacle_depth_mm=150.0, cruise_mm_s=200.0,
                encoder_sign=[1, -1], motor_sign=[-1, 1],
                ir=[dict(black=300, white=3300, b_black_low=True),
                    dict(black=3200, white=250, b_black_low=False),
                    dict(black=400, white=3100, b_black_low=True)],
                measurements_verified=True)


def template_fixture():
    # Deliberately straddle a UF2 payload boundary.
    address = 0x100000F0
    data = bytes(512)
    blocks = []
    for index in range(2):
        block = bytearray(512)
        struct.pack_into("<8I", block, 0, cal.MAGIC[0], cal.MAGIC[1], 0x2000,
                         0x10000000 + 256 * index, 256, index, 2, 0xE48BFF56)
        block[32:288] = data[256 * index:256 * (index + 1)]
        struct.pack_into("<I", block, 508, cal.MAGIC[2])
        blocks.append(bytes(block))
    template = b"".join(blocks)
    return template, dict(address=address, size=48, original_hex=bytes(48).hex(),
                          sha256=hashlib.sha256(template).hexdigest())


class CalibrationTests(unittest.TestCase):
    def test_wrapping_counts(self):
        self.assertEqual(cal.signed_delta(65530, 14), 20)
        self.assertEqual(cal.signed_delta(10, 65520), -26)

    def test_scale_and_direction(self):
        trials = [(1000, [2000, -2000]), (-1000, [-2000, 2000]),
                  (1000, [1990, -2010]), (-1000, [-2010, 1990])]
        scale, signs = cal.derive_wheels(trials)
        self.assertAlmostEqual(scale, 0.5, places=4)
        self.assertEqual(signs, [1, -1])

    def test_unreliable_rolls_rejected(self):
        for trials in ([(1000, [0, 2000])], [(1000, [2000, 1000])],
                       [(1000, [2000, 2000]), (-1000, [2000, -2000])],
                       [(1000, [2000, 2000]), (1000, [3000, 2000])]):
            with self.subTest(trials=trials), self.assertRaises(ValueError):
                cal.derive_wheels(trials)

    def test_unmeasured_invalid_configs_rejected(self):
        for field, value in (("measurements_verified", False), ("track_mm", 0),
                             ("wheel_mm_per_edge", float("nan")),
                             ("motor_sign", [1, 0]), ("cruise_mm_s", 351),
                             ("body_length_mm", float("inf"))):
            config = measured_example()
            config[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                cal.pack_config(config)
        for black, white, polarity in ((10, 100, True), (300, 3300, False), (-1, 3300, True)):
            config = measured_example()
            config["ir"][0] = dict(black=black, white=white, b_black_low=polarity)
            with self.assertRaises(ValueError):
                cal.pack_config(config)

    def test_patch_preserves_program_across_block_boundary(self):
        template, manifest = template_fixture()
        packed = cal.pack_config(measured_example())
        result = cal.patch_uf2(template, manifest, measured_example())
        expected = bytearray(template)
        expected[32 + 240:32 + 256] = packed[:16]
        expected[512 + 32:512 + 32 + 32] = packed[16:]
        self.assertEqual(bytes(expected), result)
        self.assertEqual(len(result), len(template))

    def test_bad_template_and_mapping_rejected(self):
        template, manifest = template_fixture()
        for key, value in (("sha256", "bad"), ("address", 0x20000000),
                           ("original_hex", bytes([255] * 48).hex()),
                           ("address", 0x10001000)):
            changed = dict(manifest, **{key: value})
            with self.subTest(key=key), self.assertRaises(ValueError):
                cal.patch_uf2(template, changed, measured_example())
        for offset, value in ((0, 0), (8, 0), (20 + 512, 0), (28, 0)):
            changed = bytearray(template)
            struct.pack_into("<I", changed, offset, value)
            changed = bytes(changed)
            changed_manifest = dict(manifest, sha256=hashlib.sha256(changed).hexdigest())
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                cal.patch_uf2(changed, changed_manifest, measured_example())

    def test_packed_object_accepted_by_actual_c_firmware(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            (directory / "config.bin").write_bytes(cal.pack_config(measured_example()))
            source = '''#include "robot.h"
#include <assert.h>
#include <stdio.h>
int main(int argc, char **argv) {
    robot_config_t config;
    assert(argc == 2);
    FILE *stream = fopen(argv[1], "rb"); assert(stream);
    assert(fread(&config, sizeof(config), 1, stream) == 1); fclose(stream);
    assert(sizeof(config) == 48); assert(robot_config_valid(&config));
    assert(config.wheel_mm_per_edge == 0.5f && config.track_mm == 140.0f);
    assert(config.encoder_sign[1] == -1 && config.motor_sign[0] == -1);
    assert(config.ir[1].black == 3200 && !config.ir[1].b_black_low);
    assert(config.ir[2].white == 3100 && config.b_calibrated);
    config.b_calibrated = false; assert(!robot_config_valid(&config));
    return 0;
}'''
            (directory / "verify.c").write_text(source)
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "app_program"),
                            str(directory / "verify.c"), str(ROOT / "app_program/subsystem2/motion.c"),
                            "-lm", "-o", str(directory / "verify")], check=True)
            subprocess.run([str(directory / "verify"), str(directory / "config.bin")], check=True)


if __name__ == "__main__":
    unittest.main()
