/* SPDX-License-Identifier: GPL-3.0-only */

/* Bass Enhancement: Settings > Bass Enhancement opens the editor screen, which
 * holds the strength, Off or 25 to 100 %. The output-stage hook runs what is
 * chosen on every buffer. */

#include "rv_api.h"
#include "bank.h"
#include "rv_libc.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"
#include "shared.h"

static const struct rv_api *api;
static rv_extension_handle self;
static struct bank bank;
_Static_assert(BANK_STAGES == BASS_STAGES, "the Makefile's BANK_STAGES");

static const struct rv_setting settings[] = {
    RV_SETTING_LABELLED(KEY_STRENGTH, "Bass Enhancement", STRENGTH_DEFAULT,
                        strength_labels),
};

/* Designs the filters of the chosen strength for both banks and leaves them for
 * the pump task. */
static void publish(void)
{
    unsigned int strength = (unsigned int)api->setting_get_int(self, KEY_STRENGTH);
    struct bank_set set = {.on = strength != BASS_OFF};
    for (unsigned int b = 0; b < BANKS; b++) {
        unsigned int precut;
        if (!bass_design(set.coefficients[b], &precut, strength, bank_rate(b)))
            set.on = 0;
    }
    bank_publish(&bank, &set);
}

static void setting_changed(uint16_t key)
{
    (void)key;
    publish();
}

/* The processed output before Apple submits it: (samples, frames, context).
 * Only 16-bit stereo is touched. */
static uint32_t on_output(uint32_t samples, uint32_t frames, uint32_t context,
                          const void *next)
{
    uint32_t rate;
    if (rv_audio_stereo16((const void *)context, frames, 0, UINT32_MAX, &rate))
        bank_process(&bank, (int16_t *)samples, frames, rate);
    return api->hook_next(next, samples, frames, context, 0, 0, 0);
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER_AT(RV_SITE_AudioOutputQueue_Get_call, on_output,
                      RV_AUDIO_STAGE_BASS_ENHANCEMENT),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_SETTINGS, RV_SETTINGS_SOUND_CHECK,
                  RV_ROW_PUSH("Bass Enhancement", "editor"),
                  .pane = RV_PANE(.bitmap = RV_BITMAP("settings-bass-enhancement")),
                  .caption_setting = KEY_STRENGTH, .host = "sound-menu",
                  .host_screen = "sound", .host_after = "sound-check"),
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
    bank.effect.api = core;
    return &extension;
}
