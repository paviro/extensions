#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render settings-channels.png, the Settings pane icon: the editor's half
disc with a lobe inside it on a 78 x 121 gray coverage mask in the style of the
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
from settings_icon import HEIGHT, WIDTH, canvas, icon, main

OUT = HERE / "settings-channels.png"
# The half disc: a ring about CENTRE, the upper half only.
CENTRE = (39, 50)
OUTER, INNER = 26, 22
# The lobe: a quarter-circle fan from the centre, LOBE from it, between the
# arms at 45 degrees.
LOBE = 18


def fan():
    """Points along the lobe's arc between the arms, left to right."""
    cx, cy = CENTRE
    points = []
    for step in range(0, 91, 3):
        t = math.radians(225 + step)
        points.append(f"{cx + LOBE * math.cos(t):.2f},{cy + LOBE * math.sin(t):.2f}")
    return points


def shapes():
    """magick -draw arguments for the half disc and lobe, white on black."""
    cx, cy = CENTRE
    return [
        "-fill",
        "white",
        "-draw",
        f"ellipse {cx},{cy} {OUTER},{OUTER} 180,360",
        "-fill",
        "black",
        "-draw",
        f"ellipse {cx},{cy} {INNER},{INNER} 180,360",
        "-draw",
        f"rectangle 0,{cy} {WIDTH},{HEIGHT}",
        "-fill",
        "white",
        "-draw",
        "polygon " + " ".join([f"{cx},{cy}", *fan()]),
    ]


def render(out, work):
    icon(out, work, canvas(*shapes()), CENTRE[1] - OUTER, CENTRE[1])


if __name__ == "__main__":
    main(__doc__, {OUT: render})
