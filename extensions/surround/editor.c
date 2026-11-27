/* SPDX-License-Identifier: GPL-3.0-only */

/* The Surround screen: what happens to the right channel, as two curves over
 * the switch, the delay and the five values that shape the stage. One curve is
 * the band that stays in place, between Low and High, the other what arrives
 * late. The wheel picks a value, Select edits it, holding Select puts it back
 * to its default and Menu steps back out. Every change plays at once. */

#include "labels.h"
#include "rv_canvas.h"
#include "rv_libc.h"
#include "rv_ui.h"
#include "shared.h"

static const struct rv_api *api;
static rv_extension_handle self;

enum { DEPTH_PARAMETER, DEPTH_EDIT, DEPTHS };

static const char *const titles[PARAM_COUNT] = {
    "State", "Delay", "Mix", "Emphasis", "Low", "High", "Side only"};
static const uint8_t keys[PARAM_COUNT] = {
    KEY_ON, KEY_DELAY, KEY_MIX, KEY_EMPHASIS, KEY_LOW, KEY_HIGH, KEY_SIDE_ONLY};
static const uint8_t counts[PARAM_COUNT] = {ON_LABELS,       DELAY_LABELS, MIX_LABELS,
                                            EMPHASIS_LABELS, LOW_LABELS,   HIGH_LABELS,
                                            SIDE_ONLY_LABELS};
static const uint8_t defaults[PARAM_COUNT] = {
    ON_DEFAULT,  DELAY_DEFAULT, MIX_DEFAULT,      EMPHASIS_DEFAULT,
    LOW_DEFAULT, HIGH_DEFAULT,  SIDE_ONLY_DEFAULT};

enum {
    PLOT_POINTS = 261,
    TICK_TOP = 114,
    TICK_HEIGHT = 22,
    INLINE_TOP = 141, /* State | Delay | Mix, title left and value right */
    INLINE_HEIGHT = 27,
    INLINE_COUNT = PARAM_EMPHASIS,
    CELL_TOP = INLINE_TOP + INLINE_HEIGHT +
        1, /* the cells run to the bottom of the canvas */
    CELL_COUNT = PARAM_COUNT - INLINE_COUNT,
};

/* 0 dB at the top, -24 dB at the bottom: 3 px per decibel. */
static const struct rv_plot plot = {
    .left = 44,
    .top = 36,
    .bottom = 108,
    .points = PLOT_POINTS,
    .bottom_value = -240,
};

static struct {
    struct rv_editor input;
    int parameter;
    int16_t kept[PLOT_POINTS], delayed[PLOT_POINTS];
} editor;

static int value(int parameter)
{
    return rv_clamp(api->setting_get_int(self, keys[parameter]), 0,
                    counts[parameter] - 1);
}

static void compute_curves(void)
{
    struct surround_set set = {
        .on = (uint8_t)value(PARAM_ON),
        .delay_ms = (uint8_t)delay_ms(value(PARAM_DELAY)),
        .emphasis = (uint8_t)value(PARAM_EMPHASIS),
        .side_only = (uint8_t)value(PARAM_SIDE_ONLY),
        .mix = (uint8_t)mix_percent(value(PARAM_MIX)),
        .low_hz = (uint16_t)low_hz(value(PARAM_LOW)),
        .high_hz = (uint16_t)high_hz(value(PARAM_HIGH)),
    };
    /* Drawn for 44.1 kHz, the rate nearly everything plays at; the crossovers
     * barely move at the others. */
    surround_response(&set, 44100, editor.kept, editor.delayed, PLOT_POINTS);
}

/* The curve the highlighted value shapes draws in the accent, over the other:
 * the band kept in place for Low and High, what arrives late otherwise. Off
 * has no late path, so that curve and its swatch stay inactive. */
