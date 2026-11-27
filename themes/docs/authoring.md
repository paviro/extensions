# Authoring a theme

A theme is a folder compiled into one `.rvt` file. It sets colours, a palette per accent and a few rendering options, and replaces Apple's bitmaps with same-size art or with rules that derive them. Keys in `theme.toml` and the rule files use American spelling (`colors`, `recolor`).

## Quickstart

1. `python3 -m rvt new mytheme` writes `mytheme/theme.toml` with every role and option commented out, and empty `light/` and `dark/` folders, each with `apple/`, `reprise/` and `extensions/`. Run `python3 -m rvt` from the `themes/` directory.
2. Edit `theme.toml`. Put art in `light/apple/` and `dark/apple/`, named after the [bitmaps](bitmaps.md) it replaces, and [extension bitmaps](#extension-bitmaps) in `light/extensions/<id>/` and `dark/extensions/<id>/`.
3. `python3 -m rvt check mytheme` prints what each rule matched, the warnings and a size table. It writes nothing.
4. `python3 -m rvt compile mytheme` writes `dist/mytheme/mytheme.rvt` and `report.json`. `--strict` turns warnings into errors.
5. Copy `mytheme.rvt` to `/Reprise/Themes/` on the iPod. It appears under Settings > Appearance > Theme.

The iPod derives the theme's art from `/Reprise/Firmware/reprise.bin`, so that file must be the build that is running; a mismatch leaves the look unchanged and the Theme row reports it.

```
mytheme/
  theme.toml
  light/
    apple/          art for the light appearance: a PNG per replaced Apple bitmap, a TOML per derived one
    extensions/     one folder per extension id, with a PNG per bitmap asset it replaces
    reprise/        a PNG per bitmap of the core it replaces; `reprise` is the reserved owner id of the core's own bitmaps
  dark/             the same for dark
```

A file directly under `light/` or `dark/` is an error.

## theme.toml

```toml
schema = 1
id = "mytheme"          # the file's stem; "classic" is the built-in theme
name = "My Theme"       # shown in Settings
author = "..."          # optional, report only
version = "1.0"         # optional, report only

[light]
menu_covers_default = true  # the Menu Covers setting until the listener chooses one

[light.colors]          # any subset of the roles below, "#rrggbb"
accent_fill = "#0071e3"
icon_tint = "#0071e3"   # left out, it follows accent_text

[light.accents]         # one colour per accent, or a table; see Accents
blue = "#007aff"
yellow = { accent = "#ffcc00", on_accent = "#000000" }

[light.rendering]
flat_selection = false
flat_now_playing = false

[light.tint]            # role = [catalog names, globs or id/asset]; see Tint
icon_tint = ["Settings_EQ_*_Image", "Media_*Proxy_Image"]
text = ["StatusBar*_Battery_Image_[3-9]", "StatusBar*_Battery_Image_[12][0-9]"]
darkness_coverage = ["NowPlaying_Blue_Dot_Image"]
solid_coverage = ["Settings_AppleLogo_Image"]

[light.derive]          # rules for derived art; see Derived art
"Settings_Brightness_Image" = { kind = "invert" }

[dark]                  # the same tables; an empty table is Classic dark colours and no art
```

Declare `[light]`, `[dark]` or both. For a missing appearance the iPod keeps Classic, and turning Dark Mode on with a light-only theme says the theme has no dark appearance. Unknown keys are rejected. `fonts`, `layouts`, `metrics`, `animations`, `sounds`, `strings` and `wallpapers` are rejected as unsupported.

### Colour roles

Roles left out keep their Classic value, except `icon_tint`, which follows `accent_text`.

| Role | Light | Dark | What it colours |
|---|---|---|---|
| `background` | `#ffffff` | `#181818` | Native surfaces; labels on light art |
| `text` | `#202020` | `#e8e8e8` | Native text; labels on dark art |
| `accent_fill` | `#236cc5` | `#00a1ff` | Native accent fills; stock progress, dot, star and playback art the theme leaves stock |
| `accent_text` | `#3185d0` | `#00a1ff` | Native accent text, the Search caret |
| `on_accent` | `#ffffff` | `#ffffff` | Selected-row text and tinted icons on the selection bar, the highlighted Search key, the selected calendar day |
| `selection` | `#236cc5` | `#00a1ff` | The list selection bar; stock selection and focus art the theme leaves stock |
| `cover_border` | `#7e8183` | `#383838` | Cover Flow border |
| `icon_tint` | `#3185d0` | `#00a1ff` | Tinted icons: the Settings icons of extensions and the bitmaps `[tint]` lists under it |

Keep `on_accent` readable on both `accent_fill` and `selection`. `report.json` lists the contrast of each text and background pair for Theme Default and every accent, and `rvt check` warns about pairs the theme colours that fall below 4.5:1.

Labels Apple draws in a fixed colour over art take `text` or `background`, whichever differs more in brightness from the centre pixel of the art behind them:

| Label | Art |
|---|---|
| Stopwatch and Charging header title | `NowPlaying_StatusBar_Image` |
| Screen Lock and Volume Limit combination digits | `LockScreen_TumblerBgnd_Image` |
| Date and time picker values | `DateTimePicker_Well` |
| Cover Flow album title and artist (track list) | `CoverFlow_Backside_Top_Image` |
| Calendar days of the shown month | `Calendar_Active_Cell` |

Keep `LockScreen_TumblerFocus_Image` and `DateTimePicker_Selection` about as light or dark as the art above. The focused digit and selected value use the same colour as the rest.

Calendar days of neighbouring months follow the rule against `Calendar_Inactive_Cell`, mixed halfway toward the cell. Weekday names follow it against `background`, as does a cell with a transparent centre. The selected day and its tinted markers use `on_accent` on `Calendar_Hilite_Cell`.

The Search letter strip and option bars (such as the Clock's Add/Edit bar) follow the rule against the well art (`OptionBar_*_Well_Center_Image`). The highlighted letter uses `on_accent` on the thumb, so keep it readable there. The shuffle selector on Now Playing follows the same rule against its own well under a theme and in Classic's dark appearance, whose art repaints that well. Its thumb does not follow the accent, so in Classic the highlighted label keeps its stock colour. Text typed into the Search box follows the rule against `System_Input_Field_Middle_Image`, and its caret uses `accent_text`.

Apart from that, Classic keeps the stock colours of all these labels.

### Accents

The accents are `blue`, `purple`, `pink`, `red`, `orange`, `green`, `teal`, `yellow` and `graphite`. Each has a built-in fill (`accent_fill`, `selection`) and text colour (`accent_text`, `icon_tint`) per appearance, and uses the appearance's `on_accent`. Blue is Classic's. In `[light.accents]` an accent takes one of these forms:

| Form | Sets |
|---|---|
| `blue = "#007aff"` | `accent_fill`, `accent_text`, `selection` and `icon_tint` to that colour |
| `yellow = { accent = "#ffcc00", on_accent = "#000000" }` | the same four, and `on_accent` |
| `teal = { accent_fill = "...", accent_text = "...", selection = "...", on_accent = "...", icon_tint = "..." }` | any subset of the five palette slots, each named after the role it replaces |

Accents left out keep the built-in colours. `report.json` lists them under `accents_builtin`.

### Menu Covers

`menu_covers_default`, directly in `[light]` or `[dark]`, sets whether the Main and Music menus show the highlighted row's cover until the listener changes Menu Covers. The default is `true`.

### Rendering

| Key | Default | `true` / `false` |
|---|---|---|
| `flat_selection` | `false` | `true`: `selection` throughout the bar. `false`: a gradient ending at `selection`, starting at `selection` lightened 25 % toward white, with a top line lightened 55 % |
| `flat_now_playing` | `false` | `true`: the cover face-on, no reflection; Cover Flow is unchanged. `false`: the tilted cover with its reflection. The flag holds until the listener changes Now Playing Cover |
| `flat_icons` | `false` | `true`: the Settings icons of [extensions](#extension-bitmaps) the theme does not replace are drawn without their reflection. `false`: they keep it |

Stock blue art for selection, focus and progress that the theme leaves stock is recoloured with `selection` or `accent_fill`, keeping its shading. This covers option bar thumbs, the active button, picker and calendar highlights, the Settings checkmark, the progress and Genius sliders, new-item dots, rating stars and playback glyphs. Replaced art is not recoloured.

Screens RepriseOS draws itself mix their greys from `background` toward `text`, draw the highlighted row or the cell being edited on the lists' selection bar with `on_accent` text, tint the selected cell from `background` toward `accent_text`, and use `accent_text` for accent text.

## Art

A file in `light/apple/` or `dark/apple/` is named after a catalog bitmap. `Settings_About_Band_Image.png` replaces its pixels; `Settings_About_Band_Image.toml` [derives](#derived-art) them from the stock bitmap. A name the catalog lists more than once is given as its hex id, such as `light/apple/0x0DAD012A.png`. Every file must match a catalog bitmap, once per appearance.

Art must be the bitmap's exact native size. The compiler converts it to the native format:

| Format | Art |
|---|---|
| Masks (4 and 8) | Opaque grayscale; 4-bit masks round to 16 levels |
| Paletted (0x64, 0x65) | Any RGBA; extra colours are reduced to fit the native palette, with a warning (`palette_reduced` in `report.json`) |
| RGB565 | Opaque |
| BGRA8888 (0x1888) | Any RGBA |

### Tint

`[light.tint]` draws bitmaps in a colour role, following the picked accent. Each key is a role listing catalog names, hex ids, case-sensitive globs (`*`, `?`, `[...]`) or extension bitmaps as `<id>/<asset>` ([below](#extension-bitmaps)). A glob that matches nothing is a warning; an unknown name is an error. A bitmap may appear under one role only.

Coverage comes from the PNG if there is one (its alpha, else its gray level; coloured opaque art is rejected), otherwise from the stock bitmap. Tinted art with more coverage levels than the palette holds is reduced on coverage alone.

| Format | Needs art | How it is tinted |
|---|---|---|
| Masks | No | At draw time |
| Paletted | No | Palette entries rewritten, alpha kept |
| BGRA8888 | No | Pixels rewritten, alpha kept |
| RGB565 | Cannot be tinted | |

`darkness_coverage` lists tinted bitmaps whose coverage is their darkness instead of their alpha, for stock glyphs drawn black on transparent. `solid_coverage` lists tinted bitmaps drawn flat: coverage levels from 150 of 255 up become full and lower levels stay, for stock masks with gloss in their levels, such as the Apple logo. A bitmap can be in only one of the two, and both apply to stock bitmaps only, not to a PNG. A tinted bitmap cannot also be derived.

### Derived art

A rule describes a bitmap as a change to the stock bitmap, which the iPod computes from its firmware when the theme is applied. A rule is either an entry of `[dark.derive]`, keyed by catalog name, hex id or glob, or a file `dark/apple/<CatalogName>.toml` with the same table. The file suits a long recolour map. A `[derive]` rule must match at least one bitmap, none with a PNG or `.toml`, and no bitmap matched by another rule. A rule has a `kind` and that kind's keys:

| `kind` | Keys | Formats | Result |
|---|---|---|---|
| `recolor` | `[map]`: `"#rrggbbaa" = "#rrggbbaa"`, a stock colour and its replacement; keys must be quoted | paletted, BGRA8888, RGB565 | Each palette entry, pixel or RGB565 word equal to a stock colour in the map takes its replacement; the rest stay |
| `alias` | `source`: a catalog name or hex id | any but masks, the same size | The pixels of `source` (the theme's version if it has one, else stock), re-encoded when the layouts differ; a paletted target must have room for the source's colours |
| `invert` | | any but masks | RGB inverted, alpha kept |
| `relight` | the rule's values, all optional, below | paletted | A light icon on a light square redrawn as grays on a dark one |

Any kind may add `from_other = true` to start from the theme's PNG for that bitmap in the other appearance instead of the stock bitmap. The compiler computes and stores the result. That PNG must exist and must not be tinted. An alias with `from_other` takes the other appearance's PNG of `source`, or of the bitmap itself when `source` is left out.

```toml
# theme.toml
[dark.derive]
"Clock_*_Image_*" = { kind = "alias", from_other = true }      # every digit reuses its light art
"Settings_Brightness_Image" = { kind = "invert", from_other = true }
"NowPlaying_White_Pause_Image" = { kind = "alias", source = "NowPlaying_Black_Pause_Image" }
"Media_*Proxy_Image" = { kind = "relight" }

# dark/apple/System_ScrollbarCenter_Image.toml
kind = "recolor"
[map]
"#ffffff26" = "#91919126"
"#00000073" = "#7e7e7e73"
```

On RGB565 both recolour colours are opaque, with `r` and `b` multiples of 8 and `g` a multiple of 4. An alias may not point at another alias or a tinted bitmap. Aliases with `from_other` do not count against the alias [limit](#limits).

#### Relight keys

The keys are `t_zero` and `t_full` (the offsets from the row's background at which a pixel counts as no icon and as all icon), `bg_base`, `bg_ref`, `bg_slope`, `bg_lo`, `bg_hi`, `icon_base`, `icon_luma`, `icon_row`, `icon_lo` and `icon_hi`. [File format](format.md#relight) gives the arithmetic. The defaults are Classic's: `t_zero = 3`, `t_full = 14`, `bg_base = 25`, `bg_ref = 216`, `bg_slope = 0.357` (stored as 91/256), `bg_lo = 24`, `bg_hi = 35`, `icon_base = 394.6`, `icon_luma = -1.170`, `icon_row = -1.769`, `icon_lo = 40`, `icon_hi = 160`.

Values are rounded to 1/256 and must satisfy `0 <= t_zero < t_full <= 255`, `0 <= bg_lo <= bg_hi <= icon_lo <= icon_hi <= 255`, `bg_base` and `icon_base` within ±1024, `bg_ref`, `icon_luma` and `icon_row` within ±255 and `bg_slope` within ±16. The bitmap must have at most 3,840 pixels and a palette of at least `icon_hi - bg_lo + 1` entries.

#### Recolour rule from a PNG

With a decrypted firmware:

```sh
python3 -m rvt recolor mytheme/dark/apple/Foo.png --firmware osos.bin
```

writes `dark/apple/Foo.toml` when the PNG is a pure recolour of the stock bitmap, ignoring small dithering and stray pixels. Otherwise it lists the stock colours painted in more than one colour. Delete the PNG to use the rule.

#### Previewing derived art

```sh
python3 -m rvt preview mytheme --firmware osos.bin
```

renders every derived bitmap to `dist/mytheme/derived/<appearance>/<CatalogName>.png`, writes `derived.json` with the address, size and CRC-32 of each, and warns about map colours that match nothing.

## Extension bitmaps

Extensions ship bitmaps as assets: the 78 x 121 Settings icon of their row and any pictures their screens draw. `light/extensions/<id>/<asset>.png` replaces asset `<asset>` of extension `<id>` in light, `dark/extensions/` in dark. The core's id is `reprise`, the reserved owner id of the core's own bitmaps in the theme format, and its bitmaps go in `light/reprise/` and `dark/reprise/`. The extension asset [catalog](catalog.md#extension-asset-catalog) lists which bitmaps exist:

```sh
python3 -m rvt catalog list --extensions                  # id, asset, size, format, palette, menu
python3 -m rvt catalog list --extensions --group appearance
python3 -m rvt catalog show appearance/settings-moon
```

[Extension bitmaps](extension-bitmaps.md) is the same list as a page. For an extension outside the built-in catalog, pass its `assets.json` to `check`, `compile` and `inspect` with `--extension` (repeatable). Its author makes the file with `python3 <sdk>/assets.py manifest.toml file.rvx > assets.json`. The compiler encodes the art as the extension's build would:

| Bitmap | Art | Stored as |
|---|---|---|
| Settings icon (`menu` in the catalog) | Exactly 78 x 121; coverage is the alpha if any pixel is translucent, else the gray level; coloured opaque art is rejected | A Mask8 coverage mask drawn in the icon tint |
| Any other bitmap | Exactly the catalog's width and height, any RGBA | Indexed8 with a palette when the art has at most 256 colours, else BGRA8888; at most 65,536 bytes with its header |

A `[tint]` role can list a picture as `<id>/<asset>` (`reprise/<asset>` for the core). `icon_tint = ["homebrew/apps"]` reads `extensions/homebrew/apps.png` as coverage, like a Settings icon, and draws it in that role. The PNG is needed in every appearance that lists it, `darkness_coverage` and `solid_coverage` do not apply, and Settings icons cannot be listed because they are already drawn in the icon tint.

An unknown (id, asset) compiles only as a Settings icon, from a 78 x 121 coverage PNG, with a warning. Any other size is an error asking for `--extension`. The iPod ignores bitmaps no installed extension uses.

A Settings icon's art box is rows 12..62, centred on column 39. Stock icons hold their reflection from row 79 down. With `flat_icons` ([Rendering](#rendering)) off, an icon with nothing below row 78 is a warning, because the icons the theme leaves stock keep their reflection. Icons are coverage in both appearances, so one shape goes into both folders.

## Limits

| What | Limit |
|---|---|
| `id` | `[a-z0-9][a-z0-9_-]{0,30}`; `classic` is reserved |
| `name` | at most 47 UTF-8 bytes, no control characters |
| Art size | the bitmap's native width and height, nothing else; Settings icons 78 x 121, other extension bitmaps the catalog's size |
| Extension bitmaps | at most 64 per appearance, each at most 65,536 bytes encoded; ids `[a-z][a-z0-9_-]{0,31}`, assets `[a-z0-9_-]{1,31}`, no two with equal hashes; a tinted one is a picture, not a Settings icon |
| Art files | non-interlaced PNG with 8-bit channels or a palette, at most 16 MiB; plain file names inside the folder, no symlinks that leave it |
| `theme.toml`, rule files | at most 256 KiB each |
| Palette per bitmap | the native palette's entries (see [Bitmaps](bitmaps.md)); more colours are reduced with a warning |
| Recolour map | at most 512 pairs, and no more than the bitmap has palette entries or pixels |
| Aliases of stock art | at most 16 per appearance; an alias between differing layouts is re-encoded on the iPod and allowed for bitmaps of at most 3,840 pixels |
| Relight | bitmaps of at most 3,840 pixels; the bounds above |
| Colours | opaque `#rrggbb`; translucent roles are not supported |

## Unsupported

Fonts, layouts, metrics, animations, sounds, strings, wallpapers, new bitmaps, bitmaps of another size, translucent colours and appearances other than light and dark.
