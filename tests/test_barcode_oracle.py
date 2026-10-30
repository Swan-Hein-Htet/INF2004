"""Decode checked-in python-barcode 0.16.1 golden symbols independently."""
import ctypes
import json
import pathlib
import random
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
fixtures = json.loads((ROOT / "tests/code39_golden.json").read_text())
with tempfile.TemporaryDirectory() as directory:
    library = pathlib.Path(directory) / "barcode.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-I" + str(ROOT / "app_program"),
                    str(ROOT / "app_program/subsystem3/sensing.c"), "-o", str(library)], check=True)
    native = ctypes.CDLL(str(library))
    native.robot_barcode_decode.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    native.robot_barcode_decode.restype = ctypes.c_char
    generator = random.Random(39)
    cases = 0
    for char, elements in fixtures.items():
        for narrow in (9, 10, 12, 15, 20, 25, 30):
            for reverse in (False, True):
                for repeat in range(20):
                    widths = [round(narrow * (1 if element == 1 else 3) *
                                    generator.uniform(0.9, 1.1)) for element in elements]
                    if reverse: widths.reverse()
                    frame = (ctypes.c_uint8 * 29)(*widths)
                    assert native.robot_barcode_decode(frame, 29).decode() == char
                    cases += 1
    for trial in range(10000):
        frame = (ctypes.c_uint8 * 29)(*(generator.randrange(256) for _ in range(29)))
        assert native.robot_barcode_decode(frame, 29) == b"\0"
print(f"barcode oracle: {cases} independent jittered forward/reverse symbols; 10000 noise frames rejected")
