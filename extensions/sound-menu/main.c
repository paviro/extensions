/* SPDX-License-Identifier: GPL-3.0-only */

/* Sound Menu: Settings > Sound, a list that takes Apple's Volume Limit, EQ
 * and Sound Check rows out of Settings and hosts the audio rows other
 * extensions anchor on them. */

#include "rv_api.h"
#include "rv_menus.h"
#include "rv_ui.h"

static const struct rv_row rows[] = {
    RV_ROW_STOCK(RV_SETTINGS_VOLUME_LIMIT, .id = "volume-limit"),
    RV_ROW_STOCK(RV_SETTINGS_EQ, .id = "eq"),
    RV_ROW_STOCK(RV_SETTINGS_SOUND_CHECK, .id = "sound-check"),
};
/* A pane per row, so hosted rows show theirs; the stock rows bring Apple's. */
static const struct rv_pane panes[RV_COUNT(rows)];

static const struct rv_screen screens[] = {
    RV_SCREEN_LIST_PANES("sound", "Sound", rows, panes),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_SETTINGS, RV_SETTINGS_MUSIC_MENU,
                  RV_ROW_PUSH("Sound", "sound"),
                  .pane = RV_PANE(.caption = "Shape How Your Media Sounds",
                                  .bitmap = RV_BITMAP("settings-sound"))),
};

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_SCREENS(screens),
    RV_ENTRIES(entries),
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    (void)core;
    (void)handle;
    return &extension;
}
