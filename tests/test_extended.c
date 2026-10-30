/* Extended adversarial host tests. No physical hardware is simulated here. */
#include "robot.h"
#include "subsystem2/motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_random = UINT32_C(0x200430);

static uint32_t test_random(void)
{
    g_random ^= g_random << 13U;
    g_random ^= g_random >> 17U;
    g_random ^= g_random << 5U;
    return g_random;
}

static robot_config_t test_config(void)
{
    robot_config_t config = {
        .wheel_mm_per_edge = 1.0f, .track_mm = 140.0f,
        .body_width_mm = 150.0f, .body_length_mm = 200.0f,
        .obstacle_depth_mm = 200.0f, .cruise_mm_s = 200.0f,
        .encoder_sign = {1, 1}, .motor_sign = {1, 1},
        .ir = {{100U, 3900U, true}, {100U, 3900U, true},
               {100U, 3900U, true}}, .b_calibrated = true};
    return config;
}

static void test_motion_requests(void)
{
    uint32_t owner = 0U;
    uint32_t raw = 0U;
    for (owner = 0U; 2U > owner; ++owner)
    {
        for (raw = 0U; UINT16_MAX >= raw; ++raw)
        {
            motion_job_t job = {0};
            float left = 999.0f;
            float right = 999.0f;
            uint32_t command = raw & 7U;
            uint32_t argument = (raw >> 3U) & 4095U;
            bool expected = ((raw >> 15U) == owner) && (command <= 5U);
            expected = expected &&
                       (!((2U == command) || (3U == command)) ||
                        (180U >= argument)) &&
                       ((4U != command) || (350U >= argument));
            assert(expected == motion_begin(&job, (uint16_t)raw, 0U != owner,
                                             200.0f, 30.0f, 0U));
            if (expected)
            {
                (void)motion_step(&job, 200.0f, 30.0f, 10U, &left, &right);
                assert(isfinite(left) && isfinite(right));
                assert((350.0f >= fabsf(left)) && (350.0f >= fabsf(right)));
                if (5U == command)
                {
                    assert((0.0f == left) && (0.0f == right));
                }
            }
        }
    }
    puts("extended motion: 131072 request/owner combinations passed");
}

static void test_commands_and_buffers(void)
{
    uint32_t raw = 0U;
    uint32_t length = 0U;
    uint8_t sequence = 0U;
    bool seen = false;
    uint8_t command = 255U;
    for (raw = 0U; 256U > raw; ++raw)
    {
        uint8_t value = (uint8_t)raw;
        uint8_t same_sequence = value ^ 3U;
        seen = false;
        assert(robot_command(&value, 1U, &sequence, &seen, &command));
        assert(command == (value & 3U));
        assert(sequence == value >> 2U);
        assert(!robot_command(&value, 1U, &sequence, &seen, &command));
        assert(!robot_command(&same_sequence, 1U, &sequence, &seen, &command));
        for (length = 0U; 32U > length; ++length)
        {
            if (1U != length)
            {
                seen = false;
                sequence = 19U;
                command = 255U;
                assert(!robot_command(&value, length, &sequence, &seen, &command));
                assert(!seen && (19U == sequence) && (255U == command));
            }
        }
    }
    seen = false;
    for (raw = 0U; 256U > raw; ++raw)
    {
        uint8_t value = (uint8_t)(((raw % 64U) << 2U) | (raw % 4U));
        assert(robot_command(&value, 1U, &sequence, &seen, &command));
    }
    assert(!robot_command(NULL, 1U, &sequence, &seen, &command));
    for (length = 0U; 65U > length; ++length)
    {
        uint8_t buffer[68];
        uint32_t fields[24] = {0U};
        uint32_t i = 0U;
        memset(buffer, 0xa5, sizeof(buffer));
        for (i = 0U; 24U > i; ++i)
        {
            fields[i] = test_random();
        }
        assert((26U <= length ? 26U : 0U) ==
               robot_pack(&buffer[1], length, fields));
        assert(0xa5U == buffer[0]);
        for (i = (26U <= length ? 27U : 1U); sizeof(buffer) > i; ++i)
        {
            assert(0xa5U == buffer[i]);
        }
        if (26U <= length)
        {
            assert(0U == (buffer[26] & 0xfcU));
        }
    }
    puts("extended protocol: all 256 bytes, duplicates, four sequence wraps, 7936 invalid lengths and 65 buffer capacities passed");
}

