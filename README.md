# RepriseOS extensions

This repository hosts the source for these RepriseOS extensions and accepts proposals for the extension catalog.

- [Balance](extensions/balance/) adjusts the volume of the left and right channels independently of the main volume.
- [Night Shift](extensions/night-shift/) warms the display manually or automatically from sunset to sunrise.
- [Playback Log](extensions/playback-log/) records listening history for scrobbling and optional detailed reports.
- [Crossfeed](extensions/crossfeed/) blends a little of each stereo channel into the other for headphone listening. It uses the included [DSP library](extensions/dsp/).
- [Sleep Timer](extensions/sleep-timer/) sets Apple's sleep timer from the Now Playing context menu and shows the minutes left in Now Playing.
- [Fast Seek](extensions/fast-seek/) makes seeks in long AAC and Apple Lossless files jump to the target instead of stepping through every frame.
- [Long Audio Fix](extensions/long-audio-fix/) plays AAC and Apple Lossless files longer than 13.5 hours to the end.
- [Chapter Menu](extensions/chapter-menu/) lists the chapters of a podcast or iTunes U episode, or of the playing track, in its context menu and jumps to one.

To add your extension to the catalog, open a PR following the [submission guide](catalog/README.md). Later releases are picked up automatically.
