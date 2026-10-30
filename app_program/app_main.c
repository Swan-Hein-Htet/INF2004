/* Task integration; no network work runs under the state mutex. */
#include "app.h"
#include <bsp/libbsp.h>
#include <math.h>
#include <string.h>

#if TK_SUPPORT_SMP
#error "Car firmware requires SMP=0: interrupt-masked sharing is core-0 only"
#endif

#define APP_STACK_BYTES 3072U
#define APP_HEALTH_TASKS 6U
#ifndef APP_SERVO_TEST
#define APP_SERVO_TEST 0
#endif

static ID g_h_state = 0;
static ID g_h_sample = 0;
static ID g_h_cycle = 0;
static volatile uint16_t g_faults = 0U;
static volatile uint32_t g_health[APP_HEALTH_TASKS] = {0U};
static robot_input_t g_input = {0};
static robot_nav_t g_nav = {0};
static robot_terrain_t g_terrain = {0};
static float g_speed[2] = {0.0f, 0.0f};
static float g_targets[2] = {0.0f, 0.0f};
static uint16_t g_counts[2] = {0U, 0U};
static volatile uint32_t g_line_stamp = 0U;
static uint8_t g_commands = 0U;
static volatile uint16_t g_overruns = 0U;
static uint16_t g_adc_raw[3] = {0U, 0U, 0U};
static char g_last_barcode = '\0';
static uint8_t g_bar_reject = 0U;
static uint16_t g_run_generation = 0U;
static float g_run_base_mm = 0.0f;
static bool g_b_scan_requested = false;
static bool g_b_scan_complete = false;

static void app_lock(void)
{
    if (E_OK != tk_loc_mtx(g_h_state, TMO_FEVR))
    {
        board_force_off();
        for (;;)
        {
            (void)tk_dly_tsk(1000U);
        }
    }
}

static void app_unlock(void)
{
    (void)tk_unl_mtx(g_h_state);
}

void robot_fault_set(uint16_t mask)
{
    UINT status = 0U;
    DI(status);
    g_faults |= mask;
    EI(status);
}

uint16_t robot_fault_get(void)
{
    return g_faults;
}

void robot_remote(uint8_t command)
{
    UINT status = 0U;
    if (1U == command)
    {
        robot_fault_set(ROBOT_FAULT_STOP);
        board_force_off();
    }
    DI(status);
    g_commands |= (uint8_t)(1U << command);
    EI(status);
}

void robot_network(bool b_online)
{
    UINT status = 0U;
    DI(status);
    if (b_online)
    {
        g_faults &= (uint16_t)~ROBOT_FAULT_WIFI;
    }
    else
    {
        g_faults |= ROBOT_FAULT_WIFI;
    }
    EI(status);
}

