# Extension bitmaps

The first-party extension bitmaps a theme can replace with
`<appearance>/extensions/<id>/<asset>.png`, or `<appearance>/reprise/<asset>.png`
for the core's (id `reprise`). See [Authoring](authoring.md#extension-bitmaps).
Size is width x height in pixels, which the art must match. Menu marks a
Settings icon, a 78 x 121 coverage mask drawn in the icon tint.

The catalog and how to regenerate it are in
[Catalog](catalog.md#extension-asset-catalog).

Regenerate this page with `python3 tools/extension_bitmaps_doc.py`.

| Id | Asset | Size | Format | Palette | Menu |
|---|---|---|---|---|---|
| `appearance` | `settings-accent` | 78x121 | mask8 | - | yes |
| `appearance` | `settings-appearance` | 78x121 | mask8 | - | yes |
| `appearance` | `settings-cover` | 78x121 | mask8 | - | yes |
| `appearance` | `settings-covers` | 78x121 | mask8 | - | yes |
| `appearance` | `settings-moon` | 78x121 | mask8 | - | yes |
| `appearance` | `settings-theme` | 78x121 | mask8 | - | yes |
| `balance` | `settings-balance` | 78x121 | mask8 | - | yes |
| `crossfeed` | `settings-crossfeed` | 78x121 | mask8 | - | yes |
| `equalizer` | `settings-eq` | 78x121 | mask8 | - | yes |
| `homebrew` | `apps` | 142x87 | bgra8888 | - | no |
| `night-shift` | `settings-night-shift` | 78x121 | mask8 | - | yes |
| `playback-log` | `settings-playback-log` | 78x121 | mask8 | - | yes |
| `reprise` | `settings-info` | 78x121 | mask8 | - | yes |
| `sound-menu` | `settings-sound` | 78x121 | mask8 | - | yes |
