/* Deterministic host regressions, including rejection and fault paths. */
#include "robot.h"
#include "subsystem2/motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t const g_letter_patterns[26] = {
    0x109U, 0x049U, 0x148U, 0x019U, 0x118U, 0x058U, 0x00dU, 0x10cU, 0x04cU,
    0x01cU, 0x103U, 0x043U, 0x142U, 0x013U, 0x112U, 0x052U, 0x007U, 0x106U,
    0x046U, 0x016U, 0x181U, 0x0c1U, 0x1c0U, 0x091U, 0x190U, 0x0d0U};

static robot_config_t test_config(void)
{
    robot_config_t config = {0};
    config.wheel_mm_per_edge = 1.0f;
    config.track_mm = 140.0f;
    config.body_width_mm = 150.0f;
    config.body_length_mm = 200.0f;
    config.obstacle_depth_mm = 200.0f;
    config.cruise_mm_s = 200.0f;
    config.encoder_sign[0] = 1;
    config.encoder_sign[1] = 1;
    config.motor_sign[0] = 1;
    config.motor_sign[1] = 1;
    config.ir[0] = (robot_ir_cal_t){100U, 3900U, true};
    config.ir[1] = config.ir[0];
    config.ir[2] = config.ir[0];
    config.b_calibrated = true;
    return config;
}

static void test_symbol(uint8_t *p_widths, uint16_t pattern, uint8_t narrow)
{
    uint32_t index = 0U;
    for (index = 0U; 9U > index; ++index)
    {
        p_widths[index] = (0U != (pattern & (1U << (8U - index))))
                              ? (uint8_t)(narrow * 3U)
                              : narrow;
    }
}

static void test_frame(uint8_t *p_widths, uint32_t letter, uint8_t narrow)
{
    test_symbol(p_widths, 0x094U, narrow);
    p_widths[9] = narrow;
    test_symbol(&p_widths[10], g_letter_patterns[letter], narrow);
    p_widths[19] = narrow;
    test_symbol(&p_widths[20], 0x094U, narrow);
}

static char test_stream(robot_barcode_t *p_reader, uint8_t const *p_frame,
                        bool b_extra)
{
    uint32_t index = 0U;
    uint32_t tick = 0U;
    char result = '\0';
    for (tick = 0U; 120U > tick; ++tick)
    {
        assert('\0' == robot_barcode_sample(p_reader, false, true));
    }
    for (index = 0U; ROBOT_BAR_ELEMENTS > index; ++index)
    {
        for (tick = 0U; p_frame[index] > tick; ++tick)
        {
            assert('\0' ==
                   robot_barcode_sample(p_reader, 0U == index % 2U, true));
        }
    }
    if (b_extra)
    {
        for (tick = 0U; 5U > tick; ++tick)
        {
            (void)robot_barcode_sample(p_reader, false, true);
        }
        (void)robot_barcode_sample(p_reader, true, true);
    }
    for (tick = 0U; 120U > tick; ++tick)
    {
        char value = robot_barcode_sample(p_reader, false, true);
        if ('\0' != value)
        {
            result = value;
        }
    }
    return result;
}

static void test_barcodes(void)
{
    uint32_t letter = 0U;
    uint32_t speed = 0U;
    for (letter = 0U; 26U > letter; ++letter)
    {
        for (speed = 8U; 31U > speed; ++speed)
        {
            uint8_t frame[ROBOT_BAR_ELEMENTS] = {0U};
            uint8_t reverse[ROBOT_BAR_ELEMENTS] = {0U};
            robot_barcode_t reader = {0};
            uint32_t index = 0U;
            test_frame(frame, letter, (uint8_t)speed);
            assert((char)('A' + letter) ==
                   robot_barcode_decode(frame, sizeof(frame)));
            assert((char)('A' + letter) == test_stream(&reader, frame, false));
            memset(&reader, 0, sizeof(reader));
            assert('\0' == test_stream(&reader, frame, true));
            for (index = 0U; ROBOT_BAR_ELEMENTS > index; ++index)
            {
                reverse[index] = frame[ROBOT_BAR_ELEMENTS - 1U - index];
            }
            assert((char)('A' + letter) ==
                   robot_barcode_decode(reverse, sizeof(reverse)));
            memset(&reader, 0, sizeof(reader));
            assert((char)('A' + letter) == test_stream(&reader, reverse, false));
            frame[0] = 255U;
            assert('\0' == robot_barcode_decode(frame, sizeof(frame)));
            assert('\0' == robot_barcode_decode(reverse, sizeof(reverse) - 1U));
        }
    }
    assert('\0' == robot_barcode_decode(NULL, 29U));
    puts("barcode: 26 letters, 23 widths, both directions, stream + malformed "
         "frames passed");
}