size_t robot_telemetry(uint8_t *p_payload, size_t capacity)
{
    static uint8_t g_sequence = 0U;
    uint32_t fields[24] = {0U};
    uint16_t word = 0U;
    robot_input_t input = {0};
    robot_nav_t nav = {0};
    robot_terrain_t terrain = {0};
    float speeds[2] = {0.0f, 0.0f};
    uint16_t counts[2] = {0U, 0U};
    char barcode = '\0';
    uint8_t reject = 0U;
    uint16_t overruns = 0U;
    float run_base = 0.0f;
    app_lock();
    input = g_input;
    nav = g_nav;
    terrain = g_terrain;
    speeds[0] = g_speed[0];
    speeds[1] = g_speed[1];
    counts[0] = g_counts[0];
    counts[1] = g_counts[1];
    barcode = g_last_barcode;
    reject = g_bar_reject;
    overruns = g_overruns;
    run_base = g_run_base_mm;
    app_unlock();
    fields[0] = 2U;
    fields[1] = g_sequence++;
    fields[2] = (uint32_t)nav.mode;
    fields[3] = (uint32_t)nav.mode & 7U;
    fields[4] = (uint32_t)(int32_t)speeds[0];
    fields[5] = (uint32_t)(int32_t)speeds[1];
    fields[6] = counts[0];
    fields[7] = counts[1];
    word = input.line_word;
    fields[8] = (word >> 8U) & 3U;
    fields[9] = word & 255U;
    fields[10] = ('\0' == barcode) ? 0U : (uint32_t)(barcode - 'A' + 1);
    fields[11] = reject;
    fields[12] = (uint32_t)robot_limit(
        fmaxf(terrain.maximum_mm, terrain.peak_mm), 0.0f, 127.0f);
    fields[13] = input.profile.width_mm;
    fields[14] = (uint32_t)robot_limit(input.profile.closest_mm, 0.0f, 511.0f);
    fields[15] = input.profile.bearing;
    fields[16] = (ROBOT_BYPASS == nav.mode) ? ((0 < nav.side) ? 1U : 2U) : 0U;
    fields[17] =
        (uint32_t)robot_limit(input.distance_mm - run_base, 0.0f, 32767.0f);
    if (0U != (robot_fault_get() & ROBOT_FAULT_IMPACT))
    {
        fields[18] = 5U;
    }
    else if (0.087f < fabsf(terrain.pitch_rad))
    {
        fields[18] = (0.0f < terrain.pitch_rad) ? 3U : 4U;
    }
    else if (20.0f < fabsf(speeds[1] - speeds[0]))
    {
        fields[18] = 2U;
    }
    else
    {
        fields[18] = (10.0f > fabsf(speeds[0]) + fabsf(speeds[1])) ? 0U : 1U;
    }
    fields[19] =
        (uint32_t)(int32_t)robot_limit((speeds[1] - speeds[0]) * 57.29578f /
                                           fmaxf(g_robot_config.track_mm, 1.0f),
                                       -500.0f, 500.0f);
    fields[20] = robot_fault_get();
    fields[21] = input.profile.left_mm;
    fields[22] = input.profile.right_mm;
    fields[23] = overruns;
    return robot_pack(p_payload, capacity, fields);
}

static void app_heartbeat(uint32_t index)
{
    UINT status = 0U;
    DI(status);
    g_health[index] = board_micros();
    EI(status);
}

static void app_periodic(uint32_t *p_deadline, uint32_t period_us)
{
    int32_t remaining = 0;
    *p_deadline += period_us;
    remaining = (int32_t)(*p_deadline - board_micros());
    if (0 < remaining)
    {
        (void)tk_dly_tsk((UW)((remaining + 999) / 1000));
    }
    else if ((uint32_t)(-remaining) > period_us)
    {
        robot_fault_set(ROBOT_FAULT_TIMING);
        *p_deadline = board_micros();
    }
    else
    {
        (void)tk_dly_tsk(1U);
    }
}

static void app_safety_task(INT code, void *p_context)
{
    uint32_t deadline = board_micros();
    uint32_t index = 0U;
    (void)code;
    (void)p_context;
    for (;;)
    {
        uint32_t now = board_micros();
        for (index = 1U; APP_HEALTH_TASKS > index; ++index)
        {
            if (500000U < now - g_health[index])
            {
                robot_fault_set(ROBOT_FAULT_TIMING);
            }
        }
        if (0U == gpio_get_val(21U))
        {
            robot_fault_set(ROBOT_FAULT_STOP);
        }
        if (0U != (robot_fault_get() & ROBOT_CRITICAL_MASK))
        {
            board_force_off();
        }
        board_watchdog_feed();
        app_heartbeat(0U);
        app_periodic(&deadline, 20000U);
    }
}

static void app_sample_cycle(void *p_context)
{
    (void)p_context;
    (void)tk_wup_tsk(g_h_sample);
}

