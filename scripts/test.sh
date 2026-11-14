#!/usr/bin/env bash
# VUKANG OS automated boot test: boots in QEMU headless, types commands over
# the serial port and checks the output. Usage: bash scripts/test.sh
set -e
cd "$(dirname "$0")/.."
python3 scripts/qemu_test.py
