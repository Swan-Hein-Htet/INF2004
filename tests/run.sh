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
sh tests/run_startup.sh
cc -std=c11 -Wall -Wextra -Werror -DAPP_ASSUMED_DEMO=1 -DAPP_TEST=0 \
    -ffunction-sections -fdata-sections -Itests/stubs -Iapp_program \
    tests/test_assumed_demo.c app_program/robot_config.c \
    app_program/subsystem2/motion.c -Wl,--gc-sections -lm -o tests/build/test_assumed_demo
./tests/build/test_assumed_demo
