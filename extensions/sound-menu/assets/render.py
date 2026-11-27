#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render settings-sound.png, the Settings pane icon: Cupertino Icons'
waveform fitted into the art box of a 78 x 121 gray coverage mask in the
style of the stock Settings icons, lit from the top and mirrored in a fading
reflection below. Drawing uses ImageMagick's `magick`; the font
(CupertinoIcons.ttf from the Flutter `cupertino_icons` package, MIT, see
LICENSE.cupertino-icons) is not in the repository.

    python3 assets/render.py --font CupertinoIcons.ttf
    python3 assets/render.py --font CupertinoIcons.ttf --check
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "shared"))
from settings_icon import ART_BOTTOM, ART_TOP, glyph, icon, main

OUT = HERE / "settings-sound.png"
# The glyph's code point in the font, and its reach: the longer side of the
# box it is fitted into, centred on the art box.
WAVEFORM, REACH = 0xF894, 52


def render(out, work, font):
    icon(out, work, glyph(work, font, WAVEFORM, REACH), ART_TOP, ART_BOTTOM)


if __name__ == "__main__":
    main(__doc__, {OUT: render}, font=True)
