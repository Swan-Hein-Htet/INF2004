/* RP2040 BSP register adapter; all waits are bounded by the hardware timer. */
#include "../board.h"
#include <stddef.h>
#define PROHIBIT_DEF_SIZE_T
#include <tk/tkernel.h>
#include <bsp/libbsp.h>
#include <math.h>

#define BOARD_GPIO_IRQ 13U
#define BOARD_ADC_BASE UINT32_C(0x4004c000)
#define BOARD_IIC_BASE UINT32_C(0x40044000)
#define BOARD_PWM_BASE UINT32_C(0x40050000)
#define BOARD_IO_BASE UINT32_C(0x40014000)
#define BOARD_PADS_BASE UINT32_C(0x4001c000)
#define BOARD_WATCHDOG UINT32_C(0x40058000)

static volatile uint16_t g_encoder_left = 0U;
static volatile uint16_t g_encoder_right = 0U;
static volatile uint16_t g_echo_us = 0U;
static volatile uint32_t g_echo_stamp = 0U;
static uint32_t g_echo_rise = 0U;
static volatile bool g_b_armed = false;

_Static_assert(2U == sizeof(g_encoder_left), "encoder width");
_Static_assert(2U <= _Alignof(uint16_t), "encoder alignment");

void board_watchdog_start(void)
{
    out_w(BOARD_WATCHDOG + 0x2cU, 0x200U | 12U);
    out_w(UINT32_C(0x40010008), UINT32_C(0x0001fffc));
    out_w(BOARD_WATCHDOG + 4U, 1000000U);
    out_w(BOARD_WATCHDOG, UINT32_C(0x40000000));
}

void board_watchdog_feed(void)
{
    out_w(BOARD_WATCHDOG + 4U, 1000000U);
}

uint32_t board_micros(void)
{
    return in_w(TIMER_TIMERAWL);
}

static void board_gpio_isr(UINT interrupt)
{
    uint32_t bank = 0U;
    uint32_t now = board_micros();
    (void)interrupt;

    for (bank = 0U; 3U > bank; ++bank)
    {
        uint32_t events = in_w(BOARD_IO_BASE + 0x120U + bank * 4U);
        out_w(BOARD_IO_BASE + 0xf0U + bank * 4U, events);
        if (0U == bank)
        {
            if (0U != (events & (12U << 12U)))
            {
                if (0U != gpio_get_val(3U))
                {
                    g_echo_rise = now;
                }
                else
                {
                    uint32_t width = now - g_echo_rise;
                    g_echo_us = (32767U > width) ? (uint16_t)width : 0U;
                    g_echo_stamp = now;
                }
            }
        }
        if (2U == bank)
        {
            if (0U != (events & 12U))
            {
                bool b_forward = (gpio_get_val(16U) == gpio_get_val(13U));
                int32_t step = b_forward ? 1 : -1;
                g_encoder_left =
                    (uint16_t)(g_encoder_left +
                               step * g_robot_config.encoder_sign[0]);
            }
            if (0U != (events & (12U << 4U)))
            {
                bool b_forward = (gpio_get_val(17U) == gpio_get_val(19U));
                int32_t step = b_forward ? 1 : -1;
                g_encoder_right =
                    (uint16_t)(g_encoder_right +
                               step * g_robot_config.encoder_sign[1]);
            }
            if (0U != (events & (4U << 20U)))
            {
                robot_fault_set(ROBOT_FAULT_STOP);
                board_force_off();
            }
        }
    }
}

void board_force_off(void)
{
    UINT status = 0U;
    DI(status);
    g_b_armed = false;
    out_w(BOARD_PWM_BASE + 4U * 0x14U + 0x0cU, 0U);
    out_w(BOARD_PWM_BASE + 5U * 0x14U + 0x0cU, 0U);
    EI(status);
}

void board_arm(bool b_enable)
{
    UINT status = 0U;
    DI(status);
    g_b_armed = b_enable && (0U == (robot_fault_get() & ROBOT_CRITICAL_MASK));
    EI(status);
}

void board_motor(float left, float right)
{
    float duties[2] = {left, right};
    uint32_t index = 0U;

    for (index = 0U; 2U > index; ++index)
    {
        float signed_duty = duties[index] * g_robot_config.motor_sign[index];
        uint32_t level =
            (uint32_t)(robot_limit(fabsf(signed_duty), 0.0f, 1.0f) * 6249.0f);
        uint32_t compare = (0.0f <= signed_duty) ? level : (level << 16U);
        UINT status = 0U;
        DI(status);
        if ((!g_b_armed) || (0U != (robot_fault_get() & ROBOT_CRITICAL_MASK)))
        {
            compare = 0U;
        }
        out_w(BOARD_PWM_BASE + (index + 4U) * 0x14U + 0x0cU, compare);
        EI(status);
    }
}

