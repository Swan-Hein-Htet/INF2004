/* Actual explicitly assumed configuration must enable the normal mission gate. */
#include "../app_program/app_main.c"
#include <assert.h>
#include <stdio.h>

static bool g_led = false;
void board_status_led(bool b_on) { g_led = b_on; }

int main(void)
{
    assert(robot_config_valid(&g_robot_config));
    assert(100.0f == g_robot_config.cruise_mm_s);
    assert(0.50f == g_robot_config.wheel_mm_per_edge);
    assert(140.0f == g_robot_config.track_mm);
    assert(3500U == g_robot_config.ir[0].black);
    assert(500U == g_robot_config.ir[0].white);
    assert(!g_robot_config.ir[0].b_black_low);
    assert(0.30f == APP_MAX_DUTY);
    uint16_t faults[] = {ROBOT_FAULT_IMU, ROBOT_FAULT_RANGE,
                         ROBOT_FAULT_ENCODER, ROBOT_FAULT_STALL,
                         ROBOT_FAULT_LINE, ROBOT_FAULT_TIMING,
                         ROBOT_FAULT_STOP};
    for (unsigned int fault = 0U; fault < 7U; ++fault)
    {
        unsigned int on = 0U;
        for (unsigned int phase = 0U; phase < (fault + 1U) * 2U + 8U; ++phase)
        {
            app_demo_led(phase * 150U, ROBOT_FAULT, faults[fault]);
            if (g_led) ++on;
        }
        assert(fault + 1U == on);
    }
    app_demo_led(0U, ROBOT_IDLE, 0U); assert(g_led);
    app_demo_led(500U, ROBOT_IDLE, 0U); assert(!g_led);
    app_demo_led(500U, ROBOT_FOLLOW, 0U); assert(g_led);
    app_demo_led(500U, ROBOT_FINISHED, 0U); assert(!g_led);
    float raw[3] = {-1000.0f, 0.0f, 1000.0f};
    for (unsigned int i = 0U; i < 3U; ++i)
    {
        robot_pid_t pid = {.kp = 0.002f, .ki = 0.003f};
        float duty = robot_pid_step(&pid, 100.0f, raw[i], 0.020f, 0.30f);
        assert(-0.30f <= duty && 0.30f >= duty);
    }
    puts("assumed demo: enabled estimates, 100mm/s target, actual 30% duty cap and all seven LED fault codes checked");
    return 0;
}
