/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "osos-2.0.5.h"
#include "ext.h"
static uint32_t gamma_loaded[3][DisplayGamma_Entries];
static const uint32_t *gamma_from;
static int gamma_loads;

int DisplayPipe_SetGammaTables(const uint32_t *red, const uint32_t *green,
                               const uint32_t *blue)
{
    memcpy(gamma_loaded[0], red, sizeof gamma_loaded[0]);
    memcpy(gamma_loaded[1], green, sizeof gamma_loaded[1]);
    memcpy(gamma_loaded[2], blue, sizeof gamma_loaded[2]);
    gamma_from = red;
    gamma_loads++;
    return 0;
}

static uint8_t clock_now[DateTime_bytes];
static int16_t clock_zone;
static uint8_t clock_dst;

int RTC_ReadDateTime(struct DateTime *out)
{
    memcpy(out, clock_now, sizeof clock_now);
    return 1;
}

void RTC_GetZoneOffsets(int16_t *zone_minutes, uint8_t *dst_minutes)
{
    *zone_minutes = clock_zone;
    *dst_minutes = clock_dst;
}

static uint8_t host_zones[TimeZone_Count * TimeZone_bytes];
#define TimeZones host_zones
static uint8_t host_preferences[PreferencesFile_time_zone_id + 4];
static uint8_t host_browser[MediaBrowser_preferences + sizeof(void *)];
static uint8_t host_app[App_browser + sizeof(void *)];
void *App = host_app;
#include "../shared/sun.c"
#include "../shared/schedule.c"
#include "night_shift.c"

static int allocations;
static uint32_t passed[3];

enum { next_result = 3 };

static void *allocate(rv_extension_handle ext, uint32_t bytes)
{
    (void)ext;
    allocations++;
    return calloc(1, bytes);
}

static void release(rv_extension_handle ext, void *ptr)
{
    (void)ext;
    allocations--;
    free(ptr);
}

static uint32_t next(const void *ctx, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                     uint32_t e, uint32_t f)
{
    (void)ctx;
    (void)d;
    (void)e;
    (void)f;
    passed[0] = a;
    passed[1] = b;
    passed[2] = c;
    return next_result;
}

static void put16(uint8_t *dest, uint16_t value)
{
    memcpy(dest, &value, sizeof value);
}
static const struct rv_api mock = {
    .alloc = allocate, .free = release, .hook_next = next};
const struct rv_api *api = &mock;
rv_extension_handle self;

/* Sunrise and sunset against NOAA's formula in double precision, in UTC. */
static void sun_contracts(void)
{
    static const struct {
        int place, day, rise, set;
    } cases[] = {
        {90, 172, 163, 1173}, /* Berlin, June */
        {90, 355, 435, 894},  /* Berlin, December */
        {198, 1, -313, 549},  /* Sydney, January */
        {4, 80, 852, 1580},   /* Cupertino, March */
        {84, 355, 682, 929},  /* Reykjavik, December */
        {63, 172, 293, 1564}, /* Nuuk, June */
        {47, 200, 677, 1403}, /* Quito, July */
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int rise, set;
        assert(sun_times(cases[i].place, cases[i].day, &rise, &set) == SUN_RISES);
        assert(abs(rise - cases[i].rise) <= 3 && abs(set - cases[i].set) <= 3);
    }
}

