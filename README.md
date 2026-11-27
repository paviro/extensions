# RepriseOS extensions

This repository hosts the source for these RepriseOS extensions and accepts proposals for the extension catalog.

- [Balance](extensions/balance/) adjusts the volume of the left and right channels independently of the main volume.
- [Night Shift](extensions/night-shift/) warms the display manually or automatically from sunset to sunrise.
- [Playback Log](extensions/playback-log/) records listening history for scrobbling and optional detailed reports.
- [Album Artists](extensions/album-artists/) lists the library by Album Artist, with the Artist as the fallback.
- [File Info](extensions/file-info/) shows an item's tags and file properties from its context menu.
- [Playback Queue](extensions/playback-queue/) adds Play Next, Play Last and an Up Next screen to edit the queue.
- [Crossfeed](extensions/crossfeed/) blends a little of each stereo channel into the other for headphone listening. It uses the included [DSP library](extensions/dsp/).
- [Equalizer](extensions/equalizer/) runs the EQ presets without distortion and adds a Custom preset with five bands. It uses the included [DSP library](extensions/dsp/).
- [Clicker Modes](extensions/clicker-modes/) lets Settings > Clicker choose the speaker, the headphones, both or off.
- [iAP Auth Bypass](extensions/iap-auth-bypass/) lets accessories use authentication-gated iAP commands without an Apple authentication chip.
- [EU Volume Limit](extensions/eu-volume-limit/) gives an EU iPod the volume behaviour of a non-EU one when its limit switch is off.
- [Homebrew](extensions/homebrew/) lists apps and games from `/Reprise/Apps` and `/Reprise/Games` under Extras and starts them without an Apple signature.

To add your extension to the catalog, open a PR following the [submission guide](catalog/README.md). Later releases are picked up automatically.
