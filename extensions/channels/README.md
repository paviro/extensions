# Channels

Ported from Rockbox's channel modes (`lib/rbcodec/dsp/channel_mode.c`) by Thom Johansen and Michael Sevakis, GPL-2.0-or-later.

Narrows or widens the stereo image by mixing part of each channel into the other, and plays Mono, one channel in both ears, Karaoke or swapped channels. Runs on the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md) after Crossfeed and before the Compressor.

Settings > Channels draws the stereo image as a lobe in a half disc, with the two values below. The wheel picks a value, Select edits it, holding Select resets it, Menu goes back. Changes apply immediately and are saved. The Settings pane shows the mode, or the width under Custom.

| Mode | What plays |
|---|---|
| Stereo | nothing changes |
| Mono | both ears hear the average of the channels |
| Left Only | both ears hear the left channel |
| Right Only | both ears hear the right channel |
| Karaoke | the difference between the channels, which drops a voice mixed in the centre |
| Swap | the channels exchanged |
| Custom | the image at Width: 0 to 250 % in 5 % steps, 100 % by default |

Below 100 % part of each channel is added to the other, down to Mono at 0 %. Above 100 % part of each channel is subtracted from the other. No width clips.

The Channels row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise. Stereo costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | mode and width |
