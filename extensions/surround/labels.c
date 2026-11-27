/* SPDX-License-Identifier: GPL-3.0-only */

#include "labels.h"
#include "rv_libc.h"

const char *surround_label(int parameter, int index, char out[LABEL_BYTES])
{
    switch (parameter) {
    case PARAM_ON:
    case PARAM_SIDE_ONLY:
        return index ? "On" : "Off";
    case PARAM_DELAY:
        rv_format_int(out, LABEL_BYTES, (int)delay_ms(index), " ms");
        return out;
    case PARAM_MIX:
        rv_format_int(out, LABEL_BYTES, (int)mix_percent(index), " %");
        return out;
    case PARAM_EMPHASIS:
        rv_format_int(out, LABEL_BYTES, index, " %");
        return out;
    case PARAM_LOW:
        rv_format_hz(out, LABEL_BYTES, low_hz(index));
        return out;
    default:
        rv_format_hz(out, LABEL_BYTES, high_hz(index));
        return out;
    }
}