static void test_invalid_odometry(void)
{
    robot_config_t config = test_config();
    float const invalid[] = {NAN, INFINITY, -INFINITY};
    uint32_t mode = 0U;
    uint32_t axis = 0U;
    uint32_t value = 0U;
    for (mode = ROBOT_IDLE; ROBOT_FAULT >= mode; ++mode)
    {
        for (axis = 0U; 2U > axis; ++axis)
        {
            for (value = 0U; 3U > value; ++value)
            {
                robot_nav_t nav = {0};
                robot_input_t input = {0};
                nav.mode = (robot_mode_t)mode;
                nav.b_start = true;
                nav.b_reset = true;
                nav.b_end = true;
                nav.goal_yaw_deg = 90.0f;
                nav.goal_distance_mm = 200.0f;
                input.line_word = 0x100U;
                input.distance_mm = (0U == axis) ? invalid[value] : 0.0f;
                input.yaw_deg = (1U == axis) ? invalid[value] : 0.0f;
                robot_nav_step(&nav, &input, &config);
                assert(ROBOT_FAULT == nav.mode);
                assert(0U != (nav.fault_request & ROBOT_FAULT_ENCODER));
                assert((0.0f == nav.left_mm_s) && (0.0f == nav.right_mm_s));
                assert(!nav.b_start && !nav.b_reset && !nav.b_end);
            }
        }
    }
    puts("extended odometry: NaN and +/-infinity rejected in all eight modes (48 cases)");
}

static void test_zero_speed(void)
{
    int32_t yaw = 0;
    for (yaw = -180; 180 >= yaw; yaw += 15)
    {
        motion_job_t job = {0};
        float left = 999.0f;
        float right = 999.0f;
        assert(motion_begin(&job, motion_pack(MOTION_SPEED, 0U, false),
                            false, 0.0f, 0.0f, 0U));
        (void)motion_step(&job, 0.0f, (float)yaw, 10U, &left, &right);
        assert((0.0f == left) && (0.0f == right));
    }
    puts("extended zero-speed: no rotation for 25 heading offsets");
}

static void test_critical_end(void)
{
    robot_config_t config = test_config();
    uint32_t mode = 0U;
    uint32_t bit = 0U;
    for (mode = ROBOT_IDLE; ROBOT_FAULT >= mode; ++mode)
    {
        for (bit = 0U; 11U > bit; ++bit)
        {
            uint16_t mask = (uint16_t)(1U << bit);
            robot_nav_t nav = {0};
            robot_input_t input = {0};
            if (0U == (mask & ROBOT_CRITICAL_MASK))
            {
                continue;
            }
            nav.mode = (robot_mode_t)mode;
            nav.b_end = true;
            nav.b_start = true;
            nav.b_reset = true;
            input.faults = mask;
            robot_nav_step(&nav, &input, &config);
            assert(ROBOT_FAULT == nav.mode);
            assert((0.0f == nav.left_mm_s) && (0.0f == nav.right_mm_s));
        }
    }
    /* A clean END is still allowed, including with only a noncritical fault. */
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    nav.mode = ROBOT_FOLLOW;
    nav.b_end = true;
    input.faults = ROBOT_FAULT_WIFI;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FINISHED == nav.mode);
    assert((0.0f == nav.left_mm_s) && (0.0f == nav.right_mm_s));
    puts("extended command priority: all ten critical fault bits beat END in eight modes (80 cases)");
}

