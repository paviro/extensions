/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Ported from Rockbox lib/rbcodec/dsp/afr.c.
 *
 * Copyright (C) 2014 by Chiwen Chang
 * Modified by the RevolaOS authors, 2026.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/* Rockbox's four filters per strength: a high shelf above 20 kHz and a bell at
 * 8 kHz of the treble cut, a bell at 3.15 kHz of its own cut, and a low shelf
 * below 200 Hz. Gains in tenths of a decibel. */

#include "rv_dsp.h"
#include "shared.h"

struct gains {
    int16_t treble, bass, presence;
};

static const struct gains strengths[FATIGUE_STRENGTHS] = {
    {0, 0, 0},
    {-29, 0, -13},
    {-58, -5, -29},
    {-72, -5, -48},
};

int fatigue_design(int32_t out[FATIGUE_STAGES][5], unsigned int strength,
                   unsigned int rate)
{
    if (strength >= FATIGUE_STRENGTHS)
        return 0;
    const struct gains *g = &strengths[strength];
    return rv_dsp_bishelf(out[0], 8000, 20000, 0, g->treble / 2, 0, rate) &&
           rv_dsp_biquad(out[1], RV_DSP_PEAKING, 8000, 28, g->treble, 0, rate) &&
           rv_dsp_biquad(out[2], RV_DSP_PEAKING, 3150, 28, g->presence, 0, rate) &&
           rv_dsp_bishelf(out[3], 200, 1280, g->bass, 0, 0, rate);
}
