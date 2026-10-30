/* LSM303 accelerometer-only terrain estimate, gated by acceleration norm. */
#include "terrain.h"
#include <math.h>

void robot_terrain_step(robot_terrain_t *p_terrain, float accel_x,
                        float accel_y, float accel_z, float delta_mm,
                        bool b_stationary)
{
    float norm =
        sqrtf(accel_x * accel_x + accel_y * accel_y + accel_z * accel_z);
    float pitch = 0.0f;

    if (isfinite(norm) && (0.9f < norm) && (1.1f > norm))
    {
        pitch = atan2f(-accel_x, sqrtf(accel_y * accel_y + accel_z * accel_z));
        if ((!p_terrain->b_ready) && b_stationary)
        {
            p_terrain->bias_sum += pitch;
            ++p_terrain->bias_count;
            if (200U <= p_terrain->bias_count)
            {
                p_terrain->bias_rad = p_terrain->bias_sum / 200.0f;
                p_terrain->b_ready = true;
            }
        }
        if (p_terrain->b_ready)
        {
            p_terrain->pitch_rad +=
                0.2f * (pitch - p_terrain->bias_rad - p_terrain->pitch_rad);
            if ((0.0873f < p_terrain->pitch_rad) && (0.0f < delta_mm))
            {
                p_terrain->b_hump = true;
            }
            if (p_terrain->b_hump)
            {
                p_terrain->height_mm += sinf(p_terrain->pitch_rad) * delta_mm;
                p_terrain->peak_mm =
                    fmaxf(p_terrain->peak_mm, p_terrain->height_mm);
                p_terrain->flat_count =
                    (0.035f > fabsf(p_terrain->pitch_rad))
                        ? (uint8_t)(p_terrain->flat_count + 1U)
                        : 0U;
                if (20U <= p_terrain->flat_count)
                {
                    p_terrain->maximum_mm =
                        fmaxf(p_terrain->maximum_mm, p_terrain->peak_mm);
                    p_terrain->height_mm = 0.0f;
                    p_terrain->peak_mm = 0.0f;
                    p_terrain->b_hump = false;
                    p_terrain->flat_count = 0U;
                }
            }
        }
    }
}
