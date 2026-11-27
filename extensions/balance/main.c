/* SPDX-License-Identifier: GPL-3.0-only */

/* Balance: Settings > Balance opens the editor screen, which holds the
 * position, from L only through Center to R only. The output-stage hook runs
 * what is chosen on every buffer. */

#include "rv_api.h"
#include "rv_audio.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"
#include "shared.h"

static const struct rv_api *api;
static rv_extension_handle self;

static const struct rv_setting settings[] = {
    RV_SETTING_LABELLED(KEY_BALANCE, "Balance", BALANCE_CENTRE, balance_labels),
};

static void setting_changed(uint16_t key)
{
    (void)key;
    balance_publish((unsigned int)api->setting_get_int(self, KEY_BALANCE));
}

/* The processed output before Apple submits it: (samples, frames, context).
 * Only 16-bit stereo is touched. */
static uint32_t on_output(uint32_t samples, uint32_t frames, uint32_t context,
                          const void *next)
{
    uint32_t rate;
    if (rv_audio_stereo16((const void *)context, frames, 0, UINT32_MAX, &rate))
        balance_process((int16_t *)samples, frames);
    return api->hook_next(next, samples, frames, context, 0, 0, 0);
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER_AT(RV_SITE_AudioOutputQueue_Get_call, on_output,
                      RV_AUDIO_STAGE_BALANCE),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_SETTINGS, RV_SETTINGS_VOLUME_LIMIT,
                  RV_ROW_PUSH("Balance", "editor"),
                  .pane = RV_PANE(.bitmap = RV_BITMAP("settings-balance")),
                  .caption_setting = KEY_BALANCE, .host = "sound-menu",
                  .host_screen = "sound", .host_after = "volume-limit"),
};

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_HOOKS(hooks),
    RV_SETTINGS(settings),
    .setting_changed = setting_changed,
    RV_ENTRIES(entries),
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    api = core;
    self = handle;
    balance_start(core);
    return &extension;
}
