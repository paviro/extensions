# Catalog

The compiler ships two catalogs: Apple's bitmaps in `rvt/data/osos-2.0.5.json` and the first-party extension bitmaps in `rvt/data/extensions.json`.

## Apple's bitmaps

`rvt/data/osos-2.0.5.json` lists every `BMap` bitmap of Apple's 2.0.5 firmware a theme can replace. Records and tint bytes refer to bitmaps by their index in this list. The fingerprint in the `.rvt` header makes the Appearance extension refuse a file built against a different list.

### Fields

| Field | Contents |
|---|---|
| `schema` | 1 |
| `firmware` | File name, version, byte count and SHA-256 of the decrypted `osos.bin` the offsets refer to |
| `runtime_base`, `runtime_file_offset` | How a file offset becomes an address, below |
| `fingerprint` | The fingerprint of `bitmaps`, below |
| `bitmaps` | The entries, in firmware order |

Each entry:

| Key | Contents |
|---|---|
| `id` | Resource id |
| `name` | Resource name, what `theme.toml` and the art files use |
| `offset` | File offset of the bitmap header in `osos.bin` |
| `address` | Where the bitmap lives at run time |
| `bytes` | Bitmap size including its header |
| `format`, `depth` | Native pixel format and bits per pixel: 4 and 8 are masks, 0x64 and 0x65 are paletted, 0x565 is RGB565, 0x1888 is BGRA8888 |
| `width`, `height`, `stride` | Pixel size and row bytes |
| `data_offset` | Where a theme record starts: after the header and, for paletted formats, the colour count |
| `pixels_offset` | Where the rows start, after any palette |
| `palette_colors` | Palette entries, 0 for formats without one |
| `crc32` | CRC-32 of the whole bitmap, for checking the firmware the theme is applied to |

### Addresses

Apple loads `osos.bin` at `0x08000000` with the file's first `0xb798` bytes stripped (`OSOS_RUNTIME_FILE_OFFSET` in the core), so

```
address = runtime_base + offset - runtime_file_offset
```

### Fingerprint

The first 8 bytes of the SHA-256 of the entry list, serialised as JSON with sorted keys and no whitespace. `rvt.catalog.fingerprint` computes it.

### Browsing

`rvt catalog list` prints one line per bitmap, filtered by name prefix with `--group` or by format with `--format`. `rvt catalog show <Name>` prints every field of one bitmap. [Bitmaps](bitmaps.md) is the same list as a page.

### Provenance

`rvt/data/osos-2.0.5.json` was produced once from the core repository's former `firmware/bitmaps.json`, the bitmap table its old theme compiler read out of the 2.0.5 resource index (core commit 41ac06e, before both were removed). RepriseOS supports only that firmware, so no regeneration is expected; a catalog for another image would need a walker over its resource index. The fingerprint ties every theme to this catalog, and the Appearance extension compiles the same file into its own table, so a changed catalog means recompiling every theme and the extension.

## Extension asset catalog

`rvt/data/extensions.json` lists every bitmap of the first-party extensions and the core, one JSON object per line, sorted by id then asset. The compiler uses it to check art sizes, tell Settings icons from pictures and name bitmaps in `rvt inspect`. A bitmap not in it compiles only as a 78 x 121 Settings icon, with a warning. `--extension assets.json` on `compile`, `check`, `inspect` and `catalog` adds another extension's bitmaps. Its author generates the file from the built package:

```sh
python3 <sdk>/assets.py manifest.toml build/<id>.rvx > assets.json
```

A later record for the same id and asset replaces an earlier one.

| Key | Contents |
|---|---|
| `id` | The extension's id; `reprise` is the reserved owner id of the core's own bitmaps |
| `asset` | The asset's name in the manifest |
| `menu` | `true` for a Settings icon (`menu = true` in the manifest): a 78 x 121 coverage mask |
| `format`, `depth` | The BMap format word and bits per pixel as the extension ships it: 8 Mask8, 0x64 Indexed8, 0x1888 BGRA8888 |
| `width`, `height`, `stride` | Pixel size and row bytes |
| `palette_colors` | Palette entries, 0 without one |
| `bytes` | The decoded BMap, header included |
| `preload` | `always`, `optional` or `never`: whether the core loads it with the extension's main module (`preload` in the manifest) |

`rvt catalog list --extensions [--group <id>]` prints one line per bitmap, and `rvt catalog show <id>/<asset>` prints every field and the two hashes. [Extension bitmaps](extension-bitmaps.md) is the same list as a page.

### Regenerating

`make` at the repository root writes `build/assets.json` with the core's and every built extension's bitmaps. Then, from the `themes/` directory:

```sh
python3 tools/extensions_catalog.py       # rvt/data/extensions.json, by id then asset
python3 tools/extension_bitmaps_doc.py    # docs/extension-bitmaps.md
```

Regenerate when an extension adds, removes or resizes a bitmap asset. `tests/test_assets.py` checks that the committed catalog matches the build and the committed page matches the generator.
