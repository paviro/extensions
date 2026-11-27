# Appearance

Derives from the core's former Dark Mode (payload/dark_mode.c, 01c89f6) by lgreitis and Alex Ball (alexballvideos-netizen). The Classic theme's dark background is the core's former themes/dark/background-dark-image.png by lgreitis.

Adds Settings > Appearance, after Brightness, with five rows:

| Row | Does |
|---|---|
| Theme | replaces colours and artwork |
| Accent | replaces the stock blue in the selection bar, the accent artwork and accent labels |
| Dark Mode | turns the look dark: Auto, On or Off |
| Menu Covers | shows the highlighted row's cover in the Main and Music menus; its Theme value follows the look, which is on for Classic |
| Now Playing Cover | draws the Now Playing cover tilted with its reflection, or flat; its Theme value follows the look, which is tilted for Classic |

Other extensions can add rows to the menu after any of these (their ids are `theme`, `accent`, `dark-mode`, `menu-covers` and `now-playing-cover`); Night Shift's row follows Dark Mode.

Themes are `<id>.rvt` files in `/Reprise/Themes/`, built with the compiler in this repository's [themes directory](../../themes/README.md), which also holds the Classic theme. The Theme row lists Classic and up to 64 themes by name, rescanned each time it opens. A theme with the id `classic` is ignored.

A theme may have a light appearance, a dark one or both. With only one, Dark Mode shows that one, its row value says which, and the pane says "Theme only supports dark mode" or "Theme only supports light mode". If the chosen theme's file is missing, Classic shows and Theme says "Theme unavailable"; the choice comes back with the file.

A theme that cannot be read leaves the look unchanged. One that fails part way through is undone to the stock light look, and the row the action came from says so. If the stock artwork cannot be restored from `/Reprise/Firmware/reprise.bin`, the Appearance row disappears until the next boot. Presses queued during a switch cannot reverse it. Turning the extension off restores the stock artwork.

## Auto

Dark Mode's Auto follows sunset and sunrise in the city set under Settings > Date & Time > Time Zone, by the iPod's clock, checked once a minute: dark from the sunset minute to the sunrise minute. Without a time zone, Auto is light and says "Set a Time Zone".

A theme with one appearance turns Dark Mode's Auto into the value it shows, as it does On and Off.

## Colour roles

| Role | Classic light | Classic dark | Draws |
|---|---|---|---|
| background | `#ffffff` | `#181818` | surfaces, the light greys of the stock screens |
| text | `#202020` | `#e8e8e8` | labels and the dark greys |
| accent_fill | `#236cc5` | `#00a1ff` | accent fills and the artwork that follows the accent |
| accent_text | `#3185d0` | `#00a1ff` | accent-coloured labels |
| on_accent | `#ffffff` | `#ffffff` | labels on the selection bar and on accent fills |
| selection | `#236cc5` | `#00a1ff` | the selection bar |
| cover_border | `#7e8183` | `#383838` | the Cover Flow border |
| icon_tint | `#3185d0` | `#00a1ff` | under a theme, extension icons and the artwork the theme tints with it |

The accents are Blue, Teal, Green, Yellow, Orange, Red, Pink, Purple and Graphite. Theme Default keeps the look's own, and a theme may give each accent its own palette. A theme can replace extension bitmaps in Settings panes and canvases; the rest keep the extension's art. With `flat_icons`, Settings icons are drawn without their reflection.

| | |
|---|---|
| Default | off |
| Memory | idle: the main module with the bitmap catalog and its state; Dark Mode on Auto: one timer; a look on: an index entry per bitmap the theme holds for extensions and the draw hook blocks; during a switch: the switch module; during a switch: one scratch block; the Theme list open: one entry per theme |
| Settings | theme id, Accent, Dark Mode, Menu Covers, Now Playing Cover, and `available`, the hidden bool that shows the Appearance row. The first load imports the core's former Dark Mode switch |