static void app_line_task(INT code, void *p_context)
{
    robot_barcode_t reader = {0};
    uint8_t sequence = 0U;
    bool b_black = false;
    uint32_t previous = 0U;
    (void)code;
    (void)p_context;
    for (;;)
    {
        uint16_t raw[3] = {0U};
        uint16_t word = 0U;
        uint16_t threshold = 0U;
        char barcode = '\0';
        uint32_t now = board_micros();
        ER queued = tk_can_wup(g_h_sample);
        if ((0 < queued) || ((0U != previous) && (2000U < now - previous)))
        {
            ++g_overruns;
            robot_fault_set(ROBOT_FAULT_TIMING);
        }
        previous = now;
        raw[0] = board_adc(0U);
        raw[1] = board_adc(1U);
        raw[2] = board_adc(2U);
        word = robot_line_word(g_robot_config.ir, raw[0], raw[1], sequence++);
        threshold = (uint16_t)(((uint32_t)g_robot_config.ir[2].white +
                                g_robot_config.ir[2].black) /
                               2U);
        if (g_robot_config.ir[2].b_black_low)
        {
            if (raw[2] + 40U < threshold)
            {
                b_black = true;
            }
            if (raw[2] > threshold + 40U)
            {
                b_black = false;
            }
        }
        else
        {
            if (raw[2] > threshold + 40U)
            {
                b_black = true;
            }
            if (raw[2] + 40U < threshold)
            {
                b_black = false;
            }
        }
        barcode =
            robot_barcode_sample(&reader, b_black, (1U == ((word >> 8U) & 3U)));
        app_lock();
        g_adc_raw[0] = raw[0];
        g_adc_raw[1] = raw[1];
        g_adc_raw[2] = raw[2];
        g_input.line_word = word;
        g_line_stamp = now;
        if ('\0' != barcode)
        {
            g_input.barcode = barcode;
            g_last_barcode = barcode;
        }
        g_bar_reject = reader.reject;
        app_unlock();
        app_heartbeat(1U);
        (void)tk_slp_tsk(TMO_FEVR);
    }
}

static void app_terrain_task(INT code, void *p_context)
{
    uint32_t deadline = board_micros();
    robot_terrain_t terrain = {0};
    float previous = 0.0f;
    uint32_t failures = 0U;
    uint16_t generation = 0U;
    (void)code;
    (void)p_context;
    for (;;)
    {
        float accel[3] = {0.0f, 0.0f, 0.0f};
        float distance = 0.0f;
        bool b_stationary = false;
        app_lock();
        distance = g_input.distance_mm;
        b_stationary = (ROBOT_IDLE == g_nav.mode);
        if (generation != g_run_generation)
        {
            generation = g_run_generation;
            terrain.height_mm = 0.0f;
            terrain.peak_mm = 0.0f;
            terrain.maximum_mm = 0.0f;
            terrain.b_hump = false;
            terrain.flat_count = 0U;
        }
        app_unlock();
        if (board_accel(accel))
        {
            failures = 0U;
            robot_terrain_step(&terrain, accel[0], accel[1], accel[2],
                               distance - previous, b_stationary);
            if (2.5f < sqrtf(accel[0] * accel[0] + accel[1] * accel[1] +
                             accel[2] * accel[2]))
            {
                robot_fault_set(ROBOT_FAULT_IMPACT);
            }
            app_lock();
            g_terrain = terrain;
            g_input.pitch_rad = terrain.pitch_rad;
            g_input.b_pitch_valid = terrain.b_ready;
            app_unlock();
        }
        else
        {
            ++failures;
            if (3U <= failures)
            {
                robot_fault_set(ROBOT_FAULT_IMU);
            }
        }
        previous = distance;
        app_heartbeat(2U);
        app_periodic(&deadline, 10000U);
    }
}