void board_encoder(uint16_t *p_left, uint16_t *p_right)
{
    UINT status = 0U;
    DI(status);
    *p_left = g_encoder_left;
    *p_right = g_encoder_right;
    EI(status);
}

uint16_t board_adc(uint32_t channel)
{
    uint32_t started = board_micros();
    out_w(BOARD_ADC_BASE, 1U | (channel << 12U) | 4U);
    while (0U == (in_w(BOARD_ADC_BASE) & 0x100U))
    {
        if (100U < board_micros() - started)
        {
            robot_fault_set(ROBOT_FAULT_TIMING);
            return 0U;
        }
    }
    return (uint16_t)(in_w(BOARD_ADC_BASE + 4U) & 4095U);
}

void board_servo(uint16_t degrees)
{
    degrees = (180U < degrees) ? 180U : degrees;
    uint32_t pulse = 1000U + (uint32_t)degrees * 1000U / 180U;
    (void)pwm_set_cc(12U, pulse);
}

void board_servo_off(void)
{
    (void)pwm_set_enabled(12U, FALSE);
    (void)gpio_set_val(12U, 0U);
    (void)gpio_set_pin(12U, GPIO_MODE_OUT);
}

static void board_servo_init(bool b_enable)
{
    board_servo_off();
    /* Configure slice 6 completely while its output is disabled. */
    out_w(BOARD_PWM_BASE + 6U * 0x14U, 0U);
    out_w(BOARD_PWM_BASE + 6U * 0x14U + 4U, 125U << 4U);
    out_w(BOARD_PWM_BASE + 6U * 0x14U + 8U, 0U);
    (void)pwm_set_wrap(12U, 19999U);
    board_servo(90U);
    if (b_enable)
    {
        (void)pwm_set_pin(12U);
        (void)pwm_set_enabled(12U, TRUE);
    }
}

void board_status_led(bool b_on)
{
    /* GP7's carrier status LED; its Grove signal must remain unconnected. */
    (void)gpio_set_pin(7U, GPIO_MODE_OUT);
    (void)gpio_set_val(7U, b_on ? 1U : 0U);
}

bool board_servo_test_init(bool b_enable)
{
    uint32_t pin = 0U;
    uint32_t started = board_micros();
    board_force_off();
    for (pin = 8U; 12U > pin; ++pin)
    {
        (void)gpio_set_val(pin, 0U);
        (void)gpio_set_pin(pin, GPIO_MODE_OUT);
    }
    clr_w(RESETS_RESET, 1U << 14U);
    while (0U == (in_w(RESETS_RESET_DONE) & (1U << 14U)))
    {
        if (10000U < board_micros() - started)
        {
            board_servo_off();
            robot_fault_set(ROBOT_FAULT_TIMING);
            return false;
        }
    }
    board_servo_init(b_enable);
    return true;
}

void board_ping(void)
{
    uint32_t started = board_micros();
    (void)gpio_set_val(2U, 1U);
    while (10U > board_micros() - started)
    {
        /* HC-SR04 minimum trigger pulse. */
    }
    (void)gpio_set_val(2U, 0U);
}

bool board_range(uint16_t *p_mm, uint32_t *p_stamp)
{
    uint16_t pulse = 0U;
    UINT status = 0U;
    DI(status);
    pulse = g_echo_us;
    *p_stamp = g_echo_stamp;
    EI(status);
    *p_mm = (uint16_t)((uint32_t)pulse * 343U / 2000U);
    return (116U <= pulse) && (23324U >= pulse);
}

static bool board_iic_wait(uint32_t offset, uint32_t mask)
{
    uint32_t started = board_micros();
    while (0U == (in_w(BOARD_IIC_BASE + offset) & mask))
    {
        if (1000U < board_micros() - started)
        {
            return false;
        }
    }
    return true;
}

