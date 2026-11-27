/* SPDX-License-Identifier: GPL-3.0-only */

/* The crossover coefficient, integer throughout so the pump task designs it
 * for whatever rate plays: with t = tan(pi hz / rate), the bilinear one-pole
 * low-pass y += k ((x + x1) / 2 - y) with k = 2 t / (1 + t) has its -3 dB point
 * at the cutoff. The angle's sine and cosine come from a short series and
 * k = 2 sin / (sin + cos) from one shift-and-subtract division. */

#include "surround.h"
#include "q31.h"

uint32_t surround_coefficient(unsigned int hz, unsigned int rate)
{
    if (rate == 0)
        return 0;
    /* pi hz / rate as a quarter in Q31, so the series' angle stays small. */
    int32_t ratio = (int32_t)q31_ratio(hz < rate ? hz : rate - 1, rate);
    uint32_t quarter = (uint32_t)((int64_t)ratio * Q31_QUARTER_PI >> 29);
    if (quarter > Q31_QUARTER_LIMIT)
        quarter = Q31_QUARTER_LIMIT;
    int32_t s, c;
    q31_sin_cos(quarter, &s, &c);
    /* 2 sin / (sin + cos) in Q30: below 2 while the cosine is positive. */
    return q31_divide((uint64_t)(uint32_t)s << 31, (uint32_t)(s + c));
}
