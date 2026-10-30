/* Production app_main.c and board.c run against controlled host RTOS/MMIO. */
#include "board.h"
#include <bsp/libbsp.h>
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern INT usermain(void);
static jmp_buf g_exit;
static UW g_addresses[256];
static UW g_values[256];
static UINT g_registers = 0U;
static UINT g_pin_mode[30];
static UINT g_pin_level[30];
static UINT g_pwm_enabled[8];
static UINT g_created = 0U;
static UINT g_watchdog_starts = 0U;
static UINT g_watchdog_loads = 0U;
static UINT g_locks = 0U;
static UINT g_i2c_writes = 0U;
static bool g_imu_ok = true;
static bool g_mutex_ok = true;
static bool g_enforce_pwm = false;
static bool g_reset_ok = true;
static UINT g_delay_limit = 20U;
static UINT g_delays = 0U;
static UINT g_led_states[512];
static char const *g_scenario = "";
static UINT g_motor_pulses[2];
static bool g_motor_live[2];
static UW g_motor_start[2];
static bool g_sensor_log_seen = false;
static PRI g_diagnostic_priority = 0;
static bool g_fail_priority_change = false;
static PRI g_fail_priority = 0;
static PRI g_pending_priority = 0;
static bool g_fail_start = false;
static bool g_safety_started = false;
static FP g_safety = NULL;
static UW g_clock = 0U;