static void test_sensor_stress(void)
{
    robot_config_t config = test_config();
    robot_barcode_t reader = {0};
    uint32_t tick = 0U;
    for (tick = 0U; 1000000U > tick; ++tick)
    {
        uint32_t value = test_random();
        uint16_t word = robot_line_word(config.ir, (uint16_t)(value & 4095U),
                                       (uint16_t)((value >> 12U) & 4095U),
                                       (uint8_t)value);
        char barcode = robot_barcode_sample(&reader, 0U != (value & 1U),
                                            0U != (value & 0xfff0U));
        int32_t error = (int8_t)(uint8_t)word;
        assert((-127 <= error) && (127 >= error));
        assert((1U <= ((word >> 8U) & 3U)) && (3U >= ((word >> 8U) & 3U)));
        assert(('\0' == barcode) || (('A' <= barcode) && ('Z' >= barcode)));
        assert(ROBOT_BAR_ELEMENTS >= reader.count);
        assert(254U >= reader.run_ms);
    }
    for (tick = 0U; 10000U > tick; ++tick)
    {
        robot_profile_t profile = {0};
        uint32_t point = 0U;
        for (point = 0U; ROBOT_SCAN_POINTS > point; ++point)
        {
            profile.range_mm[point] = (uint16_t)(test_random() % 4501U);
        }
        if (robot_profile_build(&profile, &config))
        {
            assert((0U < profile.width_mm) && (1023U >= profile.width_mm));
            assert((20U <= profile.closest_mm) && (600U > profile.closest_mm));
            assert((3U <= profile.bearing) && (15U >= profile.bearing));
        }
        profile.range_mm[tick % ROBOT_SCAN_POINTS] = (0U == (tick & 1U)) ? 0U : 4001U;
        assert(!robot_profile_build(&profile, &config));
        assert(!profile.b_valid);
    }
    puts("extended sensors: 1000000 streaming ticks, 10000 scan profiles and 10000 injected invalid ranges passed (sanitizers)");
}

static void test_navigation_stress(void)
{
    robot_config_t config = test_config();
    uint32_t tick = 0U;
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    input.b_pitch_valid = true;
    input.b_front_valid = true;
    input.front_mm = 1000U;
    for (tick = 0U; 100000U > tick; ++tick)
    {
        uint32_t random = test_random();
        input.now_ms += 10U;
        input.distance_mm += 1.0f;
        input.line_word = (uint16_t)(((random % 3U) + 1U) << 8U);
        input.barcode = (0U == random % 37U) ? (char)('A' + random % 26U) : '\0';
        input.faults = (0U == random % 101U) ? ROBOT_FAULT_STOP : 0U;
        nav.b_start = 0U == random % 7U;
        nav.b_reset = 0U == random % 11U;
        nav.b_end = 0U == random % 113U;
        robot_nav_step(&nav, &input, &config);
        assert(isfinite(nav.left_mm_s) && isfinite(nav.right_mm_s));
        assert((350.0f >= fabsf(nav.left_mm_s)) && (350.0f >= fabsf(nav.right_mm_s)));
        if ((0U != input.faults) || (ROBOT_IDLE == nav.mode) ||
            (ROBOT_FAULT == nav.mode) || (ROBOT_FINISHED == nav.mode))
        {
            assert((0.0f == nav.left_mm_s) && (0.0f == nav.right_mm_s));
        }
    }
    puts("extended navigation: 100000 deterministic mixed-input steps passed");
}

int main(int argc, char **argv)
{
    (void)setvbuf(stdout, NULL, _IONBF, 0U);
#define RUN(name, function) if ((1 == argc) || (0 == strcmp(argv[1], name))) { function(); }
    RUN("motion", test_motion_requests)
    RUN("commands", test_commands_and_buffers)
    RUN("invalid_odometry", test_invalid_odometry)
    RUN("zero_speed", test_zero_speed)
    RUN("critical_end", test_critical_end)
    RUN("sensors", test_sensor_stress)
    RUN("navigation", test_navigation_stress)
#undef RUN
    return 0;
}
