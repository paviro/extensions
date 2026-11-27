/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef SURROUND_LABELS_H
#define SURROUND_LABELS_H

#include "shared.h"

/* The editor's values in wheel order, which is reading order: the switch |
 * Delay | Mix, then Emphasis | Low | High | Side only. */
enum {
    PARAM_ON,
    PARAM_DELAY,
    PARAM_MIX,
    PARAM_EMPHASIS,
    PARAM_LOW,
    PARAM_HIGH,
    PARAM_SIDE_ONLY,
    PARAM_COUNT,
};

enum { LABEL_BYTES = 8 }; /* "7.8k Hz" */

/* Value `index` of `parameter` as the editor shows it: a constant, or
 * formatted into `out`. */
const char *surround_label(int parameter, int index, char out[LABEL_BYTES]);

#endif
