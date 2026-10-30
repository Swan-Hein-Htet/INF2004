#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./tests/run.sh
python3 tests/test_barcode_oracle.py
python3 tests/test_barcode_stream.py
python3 tests/test_protocol.py
python3 tests/test_guided_calibration.py
