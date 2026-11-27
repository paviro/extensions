/* SPDX-License-Identifier: GPL-3.0-only */

/* The Fatigue Reduction screen: the response of the chosen strength as a
 * curve over the strength itself. The wheel picks the strength, holding Select
 * puts it back to Off and Menu steps back out. Every change plays at once. */

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
    FIELD_GAP = 5,     /* tick labels to the field row */
    FIELD_HEIGHT = 27, /* Strength, title left and value right, at the bottom */
};

/* 0 dB at the top, -8 dB at the bottom, just under Strong's deepest cut; the
 * plot runs down to the tick labels above the field row, which sits on the
 * bottom edge. */
static struct rv_plot plot_for(int height)
{
    struct rv_plot plot = {
        .left = 44, .top = PLOT_TOP, .points = PLOT_POINTS, .bottom_value = -80};
    plot.bottom = (int16_t)(height - FIELD_HEIGHT - FIELD_GAP - TICK_HEIGHT - TICK_GAP);
    return plot;
}

static struct {
    struct rv_editor input;
    int16_t curve[PLOT_POINTS];
} editor;

static int strength(void)
{
    return rv_clamp(api->setting_get_int(self, KEY_STRENGTH), FATIGUE_OFF,
                    FATIGUE_STRONG);
}

/* The chosen strength's curve, drawn for 44.1 kHz, the rate nearly everything
 * plays at; it barely moves at 48 kHz. Off is the zero line. */
static void compute_curve(void)
{
    int32_t coefficients[FATIGUE_STAGES][5];
    int chosen = strength();
    if (chosen != FATIGUE_OFF &&
        fatigue_design(coefficients, (unsigned int)chosen, 44100))
        rv_dsp_response(coefficients, FATIGUE_STAGES, 44100, editor.curve, PLOT_POINTS);
    else
        memset(editor.curve, 0, sizeof editor.curve);
}

static void draw_plot(const struct rv_canvas *c, const struct rv_plot *plot)
{
    static const char *const ticks[] = {"20", "200", "2k", "20k Hz"};
    static const char *const scale[] = {"0", "-4", "-8"};

    rv_plot_ticks(c, plot, ticks, 4, plot->bottom + TICK_GAP, TICK_HEIGHT);
    rv_plot_grid(c, plot, scale, 3);
    rv_canvas_header(c, "Response", NULL, 0);
    rv_plot_curve(c, plot, editor.curve, 2, RV_CANVAS_ACCENT);
}

static void draw_strength(const struct rv_canvas *c, int height)
{
    static const char *const title = "Strength";
    const char *value = strength_labels[strength()];
    uint8_t state = RV_CELL_EDITING;
    rv_canvas_fields(c, height - FIELD_HEIGHT, FIELD_HEIGHT, &title, &value, &state, 1);
}

static void draw(const struct rv_context *ctx, const struct rv_gfx *g, int width,
                 int height)
{
    struct rv_canvas c;
    (void)ctx;
    struct rv_plot plot = plot_for(height);
    rv_canvas_begin(&c, g, width, height);
    draw_plot(&c, &plot);
    draw_strength(&c, height);
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
        set_strength(rv_clamp(strength() + input->delta, FATIGUE_OFF, FATIGUE_STRONG));
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
    RV_SCREEN_CANVAS("editor", "Fatigue Reduction", draw, input,
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
