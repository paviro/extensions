/* SPDX-License-Identifier: GPL-3.0-only */

/* A Haas stage on the native interleaved S16 output, in place: the right
 * channel, or the side in Side Only, is split at two cutoffs; the band between
 * them stays where it is and the rest reaches the right ear a few milliseconds
 * late, so the stage widens while voices keep their place. Emphasis moves
 * level from the leading channel to the lagging one, which recentres the
 * image, and the mix blends the result with the original.
 *
 * Based on Rockbox lib/rbcodec/dsp/surround.c (Copyright (C) 2014 by Chiwen
 * Chang).
 *
 * The UI task leaves a set with interrupts masked and moves the generation;
 * the pump task takes it over when it sees the generation move and fades the
 * state that played into the new one. Only the pump task touches what plays.
 * Samples are S16 raised by SAMPLE_SHIFT bits; the one-pole coefficients are
 * Q30 and the gains Q15, so every product needs 64 bits. */

#include "surround.h"
#include "rv_dsp.h"
#include "rv_libc.h"

enum {
    SAMPLE_SHIFT = 12,
    UNITY = 1 << 15,
};

static struct surround_set next;

/* Pump task only. */
struct surround_state {
    struct surround_set set;
    uint8_t on;
    int32_t k_low, k_high; /* Q30, the one-pole coefficients at the two cutoffs */
    int32_t low, high;     /* the one-pole states */
    int32_t previous;      /* the last input of the split, for the bilinear zero */
    int32_t lead, lag;     /* Q15, the emphasis gains of the two channels */
    int32_t wet;           /* Q15, the mix */
    uint32_t delay_index, delay_length;
    int16_t delay[SURROUND_DELAY_FRAMES];
};
static struct surround_state playing, fading;
static struct rv_effect effect;
static struct rv_limiter limiter;

void surround_start(const struct rv_api *core)
{
    effect.api = core;
}

void surround_publish(const struct surround_set *set)
{
    rv_effect_publish(&effect, &next, set, sizeof next);
}

/* Back to the S16 scale, rounded, for the limiter. */
static inline int32_t output(int32_t sample)
{
    return (sample + (1 << (SAMPLE_SHIFT - 1))) >> SAMPLE_SHIFT;
}

/* Takes over the set the UI task left, fading from what played when the rate is
 * the same. A set left meanwhile moves the generation again, and the next call
 * takes that one; a set left while an on/off fade or a lead lasts waits for
 * its end, which surround_process checks. The delay line starts over when the
 * delay, Side Only or the rate changes, and the fade waits for the new line to
 * fill; the filters start over with it except for a new delay alone, and new
 * cutoffs, emphasis or mix continue everything. A rate change comes with a
 * track, so it ends a fade instead of starting one. */
static void take(uint32_t rate)
{
    struct surround_set was = playing.set;
    unsigned int was_on = playing.on, same_rate = rate == effect.rate;
    if (same_rate)
        fading = playing;
    rv_handoff_take(&effect.handoff, &playing.set, &next, sizeof playing.set);
    const struct surround_set *set = &playing.set;
    playing.on = set->on && set->mix != 0;
    playing.k_low = (int32_t)surround_coefficient(set->low_hz, rate);
    playing.k_high = (int32_t)surround_coefficient(set->high_hz, rate);
    playing.lead = UNITY - (int32_t)(UNITY * (uint32_t)set->emphasis / 200);
    playing.lag = UNITY + (int32_t)(UNITY * (uint32_t)set->emphasis / 200);
    playing.wet = (int32_t)(UNITY * (uint32_t)set->mix / 100);
    uint32_t length = set->delay_ms * rate / 1000;
    if (length > SURROUND_DELAY_FRAMES)
        length = SURROUND_DELAY_FRAMES;
    int new_input = !was_on || !same_rate || set->side_only != was.side_only;
    int restart = new_input || set->delay_ms != was.delay_ms;
    if (new_input)
        playing.low = playing.high = playing.previous = 0;
    if (restart) {
        memset(playing.delay, 0, sizeof playing.delay);
        playing.delay_index = 0;
        playing.delay_length = length;
    }
    /* Turning on, the limiter starts empty and the line fills while the old
     * state plays alone; a new line while on waits for the fill too. Turning
     * on with a new rate has no fade, so its first frames are silence. */
    int turns_on = playing.on && !was_on;
    if (turns_on)
        rv_limiter_rest(&limiter);
    rv_effect_taken(&effect, rate,
                    turns_on                ? length + RV_LIMITER_FRAMES
                    : restart && playing.on ? length
                                            : 0);
}

static int run(void *state, const int16_t *in, int32_t *out, uint32_t frames)
{
    struct surround_state *s = state;
    if (!s->on)
        return 0;
    const int32_t k_low = s->k_low, k_high = s->k_high, lead = s->lead, lag = s->lag,
                  wet = s->wet, side_only = s->set.side_only;
    int32_t low = s->low, high = s->high, previous = s->previous;
    int16_t *delay = s->delay, *di = delay + s->delay_index;
    const int16_t *end_of_line = delay + s->delay_length;
    for (const int16_t *end = in + frames * 2; in < end; in += 2, out += 2) {
        int32_t l = (int32_t)in[0] * (1 << SAMPLE_SHIFT);
        int32_t r = (int32_t)in[1] * (1 << SAMPLE_SHIFT);
        int32_t u = side_only ? (l - r) >> 1 : r;

        /* The band between the cutoffs stays; the rest goes down the line.
         * Each low-pass is the bilinear one: the pole from the coefficient,
         * the zero at Nyquist from the mean of this input and the last. */
        int32_t half = (u + previous) >> 1;
        previous = u;
        low += (int32_t)(((int64_t)k_low * (half - low)) >> 30);
        high += (int32_t)(((int64_t)k_high * (half - high)) >> 30);
        int32_t kept = high - low, band = u - kept;
        int32_t late = (int32_t)*di * (1 << SAMPLE_SHIFT);
        *di = rv_saturate16(output(band));
        if (++di >= end_of_line)
            di = delay;
        int32_t processed = kept + late;

        int32_t wet_l, wet_r;
        if (side_only) {
            /* The emphasis moves the side parts only; the mid stays put. */
            int32_t mid = (l + r) >> 1;
            wet_l = mid + (int32_t)(((int64_t)u * lead) >> 15);
            wet_r = mid - (int32_t)(((int64_t)processed * lag) >> 15);
        } else {
            wet_l = (int32_t)(((int64_t)l * lead) >> 15);
            wet_r = (int32_t)(((int64_t)processed * lag) >> 15);
        }
        out[0] = output(l + (int32_t)(((int64_t)(wet_l - l) * wet) >> 15));
        out[1] = output(r + (int32_t)(((int64_t)(wet_r - r) * wet) >> 15));
    }
    s->low = low;
    s->high = high;
    s->previous = previous;
    s->delay_index = (uint32_t)(di - delay);
    return 1;
}

/* A set waits while an on/off fade lasts and while a lead runs, so a fade
 * never carries over a line that is still filling. */
void surround_process(int16_t *samples, uint32_t frames, uint32_t rate)
{
    int lead = effect.fade.remaining > RV_FADE_FRAMES;
    if ((rv_effect_pending(&effect, rate) && !lead) || rate != effect.rate)
        take(rate);
    rv_fade_run_limited(&effect.fade, &limiter, run, &playing, &fading, samples,
                        frames);
}
