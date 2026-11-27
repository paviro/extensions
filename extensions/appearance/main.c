/* SPDX-License-Identifier: GPL-3.0-only */

/* Appearance: a Settings row after Brightness opening a menu of Theme,
 * Accent, Dark Mode, Menu Covers and Now Playing Cover, whose rows other
 * extensions anchor theirs on. A look is a theme file's pack or the Classic
 * dark art from the `classic` asset streamed into the resident bitmaps, plus
 * the colour roles the draw hooks map the stock screens to; the core's
 * canvases follow through ui_set_colors. The settings hold the theme id, Dark
 * Mode and the accent; whatever cannot be put on the screen is written back
 * as what is. While Dark Mode is on Auto, a minute timer reads the schedule
 * and it follows.
 *
 * This module is always loaded: the bitmap catalog, colour roles, draw hooks
 * (bound only while the look is not stock light; the cover and menu cover
 * hooks always), the index of the look's extension bitmaps and the provider
 * that reads them when the core decodes one, the Theme list and the settings.
 * Reading a theme file, deriving art and restoring stock bitmaps are in the
 * switch module. */

#include "../shared/legacy.h"
#include "catalog.h"
#include "crc.h"
#include "dark_mode.h"
#include "ext.h"
#include "assets.h"
#include "flat_cover.h"
#include "latch.h"
#include "look.h"
#include "lz.h"
#include "menu_covers.h"
#include "rv_api.h"
#include "rv_libc.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"
#include "schedule.h"
#include "services.h"
#include "style.h"
#include "theme_picker.h"
#include "tint.h"
#include <stddef.h>

const struct rv_api *api;
rv_extension_handle self;

static const struct rv_setting settings[] = {
    RV_SETTING_CHOICE(KEY_DARK, "Dark Mode", DARK_OFF, dark_mode_labels),
    RV_SETTING_CHOICE(KEY_ACCENT, "Accent", ACCENT_DEFAULT, accent_labels),
    RV_SETTING_CHOICE(KEY_MENU_COVERS, "Menu Covers", COVERS_THEME, menu_covers_labels),
    RV_SETTING_CHOICE(KEY_NOW_PLAYING_COVER, "Now Playing Cover", NP_COVER_THEME,
                      now_playing_cover_labels),
    RV_SETTING_BOOL(KEY_AVAILABLE, "Appearance", 1),
};

static struct rv_timer *timer; /* the minute tick while Dark Mode is on Auto */
static int zone_known;

/* In Auto, the appearance the schedule asks for; a switch in progress, or
 * one whose latch is still closed, keeps the setting and waits a minute. */
static void dark_follow(const struct schedule *s)
{
    char id[RVT_ID_BYTES];
    int night = schedule_night(s);
    if (!dark_mode_auto() || look_theming_off() || look_busy() || latch_held() ||
        night == look_active()->dark)
        return;
    memset(id, 0, sizeof id);
    api->setting_read(self, KEY_THEME, id, sizeof id - 1);
    look_switch(id, night, (unsigned)api->setting_get_int(self, KEY_ACCENT), PANE_DARK);
}

static void tick(void *arg)
{
    (void)arg;
    struct schedule s;
    schedule_read(&s);
    dark_follow(&s);
    if (zone_known != (s.state != SCHEDULE_NO_ZONE)) {
        zone_known = !zone_known;
        api->ui_pane_refresh(self);
    }
}

/* The minute timer while Dark Mode is on Auto. At load the extension runs inside
 * Apple's preferences load, before the browser holds the preferences object
 * that names the time zone: a read that finds no zone is tried again from the
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

/* The stored theme in appearance `dark` with `accent` is what the screen shows. */
static int on_screen(const char *id, int dark, unsigned accent)
{
    const struct look *active = look_active();
    return !memcmp(id, active->id, RVT_ID_BYTES) && dark == active->dark &&
           accent == active->accent;
}

/* The core's former Dark Mode switch, store record DARK = DRK1, comes in once on
 * the first load as Dark Mode On; the write it makes is not a change of its own. */