static bool board_iic(uint8_t reg, uint8_t *p_bytes, uint32_t count,
                      bool b_read)
{
    uint32_t index = 0U;
    out_w(BOARD_IIC_BASE + 0x6cU, 0U);
    out_w(BOARD_IIC_BASE + 4U, 0x19U);
    (void)in_w(BOARD_IIC_BASE + 0x40U);
    out_w(BOARD_IIC_BASE + 0x6cU, 1U);
    out_w(BOARD_IIC_BASE + 0x10U, reg);
    for (index = 0U; count > index; ++index)
    {
        uint32_t command = b_read ? 0x100U : p_bytes[index];
        command |= (count - 1U == index) ? 0x200U : 0U;
        command |= (b_read && (0U == index)) ? 0x400U : 0U;
        if (!board_iic_wait(0x70U, 2U))
        {
            return false;
        }
        out_w(BOARD_IIC_BASE + 0x10U, command);
        if (b_read)
        {
            if (!board_iic_wait(0x70U, 8U))
            {
                return false;
            }
            p_bytes[index] = (uint8_t)in_w(BOARD_IIC_BASE + 0x10U);
        }
    }
    return board_iic_wait(0x34U, 0x200U) &&
           (0U == in_w(BOARD_IIC_BASE + 0x80U));
}

bool board_accel(float *p_accel)
{
    uint8_t raw[6] = {0U};
    uint32_t index = 0U;
    if (!board_iic(0xa8U, raw, 6U, true))
    {
        return false;
    }
    for (index = 0U; 3U > index; ++index)
    {
        uint16_t word = (uint16_t)((uint16_t)raw[index * 2U] |
                                   ((uint16_t)raw[index * 2U + 1U] << 8U));
        p_accel[index] = (float)(int16_t)word / 16000.0f;
    }
    return true;
}

bool board_init(void)
{
    uint32_t pin = 0U;
    uint32_t started = board_micros();
    uint8_t setting = 0x57U;
    T_DINT definition = {0};
    static uint8_t const g_inputs[7] = {3U, 13U, 16U, 17U, 19U, 20U, 21U};

    board_force_off();
    (void)gpio_set_pin(2U, GPIO_MODE_OUT);
    for (pin = 0U; 7U > pin; ++pin)
    {
        (void)gpio_set_pin(g_inputs[pin], GPIO_MODE_IN);
    }
    for (pin = 20U; 22U > pin; ++pin)
    {
        set_w(BOARD_PADS_BASE + 4U + pin * 4U, 8U);
        clr_w(BOARD_PADS_BASE + 4U + pin * 4U, 4U);
    }
    clr_w(RESETS_RESET, (1U << 0U) | (1U << 3U) | (1U << 14U));
    while (
        (in_w(RESETS_RESET_DONE) & ((1U << 0U) | (1U << 3U) | (1U << 14U))) !=
        ((1U << 0U) | (1U << 3U) | (1U << 14U)))
    {
        if (10000U < board_micros() - started)
        {
            return false;
        }
    }
    for (pin = 8U; 12U > pin; ++pin)
    {
        (void)pwm_set_pin(pin);
        (void)pwm_set_wrap(pin, 6249U);
        (void)pwm_set_cc(pin, 0U);
        (void)pwm_set_enabled(pin, TRUE);
    }
    board_servo_init(false);
    for (pin = 26U; 29U > pin; ++pin)
    {
        out_w(GPIO_CTRL(pin), 31U);
        clr_w(BOARD_PADS_BASE + 4U + pin * 4U, 0x4cU);
    }
    for (pin = 4U; 6U > pin; ++pin)
    {
        out_w(GPIO_CTRL(pin), 3U);
        set_w(BOARD_PADS_BASE + 4U + pin * 4U, 8U);
    }
    out_w(BOARD_IIC_BASE + 0x6cU, 0U);
    out_w(BOARD_IIC_BASE, 0x63U);
    out_w(BOARD_IIC_BASE + 0x14U, 600U);
    out_w(BOARD_IIC_BASE + 0x18U, 650U);
    definition.intatr = TA_HLNG;
    definition.inthdr = (FP)board_gpio_isr;
    if (E_OK != tk_def_int(BOARD_GPIO_IRQ, &definition))
    {
        return false;
    }
    out_w(BOARD_IO_BASE + 0x100U, 12U << 12U);
    out_w(BOARD_IO_BASE + 0x108U, 12U | (12U << 4U) | (4U << 20U));
    EnableInt(BOARD_GPIO_IRQ, 2U);
    if (!board_iic(0x20U, &setting, 1U, false))
    {
        return false;
    }
    setting = 0x88U;
    if (!board_iic(0x23U, &setting, 1U, false))
    {
        return false;
    }
    board_servo_init(true);
    return true;
}
