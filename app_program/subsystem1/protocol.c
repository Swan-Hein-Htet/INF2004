/* Explicit bit ordering avoids compiler-dependent bit-field wire layouts. */
#include "protocol.h"
#include <string.h>

static uint8_t const g_field_widths[24] = {
    4U, 8U,  4U, 3U, 10U, 10U, 16U, 16U, 2U,  8U,  5U,  3U,
    7U, 10U, 9U, 5U, 2U,  15U, 3U,  10U, 16U, 10U, 10U, 16U};

size_t robot_pack(uint8_t *p_payload, size_t capacity, uint32_t const *p_fields)
{
    uint32_t field = 0U;
    uint32_t offset = 0U;

    if ((NULL == p_payload) || (NULL == p_fields) ||
        (ROBOT_TELEMETRY_BYTES > capacity))
    {
        return 0U;
    }
    memset(p_payload, 0, ROBOT_TELEMETRY_BYTES);
    for (field = 0U; 24U > field; ++field)
    {
        uint32_t bit = 0U;
        for (bit = 0U; g_field_widths[field] > bit; ++bit)
        {
            uint32_t value = (p_fields[field] >> bit) & 1U;
            p_payload[offset / 8U] |= (uint8_t)(value << (offset % 8U));
            ++offset;
        }
    }
    return (offset + 7U) / 8U;
}

bool robot_command(uint8_t const *p_payload, size_t length, uint8_t *p_sequence,
                   bool *p_b_seen, uint8_t *p_command)
{
    bool b_valid = false;
    uint8_t sequence = 0U;

    if ((NULL != p_payload) && (1U == length))
    {
        sequence = p_payload[0] >> 2U;
        if ((!*p_b_seen) || (sequence != *p_sequence))
        {
            *p_sequence = sequence;
            *p_b_seen = true;
            *p_command = p_payload[0] & 3U;
            b_valid = true;
        }
    }
    return b_valid;
}
