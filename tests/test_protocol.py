"""Exercise the C encoder against the independent Python decoder."""
import importlib.util
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("protocol", ROOT / "tools/decode_telemetry.py")
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)
with tempfile.TemporaryDirectory() as directory:
    library = pathlib.Path(directory) / "protocol.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-I" + str(ROOT / "app_program"),
                    str(ROOT / "app_program/subsystem1/protocol.c"), "-o", str(library)], check=True)
    import ctypes
    import random
    native = ctypes.CDLL(str(library))
    native.robot_pack.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                                 ctypes.POINTER(ctypes.c_uint32)]
    native.robot_pack.restype = ctypes.c_size_t
    generator = random.Random(2004)
    for trial in range(2000):
        expected = {}
        encoded = []
        for name, width, signed in protocol.FIELDS:
            raw = generator.randrange(1 << width)
            if name == "version": raw = 2
            if name == "barcode": raw %= 27
            encoded.append(raw)
            value = raw - (1 << width) if signed and raw & (1 << (width - 1)) else raw
            expected[name] = value
        expected["barcode"] = chr(64 + expected["barcode"]) if expected["barcode"] else None
        buffer = (ctypes.c_uint8 * 26)()
        fields = (ctypes.c_uint32 * 24)(*encoded)
        assert native.robot_pack(buffer, 26, fields) == 26
        assert protocol.decode(bytes(buffer)) == expected
    for invalid in (b"", bytes(26), bytes(25), bytes(27)):
        try:
            protocol.decode(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid packet accepted")
print("protocol: 2000 independently decoded C packets and bad version/length cases passed")
