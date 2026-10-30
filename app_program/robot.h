/* Autonomous car interfaces. See README.md for contracts and units. */
#ifndef ROBOT_H
#define ROBOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROBOT_BAR_ELEMENTS 29U
#define ROBOT_SCAN_POINTS 13U
#define ROBOT_TELEMETRY_BYTES 26U
#define ROBOT_FAULT_STOP UINT16_C(1)
#define ROBOT_FAULT_STALL UINT16_C(2)
#define ROBOT_FAULT_ENCODER UINT16_C(4)
#define ROBOT_FAULT_IMU UINT16_C(8)
#define ROBOT_FAULT_IMPACT UINT16_C(16)
#define ROBOT_FAULT_RANGE UINT16_C(32)
#define ROBOT_FAULT_BYPASS UINT16_C(64)
#define ROBOT_FAULT_WIFI UINT16_C(128)
#define ROBOT_FAULT_TIMING UINT16_C(256)
#define ROBOT_FAULT_LINE UINT16_C(512)
#define ROBOT_FAULT_CONFIG UINT16_C(1024)
#define ROBOT_CRITICAL_MASK UINT16_C(0x077f)

typedef enum
{
    ROBOT_IDLE,
    ROBOT_FOLLOW,
    ROBOT_TURN,
    ROBOT_SCAN,
    ROBOT_BYPASS,
    ROBOT_RECOVER,
    ROBOT_FINISHED,
    ROBOT_FAULT
} robot_mode_t;

typedef struct
{
    uint16_t black;
    uint16_t white;
    bool b_black_low;
} robot_ir_cal_t;

typedef struct
{
    uint8_t widths[ROBOT_BAR_ELEMENTS];
    uint8_t count;
    uint8_t run_ms;
    uint16_t quiet_ms;
    bool b_previous_black;
    bool b_capture;
    bool b_armed;
    uint8_t reject;
} robot_barcode_t;

typedef struct
{
    float integral;
    float previous;
    float kp;
    float ki;
    float kd;
} robot_pid_t;

typedef struct
{
    float pitch_rad;
    float height_mm;
    float peak_mm;
    float maximum_mm;
    float bias_rad;
    uint16_t bias_count;
    float bias_sum;
    uint8_t flat_count;
    bool b_hump;
    bool b_ready;
} robot_terrain_t;

typedef struct
{
    uint16_t range_mm[ROBOT_SCAN_POINTS];
    uint16_t width_mm;
    uint16_t closest_mm;
    uint16_t left_mm;
    uint16_t right_mm;
    uint8_t bearing;
    bool b_valid;
} robot_profile_t;

typedef struct
{
    float wheel_mm_per_edge;
    float track_mm;
    float body_width_mm;
    float body_length_mm;
    float obstacle_depth_mm;
    float cruise_mm_s;
    int8_t encoder_sign[2];
    int8_t motor_sign[2];
    robot_ir_cal_t ir[3];
    bool b_calibrated;
} robot_config_t;

typedef struct
{
    uint32_t now_ms;
    float distance_mm;
    float yaw_deg;
    uint16_t line_word;
    uint16_t front_mm;
    bool b_front_valid;
    bool b_profile_ready;
    bool b_pitch_valid;
    float pitch_rad;
    robot_profile_t profile;
    char barcode;
    uint16_t faults;
} robot_input_t;

typedef struct
{
    robot_mode_t mode;
    char pending;
    float pending_budget_mm;
    float last_distance_mm;
    float goal_distance_mm;
    float goal_yaw_deg;
    float turn_degrees;
    float black_start_mm;
    float bypass_side_mm;
    float bypass_forward_mm;
    float recovery_start_mm;
    uint32_t state_since_ms;
    uint32_t run_since_ms;
    uint8_t leg;
    uint8_t junction_count;
    bool b_junction_latched;
    bool b_black_run;
    bool b_start;
    bool b_reset;
    bool b_end;
    bool b_scan;
    int8_t side;
    float left_mm_s;
    float right_mm_s;
    uint16_t fault_request;
} robot_nav_t;

bool robot_config_valid(robot_config_t const *p_config);
float robot_limit(float value, float low, float high);
float robot_pid_step(robot_pid_t *p_pid, float target, float measured,
                     float seconds, float maximum);
uint16_t robot_line_word(robot_ir_cal_t const *p_cal, uint16_t left,
                         uint16_t right, uint8_t sequence);
char robot_barcode_decode(uint8_t const *p_widths, size_t count);
char robot_barcode_sample(robot_barcode_t *p_reader, bool b_black,
                          bool b_line_ok);
void robot_terrain_step(robot_terrain_t *p_terrain, float accel_x,
                        float accel_y, float accel_z, float delta_mm,
                        bool b_stationary);
bool robot_profile_build(robot_profile_t *p_profile,
                         robot_config_t const *p_config);
void robot_nav_step(robot_nav_t *p_nav, robot_input_t const *p_input,
                    robot_config_t const *p_config);
size_t robot_pack(uint8_t *p_payload, size_t capacity,
                  uint32_t const *p_fields);
bool robot_command(uint8_t const *p_payload, size_t length, uint8_t *p_sequence,
                   bool *p_b_seen, uint8_t *p_command);

#endif
