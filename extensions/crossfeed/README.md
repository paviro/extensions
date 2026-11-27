# Crossfeed

Ported from Rockbox's crossfeed (`lib/rbcodec/dsp/crossfeed.c`) by Thom Johansen, Bertrik Sikken and Michael Sevakis, GPL-2.0-or-later. Simple is Jan Meier's passive circuit.

Feeds a delayed, filtered part of each headphone channel into the other, as with speakers. Runs first on the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md).

Settings > Crossfeed plots what each ear hears, with Mode and the Custom values below. The wheel picks a value, Select edits it, holding Select resets it, Menu goes back. Changes apply immediately and are saved; the Custom values keep when the mode changes.

| Mode | What plays |
|---|---|
| Off | nothing changes |
| Simple | Meier's circuit: each channel's filtered difference signal leaks into the other, with no settings |
| Custom | each channel reaches the other after about 300 µs through a high shelf, and the direct path is turned down; five values below |

The plot shows the ear's own channel and the one fed across, with the precut applied. Mode and Precut share the first row under it, the four shape values the second. Outside Custom, Precut and the shape values are greyed but still editable.

| Value | Range | Default |
|---|---|---|
| Direct | -6.0 to 0.0 dB in 0.5 dB steps | -1.5 dB |
| Cross | -12.0 to -3.0 dB in 0.5 dB steps | -6.0 dB |
| Highs | -24.0 to -6.0 dB in 0.5 dB steps | -16.0 dB |
| Cutoff | 500 to 2000 Hz in 100 Hz steps | 700 Hz |
| Precut | Auto, or 0.0 to -6.0 dB in 0.5 dB steps | Auto |

Cross is the level of the other channel below the Cutoff, Highs its level above. A centred passage reaches each ear through both paths, so below the Cutoff it plays at Direct plus Cross, about +2.5 dB at the defaults. Precut lowers both paths: Auto picks the smallest 0.5 dB step that keeps the largest possible sum at or below 0 dB. Simple has no precut and can go up to 3.4 dB over when the channels differ. A look-ahead limiter catches what still goes over, in Simple or with a fixed precut, and delays the output by about 0.7 ms.

The Crossfeed row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise, with the mode as its caption. Off costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | mode, the four shape values and the precut |