static void app_motion_task(INT code, void *p_context)
{
    uint32_t deadline = board_micros();
    robot_pid_t controllers[2] = {{0.0f, 0.0f, 0.002f, 0.003f, 0.0f},
                                  {0.0f, 0.0f, 0.002f, 0.003f, 0.0f}};
    uint16_t previous[2] = {0U, 0U};
    uint32_t last = board_micros();
    uint32_t stalled[2] = {0U, 0U};
    float distance = 0.0f;
    float yaw = 0.0f;
    (void)code;
    (void)p_context;
    board_encoder(&previous[0], &previous[1]);
    for (;;)
    {
        uint16_t counts[2] = {0U, 0U};
        float speeds[2] = {0.0f, 0.0f};
        float targets[2] = {0.0f, 0.0f};
        float duties[2] = {0.0f, 0.0f};
        float travel[2] = {0.0f, 0.0f};
        uint32_t now = board_micros();
        if (30000U < now - g_line_stamp)
        {
            robot_fault_set(ROBOT_FAULT_TIMING);
        }
        uint32_t elapsed = now - last;
        uint32_t index = 0U;
        last = now;
        board_encoder(&counts[0], &counts[1]);
        app_lock();
        targets[0] = g_targets[0];
        targets[1] = g_targets[1];
        app_unlock();
        for (index = 0U; 2U > index; ++index)
        {
            int32_t delta =
                (int16_t)(uint16_t)(counts[index] - previous[index]);
            previous[index] = counts[index];
            travel[index] = (float)delta * g_robot_config.wheel_mm_per_edge;
            speeds[index] = travel[index] * 1000000.0f / (float)(elapsed + 1U);
            duties[index] = robot_pid_step(&controllers[index], targets[index],
                                           speeds[index],
                                           (float)elapsed / 1000000.0f, 0.8f);
            stalled[index] = ((50.0f < fabsf(targets[index])) && (0 == delta))
                                 ? stalled[index] + elapsed
                                 : 0U;
            if (750000U < stalled[index])
            {
                robot_fault_set(ROBOT_FAULT_STALL);
            }
            if (1000.0f < fabsf(speeds[index]))
            {
                robot_fault_set(ROBOT_FAULT_ENCODER);
            }
        }
        board_motor(duties[0], duties[1]);
        distance += fabsf((travel[0] + travel[1]) * 0.5f);
        yaw += (travel[1] - travel[0]) * 57.29578f /
               fmaxf(g_robot_config.track_mm, 1.0f);
        app_lock();
        g_counts[0] = counts[0];
        g_counts[1] = counts[1];
        g_speed[0] = speeds[0];
        g_speed[1] = speeds[1];
        g_input.distance_mm = distance;
        g_input.yaw_deg = yaw;
        app_unlock();
        app_heartbeat(3U);
        app_periodic(&deadline, 20000U);
    }
}

static void app_wait(uint32_t milliseconds)
{
    while (0U < milliseconds)
    {
        uint32_t delay = (20U < milliseconds) ? 20U : milliseconds;
        app_heartbeat(4U);
        (void)tk_dly_tsk(delay);
        milliseconds -= delay;
    }
}

static bool app_measure(uint16_t *p_range)
{
    uint32_t before = board_micros();
    uint32_t stamp = 0U;
    bool b_valid = false;
    board_ping();
    app_wait(30U);
    b_valid = board_range(p_range, &stamp);
    app_wait(40U);
    return b_valid && ((int32_t)(stamp - before) >= 0);
}

static void app_obstacle_task(INT code, void *p_context)
{
    uint32_t failures = 0U;
    (void)code;
    (void)p_context;
    for (;;)
    {
        bool b_scan = false;
        uint16_t range = 0U;
        app_lock();
        b_scan = g_b_scan_requested;
        g_b_scan_requested = false;
        app_unlock();
        if (b_scan)
        {
            robot_profile_t profile = {0};
            uint32_t index = 0U;
            static uint8_t const g_scan_order[ROBOT_SCAN_POINTS] = {
                0U, 3U, 6U, 9U, 12U, 1U, 2U, 4U, 5U, 7U, 8U, 10U, 11U};
            uint32_t last_angle = 90U;
            for (index = 0U; ROBOT_SCAN_POINTS > index; ++index)
            {
                uint32_t point = g_scan_order[index];
                uint32_t angle = 30U + point * 10U;
                uint32_t change = (angle > last_angle) ? (angle - last_angle)
                                                       : (last_angle - angle);
                board_servo((uint16_t)angle);
                app_wait(40U + change * 3U);
                last_angle = angle;
                if (!app_measure(&profile.range_mm[point]))
                {
                    robot_fault_set(ROBOT_FAULT_RANGE);
                    break;
                }
            }
            (void)robot_profile_build(&profile, &g_robot_config);
            board_servo(90U);
            app_wait(300U);
            app_lock();
            g_input.profile = profile;
            g_b_scan_complete = true;
            app_unlock();
        }
        board_servo(90U);
        if (app_measure(&range))
        {
            failures = 0U;
            app_lock();
            g_input.front_mm = range;
            g_input.b_front_valid = true;
            app_unlock();
        }
        else
        {
            ++failures;
            app_lock();
            g_input.b_front_valid = false;
            app_unlock();
            if (3U <= failures)
            {
                robot_fault_set(ROBOT_FAULT_RANGE);
            }
        }
        app_heartbeat(4U);
    }
}

