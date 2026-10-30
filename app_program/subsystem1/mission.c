/* Mission and bypass ownership live in one controller to avoid queue races. */
#include "mission.h"
#include <math.h>
#include <string.h>

static void mission_enter(robot_nav_t *p_nav, robot_mode_t mode,
                          robot_input_t const *p_input)
{
    p_nav->mode = mode;
    p_nav->state_since_ms = p_input->now_ms;
    p_nav->left_mm_s = 0.0f;
    p_nav->right_mm_s = 0.0f;
}

static void mission_turn(robot_nav_t *p_nav, robot_input_t const *p_input,
                         float degrees)
{
    p_nav->goal_yaw_deg = p_input->yaw_deg + degrees;
    p_nav->turn_degrees = degrees;
    p_nav->state_since_ms = p_input->now_ms;
}

static bool mission_turn_step(robot_nav_t *p_nav, robot_input_t const *p_input)
{
    float error = p_nav->goal_yaw_deg - p_input->yaw_deg;
    float speed = robot_limit(error * 3.0f, -100.0f, 100.0f);
    bool b_done = (2.0f >= fabsf(error));

    p_nav->left_mm_s = b_done ? 0.0f : -speed;
    p_nav->right_mm_s = b_done ? 0.0f : speed;
    return b_done;
}

static bool mission_move_step(robot_nav_t *p_nav, robot_input_t const *p_input)
{
    float remaining = p_nav->goal_distance_mm - p_input->distance_mm;
    float correction = robot_limit(
        (p_nav->goal_yaw_deg - p_input->yaw_deg) * 4.0f, -60.0f, 60.0f);
    bool b_done = (3.0f >= remaining);
    float speed = b_done ? 0.0f : robot_limit(remaining * 3.0f, 60.0f, 200.0f);

    p_nav->left_mm_s = b_done ? 0.0f : speed - correction;
    p_nav->right_mm_s = b_done ? 0.0f : speed + correction;
    return b_done;
}

static void mission_bypass(robot_nav_t *p_nav, robot_input_t const *p_input)
{
    bool b_done = false;

    if (0U == (p_nav->leg & 1U))
    {
        b_done = mission_turn_step(p_nav, p_input);
    }
    else
    {
        b_done = mission_move_step(p_nav, p_input);
    }
    if (b_done)
    {
        ++p_nav->leg;
        p_nav->state_since_ms = p_input->now_ms;
        switch (p_nav->leg)
        {
            case 1U:
                p_nav->goal_distance_mm =
                    p_input->distance_mm + p_nav->bypass_side_mm;
                break;
            case 2U:
                mission_turn(p_nav, p_input, -90.0f * p_nav->side);
                break;
            case 3U:
                p_nav->goal_distance_mm =
                    p_input->distance_mm + p_nav->bypass_forward_mm;
                break;
            case 4U:
                mission_turn(p_nav, p_input, -90.0f * p_nav->side);
                break;
            case 5U:
                p_nav->goal_distance_mm =
                    p_input->distance_mm + p_nav->bypass_side_mm - 60.0f;
                p_nav->recovery_start_mm = p_input->distance_mm;
                break;
            default:
                mission_enter(p_nav, ROBOT_RECOVER, p_input);
                p_nav->b_black_run = false;
                p_nav->goal_distance_mm = p_input->distance_mm + 80.0f;
                break;
        }
    }
    if ((5U == p_nav->leg) &&
        (p_input->distance_mm > p_nav->goal_distance_mm - 60.0f) &&
        (0.0f < p_input->front_mm) && (80U > p_input->front_mm))
    {
        p_nav->fault_request |= ROBOT_FAULT_BYPASS;
    }
}

static void mission_follow(robot_nav_t *p_nav, robot_input_t const *p_input,
                           robot_config_t const *p_config)
{
    uint16_t line_state = (p_input->line_word >> 8U) & 3U;
    int8_t error = (int8_t)(p_input->line_word & 255U);
    float speed = p_config->cruise_mm_s;
    float delta = fmaxf(0.0f, p_input->distance_mm - p_nav->last_distance_mm);

    if ('\0' != p_nav->pending)
    {
        p_nav->pending_budget_mm -= delta;
        if (0.0f >= p_nav->pending_budget_mm)
        {
            p_nav->pending = '\0';
        }
    }
    if (('A' <= p_input->barcode) && ('D' >= p_input->barcode))
    {
        p_nav->pending = p_input->barcode;
        p_nav->pending_budget_mm = 500.0f;
    }
    if ('D' == p_nav->pending)
    {
        p_nav->pending = '\0';
        mission_enter(p_nav, ROBOT_TURN, p_input);
        mission_turn(p_nav, p_input, 180.0f);
        return;
    }
    if (2U == line_state)
    {
        if (!p_nav->b_black_run)
        {
            p_nav->black_start_mm = p_input->distance_mm;
            p_nav->b_black_run = true;
        }
        if (60.0f < p_input->distance_mm - p_nav->black_start_mm)
        {
            mission_enter(p_nav, ROBOT_FINISHED, p_input);
            return;
        }
        if (3U > p_nav->junction_count)
        {
            ++p_nav->junction_count;
        }
    }
    else
    {
        p_nav->b_black_run = false;
        p_nav->b_junction_latched = false;
    }
    if ((2U != line_state) && (3U == p_nav->junction_count) &&
        (!p_nav->b_junction_latched))
    {
        char command = p_nav->pending;
        p_nav->pending = '\0';
        p_nav->junction_count = 0U;
        p_nav->b_junction_latched = true;
        if (('A' == command) || ('B' == command))
        {
            mission_enter(p_nav, ROBOT_TURN, p_input);
            mission_turn(p_nav, p_input, ('A' == command) ? 90.0f : -90.0f);
            return;
        }
    }
    if (p_input->b_front_valid && (200U > p_input->front_mm))
    {
        mission_enter(p_nav, ROBOT_SCAN, p_input);
        p_nav->b_scan = true;
        return;
    }
    if (3U == line_state)
    {
        mission_enter(p_nav, ROBOT_RECOVER, p_input);
        p_nav->recovery_start_mm = p_input->distance_mm;
        p_nav->goal_distance_mm = p_input->distance_mm + 80.0f;
        return;
    }
    speed = p_input->b_pitch_valid && (0.06f < fabsf(p_input->pitch_rad))
                ? 150.0f
                : fminf(speed, 200.0f);
    p_nav->left_mm_s = robot_limit(speed + (float)error * 0.8f, 0.0f, 350.0f);
    p_nav->right_mm_s = robot_limit(speed - (float)error * 0.8f, 0.0f, 350.0f);
}

