/* SPDX-License-Identifier: GPL-3.0-only */

/* The five filters, with every gain scaled by the strength: a high shelf of
 * +5.3 dB above 16 kHz with the whole cascade down 0.5 dB, bells of +5.3 dB at
 * 64 Hz and +5.8 dB at 2 kHz with Q 2.8, and bells of -8.2 dB at 7.5 kHz and
 * -2.9 dB at 10 kHz with Q 4.3. At 100 % these are the gains of Rockbox's
 * perceptual bass enhancement; a lower strength scales each in decibels. The
 * precut is the cascade's peak, so the output never exceeds the input's level
 * before the limiter.
 *
 * Based on Rockbox lib/rbcodec/dsp/pbe.c (Copyright (C) 2014 by Chiwen
 * Chang). */

#include "rv_dsp.h"
#include "shared.h"

struct bell {
    uint16_t hz;
    uint8_t q_tenths;
    int16_t gain_tenths;
};

static const struct bell bells[BASS_STAGES - 1] = {
    {64, 28, 53},
    {2000, 28, 58},
    {7500, 43, -82},
    {10000, 43, -29},
};

enum { SHELF_HZ = 16000, SHELF_GAIN = 53, OVERALL_GAIN = -5, PRECUT_LIMIT = 100 };

/* `tenths` at `percent`, rounded to the nearest tenth of a decibel. */
static int scaled(int tenths, unsigned int percent)
{
    int value = tenths * (int)percent;
    return (value + (value < 0 ? -50 : 50)) / 100;
}

int bass_design(int32_t out[BASS_STAGES][5], unsigned int *precut_tenths,
                unsigned int strength, unsigned int rate)
{
    if (strength >= BASS_STRENGTHS)
        return 0;
    unsigned int percent = bass_percent(strength);
    if (!rv_dsp_bishelf(out[0], 20, SHELF_HZ, 0, scaled(SHELF_GAIN, percent),
                        scaled(OVERALL_GAIN, percent), rate))
        return 0;
    for (unsigned int i = 0; i < BASS_STAGES - 1; i++)
        if (!rv_dsp_biquad(out[i + 1], RV_DSP_PEAKING, bells[i].hz, bells[i].q_tenths,
                           scaled(bells[i].gain_tenths, percent), 0, rate))
            return 0;
    int peak = strength == BASS_OFF ? 0 : rv_dsp_peak(out, BASS_STAGES, rate);
    unsigned int precut = peak <= 0             ? 0
                          : peak > PRECUT_LIMIT ? PRECUT_LIMIT
                                                : (unsigned int)peak;
    rv_dsp_attenuate(out[0], precut);
    *precut_tenths = precut;
    return 1;
}
