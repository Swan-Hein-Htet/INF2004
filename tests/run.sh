#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
cc -std=c11 -g -O1 -Wall -Wextra -Werror -Wconversion -Wshadow \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Iapp_program tests/test_robot.c app_program/subsystem1/mission.c \
    app_program/subsystem1/protocol.c app_program/subsystem2/motion.c \
    app_program/subsystem3/sensing.c app_program/subsystem4/terrain.c \
    app_program/subsystem5/obstacle.c -lm -o tests/build/test_robot
ASAN_OPTIONS=detect_leaks=0 ./tests/build/test_robot
