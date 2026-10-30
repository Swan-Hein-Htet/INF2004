/* Code 39 framing and independently calibrated line sensing. */
#include "sensing.h"
#include <string.h>

static uint16_t const g_patterns[27] = {
    0x109U, 0x049U, 0x148U, 0x019U, 0x118U, 0x058U, 0x00dU, 0x10cU, 0x04cU,
    0x01cU, 0x103U, 0x043U, 0x142U, 0x013U, 0x112U, 0x052U, 0x007U, 0x106U,
    0x046U, 0x016U, 0x181U, 0x0c1U, 0x1c0U, 0x091U, 0x190U, 0x0d0U, 0x094U};

static int32_t sensing_blackness(robot_ir_cal_t const *p_cal, uint16_t raw)
{
    int32_t denominator = (int32_t)p_cal->black - (int32_t)p_cal->white;
    int32_t value = 0;

    if (0 != denominator)
    {
        value = ((int32_t)raw - (int32_t)p_cal->white) * 127 / denominator;
        value = (0 > value) ? 0 : value;
        value = (127 < value) ? 127 : value;
    }
    return value;
}

uint16_t robot_line_word(robot_ir_cal_t const *p_cal, uint16_t left,
                         uint16_t right, uint8_t sequence)
{
    int32_t left_black = sensing_blackness(&p_cal[0], left);
    int32_t right_black = sensing_blackness(&p_cal[1], right);
    int32_t error = right_black - left_black;
    uint16_t state = 0U;

    if ((64 < left_black) && (64 < right_black))
    {
        state = 2U;
    }
    else if ((32 > left_black) && (32 > right_black))
    {
        state = 3U;
    }
    else
    {
        state = 1U;
    }
    return (uint16_t)((uint16_t)(uint8_t)(int8_t)error |
                      ((uint32_t)state << 8U) | ((sequence & 15U) << 12U));
}

static int32_t sensing_symbol(uint8_t const *p_widths, bool b_reverse)
{
    uint8_t sorted[9] = {0U};
    uint32_t index = 0U;
    uint32_t inner = 0U;
    uint16_t pattern = 0U;
    int32_t symbol = -1;
    uint32_t narrow_sum = 0U;
    uint32_t wide_sum = 0U;
    bool b_valid = true;

    memcpy(sorted, p_widths, sizeof(sorted));
    for (index = 1U; 9U > index; ++index)
    {
        uint8_t value = sorted[index];
        inner = index;
        while ((0U < inner) && (value < sorted[inner - 1U]))
        {
            sorted[inner] = sorted[inner - 1U];
            --inner;
        }
        sorted[inner] = value;
    }
    for (index = 0U; 9U > index; ++index)
    {
        b_valid = b_valid && (3U <= sorted[index]) && (255U != sorted[index]);
        if (6U > index)
        {
            narrow_sum += sorted[index];
        }
        else
        {
            wide_sum += sorted[index];
        }
    }
    b_valid = b_valid && (sorted[6] * 10U >= sorted[5] * 16U) &&
              (wide_sum * 2U >= narrow_sum * 2U) &&
              (wide_sum * 2U <= narrow_sum * 4U) &&
              (sorted[5] * 10U <= sorted[0] * 18U) &&
              (sorted[8] * 10U <= sorted[6] * 18U);
    for (index = 0U; 9U > index; ++index)
    {
        uint32_t source = b_reverse ? (8U - index) : index;
        pattern = (uint16_t)(pattern << 1U);
        if (p_widths[source] >= sorted[6])
        {
            pattern |= 1U;
        }
    }
    for (index = 0U; 27U > index; ++index)
    {
        if (b_valid && (pattern == g_patterns[index]))
        {
            symbol = (int32_t)index;
        }
    }
    return symbol;
}

char robot_barcode_decode(uint8_t const *p_widths, size_t count)
{
    char result = '\0';
    uint32_t direction = 0U;

    if ((NULL != p_widths) && (ROBOT_BAR_ELEMENTS == count))
    {
        for (direction = 0U; 2U > direction; ++direction)
        {
            bool b_reverse = (0U != direction);
            int32_t first = sensing_symbol(p_widths, b_reverse);
            int32_t middle = sensing_symbol(&p_widths[10], b_reverse);
            int32_t last = sensing_symbol(&p_widths[20], b_reverse);
            uint32_t index = 0U;
            uint32_t minimum = 255U;
            for (index = 0U; 9U > index; ++index)
            {
                minimum =
                    (minimum > p_widths[index]) ? p_widths[index] : minimum;
            }
            if ((26 == first) && (26 == last) && (0 <= middle) &&
                (26 > middle) && (minimum * 3U >= p_widths[9]) &&
                (minimum * 3U >= p_widths[19]) &&
                (minimum / 2U <= p_widths[9]) && (minimum / 2U <= p_widths[19]))
            {
                result = (char)('A' + middle);
            }
        }
    }
    return result;
}

char robot_barcode_sample(robot_barcode_t *p_reader, bool b_black,
                          bool b_line_ok)
{
    char result = '\0';

    if (!b_line_ok)
    {
        memset(p_reader, 0, sizeof(*p_reader));
        p_reader->reject = 1U;
    }
    else if (!p_reader->b_capture)
    {
        if (!b_black)
        {
            p_reader->quiet_ms =
                (500U > p_reader->quiet_ms) ? (p_reader->quiet_ms + 1U) : 500U;
            p_reader->b_armed = (100U <= p_reader->quiet_ms);
        }
        else if (p_reader->b_armed)
        {
            p_reader->b_capture = true;
            p_reader->b_previous_black = true;
            p_reader->run_ms = 1U;
            p_reader->count = 0U;
            p_reader->quiet_ms = 0U;
        }
        else
        {
            p_reader->quiet_ms = 0U;
            p_reader->count = 0U;
            p_reader->reject = 4U;
        }
    }
    else if (b_black == p_reader->b_previous_black)
    {
        if (254U > p_reader->run_ms)
        {
            ++p_reader->run_ms;
        }
        else
        {
            memset(p_reader, 0, sizeof(*p_reader));
            p_reader->reject = 2U;
        }
    }
    else
    {
        p_reader->widths[p_reader->count] = p_reader->run_ms;
        ++p_reader->count;
        p_reader->run_ms = 1U;
        p_reader->b_previous_black = b_black;
        if (ROBOT_BAR_ELEMENTS == p_reader->count)
        {
            p_reader->b_capture = false;
            p_reader->b_armed = false;
            p_reader->quiet_ms = 0U;
        }
    }
    if ((!p_reader->b_capture) && (!b_black) &&
        (ROBOT_BAR_ELEMENTS == p_reader->count) && (100U <= p_reader->quiet_ms))
    {
        result = robot_barcode_decode(p_reader->widths, p_reader->count);
        p_reader->reject = ('\0' == result) ? 3U : 0U;
        p_reader->count = 0U;
    }
    return result;
}
