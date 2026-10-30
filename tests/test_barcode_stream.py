"""Independent golden symbols replayed through the C streaming API.

The temporary C bridge avoids millions of Python FFI calls; it contains no
decoder or Code 39 patterns. Expected letters come from the golden fixtures.
"""
import ctypes
import json
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = r'''
#include "robot.h"
char replay(uint8_t const *widths, int fault)
{
    robot_barcode_t reader = {0};
    char result = '\0';
    unsigned int i, tick;
    unsigned int lead = fault == 4 ? 50U : 120U;
    unsigned int elements = fault == 3 ? 20U : 29U;
    for (tick = 0U; tick < lead; ++tick)
        if (robot_barcode_sample(&reader, false, true)) return '!';
    for (i = 0U; i < elements; ++i)
        for (tick = 0U; tick < widths[i]; ++tick)
            if (robot_barcode_sample(&reader, i % 2U == 0U,
                !(fault == 1 && i == 14U && tick == 0U))) return '!';
    if (fault == 2)
    {
        for (tick = 0U; tick < 5U; ++tick)
            if (robot_barcode_sample(&reader, false, true)) return '!';
        if (robot_barcode_sample(&reader, true, true)) return '!';
    }
    for (tick = 0U; tick < 120U; ++tick)
    {
        char value = robot_barcode_sample(&reader, false, true);
        if (value)
        {
            if (result) return '!';
            result = value;
        }
    }
    return result;
}
'''

fixtures = json.loads((ROOT / "tests/code39_golden.json").read_text())
generator = random.Random(2039)
positive = negative = 0
with tempfile.TemporaryDirectory() as directory:
    bridge = Path(directory) / "bridge.c"
    library = Path(directory) / "stream.so"
    bridge.write_text(BRIDGE)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-shared", "-fPIC", "-I" + str(ROOT / "app_program"),
                    str(bridge), str(ROOT / "app_program/subsystem3/sensing.c"),
                    "-o", str(library)], check=True)
    native = ctypes.CDLL(str(library))
    native.replay.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int]
    native.replay.restype = ctypes.c_char
    for char, elements in fixtures.items():
        for narrow in (9, 10, 12, 15, 20, 25, 30):
            for reverse in (False, True):
                for repeat in range(20):
                    widths = [round(narrow * (1 if element == 1 else 3) *
                                    generator.uniform(0.9, 1.1)) for element in elements]
                    if reverse:
                        widths.reverse()
                    frame = (ctypes.c_uint8 * 29)(*widths)
                    assert native.replay(frame, 0).decode() == char, (char, widths)
                    positive += 1
                    if repeat == 0:
                        for fault in (1, 2, 3, 4):
                            assert native.replay(frame, fault) == b"\0", (char, fault)
                            negative += 1
print(f"barcode stream oracle: {positive} jittered forward/reverse symbols; "
      f"{negative} line-departure, trailing-mark, partial-frame and short-quiet cases rejected")
