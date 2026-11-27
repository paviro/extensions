/* SPDX-License-Identifier: GPL-3.0-only */

/* The Bass Enhancement screen: the response of the chosen strength as a
 * curve, with its precut included, over the strength and the precut it
 * brings. The wheel picks the strength, holding Select puts it back to Off and
 * Menu steps back out. Every change plays at once. */

#include "rv_canvas.h"
#include "rv_dsp.h"
#include "rv_libc.h"
#include "rv_ui.h"
#include "shared.h"

static const struct rv_api *api;
static rv_extension_handle self;

enum { DEPTHS = 1 }; /* the wheel edits the one value */

enum {
    PLOT_POINTS = 261,
    PLOT_TOP = 36,
    TICK_GAP = 6, /* plot bottom to the tick labels */
    TICK_HEIGHT = 22,
    FIELD_GAP = 5, /* tick labels to the field row */
    FIELD_HEIGHT =
        27,           /* Strength | Precut, title left and value right, at the bottom */
    PRECUT_BYTES = 9, /* "-10.0 dB" */
};

/* 0 dB at the top, -15 dB at the bottom, just under the deepest cut at 100 %
 * with its precut; the plot runs down to the tick labels above the field row,
 * which sits on the bottom edge. */
static struct rv_plot plot_for(int height)
{
    struct rv_plot plot = {
        .left = 44, .top = PLOT_TOP, .points = PLOT_POINTS, .bottom_value = -150};
    plot.bottom = (int16_t)(height - FIELD_HEIGHT - FIELD_GAP - TICK_HEIGHT - TICK_GAP);
    return plot;
}

static struct {
    struct rv_editor input;
    unsigned int precut; /* tenths of a decibel */
    int16_t curve[PLOT_POINTS];
} editor;

static int strength(void)
{
    return rv_clamp(api->setting_get_int(self, KEY_STRENGTH), BASS_OFF, BASS_100);
}

/* The chosen strength's curve with its precut, drawn for 44.1 kHz, the rate
 * nearly everything plays at; it barely moves at 48 kHz. Off is the zero
 * line. */
static void compute_curve(void)
{
    int32_t coefficients[BASS_STAGES][5];
    int chosen = strength();
    editor.precut = 0;
    if (chosen != BASS_OFF &&
        bass_design(coefficients, &editor.precut, (unsigned int)chosen, 44100))
        rv_dsp_response(coefficients, BASS_STAGES, 44100, editor.curve, PLOT_POINTS);
    else
        memset(editor.curve, 0, sizeof editor.curve);
}

static void draw_plot(const struct rv_canvas *c, const struct rv_plot *plot)
{
    static const char *const ticks[] = {"20", "200", "2k", "20k Hz"};
    static const char *const scale[] = {"0", "-5", "-10", "-15"};

    rv_plot_ticks(c, plot, ticks, 4, plot->bottom + TICK_GAP, TICK_HEIGHT);
    rv_plot_grid(c, plot, scale, 4);
    rv_canvas_header(c, "Response", NULL, 0);
    rv_plot_curve(c, plot, editor.curve, 2, RV_CANVAS_ACCENT);
}

/* Strength edits; the precut beside it only reads, muted, so the level under
 * the zero line has a name. */
static void draw_fields(const struct rv_canvas *c, int height)
{
    static const char *const titles[] = {"Strength", "Precut"};
    char precut[PRECUT_BYTES];
    rv_format_tenths(precut, sizeof precut, -(int)editor.precut, 0, " dB");
    const char *values[] = {strength_labels[strength()], precut};
    uint8_t states[] = {RV_CELL_EDITING, RV_CELL_IDLE | RV_CELL_MUTED};
    rv_canvas_fields(c, height - FIELD_HEIGHT, FIELD_HEIGHT, titles, values, states, 2);
}

static void draw(const struct rv_context *ctx, const struct rv_gfx *g, int width,
                 int height)
{
    struct rv_canvas c;
    (void)ctx;
    struct rv_plot plot = plot_for(height);
    rv_canvas_begin(&c, g, width, height);
    draw_plot(&c, &plot);
    draw_fields(&c, height);
}

/* Main republishes from setting_changed, so a change plays at once. */
static void set_strength(int next)
{
    if (next == strength())
        return;
    api->setting_set_int(self, KEY_STRENGTH, next);
    compute_curve();
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
        set_strength(rv_clamp(strength() + input->delta, BASS_OFF, BASS_100));
        break;
    case RV_EDITOR_RESET:
        set_strength(STRENGTH_DEFAULT);
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
    compute_curve();
}

static const struct rv_screen screens[] = {
    RV_SCREEN_CANVAS("editor", "Bass Enhancement", draw, input,
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