static int first_load = 1, importing;

static void setting_changed(uint16_t key)
{
    char id[RVT_ID_BYTES];
    if (importing)
        return;
    if (key == RV_SETTING_KEY_ALL) {
        importing = 1;
        migrate_switch(api, self, first_load, KEY_DARK, 0x4b524144, 0x314b5244, DARK_ON,
                       DARK_OFF, RV_SETTING_TYPE_INT);
        importing = 0;
        first_load = 0;
    }
    if (key == RV_SETTING_KEY_ALL || key == KEY_DARK)
        dark_mode_set(api->setting_get_int(self, KEY_DARK));
    watch(dark_mode_auto());
    if (look_busy())
        return;
    int dark = dark_mode_wanted();
    unsigned accent = (unsigned)api->setting_get_int(self, KEY_ACCENT);
    switch (key) {
    case RV_SETTING_KEY_ALL:
        /* At load, and after Reset All Settings emptied the store. */
        if (!api->setting_get_int(self, KEY_AVAILABLE) && !look_theming_off())
            api->setting_set_int(self, KEY_AVAILABLE, 1);
        menu_covers_set(api->setting_get_int(self, KEY_MENU_COVERS));
        flat_cover_set(api->setting_get_int(self, KEY_NOW_PLAYING_COVER));
        memset(id, 0, sizeof id);
        api->setting_read(self, KEY_THEME, id, sizeof id - 1);
        if (!on_screen(id, dark, accent))
            look_switch(id, dark, accent, PANE_THEME);
        break;
    case KEY_DARK:
        /* The stored theme, not the active one: while Classic stands in for a
         * theme whose file was missing, the ids differ and the theme is tried again. */
        memset(id, 0, sizeof id);
        api->setting_read(self, KEY_THEME, id, sizeof id - 1);
        if (!on_screen(id, dark, accent))
            look_switch(id, dark, accent, PANE_DARK);
        break;
    case KEY_ACCENT:
        look_accent(accent);
        break;
    case KEY_MENU_COVERS:
        menu_covers_set(api->setting_get_int(self, KEY_MENU_COVERS));
        break;
    case KEY_NOW_PLAYING_COVER:
        flat_cover_set(api->setting_get_int(self, KEY_NOW_PLAYING_COVER));
        break;
    default:
        break;
    }
}

static const char *theme_caption(const struct rv_context *ctx)
{
    (void)ctx;
    const struct look *active = look_active();
    const char *problem = look_problem(PANE_THEME);
    return problem ? problem : active->id[0] ? active->name : "Classic";
}

static const char *dark_pane(const struct rv_context *ctx)
{
    (void)ctx;
    return dark_mode_caption();
}

/* The row says Theme; the pane has room for the full phrase. */
static const char *accent_caption(const struct rv_context *ctx)
{
    (void)ctx;
    const char *problem = look_problem(PANE_ACCENT);
    if (problem)
        return problem;
    return style_accent() == ACCENT_DEFAULT ? "Theme Default"
                                            : accent_labels[style_accent()];
}

static const char *covers_caption(const struct rv_context *ctx)
{
    (void)ctx;
    return menu_covers_enabled() ? "On" : "Off";
}

static const char *cover_caption(const struct rv_context *ctx)
{
    (void)ctx;
    return flat_cover_flat() ? "Flat" : "Tilted";
}

/* The Appearance menu: each row with its own pane caption and icon, and an id
 * other extensions anchor their rows on. */
static const struct rv_row rows[] = {
    RV_ROW_PUSH("Theme", "theme", .id = "theme"),
    RV_ROW_SETTING("Accent", KEY_ACCENT, .id = "accent"),
    RV_ROW_SETTING("Dark Mode", KEY_DARK, .id = "dark-mode"),
    RV_ROW_SETTING("Menu Covers", KEY_MENU_COVERS, .id = "menu-covers"),
    RV_ROW_SETTING("Now Playing Cover", KEY_NOW_PLAYING_COVER,
                   .id = "now-playing-cover"),
};
static const struct rv_pane panes[] = {
    {.caption_text = theme_caption, .bitmap = RV_BITMAP("settings-theme")},
    {.caption_text = accent_caption, .bitmap = RV_BITMAP("settings-accent")},
    {.caption_text = dark_pane, .bitmap = RV_BITMAP("settings-moon")},
    {.caption_text = covers_caption, .bitmap = RV_BITMAP("settings-covers")},
    {.caption_text = cover_caption, .bitmap = RV_BITMAP("settings-cover")},
};
/* The choice screens the rows open, declared for their pane icons; the
 * screens the core makes from the settings have none. */
