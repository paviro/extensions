# The compiler

`rvt` is a Python 3.11+ package without dependencies. Run it from the `themes/` directory as `python3 -m rvt`. Only `preview` and `recolor` need a firmware.

## Commands

| Command | Does |
|---|---|
| `rvt new <id> [--name NAME]` | Writes `<id>/theme.toml` with every role and option as a comment, and the empty `apple/`, `reprise/` and `extensions/` folders of `<id>/light/` and `<id>/dark/`; `reprise` is the reserved owner id of the core's own bitmaps in the theme format |
| `rvt check <folder>` | Compiles without writing; prints what each derive rule matched, the warnings and a size table per appearance |
| `rvt compile <folder> [--out DIR]` | The same, then writes `<out>/<id>/<id>.rvt` and `report.json`; `--out` defaults to `dist` |
| `rvt inspect <file> [--json]` | Validates an `.rvt` against the catalog and describes it; extension bitmaps are named from the extension asset catalog, by their hashes when it does not know them |
| `rvt catalog list [--group PREFIX] [--format FMT]` | One line per Apple bitmap: name, size, format, palette entries, tintable |
| `rvt catalog list --extensions [--group ID] [--format FMT]` | One line per extension bitmap: id, asset, size, format, palette entries, menu or bitmap |
| `rvt catalog show <Name>` | Every catalog field of one Apple bitmap, by name or hex id |
| `rvt catalog show <id>/<asset>` | Every catalog field of one extension bitmap, and its two hashes |
| `rvt preview <folder> --firmware FILE` | Renders every derived bitmap of a theme from a decrypted `osos.bin` or the booted `osos-cfw.bin` to `dist/<id>/derived/` |
| `rvt recolor <png> --firmware FILE` | Turns `<theme>/<appearance>/apple/<Name>.png` into a `recolor` rule file when the PNG is a pure recolour of the stock bitmap |

| Option | On | Does |
|---|---|---|
| `--strict` | `check`, `compile`, `preview` | Turns warnings into errors |
| `--catalog FILE` | all but `new` | Uses another Apple catalog |
| `--extension assets.json` | `check`, `compile`, `preview`, `inspect`, `catalog` | Adds an extension's bitmaps ([Catalog](catalog.md#extension-asset-catalog)); repeatable |
| `--allow-reserved` | `check`, `compile`, `preview` | Accepts the id `classic` |

Warnings cover:

- a PNG with more colours than its native palette (reduced to fit)
- a text and background pair the theme colours that falls below 4.5:1
- a tint glob that matches nothing
- a Settings icon with nothing below its art box while `flat_icons` is off
- an extension bitmap not in the catalog (compiled as a Settings icon)

`report.json` has the theme's id, name, author and version and, per appearance:

| Key | Contents |
|---|---|
| `contrast` | Contrast of every text and background pair, for Theme Default and each accent |
| `accents_builtin` | Accents using the built-in colours |
| `records` | Record count per kind |
| `extension_bitmaps` | Extension bitmap count per kind |
| `extension_tinted` | Tinted extension bitmaps and their roles |
| `palette_reduced` | Palette reductions |

It also has each appearance's size in bytes. The output is deterministic, with sorted keys.

## Layout

| Path | Contents |
|---|---|
| `rvt/rvtfile.py` | The `.rvt` header and packs; every constant parsed from `rvt/data/rvt.h` |
| `rvt/data/` | The vendored contracts: the format header, the two catalogs and the SDK's bitmap encoder |
| `rvt/data/rvt.h` | The file format; the Appearance extension compiles against this header |
| `rvt/data/osos-2.0.5.json` | The [catalog](catalog.md) of Apple's bitmaps |
| `rvt/data/extensions.json` | The [extension asset catalog](catalog.md#extension-asset-catalog): the bitmaps of the first-party extensions |
| `rvt/data/bitmap.py` | The SDK's bitmap encoder, a byte copy of the SDK's `bitmap.py` |
| `rvt/catalog.py` | Catalog loading, lookup, the fingerprint |
| `rvt/theme.py` | `theme.toml`: validation and the resolution of colours, accents, tints and rules |
| `rvt/bitmaps.py` | PNG reading, palette fitting, the native pixel formats |
| `rvt/style.py` | The colours, rendering, accent palettes and tint bytes of an appearance; the Classic colours and the built-in accents |
| `rvt/derive.py` | Derivation rules, record payloads and the reference arithmetic of the iPod's derivations |
| `rvt/lz.py` | The core's lz codec |
| `rvt/assets.py` | Extension bitmaps: the folders, encoding, the extension asset catalog, the hashes and the pack table |
| `rvt/errors.py` | `fail`, the one error every check raises |
| `rvt/compiler.py`, `rvt/inspector.py` | Folder to bytes, bytes to a checked description |
| `rvt/firmware.py` | Stock bitmaps from a local firmware for `preview` and `recolor` |
| `rvt/cli.py` | The commands |
| `tests/` | Host tests against a synthetic catalog; a few compare the encoder and lz copies with the SDK checkout `SDK` names, the accent table with the Appearance extension and the extension asset catalog with the build |
| `tools/bitmaps_doc.py` | Writes [Bitmaps](bitmaps.md) from the catalog |
| `tools/extensions_catalog.py` | Writes `rvt/data/extensions.json` from the build's `assets.json`, see [Catalog](catalog.md#extension-asset-catalog) |
| `tools/extension_bitmaps_doc.py` | Writes [Extension bitmaps](extension-bitmaps.md) from `rvt/data/extensions.json` |
| `themes/` | The first-party themes; `themes/classic/recolor.py` generates Classic's recolour rules (needs `--firmware`), `themes/classic/fit.py` snaps Classic's PNGs to their palettes |

## Tests

From the repository root, after `make` (the build writes `build/assets.json` and compiles Classic with `--strict`):

```sh
SDK=../osos-lab/rvx-sdk python3 -m unittest discover -s themes/tests
ruff check . && ruff format --check themes tools tests
```

CI runs both on Python 3.12 with the pinned SDK checkout.
