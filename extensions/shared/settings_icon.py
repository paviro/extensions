# SPDX-License-Identifier: GPL-3.0-only
"""The Settings pane icon pipeline the assets/render.py scripts share: a white
on black coverage mask of 78 x 121, lit from the top and mirrored in a fading
reflection below, written as 8-bit gray PNG with ImageMagick's `magick`, and
the command line that renders the icons or checks the committed ones.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

WIDTH, HEIGHT = 78, 121
# The art box of the stock icons.
ART_TOP, ART_BOTTOM, ART_CENTRE = 12, 62, (39, 37)
# The light: gray level at the top of the art falling to its bottom.
LIGHT = (228, 155)
# The reflection mirrors the art about the line below it and fades out.
MIRROR = 77.5
REFLECTION = (0.40, 0.02)
# A supersampled mask is drawn this many times larger and scaled down, which
# rounds its edges; magick draws discs without antialiasing at this size.
SUPERSAMPLE = 8
GRAY8 = (
    "-colorspace",
    "gray",
    "-depth",
    "8",
    "-define",
    "png:color-type=0",
    "-define",
    "png:bit-depth=8",
)


def magick(*args):
    subprocess.run(["magick", *map(str, args)], check=True)


def canvas(*draw):
    """magick arguments for a mask drawn with `draw` on the black frame."""
    return ["-size", f"{WIDTH}x{HEIGHT}", "xc:black", *draw]


def supersampled(work, shapes):
    """magick arguments for a mask drawn with shapes(SUPERSAMPLE), which
    returns -draw arguments with every coordinate scaled by SUPERSAMPLE."""
    mask = work / "mask.png"
    magick(
        "-size",
        f"{WIDTH * SUPERSAMPLE}x{HEIGHT * SUPERSAMPLE}",
        "xc:black",
        *shapes(SUPERSAMPLE),
        "-filter",
        "Lanczos",
        "-resize",
        f"{WIDTH}x{HEIGHT}!",
        mask,
    )
    return [mask]


def glyph(work, font, code, reach, mirror=False):
    """magick arguments for a mask of the font's glyph at `code`, fitted into a
    square of side `reach` centred on the art box, flipped left to right when
    `mirror` is set."""
    path = work / f"{code:x}.png"
    magick(
        "-background",
        "black",
        "-fill",
        "white",
        "-font",
        font,
        "-pointsize",
        "400",
        f"label:{chr(code)}",
        "-trim",
        "+repage",
        *(["-flop"] if mirror else []),
        "-resize",
        f"{reach}x{reach}",
        path,
    )
    return canvas(
        "(",
        path,
        "-background",
        "black",
        "-gravity",
        "Center",
        "-extent",
        f"{reach}x{reach}",
        ")",
        "-gravity",
        "NorthWest",
        "-geometry",
        f"+{ART_CENTRE[0] - reach // 2}+{ART_CENTRE[1] - reach // 2}",
        "-compose",
        "Over",
        "-composite",
    )


def icon(out, work, mask, art_top, art_bottom, light=LIGHT):
    """Write the icon for `mask`, magick arguments that make the coverage mask,
    whose art spans rows art_top to art_bottom."""
    # The art under a light that fades down the frame.
    slope = (light[0] - light[1]) / (art_bottom - art_top)
    top = round(light[0] + slope * art_top)
    bottom = round(top - slope * (HEIGHT - 1))
    art = work / "art.png"
    magick(
        *mask,
        "(",
        "-size",
        f"{WIDTH}x{HEIGHT}",
        f"gradient:gray({top})-gray({bottom})",
        ")",
        "-compose",
        "Multiply",
        "-composite",
        art,
    )
    # Its reflection: flipped, moved below the mirror line, faded.
    shift = round(2 * MIRROR - (HEIGHT - 1))
    start = round(MIRROR + 0.5 + (MIRROR - art_bottom))
    fade = work / "fade.png"
    magick(
        "-size",
        f"{WIDTH}x{HEIGHT}",
        "xc:black",
        "(",
        "-size",
        f"{WIDTH}x{HEIGHT - start}",
        f"gradient:gray({REFLECTION[0] * 100}%)-gray({REFLECTION[1] * 100}%)",
        ")",
        "-geometry",
        f"+0+{start}",
        "-composite",
        fade,
    )
    magick(
        art,
        "(",
        art,
        "-flip",
        "-roll",
        f"+0+{shift}",
        fade,
        "-compose",
        "Multiply",
        "-composite",
        ")",
        "-compose",
        "Lighten",
        "-composite",
        *GRAY8,
        out,
    )


def main(doc, icons, font=False):
    """Render each committed icon in `icons`, a map from its path to a function
    that writes it given the output path and a work folder, and the font when
    `font` is set. With --check, compare fresh renders with the committed files
    instead."""
    parser = argparse.ArgumentParser(description=doc.split("\n\n")[0])
    if font:
        parser.add_argument(
            "--font",
            default=os.environ.get("CUPERTINO_ICONS"),
            help="CupertinoIcons.ttf",
        )
    parser.add_argument(
        "--check",
        action="store_true",
        help="render into a temporary folder and compare with the committed files",
    )
    args = parser.parse_args()
    if font and not args.font:
        sys.exit("give the font with --font or CUPERTINO_ICONS")
    extra = (args.font,) if font else ()
    with tempfile.TemporaryDirectory(prefix=f"{next(iter(icons)).stem}-") as name:
        work = Path(name)
        for committed, render in icons.items():
            if not args.check:
                render(committed, work, *extra)
                continue
            fresh = work / committed.name
            render(fresh, work, *extra)
            differing = subprocess.run(
                ["magick", "compare", "-metric", "AE", committed, fresh, "null:"],
                capture_output=True,
                text=True,
                check=False,
            ).stderr.split()[0]
            if differing != "0":
                sys.exit(
                    f"render.py: {committed.name} differs from a fresh render: "
                    f"{differing} px"
                )
            print(f"{committed.name} matches")