static const struct rv_screen screens[] = {
    RV_SCREEN_LIST_PANES("appearance", "Appearance", rows, panes),
    THEME_PICKER_SCREEN,
    RV_SCREEN_CHOICE("accent", "Accent", KEY_ACCENT, accent_labels,
                     .bitmap = RV_BITMAP("settings-accent")),
    RV_SCREEN_CHOICE("dark-mode", "Dark Mode", KEY_DARK, dark_mode_labels,
                     .bitmap = RV_BITMAP("settings-moon")),
    RV_SCREEN_CHOICE("menu-covers", "Menu Covers", KEY_MENU_COVERS, menu_covers_labels,
                     .bitmap = RV_BITMAP("settings-covers")),
    RV_SCREEN_CHOICE("now-playing-cover", "Now Playing Cover", KEY_NOW_PLAYING_COVER,
                     now_playing_cover_labels, .bitmap = RV_BITMAP("settings-cover")),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_SETTINGS, RV_SETTINGS_BRIGHTNESS,
                  RV_ROW_PUSH("Appearance", "appearance"),
                  .pane = RV_PANE(.caption = "Make It Your Own",
                                  .bitmap = RV_BITMAP("settings-appearance")),
                  .shown_by = KEY_AVAILABLE),
};

/* No arguments: disk mode has ended and the volume, firmware file included,
 * is readable again. */
static uint32_t disk_mode_exit(const void *next)
{
    look_volume_changed();
    return api->hook_next(next, 0, 0, 0, 0, 0, 0);
}

/* The hooks bound in every look, the Menu Covers and Now Playing Cover hooks
 * among them; the draw hooks are bound while a look is on. */
static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER(RV_SITE_MediaArtworkProvider_GetSlotArtwork_entry,
                   menu_covers_query),
    RV_HOOK_FILTER(RV_SITE_GLAlbumArtView_Draw_slot, flat_cover_view_draw),
    RV_HOOK_FILTER(RV_SITE_AlbumArt_RendererDraw_call, flat_cover_renderer_draw),
    RV_HOOK_FILTER(RV_SITE_AlbumArt_EdgeLines_call, flat_cover_edge_lines),
    RV_HOOK_FILTER(RV_SITE_DiskModeExit_PopToMainScreen_call, disk_mode_exit),
};

/* The core stops the timer itself as the module unloads. */
static void unload(void)
{
    timer = NULL;
    look_unload();
}

static const struct main_service service = {
    .state = &look_state,
    .crc32_update = crc32_update,
    .catalog_count = catalog_count,
    .catalog_fingerprint = catalog_fingerprint,
    .catalog_data = catalog_data,
    .catalog_size = catalog_size,
    .catalog_crc = catalog_crc,
    .catalog_file_offset = catalog_file_offset,
    .catalog_layout = catalog_layout,
    .style_set = style_set,
    .style_reset = style_reset,
    .style_set_dark = style_set_dark,
    .style_set_accent = style_set_accent,
    .tint_refresh = tint_refresh,
    .tint_accent_art = tint_accent_art,
    .lz_decode = lz_decode,
    .assets_reserve = assets_reserve,
    .assets_commit = assets_commit,
    .assets_drop = assets_drop,
};

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_HOOKS(hooks),
    RV_SETTINGS(settings),
    .unload = unload,
    .setting_changed = setting_changed,
    RV_SCREENS(screens),
    RV_ENTRIES(entries),
    .service = &service,
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    api = core;
    self = handle;
    return &extension;
}