static void test_navigation(void)
{
    robot_config_t config = test_config();
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    uint32_t index = 0U;
    input.line_word = 0x100U;
    input.b_pitch_valid = true;
    input.b_front_valid = true;
    input.front_mm = 1000U;
    nav.b_start = true;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FOLLOW == nav.mode);
    input.barcode = 'A';
    input.distance_mm = 50.0f;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FOLLOW == nav.mode);
    assert('A' == nav.pending);
    input.barcode = '\0';
    input.line_word = 0x200U;
    for (index = 0U; 3U > index; ++index)
    {
        robot_nav_step(&nav, &input, &config);
    }
    assert(ROBOT_FOLLOW == nav.mode);
    input.line_word = 0x100U;
    input.distance_mm += 18.0f;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_TURN == nav.mode);
    assert(90.0f == nav.goal_yaw_deg);
    input.yaw_deg = 90.0f;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FOLLOW == nav.mode);
    input.line_word = 0x100U;
    input.barcode = 'Z';
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FOLLOW == nav.mode);
    input.barcode = 'B';
    robot_nav_step(&nav, &input, &config);
    input.barcode = '\0';
    input.distance_mm += 501.0f;
    robot_nav_step(&nav, &input, &config);
    assert('\0' == nav.pending);
    input.front_mm = 150U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_SCAN == nav.mode);
    input.b_profile_ready = true;
    input.profile.b_valid = true;
    input.profile.width_mm = 150U;
    input.profile.closest_mm = 150U;
    input.profile.left_mm = 1000U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_BYPASS == nav.mode);
    input.faults = ROBOT_FAULT_STOP;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FAULT == nav.mode);
    assert(0.0f == nav.left_mm_s);
    assert(0.0f == nav.right_mm_s);
    nav.b_reset = true;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FAULT == nav.mode);
    input.faults = 0U;
    nav.b_reset = true;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_IDLE == nav.mode);
    puts("mission: pending junction command, expiry, unknown letter, bypass, "
         "STOP/reset passed");
}

static void test_bypass_side(int8_t side)
{
    robot_config_t config = test_config();
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    uint32_t index = 0U;
    nav.mode = ROBOT_SCAN;
    nav.pending = 'A';
    nav.pending_budget_mm = 400.0f;
    input.b_profile_ready = true;
    input.profile.b_valid = true;
    input.profile.width_mm = 150U;
    input.profile.closest_mm = 150U;
    input.profile.left_mm = (0 < side) ? 1000U : 500U;
    input.profile.right_mm = (0 < side) ? 500U : 1000U;
    input.front_mm = 1000U;
    input.b_front_valid = true;
    input.line_word = 0x300U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_BYPASS == nav.mode);
    assert(side == nav.side);
    assert(90.0f * side == nav.goal_yaw_deg);
    for (index = 0U; 6U > index; ++index)
    {
        input.yaw_deg = nav.goal_yaw_deg;
        if (0U != (nav.leg & 1U))
        {
            input.distance_mm = nav.goal_distance_mm;
        }
        input.now_ms += 500U;
        robot_nav_step(&nav, &input, &config);
    }
    assert(ROBOT_RECOVER == nav.mode);
    assert(400.0f == nav.pending_budget_mm);
    input.distance_mm += 30.0f;
    input.line_word = 0x200U;
    robot_nav_step(&nav, &input, &config);
    input.distance_mm += 5.0f;
    input.line_word = 0x300U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_RECOVER == nav.mode);
    input.distance_mm += 5.0f;
    input.line_word = 0x200U;
    robot_nav_step(&nav, &input, &config);
    input.distance_mm += 18.0f;
    input.line_word = 0x100U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_TURN == nav.mode);
    input.yaw_deg = nav.goal_yaw_deg;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FOLLOW == nav.mode);
    assert(0.0f == input.yaw_deg);
    assert('A' == nav.pending);
    input.front_mm = 79U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FAULT == nav.mode);
    assert(0.0f == nav.left_mm_s);
    memset(&nav, 0, sizeof(nav));
    nav.mode = ROBOT_FOLLOW;
    nav.pending = 'A';
    nav.pending_budget_mm = 500.0f;
    input.front_mm = 1000U;
    input.distance_mm = 0.0f;
    input.line_word = 0x200U;
    robot_nav_step(&nav, &input, &config);
    input.distance_mm = 61.0f;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FINISHED == nav.mode);
    puts("bypass: full route, heading restoration, pending freeze, false-line "
         "rejection, proximity and END passed");
}