static void app_vehicle_task(INT code, void *p_context)
{
    uint32_t deadline = board_micros();
    robot_nav_t nav = {0};
    uint32_t button_since = 0U;
    bool b_button_latched = false;
    (void)code;
    (void)p_context;
    for (;;)
    {
        robot_input_t input = {0};
        uint8_t commands = 0U;
        UINT status = 0U;
        DI(status);
        commands = g_commands;
        g_commands = 0U;
        EI(status);
        if (0U == gpio_get_val(20U))
        {
            ++button_since;
            if ((5U <= button_since) && (!b_button_latched))
            {
                commands |= (ROBOT_FAULT == nav.mode) ? 4U : 1U;
                b_button_latched = true;
            }
        }
        else
        {
            button_since = 0U;
            b_button_latched = false;
        }
        if ((0U != (commands & 4U)) && (0U != gpio_get_val(21U)) &&
            (0U == (robot_fault_get() &
                    (ROBOT_CRITICAL_MASK & (uint16_t)~ROBOT_FAULT_STOP))))
        {
            DI(status);
            g_faults &= (uint16_t)~ROBOT_FAULT_STOP;
            EI(status);
            nav.b_reset = true;
        }
        nav.b_start = (0U != (commands & 1U));
        nav.b_end = (0U != (commands & 8U));
        app_lock();
        input = g_input;
        input.b_profile_ready = g_b_scan_complete;
        g_b_scan_complete = false;
        g_input.barcode = '\0';
        app_unlock();
        SYSTIM uptime = {0};
        (void)tk_get_tim(&uptime);
        input.now_ms = uptime.lo;
        input.faults = robot_fault_get();
        robot_nav_step(&nav, &input, &g_robot_config);
        robot_fault_set(nav.fault_request);
        app_lock();
        if ((ROBOT_IDLE == g_nav.mode) && (ROBOT_FOLLOW == nav.mode))
        {
            ++g_run_generation;
            g_run_base_mm = input.distance_mm;
        }
        g_nav = nav;
        g_targets[0] = nav.left_mm_s;
        g_targets[1] = nav.right_mm_s;
        if (nav.b_scan)
        {
            g_b_scan_requested = true;
            nav.b_scan = false;
        }
        app_unlock();
        board_arm((ROBOT_IDLE != nav.mode) && (ROBOT_FAULT != nav.mode) &&
                  (ROBOT_FINISHED != nav.mode));
        app_heartbeat(5U);
        app_periodic(&deadline, 10000U);
    }
}

static ID app_create_task(FP p_entry, PRI priority)
{
    T_CTSK definition = {0};
    ID h_task = 0;
    definition.task = p_entry;
    definition.itskpri = priority;
    definition.stksz = APP_STACK_BYTES;
    definition.tskatr = TA_HLNG | TA_RNG0;
    h_task = tk_cre_tsk(&definition);
    if (0 < h_task)
    {
        if (E_OK != tk_sta_tsk(h_task, 0))
        {
            h_task = E_SYS;
        }
    }
    return h_task;
}

