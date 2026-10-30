#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
for suite in robot extended; do
    cc -std=c11 -g -O1 -Wall -Wextra -Werror -Wconversion -Wshadow \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -Iapp_program "tests/test_${suite}.c" app_program/subsystem1/mission.c \
        app_program/subsystem1/protocol.c app_program/subsystem2/motion.c \
        app_program/subsystem3/sensing.c app_program/subsystem4/terrain.c \
        app_program/subsystem5/obstacle.c -lm -o "tests/build/test_${suite}"
    ASAN_OPTIONS=detect_leaks=0 "./tests/build/test_${suite}"
done
cc -std=c11 -g -O1 -Wall -Wextra -Werror -Wconversion -Wshadow \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Itests/stubs -Iapp_program tests/test_mqtt.c \
    app_program/subsystem1/protocol.c -o tests/build/test_mqtt
ASAN_OPTIONS=detect_leaks=0 ./tests/build/test_mqtt
