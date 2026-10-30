/* Nonblocking motion primitives for the single mission owner. */
#ifndef MOTION_H
#define MOTION_H
#include "../robot.h"

typedef enum
{
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT,
    MOTION_SPEED,
    MOTION_STOP
} motion_command_t;

typedef struct
{
    float goal;
    float heading;
    float speed;
    uint32_t started_ms;
    uint8_t command;
    bool b_active;
} motion_job_t;

uint16_t motion_pack(motion_command_t command, uint16_t argument,
                     bool b_obstacle_owner);
bool motion_begin(motion_job_t *p_job, uint16_t request, bool b_obstacle_owner,
                  float position_mm, float yaw_deg, uint32_t now_ms);
bool motion_step(motion_job_t *p_job, float position_mm, float yaw_deg,
                 uint32_t now_ms, float *p_left, float *p_right);
#endif
