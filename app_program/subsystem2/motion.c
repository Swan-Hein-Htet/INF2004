/* Wheel control arithmetic; hardware policy is in board.c. */
#include "motion.h"
#include <math.h>
#include <stdlib.h>

float robot_limit(float value, float low, float high)
{
    return fminf(high, fmaxf(low, value));
}

bool robot_config_valid(robot_config_t const *p_config)
{
    bool b_valid = (NULL != p_config);
    uint32_t index = 0U;

    if (b_valid)
    {
        b_valid =
            p_config->b_calibrated && isfinite(p_config->wheel_mm_per_edge) &&
            (0.01f < p_config->wheel_mm_per_edge) &&
            (20.0f > p_config->wheel_mm_per_edge) &&
            (50.0f < p_config->track_mm) && (500.0f > p_config->track_mm) &&
            (50.0f < p_config->body_width_mm) &&
            (500.0f > p_config->body_width_mm) &&
            (50.0f < p_config->body_length_mm) &&
            (600.0f > p_config->body_length_mm) &&
            (20.0f <= p_config->obstacle_depth_mm) &&
            (1000.0f >= p_config->obstacle_depth_mm) &&
            (100.0f <= p_config->cruise_mm_s) &&
            (350.0f >= p_config->cruise_mm_s);
        for (index = 0U; 3U > index; ++index)
        {
            int32_t span = (int32_t)p_config->ir[index].white -
                           (int32_t)p_config->ir[index].black;
            b_valid = b_valid && (200 <= abs(span)) &&
                      (4095U >= p_config->ir[index].white) &&
                      (4095U >= p_config->ir[index].black) &&
                      (p_config->ir[index].b_black_low == (0 < span));
        }
        for (index = 0U; 2U > index; ++index)
        {
            b_valid = b_valid && (1 == abs(p_config->encoder_sign[index])) &&
                      (1 == abs(p_config->motor_sign[index]));
        }
    }
    return b_valid;
}

float robot_pid_step(robot_pid_t *p_pid, float target, float measured,
                     float seconds, float maximum)
{
    float result = 0.0f;
    float error = target - measured;
    float candidate = 0.0f;

    if ((NULL != p_pid) && isfinite(error) && (0.0f < seconds))
    {
        candidate =
            robot_limit(p_pid->integral + error * seconds, -maximum, maximum);
        result = p_pid->kp * error + p_pid->ki * candidate +
                 p_pid->kd * (error - p_pid->previous) / seconds;
        if ((maximum > fabsf(result)) || (0.0f > result * error))
        {
            p_pid->integral = candidate;
        }
        p_pid->previous = error;
        result = robot_limit(result, -maximum, maximum);
        if (0.0f == target)
        {
            p_pid->integral = 0.0f;
            result = 0.0f;
        }
    }
    return result;
}

uint16_t motion_pack(motion_command_t command, uint16_t argument,
                     bool b_obstacle_owner)
{
    uint16_t request = UINT16_MAX;
    if ((MOTION_STOP >= command) && (4095U >= argument))
    {
        request = (uint16_t)((uint32_t)command | ((uint32_t)argument << 3U) |
                             (b_obstacle_owner ? 0x8000U : 0U));
    }
    return request;
}

bool motion_begin(motion_job_t *p_job, uint16_t request, bool b_obstacle_owner,
                  float position_mm, float yaw_deg, uint32_t now_ms)
{
    uint16_t argument = (request >> 3U) & 4095U;
    uint8_t command = request & 7U;
    bool b_source = (0U != (request & 0x8000U));
    bool b_valid = (command <= MOTION_STOP) && (b_source == b_obstacle_owner) &&
                   isfinite(position_mm) && isfinite(yaw_deg);
    if (((MOTION_LEFT == command) || (MOTION_RIGHT == command)) &&
        (180U < argument))
    {
        b_valid = false;
    }
    if ((MOTION_SPEED == command) && (350U < argument))
    {
        b_valid = false;
    }
    if (b_valid)
    {
        p_job->command = command;
        p_job->started_ms = now_ms;
        p_job->heading = yaw_deg;
        p_job->speed = (MOTION_SPEED == command) ? (float)argument : 150.0f;
        p_job->goal = position_mm;
        p_job->b_active = (MOTION_STOP != command) &&
                          ((MOTION_SPEED != command) || (0U != argument));
        if ((MOTION_FORWARD == command) || (MOTION_BACKWARD == command))
        {
            p_job->goal += (MOTION_FORWARD == command) ? (float)argument
                                                       : -(float)argument;
        }
        else if ((MOTION_LEFT == command) || (MOTION_RIGHT == command))
        {
            p_job->goal =
                yaw_deg +
                ((MOTION_LEFT == command) ? (float)argument : -(float)argument);
        }
        else
        {
            p_job->goal = 0.0f;
        }
    }
    return b_valid;
}

bool motion_step(motion_job_t *p_job, float position_mm, float yaw_deg,
                 uint32_t now_ms, float *p_left, float *p_right)
{
    float error = 0.0f;
    float correction = 0.0f;
    *p_left = 0.0f;
    *p_right = 0.0f;
    if ((30000U < now_ms - p_job->started_ms) || (!isfinite(position_mm)) ||
        (!isfinite(yaw_deg)))
    {
        p_job->b_active = false;
    }
    if (p_job->b_active)
    {
        if ((MOTION_LEFT == p_job->command) || (MOTION_RIGHT == p_job->command))
        {
            error = p_job->goal - yaw_deg;
            p_job->b_active = (2.0f < fabsf(error));
            *p_left = -robot_limit(error * 3.0f, -100.0f, 100.0f);
            *p_right = -*p_left;
        }
        else
        {
            error = p_job->goal - position_mm;
            correction =
                robot_limit((p_job->heading - yaw_deg) * 4.0f, -50.0f, 50.0f);
            p_job->b_active =
                (MOTION_SPEED == p_job->command) || (3.0f < fabsf(error));
            *p_left = (MOTION_SPEED == p_job->command)
                          ? p_job->speed
                          : robot_limit(error * 3.0f, -150.0f, 150.0f);
            *p_right = *p_left + correction;
            *p_left -= correction;
        }
    }
    if (!p_job->b_active)
    {
        *p_left = 0.0f;
        *p_right = 0.0f;
    }
    return !p_job->b_active;
}
