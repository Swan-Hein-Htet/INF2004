"""Decode version-2 car telemetry; no third-party dependencies required."""
import argparse
import json

FIELDS = (
    ("version", 4, False), ("sequence", 8, False), ("mission", 4, False),
    ("motion", 3, False), ("left_mm_s", 10, True), ("right_mm_s", 10, True),
    ("left_encoder", 16, False), ("right_encoder", 16, False),
    ("line_state", 2, False), ("line_error", 8, True), ("barcode", 5, False),
    ("reject", 3, False), ("hump_mm", 7, False), ("width_mm", 10, False),
    ("closest_mm", 9, False), ("bearing_index", 5, False),
    ("bypass", 2, False), ("distance_mm", 15, False),
    ("motion_class", 3, False), ("turn_deg_s", 10, True),
    ("faults", 16, False), ("left_clearance_mm", 10, False),
    ("right_clearance_mm", 10, False), ("sample_overruns", 16, False),
)


def decode(payload):
    if len(payload) != 26:
        raise ValueError("expected 26 bytes")
    packed = int.from_bytes(payload, "little")
    values = {}
    offset = 0
    for name, width, signed in FIELDS:
        value = (packed >> offset) & ((1 << width) - 1)
        if signed and value & (1 << (width - 1)):
            value -= 1 << width
        values[name] = value
        offset += width
    if values["version"] != 2:
        raise ValueError("unsupported telemetry version")
    if packed >> offset:
        raise ValueError("nonzero reserved bits")
    if values["barcode"] > 26:
        raise ValueError("invalid barcode")
    values["barcode"] = chr(64 + values["barcode"]) if values["barcode"] else None
    return values


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hex_payload")
    args = parser.parse_args()
    print(json.dumps(decode(bytes.fromhex(args.hex_payload)), indent=2))
