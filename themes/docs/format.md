# File format

A theme is one file, `/Reprise/Themes/<id>.rvt`: a header, then optional light and dark appearance packs. [`rvt/data/rvt.h`](../rvt/data/rvt.h) is the contract. The Appearance extension vendors it byte for byte, and `rvt/rvtfile.py` parses every `RVT_*` value from it. Integers are little-endian.

## Header

The offsets are the `RVT_HEADER_*` defines.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `"RVT0"` |
| 4 | u16 | Version, `RVT_VERSION` (3); a reader accepts exactly this value |
| 6 | u16 | Header bytes, where the packs begin |
| 8 | 8 | Catalog fingerprint, see [Catalog](catalog.md) |
| 16 | u32 | Appearances present: `RVT_APPEARANCE_LIGHT`, `RVT_APPEARANCE_DARK` |
| 20 | u32 x 4 | Light offset and bytes, dark offset and bytes; 0 when absent |
| 36 | u32 | CRC-32 of the first `RVT_HEADER_BYTES` (128) bytes with this word as zero |
| 40 | 32 | Id, `[a-z0-9][a-z0-9_-]{0,30}`, NUL-padded; the file's stem |
| 72 | 48 | Name, UTF-8, NUL-padded |
| 120 | 8 | Zero, up to `RVT_HEADER_BYTES`; a reader does not check them |

The light pack starts at the header bytes, and the dark pack follows it, 4-byte aligned. A pack's bytes include its header. A reader places the packs by the header bytes field, not the constant, and skips fields it does not know in a longer header. The CRC always covers the first 128 bytes only.

## Pack

The offsets are the `RVT_PACK_*` defines.

| Offset | Size | Field |
|---|---|---|
| 0 | u32 | Body bytes, at least `RVT_STYLE_BYTES` |
| 4 | u32 | Record count |
| 8 | u32 | CRC-32 of the body |
| 12 | u32 | Extension bitmap count, at most `RVT_EXTENSION_BITMAPS_MAX` (64) |
| 16 | | Body: the style prefix, the records, then the extension bitmaps |

A pack is at least `RVT_PACK_HEADER_BYTES` (16) plus `RVT_STYLE_BYTES` long.

## Style prefix

`RVT_STYLE_BYTES` (856), at the `RVT_STYLE_*` offsets. Every colour is R, G, B, A with A = 255.

| Offset | Size | Field |
|---|---|---|
| 0 | 8 x 4 | Colour roles in `enum rvt_role` order: background, text, accent fill, accent text, on accent, selection, cover border, icon tint |
| 32 | u32 | Rendering flags: `RVT_FLAG_FLAT_SELECTION`, `RVT_FLAG_MENU_COVERS_OFF`, `RVT_FLAG_FLAT_NOW_PLAYING`, `RVT_FLAG_FLAT_ICONS` |
| 36 | 9 x 5 x 4 | One palette per accent in `enum rvt_accent` order, each in `enum rvt_accent_slot` order: accent fill, accent text, selection, on accent, icon tint; a slot replaces the role of its name while that accent is picked |
| 216 | `RVT_CATALOG_MAX` | One tint byte per catalog index: `RVT_TINT_NONE`, or `RVT_TINT_ROLE` plus an `rvt_role`, or-ed with `RVT_TINT_DARKNESS` when the coverage comes from darkness or with `RVT_TINT_SOLID` when every level from `RVT_SOLID_FLOOR` up is full, never both. A reader packs the roles two per byte, so `RVT_TINT_ROLE + RVT_ROLES` stays at most 16 |

## Records

Records follow in ascending catalog index, each index at most once:

| Size | Field |
|---|---|
| u32 | Catalog index |
| u32 | Kind, `enum rvt_kind` |
| u32 | Payload bytes |
| bytes | Payload, zero-padded to 4 |

