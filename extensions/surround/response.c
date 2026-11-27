/* SPDX-License-Identifier: GPL-3.0-only */

/* What the processing does to the right channel, or the side, for the
 * editor's plot: the one-pole low-passes of design.c in double precision
 * without a math library, then |H| along a log frequency axis. Only the editor
 * module carries this. */

#include "rv_dsp.h"
#include "shared.h"

/* The one-pole coefficient as design.c has it, from the same tangent. */
static double coefficient(unsigned int hz, unsigned int rate)
{
    double x = 3.141592653589793238 * hz / rate;
    if (x > 0.45 * 3.141592653589793238)
        x = 0.45 * 3.141592653589793238;
    double s, c;
    rv_sin_cos(x, &s, &c);
    double t = s / c;
    return 2.0 * t / (1.0 + t);
}

/* H = (k / 2) (1 + z^-1) / (1 - (1 - k) z^-1) at the angle with cosine c and
 * sine s, as its real and imaginary parts. */
static void one_pole(double k, double c, double s, double *re, double *im)
{
    double p = 1.0 - k;
    double nr = 1.0 + c, ni = -s;        /* 1 + e^-jw */
    double dr = 1.0 - p * c, di = p * s; /* 1 - p e^-jw */
    double d = dr * dr + di * di;
    *re = 0.5 * k * (nr * dr + ni * di) / d;
    *im = 0.5 * k * (ni * dr - nr * di) / d;
}

static int16_t to_tenths(double re, double im)
{
    double power = re * re + im * im;
    if (power < 1e-9)
        return SURROUND_NO_PATH;
    double db = rv_log(power) * 43.42944819032518;
    return (int16_t)(db + (db < 0.0 ? -0.5 : 0.5));
}

void surround_response(const struct surround_set *set, unsigned int rate, int16_t *kept,
                       int16_t *delayed, unsigned int points)
{
    /* 20 Hz to 20 kHz is three decades; each point is one step of that. */
    double ratio = rv_exp(6.907755278982137052 / (points - 1));
    if (!set->on || set->mix == 0) {
        for (unsigned int i = 0; i < points; i++) {
            kept[i] = 0;
            delayed[i] = SURROUND_NO_PATH;
        }
        return;
    }
    double k_low = coefficient(set->low_hz, rate),
           k_high = coefficient(set->high_hz, rate);
    double hz = 20.0;
    for (unsigned int i = 0; i < points; i++) {
        double s, c, low_re, low_im, high_re, high_im;
        rv_sin_cos(6.283185307179586477 * hz / rate, &s, &c);
        one_pole(k_low, c, s, &low_re, &low_im);
        one_pole(k_high, c, s, &high_re, &high_im);
        double kept_re = high_re - low_re, kept_im = high_im - low_im;
        kept[i] = to_tenths(kept_re, kept_im);
        delayed[i] = to_tenths(1.0 - kept_re, -kept_im);
        hz *= ratio;
    }
}
