/* SPDX-License-Identifier: GPL-3.0-only */

/* Based on RepriseOS payload/sleep_timer.c (91d94ce) by kayboll.
 *
 * Sleep Timer in the Now Playing context menu: Apple's sleep timer choices as
 * a full-width list. Choosing one sets the stock timer, the one Extras >
 * Alarms > Sleep Timer sets, and returns to Now Playing. While the timer runs,
 * Now Playing shows the minutes left and a moon before the shuffle icon. */

#include "indicator.h"
#include "osos-2.0.5.h"
#include "rv_api.h"
#include "rv_libc.h"
#include "rv_sites.h"
#include "rv_ui.h"

#define POLL_MS 2000 /* the App counts the minutes on its own timer */

static const struct rv_api *api;
static rv_extension_handle self;
static struct rv_timer *poll;
static uint32_t shown; /* minutes on the image Now Playing has, 0 for none */
static int stopping;
static char label[16];

static void *service(void)
{
    return AlarmsService_Get();
}

static uint32_t active_choice(void)
{
    return RV_FIELD(service(), AlarmsService_sleep_timer_choice, uint32_t);
}

static uint32_t remaining(void)
{
    return RV_FIELD(App, App_sleep_timer_remaining, uint32_t);
}

/* Now Playing asks for the shuffle image again. */
static void refresh(void)
{
    Model_Notify(NowPlayingModel_Get(), Property_Bitmap, Property_ShuffleImage);
}

static void tick(void *arg)
{
    (void)arg;
    uint32_t want = active_choice() ? remaining() : 0;
    if (want != shown)
        refresh();
    if (!active_choice())
        rv_timer_cancel(api, self, &poll);
}

static uint32_t property(uintptr_t model, uint32_t type, uint32_t which, uintptr_t out,
                         const void *next)
{
    uint32_t handled = api->hook_next(next, model, type, which, out, 0, 0);
    if (type != Property_Bitmap || which != Property_ShuffleImage)
        return handled;
    uintptr_t *image = (uintptr_t *)out;
    if (stopping || !active_choice()) {
        /* Now Playing takes this answer in place of ours. */
        indicator_free(api, self);
        shown = 0;
        return handled;
    }
    const void *bitmap =
        indicator_build(api, self, remaining(), handled ? (void *)*image : NULL);
    if (!bitmap)
        return handled;
    *image = (uintptr_t)bitmap;
    shown = remaining();
    if (!poll)
        poll = api->timer_start(self, POLL_MS, 1, tick, NULL);
    return 1;
}

static uint32_t count(const struct rv_context *ctx)
{
    (void)ctx;
    return RV_FIELD(service(), AlarmsService_sleep_timer_choice_count, uint32_t);
}

static const char *row(const struct rv_context *ctx, uint32_t index,
                       const char **detail)
{
    (void)ctx;
    *detail = NULL;
    const uint8_t *choices =
        RV_FIELD(service(), AlarmsService_sleep_timer_choices, const uint8_t *);
    uint32_t minutes = RV_FIELD(choices + index * SleepTimerChoice_bytes,
                                SleepTimerChoice_minutes, uint32_t);
    if (!minutes)
        return "Off";
    rv_format(label, sizeof label, "%lu Minutes", minutes);
    return label;
}

static int choose(const struct rv_context *ctx, uint32_t index)
{
    (void)ctx;
    AlarmsService_SelectSleepTimer(service(), index);
    refresh();
    return 1;
}

static uint32_t active(const struct rv_context *ctx)
{
    (void)ctx;
    return active_choice();
}

static void unload(void)
{
    stopping = 1;
    refresh();
    indicator_free(api, self);
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER(RV_SITE_NowPlayingModel_Property_slot, property),
};

static const struct rv_screen screens[] = {
    RV_SCREEN_DYNAMIC_LIST("timer", "Sleep Timer", count, row, .dynamic.select = choose,
                           .layout = RV_LAYOUT_FULL, .dynamic.initial = active),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_NOW_PLAYING_CONTEXT, 0, RV_ROW_PUSH("Sleep Timer", "timer")),
};

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_HOOKS(hooks),
    RV_SCREENS(screens),
    RV_ENTRIES(entries),
    .unload = unload,
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    api = core;
    self = handle;
    return &extension;
}
