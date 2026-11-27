#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the Settings pane icons: a Cupertino Icons glyph each, fitted into
the art box of the stock Settings icons on a 78 x 121 gray coverage mask, lit
from the top and mirrored in a fading reflection below. Drawing uses
ImageMagick's `magick`; the font (CupertinoIcons.ttf from the Flutter
`cupertino_icons` package, MIT, see LICENSE.cupertino-icons) is not in the
repository.

    python3 assets/render.py --font CupertinoIcons.ttf
    python3 assets/render.py --font CupertinoIcons.ttf --check
"""

import sys
from functools import partial
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "shared"))
from settings_icon import ART_BOTTOM, ART_TOP, glyph, icon, main

# Icon file stem, the glyph's code point in the font, and its reach: the
# longer side of the box the glyph is fitted into, centred on the art box.
ICONS = {
    "settings-appearance": (0xF7E8, 52),  # sparkles
    "settings-theme": (0xF72F, 52),  # paintbrush_fill
    "settings-accent": (0xF665, 52),  # eyedropper_full
    "settings-moon": (0xF719, 52),  # moon_fill
    "settings-covers": (0xF809, 52),  # square_lefthalf_fill
    "settings-cover": (0xF758, 52),  # perspective, mirrored to lean like the cover
}
MIRRORED = {"settings-cover"}


def render(out, work, font, code, reach, mirror):
    icon(out, work, glyph(work, font, code, reach, mirror), ART_TOP, ART_BOTTOM)


if __name__ == "__main__":
    main(
        __doc__,
        {
            HERE / f"{stem}.png": partial(
                render, code=code, reach=reach, mirror=stem in MIRRORED
            )
            for stem, (code, reach) in ICONS.items()
        },
        font=True,
    )
