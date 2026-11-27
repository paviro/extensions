/* SPDX-License-Identifier: GPL-3.0-only */

#include "schedule.h"
#include "osos-2.0.5.h"
#include "sun.h"
#include <stddef.h>

int schedule_minute(int value)
{
    value %= 1440;
    return value < 0 ? value + 1440 : value;
}

static int day_of_year(int day, int month, int year)
{
    static const uint16_t before[12] = {0,   31,  59,  90,  120, 151,
                                        181, 212, 243, 273, 304, 334};
    int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month < 1 || month > 12)
        return 1;
    return before[month - 1] + day + (leap && month > 2);
}

void schedule_read(struct schedule *out)
{
    uint8_t *browser = App ? *(uint8_t **)((uint8_t *)App + App_browser) : NULL;
    uint8_t *preferences =
        browser ? *(uint8_t **)(browser + MediaBrowser_preferences) : NULL;
    int place =
        preferences
            ? sun_place(*(uint32_t *)(preferences + PreferencesFile_time_zone_id))
            : -1;
    out->state = SCHEDULE_NO_ZONE;
    out->now = out->set = out->rise = 0;
    if (place < 0)
        return;
    uint8_t now[DateTime_bytes] __attribute__((aligned(4)));
    int16_t zone = 0;
    uint8_t dst = 0;
    RTC_ReadDateTime((struct DateTime *)now);
    RTC_GetZoneOffsets(&zone, &dst);
    int year = now[DateTime_year] | now[DateTime_year + 1] << 8;
    switch (sun_times(place, day_of_year(now[DateTime_day], now[DateTime_month], year),
                      &out->rise, &out->set)) {
    case SUN_NEVER_RISES:
        out->state = SCHEDULE_NEVER_RISES;
        return;
    case SUN_NEVER_SETS:
        out->state = SCHEDULE_NEVER_SETS;
        return;
    default:
        break;
    }
    /* The clock keeps local time: the zone and daylight saving are applied. */
    out->state = SCHEDULE_RISES;
    out->now = now[DateTime_hour] * 60 + now[DateTime_minute];
    out->set += zone + dst;
    out->rise += zone + dst;
}

int schedule_night(const struct schedule *s)
{
    switch (s->state) {
    case SCHEDULE_RISES:
        return schedule_minute(s->now - s->set) < schedule_minute(s->rise - s->set);
    case SCHEDULE_NEVER_RISES:
        return 1;
    default:
        return 0;
    }
}

const char *schedule_caption(void)
{
    struct schedule s;
    schedule_read(&s);
    return s.state == SCHEDULE_NO_ZONE ? "Set a Time Zone" : "Sunset to Sunrise";
}
