#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render settings-compressor.png, the Settings pane icon: a waveform pressed
flat against two bars, on a 78 x 121 gray coverage mask in the style of the
stock Settings icons, lit from the top and mirrored in a fading reflection
below. Drawing uses ImageMagick's `magick`.

    python3 assets/render.py
    python3 assets/render.py --check
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "shared"))
from settings_icon import canvas, icon, main

OUT = HERE / "settings-compressor.png"
# The two bars the waveform presses against, and the gap between them.
BAR_LEFT, BAR_RIGHT = 13, 65
BAR_TOP, BAR_BOTTOM, BAR_HEIGHT, BAR_RADIUS = 12, 61, 3, 1.5
GAP_TOP, GAP_BOTTOM = BAR_TOP + BAR_HEIGHT + 3, BAR_BOTTOM - 3
# The waveform: bars of these full heights, centred in the gap and cut to it.
WAVE_HEIGHTS = (12, 24, 40, 52, 40, 24, 12)
WAVE_PITCH, WAVE_WIDTH, WAVE_RADIUS = 7.5, 4.5, 1.5


def shapes():
    """magick -draw arguments for the glyph, white on black."""
    draw = ["-fill", "white"]
    for top in (BAR_TOP, BAR_BOTTOM):
        draw += [
            "-draw",
            (
                f"roundrectangle {BAR_LEFT},{top} {BAR_RIGHT},{top + BAR_HEIGHT} "
                f"{BAR_RADIUS},{BAR_RADIUS}"
            ),
        ]
    centre = (GAP_TOP + GAP_BOTTOM) / 2
    first = (BAR_LEFT + BAR_RIGHT) / 2 - WAVE_PITCH * (len(WAVE_HEIGHTS) - 1) / 2
    for i, height in enumerate(WAVE_HEIGHTS):
        half = min(height, GAP_BOTTOM - GAP_TOP) / 2
        left = first + i * WAVE_PITCH - WAVE_WIDTH / 2
        draw += [
            "-draw",
            (
                f"roundrectangle {left:.2f},{centre - half:.2f} "
                f"{left + WAVE_WIDTH:.2f},{centre + half:.2f} "
                f"{WAVE_RADIUS},{WAVE_RADIUS}"
            ),
        ]
    return draw


def render(out, work):
    icon(out, work, canvas(*shapes()), BAR_TOP, BAR_BOTTOM + BAR_HEIGHT)


if __name__ == "__main__":
    main(__doc__, {OUT: render})
