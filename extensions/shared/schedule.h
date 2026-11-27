/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef SCHEDULE_H
#define SCHEDULE_H

/* The Auto schedule Dark Mode and Night Shift follow: sunset to sunrise in
 * the city of the time zone set under Settings > Date & Time, by the iPod's
 * clock. Reading it is a few integer multiplies; an extension reads it once a
 * minute while its row is on Auto. Compiled into each extension that uses it
 * with sun.c. */
enum {
    SCHEDULE_NO_ZONE,     /* the time zone is unset or not one of Apple's: day */
    SCHEDULE_RISES,       /* the sun rises and sets today */
    SCHEDULE_NEVER_RISES, /* polar night: night all day */
    SCHEDULE_NEVER_SETS   /* midnight sun: day all day */
};

struct schedule {
    int state;
    int now, set,
        rise; /* local minutes after midnight; set and rise may fall outside 0..1439 */
};

void schedule_read(struct schedule *out);
/* From the sunset minute up to the sunrise minute. */
int schedule_night(const struct schedule *s);
/* `value` folded into 0..1439. */
int schedule_minute(int value);
/* "Sunset to Sunrise", or "Set a time zone" without a known zone. */
const char *schedule_caption(void);

#endif
