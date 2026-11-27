#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render settings-bass-enhancement.png, the Settings pane icon: one period of
a low wave inside a ring, on a 78 x 121 gray coverage mask in the style of the
stock Settings icons, lit from the top and mirrored in a fading reflection
below. Drawing uses ImageMagick's `magick`.

    python3 assets/render.py
    python3 assets/render.py --check
"""

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "shared"))
from settings_icon import icon, main, supersampled

OUT = HERE / "settings-bass-enhancement.png"
# The ring about CENTRE between OUTER and INNER; inside it one period of a
# sine, WAVE_HALF each side of the centre and WAVE_AMPLITUDE high, stroked
# WAVE_STROKE wide with round ends, which stop short of the ring.
CENTRE = (39, 37)
OUTER, INNER = 26, 21
WAVE_HALF, WAVE_AMPLITUDE, WAVE_STROKE = 17.5, 7, 4.5
WAVE_POINTS = 240
ART_TOP, ART_BOTTOM = CENTRE[1] - OUTER, CENTRE[1] + OUTER


def wave(k):
    """The sine as a magick path, left to right, with every coordinate scaled
    by k."""
    cx, cy = CENTRE
    points = []
    for i in range(WAVE_POINTS + 1):
        x = cx - WAVE_HALF + 2 * WAVE_HALF * i / WAVE_POINTS
        y = cy - WAVE_AMPLITUDE * math.sin(2 * math.pi * i / WAVE_POINTS)
        points.append(f"{x * k:.1f},{y * k:.1f}")
    return "M " + " L ".join(points)


def shapes(k):
    """magick -draw arguments for the ring and the wave, white on black, with
    every coordinate scaled by k."""
    cx, cy = (v * k for v in CENTRE)
    return [
        "-fill",
        "white",
        "-draw",
        f"circle {cx},{cy} {cx + OUTER * k},{cy}",
        "-fill",
        "black",
        "-draw",
        f"circle {cx},{cy} {cx + INNER * k},{cy}",
        "-fill",
        "none",
        "-stroke",
        "white",
        "-strokewidth",
        str(WAVE_STROKE * k),
        "-draw",
        f"stroke-linecap round path '{wave(k)}'",
        "-stroke",
        "none",
    ]


def render(out, work):
    icon(out, work, supersampled(work, shapes), ART_TOP, ART_BOTTOM)


if __name__ == "__main__":
    main(__doc__, {OUT: render})
