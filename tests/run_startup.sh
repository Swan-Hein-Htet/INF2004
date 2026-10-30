#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
for mode in 0 1 2 3; do
    cc -std=c11 -O1 -g -Wall -Wextra -Wconversion -Wshadow -Werror \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -DAPP_TEST=1 -DAPP_SERVO_TEST="$mode" -Itests/stubs -Iapp_program \
        tests/test_startup.c app_program/app_main.c app_program/subsystem2/board.c \
        app_program/robot_config.c app_program/subsystem1/mission.c \
        app_program/subsystem1/protocol.c app_program/subsystem2/motion.c \
        app_program/subsystem3/sensing.c app_program/subsystem4/terrain.c \
        app_program/subsystem5/obstacle.c -lm -o "tests/build/startup_${mode}"
    if [ "$mode" -eq 0 ]; then
        for scenario in healthy imu-fail mutex-fail safety-fail safety-start-fail task-fail diagnostic-priority-fail pwm servo-range; do
            ASAN_OPTIONS=detect_leaks=0 "tests/build/startup_${mode}" "$scenario"
        done
    elif [ "$mode" -eq 3 ]; then
        for scenario in bench-pulses bench-held bench-release bench-estop imu-fail; do
            ASAN_OPTIONS=detect_leaks=0 "tests/build/startup_${mode}" "$scenario"
        done
    else
        ASAN_OPTIONS=detect_leaks=0 "tests/build/startup_${mode}" isolation
        ASAN_OPTIONS=detect_leaks=0 "tests/build/startup_${mode}" isolation-reset-fail
    fi
done
