# RepriseOS extensions

This repository hosts the source for these RepriseOS extensions and accepts proposals for the extension catalog.

- [Appearance](extensions/appearance/) adds Dark Mode with Auto, accent colours and themes from `/Reprise/Themes` that recolour the screens and replace the artwork.
- [Balance](extensions/balance/) adjusts the volume of the left and right channels independently of the main volume.
- [Night Shift](extensions/night-shift/) warms the display manually or automatically from sunset to sunrise; its row joins the Appearance menu when that extension runs.
- [Playback Log](extensions/playback-log/) records listening history for scrobbling and optional detailed reports.
- [Album Artists](extensions/album-artists/) lists the library by Album Artist, with the Artist as the fallback.
- [File Info](extensions/file-info/) shows an item's tags and file properties from its context menu.
- [Playback Queue](extensions/playback-queue/) adds Play Next, Play Last and an Up Next screen to edit the queue.
- [Crossfeed](extensions/crossfeed/) blends a little of each stereo channel into the other for headphone listening. It uses the included [DSP library](extensions/dsp/).
- [Equalizer](extensions/equalizer/) runs the EQ presets without distortion and adds a Custom preset with five bands. It uses the included [DSP library](extensions/dsp/).
- [Channels](extensions/channels/) narrows or widens the stereo image, or plays mono, one channel in both ears, karaoke or swapped channels.
- [Compressor](extensions/compressor/) turns loud passages down so quiet ones stay audible. It uses the included [DSP library](extensions/dsp/).
- [Surround](extensions/surround/) widens the stage for headphones by delaying the right channel outside a band that stays in place. It uses the included [DSP library](extensions/dsp/).
- [Bass Enhancement](extensions/bass-enhancement/) fills out the bass and clears the mids with five fixed filters at a chosen strength. It uses the included [DSP library](extensions/dsp/).
- [Fatigue Reduction](extensions/fatigue-reduction/) takes the edge off the upper mids and treble in three strengths for long listening. It uses the included [DSP library](extensions/dsp/).
- [Sound Menu](extensions/sound-menu/) adds Settings > Sound with Apple's Volume Limit, EQ and Sound Check rows and the audio extensions' rows after them.
- [Clicker Modes](extensions/clicker-modes/) lets Settings > Clicker choose the speaker, the headphones, both or off.
- [iAP Auth Bypass](extensions/iap-auth-bypass/) lets accessories use authentication-gated iAP commands without an Apple authentication chip.
- [EU Volume Limit](extensions/eu-volume-limit/) gives an EU iPod the volume behaviour of a non-EU one when its limit switch is off.
- [Homebrew](extensions/homebrew/) lists apps and games from `/Reprise/Apps` and `/Reprise/Games` under Extras and starts them without an Apple signature.

To add your extension to the catalog, open a PR following the [submission guide](catalog/README.md). Later releases are picked up automatically.