static void night_shift_contracts(void)
{
    /* A night from 21:00 to 06:00. */
    assert(night_shift_ramp(20 * 60, 21 * 60, 6 * 60) == 0);
    assert(night_shift_ramp(21 * 60, 21 * 60, 6 * 60) == 0);
    assert(night_shift_ramp(21 * 60 + 30, 21 * 60, 6 * 60) == 128);
    assert(night_shift_ramp(22 * 60, 21 * 60, 6 * 60) == 256);
    assert(night_shift_ramp(2 * 60, 21 * 60, 6 * 60) == 256);
    assert(night_shift_ramp(5 * 60 + 30, 21 * 60, 6 * 60) == 128);
    assert(night_shift_ramp(6 * 60, 21 * 60, 6 * 60) == 0);
    assert(night_shift_ramp(12 * 60, 21 * 60, 6 * 60) == 0);
    /* Times past midnight either way, as sun_times gives them. */
    assert(night_shift_ramp(30, 1440 + 22 * 60, -1440 + 7 * 60) == 256);
    /* A night shorter than both ramps peaks halfway. */
    assert(night_shift_ramp(23 * 60 + 30, 23 * 60, 24 * 60) == 128);

    /* On loads the warm tables; Off loads the identity ramp and frees them. */
    int out = allocations, loads = gamma_loads;
    night_shift_set(NIGHT_SHIFT_ON);
    assert(!strcmp(night_shift_caption(), "Warm colors"));
    assert(gamma_loads == loads + 1 && allocations == out + 1);
    assert(gamma_loaded[0][255] == 1020 && gamma_loaded[1][255] == (1020 * 216) >> 8 &&
           gamma_loaded[2][255] == (1020 * 150) >> 8);
    /* Display init after a wake gets the same tables. */
    assert(night_shift_init_gamma(1, 1, 1, NULL) == next_result);
    assert(passed[0] == (uint32_t)gamma_from &&
           passed[2] == (uint32_t)(gamma_from + 2 * DisplayGamma_Entries));
    night_shift_set(NIGHT_SHIFT_ON);
    assert(gamma_loads == loads + 1);
    night_shift_set(NIGHT_SHIFT_OFF);
    assert(!strcmp(night_shift_caption(), "Standard colors"));
    assert(gamma_loads == loads + 2 && allocations == out);
    assert(gamma_loaded[1][255] == 1020 && gamma_loaded[2][128] == 512);
    assert(night_shift_init_gamma(1, 2, 3, NULL) == next_result && passed[2] == 3);

    /* Auto without a known zone stays neutral and says so. */
    night_shift_set(NIGHT_SHIFT_AUTO);
    assert(gamma_loads == loads + 2);
    assert(!strcmp(night_shift_caption(), "Set a time zone"));
    /* Berlin, 21 June, 23:00 CEST: an hour and a half after sunset. */
    put16(host_zones + 90 * TimeZone_bytes + TimeZone_zone_id, 0x1234);
    memcpy(host_app + App_browser, &(uint8_t *){host_browser}, sizeof(void *));
    memcpy(host_browser + MediaBrowser_preferences, &(uint8_t *){host_preferences},
           sizeof(void *));
    put16(host_preferences + PreferencesFile_time_zone_id, 0x1234);
    clock_now[DateTime_hour] = 23, clock_now[DateTime_minute] = 0;
    clock_now[DateTime_day] = 21, clock_now[DateTime_month] = 6;
    put16(clock_now + DateTime_year, 2026);
    clock_zone = 60, clock_dst = 60;
    night_shift_set(NIGHT_SHIFT_AUTO);
    assert(strength == 256 && gamma_loads == loads + 3);
    assert(!strcmp(night_shift_caption(), "Sunset to Sunrise"));
    /* 13:00 the same day: neutral. */
    struct schedule s;
    clock_now[DateTime_hour] = 13;
    schedule_read(&s);
    night_shift_follow(&s);
    assert(strength == 0 && allocations == out);
    /* Off ignores the schedule. */
    night_shift_set(NIGHT_SHIFT_OFF);
    clock_now[DateTime_hour] = 23;
    schedule_read(&s);
    night_shift_follow(&s);
    assert(strength == 0);
}

/* The schedule: night from the sunset minute to the sunrise minute, by the
 * clock in the time zone's city; the polar states; no zone is day. The Berlin
 * zone and clock are as night_shift_contracts left them. */
static void schedule_contracts(void)
{
    struct schedule s;
    /* Berlin, 21 June: sunset 21:33 CEST, sunrise 04:43 (NOAA, within 3 minutes). */
    clock_now[DateTime_hour] = 21, clock_now[DateTime_minute] = 0;
    schedule_read(&s);
    assert(s.state == SCHEDULE_RISES && s.now == 21 * 60 && !schedule_night(&s));
    assert(abs(s.set - (21 * 60 + 33)) <= 3 && abs(s.rise - (4 * 60 + 43)) <= 3);
    clock_now[DateTime_hour] = (uint8_t)(s.set / 60),
    clock_now[DateTime_minute] = (uint8_t)(s.set % 60);
    schedule_read(&s);
    assert(schedule_night(&s)); /* the sunset minute itself */
    clock_now[DateTime_minute]--;
    schedule_read(&s);
    assert(!schedule_night(&s));
    clock_now[DateTime_hour] = (uint8_t)(s.rise / 60),
    clock_now[DateTime_minute] = (uint8_t)(s.rise % 60);
    schedule_read(&s);
    assert(!schedule_night(&s)); /* the sunrise minute is day */
    clock_now[DateTime_minute]--;
    schedule_read(&s);
    assert(schedule_night(&s));
    assert(!strcmp(schedule_caption(), "Sunset to Sunrise"));
    /* No city in Apple's zones lies beyond a polar circle, so the polar states
     * are checked on their own. */
    s.state = SCHEDULE_NEVER_RISES;
    assert(schedule_night(&s));
    s.state = SCHEDULE_NEVER_SETS;
    assert(!schedule_night(&s));
    /* No such zone: day, and the caption asks for one. */
    put16(host_preferences + PreferencesFile_time_zone_id, 0x7777);
    schedule_read(&s);
    assert(s.state == SCHEDULE_NO_ZONE && !schedule_night(&s));
    assert(!strcmp(schedule_caption(), "Set a time zone"));
    put16(host_preferences + PreferencesFile_time_zone_id, 0x1234);
    clock_now[DateTime_hour] = 23, clock_now[DateTime_minute] = 0;
}

int main(void)
{
    sun_contracts();
    night_shift_contracts();
    schedule_contracts();
    return 0;
}
