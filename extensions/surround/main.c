/* SPDX-License-Identifier: GPL-3.0-only */

/* Surround: Settings > Surround opens the editor screen, which switches the
 * effect on and holds the delay, 5 to 30 ms, and the five values that shape
 * the stage. The output-stage hook runs what is chosen on every buffer. */

#include "rv_api.h"
#include "rv_audio.h"
#include "rv_libc.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"
#include "shared.h"

static const struct rv_api *api;
static rv_extension_handle self;

static const struct rv_setting settings[] = {
    RV_SETTING_INT(KEY_ON, "Surround", ON_DEFAULT, 0, ON_LABELS - 1),
    RV_SETTING_INT(KEY_DELAY, NULL, DELAY_DEFAULT, 0, DELAY_LABELS - 1),
    RV_SETTING_INT(KEY_EMPHASIS, NULL, EMPHASIS_DEFAULT, 0, EMPHASIS_LABELS - 1),
    RV_SETTING_INT(KEY_LOW, NULL, LOW_DEFAULT, 0, LOW_LABELS - 1),
    RV_SETTING_INT(KEY_HIGH, NULL, HIGH_DEFAULT, 0, HIGH_LABELS - 1),
    RV_SETTING_INT(KEY_SIDE_ONLY, NULL, SIDE_ONLY_DEFAULT, 0, SIDE_ONLY_LABELS - 1),
    RV_SETTING_INT(KEY_MIX, NULL, MIX_DEFAULT, 0, MIX_LABELS - 1),
};

/* Leaves what the settings say for the pump task. */
static void publish(void)
{
    struct surround_set set = {
        .on = (uint8_t)api->setting_get_int(self, KEY_ON),
        .delay_ms = (uint8_t)delay_ms(api->setting_get_int(self, KEY_DELAY)),
        .emphasis = (uint8_t)api->setting_get_int(self, KEY_EMPHASIS),
        .side_only = (uint8_t)api->setting_get_int(self, KEY_SIDE_ONLY),
        .mix = (uint8_t)mix_percent(api->setting_get_int(self, KEY_MIX)),
        .low_hz = (uint16_t)low_hz(api->setting_get_int(self, KEY_LOW)),
        .high_hz = (uint16_t)high_hz(api->setting_get_int(self, KEY_HIGH)),
    };
    surround_publish(&set);
}

static void setting_changed(uint16_t key)
{
    (void)key;
    publish();
}

/* The processed output before Apple submits it: (samples, frames, context).
 * Only 16-bit stereo at the rates the delay line is sized for is touched. */
static uint32_t on_output(uint32_t samples, uint32_t frames, uint32_t context,
                          const void *next)
{
    uint32_t rate;
    if (rv_audio_stereo16((const void *)context, frames, 8000, SURROUND_MAX_RATE,
                          &rate))
        surround_process((int16_t *)samples, frames, rate);
    return api->hook_next(next, samples, frames, context, 0, 0, 0);
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER_AT(RV_SITE_AudioOutputQueue_Get_call, on_output,
                      RV_AUDIO_STAGE_SURROUND),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_SETTINGS, RV_SETTINGS_SOUND_CHECK,
                  RV_ROW_PUSH("Surround", "editor"),
                  .pane = RV_PANE(.bitmap = RV_BITMAP("settings-surround")),
                  .caption_setting = KEY_ON, .host = "sound-menu",
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
    surround_start(core);
    return &extension;
}
