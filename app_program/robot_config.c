/* Replace measured values only after completing README calibration. */
#include "board.h"

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