void robot_nav_step(robot_nav_t *p_nav, robot_input_t const *p_input,
                    robot_config_t const *p_config)
{
    uint16_t line_state = (p_input->line_word >> 8U) & 3U;

    p_nav->left_mm_s = 0.0f;
    p_nav->right_mm_s = 0.0f;
    if (0U != (p_input->faults & ROBOT_CRITICAL_MASK))
    {
        mission_enter(p_nav, ROBOT_FAULT, p_input);
    }
    if (p_nav->b_reset && (0U == (p_input->faults & ROBOT_CRITICAL_MASK)))
    {
        memset(p_nav, 0, sizeof(*p_nav));
        mission_enter(p_nav, ROBOT_IDLE, p_input);
    }
    p_nav->b_reset = false;
    if (p_nav->b_end)
    {
        mission_enter(p_nav, ROBOT_FINISHED, p_input);
        p_nav->b_end = false;
    }
    switch (p_nav->mode)
    {
        case ROBOT_IDLE:
            if (p_nav->b_start && robot_config_valid(p_config) &&
                p_input->b_pitch_valid && p_input->b_front_valid)
            {
                mission_enter(p_nav, ROBOT_FOLLOW, p_input);
                p_nav->run_since_ms = p_input->now_ms;
            }
            break;
        case ROBOT_FOLLOW:
            mission_follow(p_nav, p_input, p_config);
            break;
        case ROBOT_TURN:
            if (mission_turn_step(p_nav, p_input))
            {
                mission_enter(p_nav, ROBOT_FOLLOW, p_input);
            }
            break;
        case ROBOT_SCAN:
            if (p_input->b_profile_ready && p_input->profile.b_valid)
            {
                float beam_margin = p_input->profile.closest_mm * 0.27f;
                p_nav->side =
                    (p_input->profile.left_mm >= p_input->profile.right_mm)
                        ? 1
                        : -1;
                if (p_config->body_width_mm + 100.0f >
                    fmaxf(p_input->profile.left_mm, p_input->profile.right_mm))
                {
                    p_nav->fault_request |= ROBOT_FAULT_BYPASS;
                    break;
                }
                p_nav->bypass_side_mm = p_input->profile.width_mm +
                                        p_config->body_width_mm * 0.5f + 75.0f +
                                        beam_margin;
                p_nav->bypass_forward_mm = p_input->profile.closest_mm +
                                           p_config->obstacle_depth_mm +
                                           p_config->body_length_mm + 100.0f;
                p_nav->leg = 0U;
                mission_enter(p_nav, ROBOT_BYPASS, p_input);
                mission_turn(p_nav, p_input, 90.0f * p_nav->side);
            }
            break;
        case ROBOT_BYPASS:
            mission_bypass(p_nav, p_input);
            break;
        case ROBOT_RECOVER:
            p_nav->left_mm_s = 80.0f;
            p_nav->right_mm_s = 80.0f;
            if ((2U == line_state) && (!p_nav->b_black_run))
            {
                p_nav->black_start_mm = p_input->distance_mm;
                p_nav->b_black_run = true;
            }
            if (p_nav->b_black_run && (2U != line_state))
            {
                float width = p_input->distance_mm - p_nav->black_start_mm;
                p_nav->b_black_run = false;
                if ((12.0f <= width) && (26.0f >= width))
                {
                    mission_enter(p_nav, ROBOT_TURN, p_input);
                    mission_turn(p_nav, p_input, 90.0f * p_nav->side);
                }
            }
            if ((p_input->distance_mm > p_nav->goal_distance_mm) ||
                (8000U < p_input->now_ms - p_nav->state_since_ms))
            {
                p_nav->fault_request |= ROBOT_FAULT_LINE;
            }
            break;
        case ROBOT_FINISHED:
        case ROBOT_FAULT:
        default:
            break;
    }
    p_nav->b_start = false;
    p_nav->last_distance_mm = p_input->distance_mm;
    if (((ROBOT_TURN == p_nav->mode) || (ROBOT_BYPASS == p_nav->mode) ||
         (ROBOT_SCAN == p_nav->mode)) &&
        (12000U < p_input->now_ms - p_nav->state_since_ms))
    {
        p_nav->fault_request |= ROBOT_FAULT_BYPASS;
    }
    if ((ROBOT_IDLE != p_nav->mode) && p_input->b_front_valid &&
        (80U > p_input->front_mm))
    {
        p_nav->fault_request |= ROBOT_FAULT_BYPASS;
    }
    if (0U != p_nav->fault_request)
    {
        mission_enter(p_nav, ROBOT_FAULT, p_input);
    }
}