static void app_servo_test(void)
{
    uint32_t step = 0U;
    uint32_t pulses = (2 == APP_SERVO_TEST) ? 2U : 1U;
    if (!board_servo_test_init(2 == APP_SERVO_TEST))
    {
        pulses = 3U;
    }
    board_watchdog_start();
    for (;;)
    {
        board_status_led((step < pulses * 2U) && (0U == (step & 1U)));
        board_watchdog_feed();
        (void)tk_dly_tsk(100U);
        step = (step + 1U) % (pulses * 2U + 8U);
    }
}

INT usermain(void)
{
    T_CMTX mutex_definition = {0};
    T_CCYC cycle_definition = {0};
    uint32_t index = 0U;
#if APP_TEST
    uint32_t status_step = 0U;
#endif
    uint32_t status_pulses = 1U;
    bool b_ready = true;
    bool b_safety_ready = false;
    if (0 != APP_SERVO_TEST)
    {
        app_servo_test();
        return 0;
    }
    mutex_definition.mtxatr = TA_INHERIT;
    g_h_state = tk_cre_mtx(&mutex_definition);
    b_ready = (0 < g_h_state) && board_init();
    if (APP_TEST || !robot_config_valid(&g_robot_config))
    {
        robot_fault_set(ROBOT_FAULT_CONFIG);
    }
    if (!b_ready)
    {
        robot_fault_set(ROBOT_FAULT_IMU);
        status_pulses = 3U;
    }
    for (index = 0U; APP_HEALTH_TASKS > index; ++index)
    {
        g_health[index] = board_micros();
    }
    /* Safety must exist even when peripheral initialization has failed. */
    b_safety_ready = (0 < app_create_task((FP)app_safety_task, 2));
    b_ready = b_ready && b_safety_ready;
    g_h_sample = b_ready ? app_create_task((FP)app_line_task, 5) : E_SYS;
    b_ready = b_ready && (0 < g_h_sample);
    b_ready = b_ready && (0 < app_create_task((FP)app_terrain_task, 6));
    b_ready = b_ready && (0 < app_create_task((FP)app_motion_task, 7));
    b_ready = b_ready && (0 < app_create_task((FP)app_obstacle_task, 9));
    b_ready = b_ready && (0 < app_create_task((FP)app_vehicle_task, 10));
    cycle_definition.cycatr = TA_HLNG;
    cycle_definition.cychdr = (FP)app_sample_cycle;
    cycle_definition.cyctim = 1U;
    cycle_definition.cycphs = 1U;
    g_h_cycle = tk_cre_cyc(&cycle_definition);
    b_ready = b_ready && (0 < g_h_cycle);
    if (b_ready)
    {
        b_ready = (E_OK == tk_sta_cyc(g_h_cycle));
    }
    if (!b_ready)
    {
        robot_fault_set(ROBOT_FAULT_TIMING);
        board_force_off();
        board_servo_off();
        status_pulses = (3U == status_pulses) ? 3U : 2U;
    }
    /* If safety cannot start, stay disarmed without a watchdog reboot loop. */
    if (b_safety_ready)
    {
        board_watchdog_start();
    }
    tm_printf((UB *)"Car boot: faults=%04x, calibration=%d startup=%u\n",
              robot_fault_get(), robot_config_valid(&g_robot_config), status_pulses);
    for (;;)
    {
#if APP_TEST
        uint16_t raw[3] = {0U, 0U, 0U};
        uint16_t counts[2] = {0U, 0U};
        if (0 < g_h_state)
        {
            app_lock();
            memcpy(raw, g_adc_raw, sizeof(raw));
            memcpy(counts, g_counts, sizeof(counts));
            app_unlock();
        }
        board_status_led((status_step < status_pulses * 2U) &&
                         (0U == (status_step & 1U)));
        status_step = (status_step + 1U) % (status_pulses * 2U + 8U);
        tm_printf((UB *)"ADC=%u,%u,%u ENC=%u,%u FAULT=%04x OVERRUN=%u\n",
                  raw[0], raw[1], raw[2], counts[0], counts[1],
                  robot_fault_get(), g_overruns);
        (void)tk_dly_tsk(200U);
#else
        (void)tk_slp_tsk(TMO_FEVR);
#endif
    }
    return 0;
}
