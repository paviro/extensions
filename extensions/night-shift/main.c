/* SPDX-License-Identifier: GPL-3.0-only */

/* Night Shift: a Settings row, Auto, On or Off, in the Appearance menu after
 * Dark Mode while that extension runs, after Brightness otherwise. While it is
 * on Auto a minute timer reads the schedule and the warmth follows it. */

#include "../shared/legacy.h"
#include "ext.h"
#include "night_shift.h"
#include "rv_api.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"
#include "schedule.h"
#include <stddef.h>

const struct rv_api *api;
rv_extension_handle self;

enum { MODE = 1 };

static const struct rv_setting settings[] = {
    RV_SETTING_CHOICE(MODE, "Night Shift", NIGHT_SHIFT_OFF, night_shift_labels),
};

static struct rv_timer *timer; /* the minute tick while on Auto */
static int zone_known;
static int first_load = 1;

static void tick(void *arg)
{
    (void)arg;
    struct schedule s;
    schedule_read(&s);
    night_shift_follow(&s);
    if (zone_known != (s.state != SCHEDULE_NO_ZONE)) {
        zone_known = !zone_known;
        api->ui_pane_refresh(self);
    }
}

/* The minute timer while on Auto. At load the extension runs inside Apple's
 * preferences load, before the browser holds the preferences object that
 * names the time zone: a read that finds no zone is tried again from the
 * event loop a second later, so the first screen already follows Auto. */
static void watch(int on)
{
    struct schedule s;
    if (on && !timer) {
        timer = api->timer_start(self, 60 * 1000, 1, tick, NULL);
        schedule_read(&s);
        zone_known = s.state != SCHEDULE_NO_ZONE;
        if (!zone_known)
            api->timer_start(self, 1000, 0, tick, NULL);
    }
    if (!on)
        rv_timer_cancel(api, self, &timer);
}

static void setting_changed(uint16_t key)
{
    if (key == RV_SETTING_KEY_ALL) {
        int import = first_load;
        first_load = 0;
        migrate_switch(api, self, import, MODE, 0x5448474e, 0x31544e53, NIGHT_SHIFT_ON,
                       NIGHT_SHIFT_OFF, RV_SETTING_TYPE_INT);
    } else if (key != MODE) {
        return;
    }
    int mode = api->setting_get_int(self, MODE);
    night_shift_set(mode);
    watch(mode == NIGHT_SHIFT_AUTO);
}

static const char *pane_caption(const struct rv_context *ctx)
{
    (void)ctx;
    return night_shift_caption();
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER(RV_SITE_DisplayPipe_Init_SetGammaTables_call,
                   night_shift_init_gamma),
};

/* Declared for its pane icon; the screen the core makes from the setting has none. */
static const struct rv_screen screens[] = {
    RV_SCREEN_CHOICE("night-shift", "Night Shift", MODE, night_shift_labels,
                     .bitmap = RV_BITMAP("settings-night-shift")),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(
        RV_MENU_SETTINGS, RV_SETTINGS_BRIGHTNESS, RV_ROW_SETTING("Night Shift", MODE),
        .pane = RV_PANE(.caption_text = pane_caption,
                        .bitmap = RV_BITMAP("settings-night-shift")),
        .host = "appearance", .host_screen = "appearance", .host_after = "dark-mode"),
};

/* The core stops the timer itself as the module unloads. */
static void unload(void)
{
    timer = NULL;
    night_shift_stop();
}

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_HOOKS(hooks),
    RV_SETTINGS(settings),
    .unload = unload,
    .setting_changed = setting_changed,
    RV_SCREENS(screens),
    RV_ENTRIES(entries),
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    api = core;
    self = handle;
    return &extension;
}
