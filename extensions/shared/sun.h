/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef SUN_H
#define SUN_H
#include <stdint.h>

enum { SUN_RISES, SUN_NEVER_RISES, SUN_NEVER_SETS };

/* The index of Apple's time zone `zone_id` in TimeZones, or -1. */
int sun_place(uint32_t zone_id);
/* Sunrise and sunset at `place` on day 1..366 of the year, in minutes after
 * midnight UTC, either of which may fall outside 0..1439; the result says
 * whether the sun rises and sets that day at all. */
int sun_times(int place, int day_of_year, int *rise, int *set);

#endif