static void test_junction_noise(void)
{
    robot_config_t config = test_config();
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    uint32_t index = 0U;
    nav.mode = ROBOT_FOLLOW;
    nav.pending = 'A';
    nav.pending_budget_mm = 500.0f;
    for (index = 0U; 3U > index; ++index)
    {
        input.line_word = 0x200U;
        robot_nav_step(&nav, &input, &config);
        input.line_word = 0x100U;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FOLLOW == nav.mode);
        assert('A' == nav.pending);
    }
    /* Only an uninterrupted three-observation black run is a junction. */
    input.line_word = 0x200U;
    for (index = 0U; 3U > index; ++index)
    {
        robot_nav_step(&nav, &input, &config);
    }
    input.line_word = 0x100U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_TURN == nav.mode);
    assert(90.0f == nav.goal_yaw_deg);
    puts("junction: isolated black noise does not consume a pending turn passed");
}

static void test_line_loss_recovery(void)
{
    robot_config_t config = test_config();
    int32_t side = 0;
    for (side = -1; 1 >= side; ++side)
    {
        robot_nav_t nav = {0};
        robot_input_t input = {0};
        nav.mode = ROBOT_FOLLOW;
        nav.side = (int8_t)side; /* May retain a side from an earlier bypass. */
        nav.pending = 'B';
        nav.pending_budget_mm = 300.0f;
        input.now_ms = 100U;
        input.line_word = 0x300U;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_RECOVER == nav.mode);
        input.distance_mm = 10.0f;
        input.line_word = 0x100U;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FOLLOW == nav.mode);
        assert(0.0f == nav.left_mm_s);
        assert(0.0f == nav.right_mm_s);
        assert('B' == nav.pending);
        assert(300.0f == nav.pending_budget_mm);
    }
    /* Seeing a line after the search budget expires must not resume motion. */
    for (side = 0; 2 > side; ++side)
    {
        robot_nav_t nav = {0};
        robot_input_t input = {0};
        nav.mode = ROBOT_RECOVER;
        nav.goal_distance_mm = 80.0f;
        input.line_word = 0x100U;
        input.now_ms = (0 == side) ? 8001U : 100U;
        input.distance_mm = (0 == side) ? 10.0f : 81.0f;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FAULT == nav.mode);
        assert(0U != (nav.fault_request & ROBOT_FAULT_LINE));
        assert(0.0f == nav.left_mm_s);
        assert(0.0f == nav.right_mm_s);
    }
    puts("recovery: ordinary line reacquisition resumes without a bypass turn passed");
}

static void test_safety_boundaries(void)
{
    robot_config_t config = test_config();
    uint32_t mode = 0U;
    for (mode = ROBOT_IDLE; ROBOT_FAULT >= mode; ++mode)
    {
        robot_nav_t nav = {0};
        robot_input_t input = {0};
        nav.mode = (robot_mode_t)mode;
        nav.left_mm_s = 200.0f;
        nav.right_mm_s = 200.0f;
        nav.b_start = true;
        nav.b_reset = true;
        input.faults = ROBOT_FAULT_STOP;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FAULT == nav.mode);
        assert(0.0f == nav.left_mm_s);
        assert(0.0f == nav.right_mm_s);
        input.faults = 0U;
        input.b_pitch_valid = true;
        input.b_front_valid = true;
        input.front_mm = 1000U;
        nav.b_reset = true;
        nav.b_start = true;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_IDLE == nav.mode); /* RESET needs a later START. */
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_IDLE == nav.mode);
        nav.b_start = true;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FOLLOW == nav.mode);
        input.front_mm = 79U;
        robot_nav_step(&nav, &input, &config);
        assert(ROBOT_FAULT == nav.mode);
        assert(0.0f == nav.left_mm_s);
        assert(0.0f == nav.right_mm_s);
    }
    /* Unsigned elapsed time must still expire a turn across uptime wrap. */
    robot_nav_t nav = {0};
    robot_input_t input = {0};
    nav.mode = ROBOT_TURN;
    nav.state_since_ms = UINT32_MAX - 100U;
    nav.goal_yaw_deg = 90.0f;
    input.now_ms = 12000U;
    robot_nav_step(&nav, &input, &config);
    assert(ROBOT_FAULT == nav.mode);
    assert(0U != (nav.fault_request & ROBOT_FAULT_BYPASS));
    assert(0.0f == nav.left_mm_s);
    assert(0.0f == nav.right_mm_s);
    puts("safety: STOP in every mode, separate RESET/START, proximity and timer wrap passed");
}

