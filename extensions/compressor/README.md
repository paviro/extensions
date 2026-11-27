# Compressor

Ported from Rockbox's compressor (`lib/rbcodec/dsp/compressor.c`) by Jeffrey Goode, GPL-2.0-or-later.

Turns loud passages down so quiet ones stay audible. Runs after Channels on the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md).

Settings > Compressor plots output level against input level, with the switch and the six values below. The wheel picks a value, Select edits it, holding Select resets it, Menu goes back. Changes apply immediately and are saved. Switching off keeps the values. The Settings pane says Off or On.

| Value | Range | Default | What it does |
|---|---|---|---|
| State | Off, On | Off | whether the compressor plays |
| Threshold | -3 to -24 dB in 3 dB steps | -12 dB | the level above which the gain comes down |
| Ratio | 2:1, 4:1, 6:1, 10:1, Limit | 4:1 | how much of a rise above the threshold still reaches the output; Limit holds it there |
| Knee | Hard, Soft | Soft | Soft eases the gain in over 3 dB either side of the threshold |
| Makeup | Off, Auto | Auto | Auto lifts the output back up by what a full-scale signal lost |
| Attack | 0 to 30 ms in 5 ms steps | 5 ms | how quickly the gain comes down |
| Release | 100 to 1000 ms in 100 ms steps | 500 ms | how quickly it comes back up |

As in Rockbox, the gain follows the average of both channels with the treble weighted up. It acts on audio delayed by 3 ms, so Limit catches a peak before it plays. With Makeup on, a look-ahead limiter after the compressor keeps loud passages from clipping and adds about 0.7 ms of delay.

The Compressor row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise. Off costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | the switch and the six values |
