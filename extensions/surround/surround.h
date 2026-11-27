/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef SURROUND_H
#define SURROUND_H

#include <stdint.h>

struct rv_api;

/* What the settings say, in the units the engine uses: whether it plays, the
 * delay in milliseconds, the emphasis in percent, the two cutoffs in hertz,
 * side only and the wet share of the mix in percent. */
struct surround_set {
    uint8_t on;
    uint8_t delay_ms;
    uint8_t emphasis;
    uint8_t side_only;
    uint8_t mix;
    uint16_t low_hz, high_hz;
};

enum {
    SURROUND_MAX_DELAY_MS = 30,
    SURROUND_MAX_RATE = 48000,
    /* The delay line, one channel, at the longest delay and the highest rate. */
    SURROUND_DELAY_FRAMES = SURROUND_MAX_DELAY_MS * SURROUND_MAX_RATE / 1000,
};

/* design.c: the coefficient of the bilinear one-pole low-pass for a cutoff at
 * `rate` as Q30, 2 t / (1 + t) with t = tan(pi hz / rate), the cutoff held
 * below 0.45 of the rate; integer throughout, for the pump task. */
uint32_t surround_coefficient(unsigned int hz, unsigned int rate);

/* surround.c: `publish` leaves a set for the pump task from any task;
 * `process` runs the set that plays on `frames` interleaved stereo S16 frames in
 * place, on the pump task only. */
void surround_start(const struct rv_api *core);
void surround_publish(const struct surround_set *set);
void surround_process(int16_t *samples, uint32_t frames, uint32_t rate);

#endif