static void test_motion_api(void)
{
    motion_job_t job = {0};
    float left = 0.0f;
    float right = 0.0f;
    uint16_t request = motion_pack(MOTION_BACKWARD, 100U, false);
    assert(!motion_begin(&job, request, true, 0.0f, 0.0f, 0U));
    assert(motion_begin(&job, request, false, 0.0f, 0.0f, 0U));
    assert(!motion_step(&job, 0.0f, 0.0f, 10U, &left, &right));
    assert(0.0f > left);
    assert(motion_step(&job, -100.0f, 0.0f, 20U, &left, &right));
    assert(0.0f == left);
    request = motion_pack(MOTION_RIGHT, 90U, false);
    assert(motion_begin(&job, request, false, 0.0f, 0.0f, 0U));
    assert(!motion_step(&job, 0.0f, 0.0f, 10U, &left, &right));
    assert((0.0f < left) && (0.0f > right));
    assert(motion_step(&job, 0.0f, -90.0f, 20U, &left, &right));
    assert(!motion_begin(&job, motion_pack(MOTION_LEFT, 181U, false), false,
                         0.0f, 0.0f, 0U));
    request = motion_pack(MOTION_SPEED, 200U, false);
    assert(motion_begin(&job, request, false, 0.0f, 0.0f, 0U));
    assert(motion_step(&job, 0.0f, 0.0f, 30001U, &left, &right));
    puts("motion API: source ownership, reverse, right turn, bounds and "
         "timeout passed");
}

static void test_sensors(void)
{
    robot_config_t config = test_config();
    robot_profile_t profile = {0};
    robot_terrain_t terrain = {0};
    robot_pid_t pid = {0.0f, 0.0f, 0.1f, 0.1f, 0.0f};
    uint32_t index = 0U;
    assert(robot_config_valid(&config));
    config.b_calibrated = false;
    assert(!robot_config_valid(&config));
    assert(2U == ((robot_line_word(config.ir, 100U, 100U, 0U) >> 8U) & 3U));
    assert(3U == ((robot_line_word(config.ir, 3900U, 3900U, 0U) >> 8U) & 3U));
    assert(0.8f == robot_pid_step(&pid, 200.0f, 0.0f, 0.02f, 0.8f));
    assert(0.0f == robot_pid_step(&pid, 0.0f, 0.0f, 0.02f, 0.8f));
    for (index = 0U; 200U > index; ++index)
    {
        robot_terrain_step(&terrain, 0.0f, 0.0f, 1.0f, 0.0f, true);
    }
    assert(terrain.b_ready);
    for (index = 0U; 100U > index; ++index)
    {
        robot_terrain_step(&terrain, -0.2f, 0.0f, 0.9799f, 1.0f, false);
    }
    assert(17.0f < terrain.peak_mm);
    assert(21.0f > terrain.peak_mm);
    for (index = 0U; ROBOT_SCAN_POINTS > index; ++index)
    {
        profile.range_mm[index] =
            ((4U <= index) && (8U >= index)) ? 200U : 2000U;
    }
    assert(robot_profile_build(&profile, &config));
    assert(200U == profile.closest_mm);
    assert(0U < profile.width_mm);
    profile.range_mm[1] = 0U;
    assert(!robot_profile_build(&profile, &config));
    puts("sensors: calibration gate, line, PID limits, terrain, scan rejection "
         "passed");
}

static void test_protocol(void)
{
    uint32_t fields[24] = {0U};
    uint8_t payload[ROBOT_TELEMETRY_BYTES + 1U] = {0U};
    uint8_t sequence = 0U;
    uint8_t command = 0U;
    uint8_t received = 5U;
    bool b_seen = false;
    fields[0] = 2U;
    fields[1] = 255U;
    fields[10] = 26U;
    payload[ROBOT_TELEMETRY_BYTES] = 0xa5U;
    assert(ROBOT_TELEMETRY_BYTES ==
           robot_pack(payload, sizeof(payload), fields));
    assert(0xa5U == payload[ROBOT_TELEMETRY_BYTES]);
    assert(0xf2U == payload[0]);
    assert(0U == robot_pack(payload, 1U, fields));
    assert(robot_command(&received, 1U, &sequence, &b_seen, &command));
    assert(1U == command);
    assert(!robot_command(&received, 1U, &sequence, &b_seen, &command));
    assert(!robot_command(&received, 2U, &sequence, &b_seen, &command));
    puts("protocol: bounded packing, version, command duplicate/length "
         "rejection passed");
}

int main(void)
{
    test_barcodes();
    test_navigation();
    test_bypass_side(1);
    test_bypass_side(-1);
    test_line_loss_recovery();
    test_junction_noise();
    test_safety_boundaries();
    test_motion_api();
    test_sensors();
    test_protocol();
    return 0;
}
