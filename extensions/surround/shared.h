/* SPDX-License-Identifier: GPL-3.0-only */

/* What the two modules of Surround agree on. */

#ifndef SURROUND_SHARED_H
#define SURROUND_SHARED_H

#include "surround.h"

enum {
    KEY_ON = 1, /* int: 0 Off, 1 On */
    KEY_DELAY,  /* int: indexes into the tables below */
    KEY_EMPHASIS,
    KEY_LOW,
    KEY_HIGH,
    KEY_SIDE_ONLY,
    KEY_MIX,
};

/* Each value is an index; these turn it into the engine's units. */
enum {
    ON_LABELS = 2,
    DELAY_LABELS = 5, /* 5, 8, 10, 15, 30 ms */
    EMPHASIS_LABELS = 100,
    LOW_LABELS = 10,  /* 40 to 400 Hz */
    HIGH_LABELS = 38, /* 600 to 8000 Hz */
    SIDE_ONLY_LABELS = 2,
    MIX_LABELS = 21, /* 0 to 100 % */
    ON_DEFAULT = 0,
    DELAY_DEFAULT = 2, /* 10 ms */
    EMPHASIS_DEFAULT = 35,
    LOW_DEFAULT = 7,   /* 320 Hz */
    HIGH_DEFAULT = 14, /* 3400 Hz */
    SIDE_ONLY_DEFAULT = 0,
    MIX_DEFAULT = 10, /* 50 % */
};

static inline unsigned int delay_ms(int index)
{
    static const uint8_t milliseconds[DELAY_LABELS] = {5, 8, 10, 15, 30};
    return milliseconds[index];
}

static inline unsigned int low_hz(int index)
{
    return 40 + 40 * (unsigned int)index;
}

static inline unsigned int high_hz(int index)
{
    return 600 + 200 * (unsigned int)index;
}

static inline unsigned int mix_percent(int index)
{
    return 5 * (unsigned int)index;
}

/* response.c: what the processing does to the right channel, or the side, at
 * `rate`, as gains at `points` frequencies from 20 Hz to 20 kHz evenly spaced
 * in octaves, in tenths of a decibel: `kept` is the band that stays in place,
 * `delayed` the rest, which arrives late. Off leaves `kept` at 0 and fills
 * `delayed` with SURROUND_NO_PATH. */
enum { SURROUND_NO_PATH = INT16_MIN };

void surround_response(const struct surround_set *set, unsigned int rate, int16_t *kept,
                       int16_t *delayed, unsigned int points);

#endif
