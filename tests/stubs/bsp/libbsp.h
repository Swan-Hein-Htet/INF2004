#ifndef TEST_LIBBSP_H
#define TEST_LIBBSP_H
#include <tk/tkernel.h>
#define TRUE 1
#define FALSE 0
#define GPIO_MODE_OUT 1U
#define GPIO_MODE_IN 0U
#define RESETS_RESET UINT32_C(0x4000c000)
#define RESETS_RESET_DONE UINT32_C(0x4000c008)
#define TIMER_TIMERAWL UINT32_C(0x40054028)
#define GPIO_CTRL(pin) (UINT32_C(0x40014004) + (pin) * 8U)
UW in_w(UW address);
void out_w(UW address, UW value);
void set_w(UW address, UW mask);
void clr_w(UW address, UW mask);
void EnableInt(UINT number, UINT priority);
ER gpio_set_pin(UINT pin, UINT mode);
ER gpio_set_val(UINT pin, UINT value);
UINT gpio_get_val(UINT pin);
ER pwm_set_pin(UINT pin);
ER pwm_set_wrap(UINT pin, UW top);
ER pwm_set_cc(UINT pin, UW compare);
ER pwm_set_enabled(UINT pin, INT enabled);
#endif