| Kind | Payload | The iPod writes |
|---|---|---|
| `pixels` | The native bytes from `data_offset` to the end: palette and rows | them in place |
| `pixels_lz` | The same bytes as an lz stream; the payload bytes are the stream's size | them in place while decoding |
| `recolor` | u32 count, then count × (`from` RGBA, `to` RGBA), strictly ascending by `from` as a little-endian u32, no `from` equal to its `to`; count 1..`RVT_RECOLOR_MAX` | each palette entry, BGRA8888 pixel or RGB565 word equal to a `from` as its `to` |
| `alias` | u32 source catalog index | the source's bytes after its header when the layouts are equal, else its pixels re-encoded in this bitmap's format; after every other record of the pack |
| `invert` | none | RGB inverted, alpha kept |
| `relight` | `RVT_RELIGHT_WORDS` × i32, 8.8 fixed point, `enum rvt_relight` order | grays per [Relight](#relight) below, a palette of the distinct grays in ascending order with zeros after it, and the rows re-indexed |

The pixel kinds replace the stock bytes. The other kinds are computed from them, so the iPod restores a bitmap from its firmware before deriving it. The compiler writes `pixels_lz` when the stream is smaller than the raw bytes.

**lz stream.** The layout is the core's (`rvx-sdk/rvx.h`, summarised in `rvt.h`). Each group is a flag byte and up to 8 items, bit 0 first. A clear bit is a literal byte. A set bit is a two-byte match `LLLLDDDD DDDDDDDD` copying L + 3 bytes from D + 1 bytes back in the output; when L = 15, following bytes are added to the length while they are 255, plus the first that is not. Matches copy from the output itself. The stream has no length field: a decoder stops after the bitmap's bytes (catalog size minus `data_offset`), and the stream must end exactly at the record's end. A stream that ends early, overruns the output or copies from before its start makes the record invalid. The palette-index check and the pack CRC apply to the stored bytes.

**Tint bytes.** On a paletted or BGRA8888 bitmap without a record, a tint byte tints the stock bitmap with the coverage its `RVT_TINT_DARKNESS` or `RVT_TINT_SOLID` bit names. With a pixel record, the coverage is the record's alpha. A tint byte on a derived bitmap is invalid. On a mask, a tint byte needs no record and keeps the stock levels unless it carries `RVT_TINT_SOLID`, which rewrites them (a 4-bit level counts as level x 17). RGB565 bitmaps take no tint.

**Derived records.** `recolor` on RGB565 uses opaque, 5-6-5-exact colours. `invert` and `alias` are not for masks. `relight` needs a paletted bitmap of at most `RVT_SCRATCH_PIXELS` pixels with a palette of at least `icon_hi − bg_lo + 1` entries. A pack has at most `RVT_ALIASES_MAX` aliases, and an alias source is neither an alias nor tinted. An alias between equal layouts is a byte copy. Between differing layouts the iPod re-encodes the source's pixels, for bitmaps of at most `RVT_SCRATCH_PIXELS` pixels. If the source has more colours than a paletted target holds, the alias fails and the stock bitmap stays.

## Relight

Every word is 8.8 fixed point; `luma = (77 r + 150 g + 29 b) >> 8`. The iPod rejects the record unless `0 <= t_zero < t_full <= 255`, `0 <= bg_lo <= bg_hi <= icon_lo <= icon_hi <= 255`, `|bg_base|` and `|icon_base|` are at most 1024, `|bg_ref|`, `|icon_luma|` and `|icon_row|` at most 255, and `|bg_slope|` at most 16, in pixel units before scaling.

For each row, the stock pixel in column 0 is the background `bg`. A pixel's offset `o` is its largest channel difference from `bg`, and `t = clamp((o - t_zero) / (t_full - t_zero), 0, 1)` is how much of it is icon. A pixel with `0 < t < 1` and no 8-neighbour at `t = 1` is a drop shadow and takes `t = 0`. Then:

- `bg_dark = clamp(bg_base + (luma(bg) - bg_ref) * bg_slope, bg_lo, bg_hi)`
- `icon_dark = clamp(icon_base + icon_luma * luma(c) + icon_row * y, icon_lo, icon_hi)`
- the pixel is `bg_dark + (icon_dark - bg_dark) * t`, rounded, opaque.

The palette becomes the distinct grays in ascending order with zeros after them, and the rows are re-indexed. `rvt.derive` is the reference; the iPod's division must match it byte for byte.

`from_other` art ([Authoring](authoring.md#derived-art)) is computed by the compiler and stored as a pixel record. One pack never refers into the other.

## Extension bitmaps

The extension bitmaps ([Authoring](authoring.md#extension-bitmaps)) follow the last record, ascending by owner then asset, each pair once, at most `RVT_EXTENSION_BITMAPS_MAX` (64) per pack. Each has a `RVT_EXTENSION_BITMAP_BYTES` (24) header:

| Size | Field |
|---|---|
| u32 | Owner: FNV-1a of the extension id; the core's own bitmaps are owned by the hash of `reprise`, the reserved owner id of the core in the theme format |
| u32 | Asset: FNV-1a of the asset name |
| u32 | Kind, `enum rvt_extension_kind`: `RVT_EXTENSION_BITMAP` (1) the bitmap raw, `RVT_EXTENSION_BITMAP_LZ` (2) the bitmap as one lz stream |
| u32 | Stored bytes: the payload's length |
| u32 | Decoded bytes: the bitmap's length |
| u32 | Tint: `RVT_TINT_NONE` (0), or `RVT_TINT_ROLE` plus an `rvt_role` without coverage bits for a picture the reader paints in that role |
| bytes | Payload, zero-padded to 4 |

A bitmap is the whole BMap as the iPod decodes it: the `RVT_BMAP_HEADER_BYTES` (28) header (format, reserved, stride, depth, bounds top, left, bottom, right, payload bytes), then the palette and rows. The payload is the bitmap raw (stored equals decoded) or one lz stream that decodes to exactly the decoded bytes and is at most those plus one flag byte per 8. The lz stream is the same as in `pixels_lz`, and the compiler uses it when it is smaller.

The decoded bytes are at most `RVT_EXTENSION_BITMAP_MAX_BYTES` (65,536) and equal the header plus its payload bytes. A reader checks the header as the core does and rejects the bitmap unless the bounds are in order and within 16 bits, the depth is at most 32, a row fits the stride, and an Indexed8 palette of at most 256 entries and the rows fit the payload.

A Settings icon has the menu header: Mask8 (8), stride 80, depth 8, bounds 0, 0, 121, 78, 9,680 payload bytes. A tinted bitmap is an Indexed8 or BGRA8888 picture, never a Settings icon. Its palette entries or pixels carry the coverage in their alpha with a placeholder colour, which the reader replaces with the role's colour at each decode.

The hashes are the core's FNV-1a of the extension id and asset name (`rvx-sdk/rvx.py`, `fnv1a`). The pack CRC covers the extension bitmaps with the records.
