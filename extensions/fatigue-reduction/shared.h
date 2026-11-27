/* SPDX-License-Identifier: GPL-3.0-only */

/* What the two modules of Fatigue Reduction agree on. */

#ifndef FATIGUE_SHARED_H
#define FATIGUE_SHARED_H

#include "fatigue.h"
#include <stdint.h>

enum {
    KEY_STRENGTH = 1, /* int: FATIGUE_OFF to FATIGUE_STRONG */
};

enum { STRENGTH_DEFAULT = FATIGUE_OFF };

/* design.c: the four filters of `strength` at `rate`, 44100 or 48000 Hz, in
 * rv_dsp_biquad's format; Off is flat. 0 when a filter is not stable. */
int fatigue_design(int32_t out[FATIGUE_STAGES][5], unsigned int strength,
                   unsigned int rate);

/* modes.c: the strengths by name, for the Settings caption and the editor. */
extern const char *const strength_labels[FATIGUE_STRENGTHS];

#endif
