#!/bin/sh
set -eu
cd "$(dirname "$0")/../build_make"
profile=${1:-offline}
case "$profile" in
    offline) make SMP=0 CONSOLE=uart WIFI=none -j4 ;;
    diagnostics) make SMP=0 CONSOLE=uart WIFI=none TEST=1 -j4 ;;
    diagnostics-usb) make SMP=0 CONSOLE=usb_cdc WIFI=none TEST=1 -j4 ;;
    servo-off) make SMP=0 CONSOLE=uart WIFI=none TEST=1 SERVO_TEST=1 -j4 ;;
    servo-hold) make SMP=0 CONSOLE=uart WIFI=none TEST=1 SERVO_TEST=2 -j4 ;;
    network) make SMP=0 CONSOLE=uart WIFI=cyw43 NET=lwip MQTT=1 -j4 ;;
    *) echo 'Use offline, diagnostics, diagnostics-usb, network, servo-off or servo-hold' >&2; exit 2 ;;
esac
