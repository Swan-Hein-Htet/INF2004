/* Hardware and cross-task bridge. */
#ifndef BOARD_H
#define BOARD_H
#include "robot.h"

extern robot_config_t const g_robot_config;
void board_watchdog_start(void);
void board_watchdog_feed(void);
uint32_t board_micros(void);
bool board_init(void);
void board_motor(float left, float right);
void board_force_off(void);
void board_arm(bool b_enable);
uint16_t board_adc(uint32_t channel);
void board_encoder(uint16_t *p_left, uint16_t *p_right);
void board_servo(uint16_t degrees);
void board_servo_off(void);
bool board_servo_test_init(bool b_enable);
void board_status_led(bool b_on);
void board_ping(void);
bool board_range(uint16_t *p_mm, uint32_t *p_stamp);
bool board_accel(float *p_accel);
void robot_fault_set(uint16_t mask);
uint16_t robot_fault_get(void);
void robot_remote(uint8_t command);
size_t robot_telemetry(uint8_t *p_payload, size_t capacity);
void robot_network(bool b_online);
#endif
