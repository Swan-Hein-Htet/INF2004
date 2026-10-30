/* Replace measured values only after completing README calibration. */
#include "board.h"

/* The guided host tool replaces only this object in a hash-checked UF2. */
_Static_assert(48U == sizeof(robot_config_t), "calibration object size");
_Static_assert(24U == offsetof(robot_config_t, encoder_sign), "encoder layout");
_Static_assert(26U == offsetof(robot_config_t, motor_sign), "motor layout");
_Static_assert(28U == offsetof(robot_config_t, ir), "IR layout");
_Static_assert(6U == sizeof(robot_ir_cal_t), "IR stride");
_Static_assert(46U == offsetof(robot_config_t, b_calibrated), "calibration flag");

#if APP_ASSUMED_DEMO
/* USER-REQUESTED ESTIMATES, NOT MEASUREMENTS. For supervised demo trials only.
 * b_calibrated enables the legacy configuration gate; it is not a measurement
 * claim in this explicitly named demo build. All runtime fault stops remain. */
robot_config_t const g_robot_config = {
    .wheel_mm_per_edge = 0.50f,
    .track_mm = 140.0f,
    .body_width_mm = 180.0f,
    .body_length_mm = 240.0f,
    .obstacle_depth_mm = 150.0f,
    .cruise_mm_s = 100.0f,
    .encoder_sign = {1, 1},
    .motor_sign = {1, 1},
    .ir = {{3500U, 500U, false}, {3500U, 500U, false},
           {3500U, 500U, false}},
    .b_calibrated = true};
#else
robot_config_t const g_robot_config = {
    .wheel_mm_per_edge = 0.0f,
    .track_mm = 0.0f,
    .body_width_mm = 0.0f,
    .body_length_mm = 0.0f,
    .obstacle_depth_mm = 0.0f,
    .cruise_mm_s = 200.0f,
    .encoder_sign = {1, 1},
    .motor_sign = {1, 1},
    .ir = {{0U, 4095U, true}, {0U, 4095U, true}, {0U, 4095U, true}},
    .b_calibrated = false};
#endif
