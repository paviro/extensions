/* SPDX-License-Identifier: GPL-3.0-only */

/* The timer is drawn as the stock shuffle glyph is stored: an Indexed8 BMap of
 * black at alpha levels up to the glyph's own 0xce. The minutes are drawn in
 * Apple's Helvetica, black on an opaque white scratch, and read back as
 * coverage, so the glyph blitter's handling of alpha does not matter. */

#include "indicator.h"
#include "moon.h"
#include "osos-2.0.5.h"
#include "rv_libc.h"
#include <stddef.h>

enum {
    HEIGHT = 14,   /* the shuffle glyph's */
    MOON_GAP = 3,  /* between the minutes and the moon */
    CHAIN_GAP = 5, /* before the shuffle glyph, as between shuffle and repeat */
    LEVELS = 33,
    INK = 0xce,     /* the shuffle glyph's darkest alpha */
    SCRATCH_W = 48, /* the minutes, "2:00" at most */
    SCRATCH_H = 24,
    FONT_SIZE = 0x40, /* bold, as tall as the glyphs beside it */
};

static uint32_t font[2];
static uint8_t font_built;
static uint8_t *built;
static uint32_t built_minutes;
static const void *built_shuffle;

static uint32_t word(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put_word(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(value >> (8 * i));
}

static void header(uint8_t *b, uint16_t format, uint16_t stride, uint16_t depth,
                   uint32_t width, uint32_t height, uint32_t payload)
{
    for (unsigned i = 0; i < BMap_payload; i++)
        b[i] = 0;
    b[BMap_format] = (uint8_t)format;
    b[BMap_format + 1] = (uint8_t)(format >> 8);
    b[BMap_stride] = (uint8_t)stride;
    b[BMap_stride + 1] = (uint8_t)(stride >> 8);
    b[BMap_depth] = (uint8_t)depth;
    put_word(b + BMap_bounds + 8, height);
    put_word(b + BMap_bounds + 12, width);
    put_word(b + BMap_payload_bytes, payload);
}

/* Draws `text` into a scratch block from `api` and leaves its coverage, trimmed
 * to the ink, at the start of the block, SCRATCH_W bytes a row; the ink's width
 * and height in *ink_w and *ink_h. NULL when nothing could be drawn. */
static uint8_t *draw_minutes(const struct rv_api *api, rv_extension_handle self,
                             const char *text, int *ink_w, int *ink_h)
{
    uint8_t *scratch = api->alloc(self, BMap_payload + SCRATCH_W * SCRATCH_H * 4);
    if (!scratch)
        return NULL;
    header(scratch, BMapFormat_BGRA8888, SCRATCH_W * 4, 32, SCRATCH_W, SCRATCH_H,
           SCRATCH_W * SCRATCH_H * 4);
    uint32_t storage[Bitmap_bytes / 4];
    uint32_t context[GraphicsContext_bytes / 4];
    Bitmap_Construct(storage);
    GraphicsContext_InitFromBitmapResource(context, storage, scratch);
    static const uint8_t white[4] = {0xff, 0xff, 0xff, 0xff},
                         black[4] = {0, 0, 0, 0xff};
    int32_t all[4] = {0, 0, SCRATCH_H, SCRATCH_W};
    GraphicsContext_SetColor(context, white);
    GraphicsContext_SetDrawMode(context, GraphicsDrawMode_Copy);
    GraphicsContext_FillRect(context, all);
    if (!font_built) {
        Font_Construct(font, "Helvetica", FONT_SIZE, 1);
        font_built = 1;
    }
    GraphicsContext_SetFont(context, font);
    GraphicsContext_SetColor(context, black);
    GraphicsContext_SetDrawMode(context, GraphicsDrawMode_Text);
    GraphicsContext_SetTextPen(context, 2, 2 + GraphicsContext_GetFontAscent(context));
    GraphicsContext_DrawUtf8(context, text);
    GraphicsContext_Destroy(context);
    Bitmap_Destroy(storage);

    /* Each coverage byte lands at or before the pixel it is read from. */
    const uint8_t *pixels = scratch + BMap_payload;
    int left = SCRATCH_W, right = -1, top = SCRATCH_H, bottom = -1;
    for (int i = 0; i < SCRATCH_W * SCRATCH_H; i++) {
        uint8_t c = 0xff - pixels[i * 4 + 2];
        scratch[i] = c;
        if (c) {
            int x = i % SCRATCH_W, y = i / SCRATCH_W;
            left = x < left ? x : left;
            right = x > right ? x : right;
            top = y < top ? y : top;
            bottom = y > bottom ? y : bottom;
        }
    }
    if (right < 0) {
        api->free(self, scratch);
        return NULL;
    }
    for (int y = top; y <= bottom; y++)
        for (int x = left; x <= right; x++)
            scratch[(y - top) * SCRATCH_W + x - left] = scratch[y * SCRATCH_W + x];
    *ink_w = right - left + 1;
    *ink_h = bottom - top + 1;
    return scratch;
}

static uint8_t level(uint32_t coverage)
{
    return (uint8_t)((coverage * (LEVELS - 1) + 127) / 255);
}

/* The stock glyph's pixel alpha at (x, y) as one of our levels. */
static uint8_t glyph_level(const uint8_t *g, int x, int y)
{
    uint32_t colors = word(g + BMap_payload),
             stride = g[BMap_stride] | g[BMap_stride + 1] << 8;
    uint8_t index = g[BMap_payload + 4 + colors * 4 + y * stride + x];
    if (index >= colors)
        return 0;
    uint32_t alpha = g[BMap_payload + 4 + index * 4 + 3];
    uint32_t l = (alpha * (LEVELS - 1) + INK / 2) / INK;
    return (uint8_t)(l < LEVELS ? l : LEVELS - 1);
}

static int glyph_usable(const uint8_t *g, int *w, int *h)
{
    const int32_t top = (int32_t)word(g + BMap_bounds),
                  left = (int32_t)word(g + BMap_bounds + 4);
    *h = (int32_t)word(g + BMap_bounds + 8) - top;
    *w = (int32_t)word(g + BMap_bounds + 12) - left;
    return g[BMap_format] == (uint8_t)BMapFormat_Indexed8 && !g[BMap_format + 1] &&
           g[BMap_depth] == 8 && !top && !left && *w > 0 && *h > 0 && *h <= HEIGHT &&
           word(g + BMap_payload) <= 256;
}

void indicator_free(const struct rv_api *api, rv_extension_handle self)
{
    if (built)
        api->free(self, built);
    built = NULL;
}

const void *indicator_build(const struct rv_api *api, rv_extension_handle self,
                            uint32_t minutes, const void *shuffle)
{
    if (built && built_minutes == minutes && built_shuffle == shuffle)
        return built;
    char text[8];
    if (minutes < 60)
        rv_format(text, sizeof text, "%lu", minutes);
    else
        rv_format(text, sizeof text, "%lu:%02lu", minutes / 60, minutes % 60);
    int text_w, text_h;
    uint8_t *cover = draw_minutes(api, self, text, &text_w, &text_h);
    if (!cover)
        return NULL;
    int glyph_w = 0, glyph_h = 0;
    if (shuffle && !glyph_usable(shuffle, &glyph_w, &glyph_h))
        shuffle = NULL;

    int timer = text_w + MOON_GAP + MOON_SIZE;
    int width = timer + (shuffle ? CHAIN_GAP + glyph_w : 0);
    uint32_t pixels = BMap_payload + 4 + LEVELS * 4;
    uint8_t *b = api->alloc(self, pixels + (uint32_t)(width * HEIGHT));
    if (!b) {
        api->free(self, cover);
        return NULL;
    }
    header(b, BMapFormat_Indexed8, (uint16_t)width, 8, (uint32_t)width, HEIGHT,
           4 + LEVELS * 4 + (uint32_t)(width * HEIGHT));
    put_word(b + BMap_payload, LEVELS);
    for (unsigned i = 0; i < LEVELS; i++) {
        uint8_t *entry = b + BMap_payload + 4 + i * 4;
        entry[0] = entry[1] = entry[2] = 0;
        entry[3] = (uint8_t)(i * INK / (LEVELS - 1));
    }
    int text_y = (HEIGHT - text_h) / 2; /* ink taller than HEIGHT is clipped */
    int moon_x = text_w + MOON_GAP, moon_y = (HEIGHT - MOON_SIZE) / 2;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < width; x++) {
            uint8_t *out = b + pixels + y * width + x;
            if (x < text_w)
                *out = y >= text_y && y < text_y + text_h
                           ? level(cover[(y - text_y) * SCRATCH_W + x])
                           : 0;
            else if (x >= moon_x && x < timer)
                *out = y >= moon_y && y < moon_y + MOON_SIZE
                           ? level(moon[(y - moon_y) * MOON_SIZE + x - moon_x])
                           : 0;
            else if (shuffle && x >= timer + CHAIN_GAP && y < glyph_h)
                *out = glyph_level(shuffle, x - timer - CHAIN_GAP, y);
            else
                *out = 0;
        }
    api->free(self, cover);
    indicator_free(api, self);
    built = b;
    built_minutes = minutes;
    built_shuffle = shuffle;
    return built;
}
