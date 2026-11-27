#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Write docs/extension-bitmaps.md: the bitmaps of the first-party extensions
a theme can replace, from the vendored extension asset catalog
rvt/data/extensions.json."""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from rvt import assets  # noqa: E402

PAGE = ROOT / "docs" / "extension-bitmaps.md"
HEAD = """# Extension bitmaps

The first-party extension bitmaps a theme can replace with
`<appearance>/extensions/<id>/<asset>.png`, or `<appearance>/reprise/<asset>.png`
for the core's (id `reprise`). See [Authoring](authoring.md#extension-bitmaps).
Size is width x height in pixels, which the art must match. Menu marks a
Settings icon, a 78 x 121 coverage mask drawn in the icon tint.

The catalog and how to regenerate it are in
[Catalog](catalog.md#extension-asset-catalog).

Regenerate this page with `python3 tools/extension_bitmaps_doc.py`.
"""


def page(records: dict[tuple[str, str], dict]) -> str:
    lines = [
        HEAD,
        "| Id | Asset | Size | Format | Palette | Menu |",
        "|---|---|---|---|---|---|",
    ]
    for (identifier, asset), record in sorted(records.items()):
        fmt = assets.FORMAT_NAMES.get(record["format"], f"{record['format']:#x}")
        lines.append(
            f"| `{identifier}` | `{asset}` | {record['width']}x{record['height']} | "
            f"{fmt} | {record['palette_colors'] or '-'} | "
            f"{'yes' if record['menu'] else 'no'} |"
        )
    return "\n".join(lines) + "\n"


def current() -> str:
    """The page as the generator writes it today."""
    return page(assets.load_catalog())


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--out", type=Path, default=PAGE)
    args = parser.parse_args(argv)
    args.out.write_text(current())
    print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
