/* SPDX-License-Identifier: GPL-3.0-only */

/* What the two modules of Bass Enhancement agree on. */

#ifndef BASS_SHARED_H
#define BASS_SHARED_H

#include "bass.h"
#include <stdint.h>

enum {
    KEY_STRENGTH = 1, /* int: BASS_OFF to BASS_100 */
};

enum { STRENGTH_DEFAULT = BASS_OFF };

/* The strength as a percentage. */
static inline unsigned int bass_percent(unsigned int strength)
{
    return 25 * strength;
}

/* design.c: the five filters of `strength` at `rate`, 44100 or 48000 Hz, in
 * rv_dsp_biquad's format, the first cut by the precut that keeps the cascade's
 * peak at or below 0 dB, which `precut_tenths` receives; Off is flat with no
 * precut. 0 when a filter is not stable. */
int bass_design(int32_t out[BASS_STAGES][5], unsigned int *precut_tenths,
                unsigned int strength, unsigned int rate);

/* modes.c: the strengths by name, for the Settings caption and the editor. */
extern const char *const strength_labels[BASS_STRENGTHS];

#endif