UW in_w(UW address)
{
    UINT i = 0U;
    if (TIMER_TIMERAWL == address) { g_clock += 100U; return g_clock; }
    if (RESETS_RESET_DONE == address) { return g_reset_ok ? UINT32_MAX : 0U; }
    if (UINT32_C(0x40044070) == address) { return g_imu_ok ? 10U : 0U; }
    if (UINT32_C(0x40044034) == address) { return 0x200U; }
    if (UINT32_C(0x40044080) == address) { return 0U; }
    for (i = 0U; g_registers > i; ++i)
        if (g_addresses[i] == address) { return g_values[i]; }
    return 0U;
}
void out_w(UW address, UW value)
{
    UINT i = 0U;
    if ((3 == APP_SERVO_TEST) &&
        ((UINT32_C(0x4005005c) == address) || (UINT32_C(0x40050070) == address)))
    {
        UINT motor = (UINT32_C(0x4005005c) == address) ? 0U : 1U;
        if ((0U != value) && (!g_motor_live[motor]))
        {
            assert(!g_motor_live[motor ^ 1U]);
            g_motor_start[motor] = g_clock;
            g_motor_live[motor] = true;
        }
        if ((0U == value) && g_motor_live[motor])
        {
            assert(306000U >= g_clock - g_motor_start[motor]);
            g_motor_live[motor] = false;
            ++g_motor_pulses[motor];
        }
    }
    if (UINT32_C(0x40058000) == address) { ++g_watchdog_starts; }
    if (UINT32_C(0x40058004) == address) { ++g_watchdog_loads; }
    if ((UINT32_C(0x40044000) <= address) && (UINT32_C(0x40044100) > address))
        ++g_i2c_writes;
    for (i = 0U; g_registers > i; ++i)
        if (g_addresses[i] == address) { g_values[i] = value; return; }
    assert(256U > g_registers);
    g_addresses[g_registers] = address;
    g_values[g_registers++] = value;
}
void set_w(UW address, UW mask) { out_w(address, in_w(address) | mask); }
void clr_w(UW address, UW mask) { out_w(address, in_w(address) & ~mask); }
void EnableInt(UINT number, UINT priority) { (void)number; (void)priority; }
ER gpio_set_pin(UINT pin, UINT mode) { assert(30U > pin); g_pin_mode[pin] = mode; return E_OK; }
ER gpio_set_val(UINT pin, UINT value) { assert(30U > pin); g_pin_level[pin] = value; return E_OK; }
UINT gpio_get_val(UINT pin)
{
    if (3 == APP_SERVO_TEST)
    {
        if ((21U == pin) && (0 == strcmp(g_scenario, "bench-estop")) && (120U <= g_delays))
            return 0U;
        if (20U == pin)
        {
            if (0 == strcmp(g_scenario, "bench-held")) return 0U;
            if ((0 == strcmp(g_scenario, "bench-release")) ||
                (0 == strcmp(g_scenario, "bench-estop")))
                return ((110U <= g_delays) && (120U > g_delays)) ? 0U : 1U;
            return (((110U <= g_delays) && (210U > g_delays)) ||
                    ((320U <= g_delays) && (420U > g_delays))) ? 0U : 1U;
        }
    }
    return 1U;
}
ER pwm_set_pin(UINT pin) { g_pin_mode[pin] = 2U; return E_OK; }
ER pwm_set_wrap(UINT pin, UW top)
{
    out_w(UINT32_C(0x40050010) + (pin / 2U) * 0x14U, top);
    return E_OK;
}
ER pwm_set_cc(UINT pin, UW compare)
{
    UW address = UINT32_C(0x4005000c) + (pin / 2U) * 0x14U;
    UW old = in_w(address);
    out_w(address, (0U == (pin & 1U)) ? ((old & 0xffff0000U) | compare)
                                     : ((old & 0xffffU) | (compare << 16U)));
    return E_OK;
}
ER pwm_set_enabled(UINT pin, INT enabled)
{
    UINT slice = pin / 2U;
    if ((12U == pin) && (0 != enabled) && g_enforce_pwm)
    {
        assert((125U << 4U) == in_w(UINT32_C(0x4005007c)));
        assert(19999U == in_w(UINT32_C(0x40050088)));
        assert(1500U == in_w(UINT32_C(0x40050084)));
        assert(0U == in_w(UINT32_C(0x40050080)));
    }
    g_pwm_enabled[slice] = (0 != enabled) ? 1U : 0U;
    return E_OK;
}
ER tk_dly_tsk(UW delay)
{
    g_clock += delay * 1000U;
    assert(512U > g_delays);
    g_led_states[g_delays++] = g_pin_level[7];
    if (g_delays >= g_delay_limit) { longjmp(g_exit, 1); }
    return E_OK;
}
ER tk_slp_tsk(INT timeout) { (void)timeout; longjmp(g_exit, 1); }
ER tk_loc_mtx(ID id, INT timeout) { (void)timeout; ++g_locks; return (0 < id) ? E_OK : E_SYS; }
ER tk_unl_mtx(ID id) { (void)id; return E_OK; }
ER tk_wup_tsk(ID id) { (void)id; return E_OK; }
ER tk_can_wup(ID id) { (void)id; return 0; }
ER tk_get_tim(SYSTIM *time) { time->lo = g_clock / 1000U; return E_OK; }
ID tk_cre_mtx(T_CMTX const *definition) { (void)definition; return g_mutex_ok ? 1 : E_SYS; }
ID tk_cre_tsk(T_CTSK const *definition)
{
    ++g_created;
    g_pending_priority = definition->itskpri;
    if (g_fail_priority == definition->itskpri) { return E_SYS; }
    if (2 == definition->itskpri) { g_safety = definition->task; }
    return (ID)(g_created + 1U);
}
ER tk_sta_tsk(ID id, INT code)
{
    (void)id; (void)code;
    if (2 == g_pending_priority)
    {
        if (g_fail_start) { return E_SYS; }
        g_safety_started = true;
    }
    return E_OK;
}
ID tk_cre_cyc(T_CCYC const *definition) { (void)definition; return 10; }
ER tk_sta_cyc(ID id) { (void)id; return E_OK; }
ER tk_def_int(UINT number, T_DINT const *definition) { (void)number; (void)definition; return E_OK; }
ER tk_chg_pri(ID id, PRI priority)
{
    assert(TSK_SELF == id);
    g_diagnostic_priority = priority;
    return g_fail_priority_change ? E_SYS : E_OK;
}
INT tm_printf(UB const *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    (void)vsnprintf(line, sizeof(line), (char const *)format, args);
    va_end(args);
    if (NULL != strstr(line, "IMU_MG="))
    {
        assert(NULL != strstr(line, "IMU_OK="));
        assert(NULL != strstr(line, "IMU_READY="));
        assert(NULL != strstr(line, "PITCH_MRAD="));
        assert(NULL != strstr(line, "RANGE_MM="));
        assert(NULL != strstr(line, "RANGE_OK="));
        g_sensor_log_seen = true;
    }
    return 0;
}

static void run_main_once(void)
{
    if (0 == setjmp(g_exit)) { (void)usermain(); assert(false); }
}

static void check_blinks(UINT expected)
{
    UINT on = 0U;
    UINT off_run = 0U;
    UINT i = 0U;
    for (i = 0U; g_delays > i; ++i)
    {
        if (0U != g_led_states[i]) { ++on; off_run = 0U; }
        else { ++off_run; }
        if (5U <= off_run) { break; }
    }
    assert(expected == on);
}

