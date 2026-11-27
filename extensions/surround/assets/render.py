#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render settings-surround.png, the Settings pane icon: a listener between two
arcs of sound at each side under an arch that joins the outer pair, on a
78 x 121 gray coverage mask in the style of the stock Settings icons, lit from
the top and mirrored in a fading reflection below. Drawing uses ImageMagick's
`magick`.

    python3 assets/render.py
    python3 assets/render.py --check
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "shared"))
from settings_icon import icon, main, supersampled

OUT = HERE / "settings-surround.png"
# The listener: a disc of HEAD radius at CENTRE. The sound: two arcs about the
# same centre at each side, ARCS radii, each SPAN degrees wide, and an arch at
# the outer radius over the head between ARCH_FROM and ARCH_TO degrees, all
# STROKE wide.
CENTRE = (39, 37)
HEAD = 6
ARCS = (16, 27)
SPAN = 70
ARCH_FROM, ARCH_TO = 200, 340
STROKE = 4.5
ART_TOP, ART_BOTTOM = CENTRE[1] - 18, CENTRE[1] + 18


def shapes(k):
    """magick -draw arguments for the listener, the arcs and the arch, white on
    black, with every coordinate scaled by k."""
    cx, cy = (v * k for v in CENTRE)
    draw = [
        "-fill",
        "none",
        "-stroke",
        "white",
        "-strokewidth",
        str(STROKE * k),
    ]
    for radius in ARCS:
        r = radius * k
        for middle in (180, 0):
            start, end = middle - SPAN / 2, middle + SPAN / 2
            box = f"{cx - r},{cy - r} {cx + r},{cy + r}"
            draw += ["-draw", f"stroke-linecap round arc {box} {start},{end}"]
    r = ARCS[-1] * k
    box = f"{cx - r},{cy - r} {cx + r},{cy + r}"
    draw += ["-draw", f"stroke-linecap round arc {box} {ARCH_FROM},{ARCH_TO}"]
    draw += [
        "-stroke",
        "none",
        "-fill",
        "white",
        "-draw",
        f"circle {cx},{cy} {cx + HEAD * k},{cy}",
    ]
    return draw


def render(out, work):
    icon(out, work, supersampled(work, shapes), ART_TOP, ART_BOTTOM)


if __name__ == "__main__":
    main(__doc__, {OUT: render})
