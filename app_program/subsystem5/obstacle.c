/* Conservative scan envelope; clearance uses raw beam bounds. */
#include "obstacle.h"
#include <math.h>

bool robot_profile_build(robot_profile_t *p_profile,
                         robot_config_t const *p_config)
{
    (void)p_config;
    uint32_t index = 0U;
    float minimum_x = 10000.0f;
    float maximum_x = -10000.0f;
    float closest = 10000.0f;
    float left_clearance = 1023.0f;
    float right_clearance = 1023.0f;
    uint32_t hits = 0U;

    p_profile->b_valid = false;
    for (index = 0U; ROBOT_SCAN_POINTS > index; ++index)
    {
        float angle = ((float)index * 10.0f - 60.0f) * 0.017453293f;
        float range = (float)p_profile->range_mm[index];
        if ((20.0f > range) || (4000.0f < range))
        {
            return false;
        }
        if (600.0f > range)
        {
            float lateral = sinf(angle) * range;
            float spread = tanf(0.1309f) * range;
            minimum_x = fminf(minimum_x, lateral - spread);
            maximum_x = fmaxf(maximum_x, lateral + spread);
            if (closest > range)
            {
                closest = range;
                p_profile->bearing = (uint8_t)(index + 3U);
            }
            ++hits;
        }
        if (3U >= index)
        {
            right_clearance = fminf(right_clearance, range);
        }
        if (9U <= index)
        {
            left_clearance = fminf(left_clearance, range);
        }
    }
    if ((0U < hits) && (1000.0f > maximum_x - minimum_x))
    {
        float beam = 2.0f * closest * tanf(0.1309f);
        p_profile->width_mm =
            (uint16_t)robot_limit(maximum_x - minimum_x - beam, 1.0f, 1023.0f);
        p_profile->closest_mm = (uint16_t)closest;
        p_profile->left_mm = (uint16_t)left_clearance;
        p_profile->right_mm = (uint16_t)right_clearance;
        p_profile->b_valid = true;
    }
    return p_profile->b_valid;
}