int main(int argc, char **argv)
{
    assert(2 == argc);
    g_scenario = argv[1];
    if (3 == APP_SERVO_TEST) g_delay_limit = 430U;
    if (0 == strcmp(argv[1], "pwm"))
    {
        g_enforce_pwm = true;
        assert(board_init());
        assert(1U == g_pwm_enabled[6]);
        puts("startup PWM: divider, period, centre compare and counter configured before enabling GP12");
        return 0;
    }
    if (0 == strcmp(argv[1], "servo-range"))
    {
        board_servo(UINT16_MAX);
        assert(2000U == in_w(UINT32_C(0x40050084)));
        puts("servo: out-of-range request clamped to 2000 us");
        return 0;
    }
    g_imu_ok = (0 != strcmp(argv[1], "imu-fail"));
    g_mutex_ok = (0 != strcmp(argv[1], "mutex-fail"));
    if (0 == strcmp(argv[1], "safety-fail")) { g_fail_priority = 2; }
    if (0 == strcmp(argv[1], "task-fail")) { g_fail_priority = 6; }
    g_fail_start = (0 == strcmp(argv[1], "safety-start-fail"));
    g_fail_priority_change = (0 == strcmp(argv[1], "diagnostic-priority-fail"));
    g_reset_ok = (0 != strcmp(argv[1], "isolation-reset-fail"));
    run_main_once();
    if (3 == APP_SERVO_TEST)
    {
        bool b_none = (0 == strcmp(g_scenario, "bench-held")) || (!g_imu_ok);
        bool b_one = (0 == strcmp(g_scenario, "bench-release")) ||
                     (0 == strcmp(g_scenario, "bench-estop"));
        assert((b_none ? 0U : 1U) == g_motor_pulses[0]);
        assert((b_none || b_one ? 0U : 1U) == g_motor_pulses[1]);
        assert(!g_motor_live[0] && !g_motor_live[1]);
        assert(0U == g_created && 0U == g_pwm_enabled[6]);
        assert(0U == in_w(UINT32_C(0x4005005c)));
        assert(0U == in_w(UINT32_C(0x40050070)));
        if (0 == strcmp(g_scenario, "bench-estop"))
            assert(0U != (robot_fault_get() & ROBOT_FAULT_STOP));
        printf("%s: bounded alternating pulses, release interlock, stop and startup inhibition passed\n", argv[1]);
        return 0;
    }
    if ((0 == strcmp(argv[1], "isolation")) || (!g_reset_ok))
    {
        assert(0U == g_created && 0U == g_i2c_writes);
        assert(1U == g_watchdog_starts && 2U <= g_watchdog_loads);
        assert(0U == in_w(UINT32_C(0x4005005c)));
        assert(0U == in_w(UINT32_C(0x40050070)));
        if ((1 == APP_SERVO_TEST) || (!g_reset_ok))
        {
            assert(0U == g_pwm_enabled[6]);
            assert(GPIO_MODE_OUT == g_pin_mode[12] && 0U == g_pin_level[12]);
        }
        else
        {
            assert(2 == APP_SERVO_TEST);
            assert(1U == g_pwm_enabled[6]);
            assert(1500U == in_w(UINT32_C(0x40050084)));
        }
        check_blinks(g_reset_ok ? (UINT)APP_SERVO_TEST : 3U);
        puts("isolation: no sensor initialization or car tasks; watchdog fed; motor outputs zero; servo policy correct");
        return 0;
    }
    check_blinks((!g_imu_ok || !g_mutex_ok) ? 3U : (((0 != g_fail_priority) || g_fail_start || g_fail_priority_change) ? 2U : 1U));
    assert(g_sensor_log_seen);
    assert(11 == g_diagnostic_priority);
    if ((0 == strcmp(argv[1], "safety-fail")) || g_fail_start)
    {
        assert(!g_safety_started);
        assert(0U == g_watchdog_starts);
    }
    else
    {
        UINT before = g_watchdog_loads;
        assert(NULL != g_safety);
        assert(g_safety_started);
        assert(1U == g_watchdog_starts);
        g_delays = 0U;
        g_delay_limit = 1U;
        if (0 == setjmp(g_exit))
            ((void (*)(INT, void *))g_safety)(0, NULL);
        assert(before < g_watchdog_loads);
    }
    if (!g_mutex_ok) { assert(0U == g_locks); }
    if ((!g_imu_ok) || (!g_mutex_ok) || (0 != g_fail_priority) || g_fail_start || g_fail_priority_change)
    {
        assert(0U == g_pwm_enabled[6]);
        assert(GPIO_MODE_OUT == g_pin_mode[12] && 0U == g_pin_level[12]);
        assert(0U != (robot_fault_get() & ROBOT_CRITICAL_MASK));
    }
    assert(0U == in_w(UINT32_C(0x4005005c)));
    assert(0U == in_w(UINT32_C(0x40050070)));
    printf("startup %s: watchdog/safety policy, servo shutdown, LED code and zero motor targets passed\n", argv[1]);
    return 0;
}
