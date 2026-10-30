#!/bin/sh
set -eu
cd "$(dirname "$0")/../build_make"
profile=${1:-offline}
case "$profile" in
    offline) make SMP=0 CONSOLE=uart WIFI=none -j4 ;;
    assumed-demo) make SMP=0 CONSOLE=uart WIFI=none DEMO=1 -j4 ;;
    diagnostics) make SMP=0 CONSOLE=uart WIFI=none TEST=1 -j4 ;;
    diagnostics-usb) make SMP=0 CONSOLE=usb_cdc WIFI=none TEST=1 -j4 ;;
    servo-off) make SMP=0 CONSOLE=uart WIFI=none TEST=1 SERVO_TEST=1 -j4 ;;
    servo-hold) make SMP=0 CONSOLE=uart WIFI=none TEST=1 SERVO_TEST=2 -j4 ;;
    motor-bench) make SMP=0 CONSOLE=usb_cdc WIFI=none TEST=1 SERVO_TEST=3 -j4 ;;
    network) make SMP=0 CONSOLE=uart WIFI=cyw43 NET=lwip MQTT=1 -j4 ;;
    *) echo 'Use offline, assumed-demo, diagnostics, diagnostics-usb, network, servo-off, servo-hold or motor-bench' >&2; exit 2 ;;
esac
