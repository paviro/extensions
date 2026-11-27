/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef FLAT_COVER_H
#define FLAT_COVER_H
#include <stdint.h>

/* The Now Playing cover drawn face on. The setting is Theme (what the look
 * says; stock tilts it), Tilted or Flat. */
enum { NP_COVER_THEME, NP_COVER_TILTED, NP_COVER_FLAT };

extern const char *const now_playing_cover_labels[3];
void flat_cover_set(int choice);
int flat_cover_flat(void);
uint32_t flat_cover_view_draw(uint32_t view, uint32_t argument, uint32_t clip,
                              const void *next);
uint32_t flat_cover_renderer_draw(uint32_t renderer, const void *next);
uint32_t flat_cover_edge_lines(uint32_t buffer, const void *next);

#endif