static void draw_plot(const struct rv_canvas *c)
{
    static const char *const ticks[] = {"20", "200", "2k", "20k Hz"};
    static const char *const scale[] = {"0", "-12", "-24"};
    int off = editor.delayed[0] == SURROUND_NO_PATH;
    int kept_side =
        off || editor.parameter == PARAM_LOW || editor.parameter == PARAM_HIGH;
    uint8_t kept_color = kept_side ? RV_CANVAS_ACCENT : RV_CANVAS_INACTIVE;
    uint8_t late_color = kept_side ? RV_CANVAS_INACTIVE : RV_CANVAS_ACCENT;
    const struct rv_canvas_legend legend[] = {{"In place", kept_color},
                                              {"Arrives late", late_color}};

    rv_plot_ticks(c, &plot, ticks, 4, TICK_TOP, TICK_HEIGHT);
    rv_plot_grid(c, &plot, scale, 3);
    rv_canvas_header(c, "The right ear hears", legend, 2);
    if (kept_side) {
        if (!off)
            rv_plot_curve(c, &plot, editor.delayed, 2, late_color);
        rv_plot_curve(c, &plot, editor.kept, 2, kept_color);
    } else {
        rv_plot_curve(c, &plot, editor.kept, 2, kept_color);
        rv_plot_curve(c, &plot, editor.delayed, 2, late_color);
    }
}

/* The shape values read inactive while nothing plays through the effect, Off
 * or a Mix of 0; they still edit. */
static void draw_parameters(const struct rv_canvas *c)
{
    char text[PARAM_COUNT][LABEL_BYTES];
    const char *values[PARAM_COUNT];
    uint8_t states[PARAM_COUNT];
    int on = value(PARAM_ON) && value(PARAM_MIX) != 0;
    for (int parameter = 0; parameter < PARAM_COUNT; parameter++) {
        values[parameter] =
            surround_label(parameter, value(parameter), text[parameter]);
        states[parameter] = parameter != editor.parameter      ? RV_CELL_IDLE
                            : editor.input.depth == DEPTH_EDIT ? RV_CELL_EDITING
                                                               : RV_CELL_SELECTED;
        if (!on && parameter != PARAM_ON && parameter != PARAM_MIX)
            states[parameter] |= RV_CELL_MUTED;
    }
    rv_canvas_fields(c, INLINE_TOP, INLINE_HEIGHT, titles, values, states,
                     INLINE_COUNT);
    rv_canvas_cells(c, CELL_TOP, c->height - CELL_TOP, titles + INLINE_COUNT,
                    values + INLINE_COUNT, states + INLINE_COUNT, CELL_COUNT);
}

static void draw(const struct rv_context *ctx, const struct rv_gfx *g, int width,
                 int height)
{
    struct rv_canvas c;
    (void)ctx;
    rv_canvas_begin(&c, g, width, height);
    draw_plot(&c);
    draw_parameters(&c);
}

static void wheel(int delta)
{
    if (editor.input.depth == DEPTH_PARAMETER) {
        editor.parameter = rv_clamp(editor.parameter + delta, 0, PARAM_COUNT - 1);
        return;
    }
    int previous = value(editor.parameter);
    int next = rv_clamp(previous + delta, 0, counts[editor.parameter] - 1);
    if (next == previous)
        return;
    /* Main republishes from setting_changed, so the change plays at once. */
    api->setting_set_int(self, keys[editor.parameter], next);
    compute_curves();
}

/* A held Select puts the highlighted value back to its default. */
static void reset(void)
{
    if (value(editor.parameter) == defaults[editor.parameter])
        return;
    /* Main republishes from setting_changed, so the change plays at once. */
    api->setting_set_int(self, keys[editor.parameter], defaults[editor.parameter]);
    compute_curves();
}

static int input(const struct rv_context *ctx, const struct rv_input *input)
{
    (void)ctx;
    switch (rv_editor_input(&editor.input, input)) {
    case RV_EDITOR_IGNORED:
        return 0;
    case RV_EDITOR_CONSUMED:
        return 1;
    case RV_EDITOR_WHEEL:
        wheel(input->delta);
        break;
    case RV_EDITOR_RESET:
        reset();
        break;
    default:
        break;
    }
    api->ui_invalidate(self);
    return 1;
}

static void opened(const struct rv_context *ctx)
{
    (void)ctx;
    memset(&editor, 0, sizeof editor);
    editor.input.depths = DEPTHS;
    compute_curves();
}

static const struct rv_screen screens[] = {
    RV_SCREEN_CANVAS("editor", "Surround", draw, input,
                     .transition = RV_TRANSITION_FROM_PANE, .opened = opened),
};

static const struct rv_descriptor module = {
    .size = sizeof module,
    RV_SCREENS(screens),
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core,
                                            rv_extension_handle handle)
{
    api = core;
    self = handle;
    return &module;
}
