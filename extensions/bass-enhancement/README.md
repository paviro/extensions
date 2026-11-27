# Bass Enhancement

Based on Rockbox's perceptual bass enhancement (`lib/rbcodec/dsp/pbe.c`) by Chiwen Chang, GPL-2.0-or-later.

Fills out the bass and clears the mids with five fixed filters: a lift at 64 Hz and at 2 kHz, a cut at 7.5 kHz and at 10 kHz, and a shelf that lifts the top above 16 kHz. Runs at its own stage of the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md), after Crossfeed and before Fatigue Reduction.

Settings > Bass Enhancement plots the response of the chosen strength, with the strength and its precut below. The wheel picks a strength, holding Select resets it to Off, Menu goes back. Changes apply immediately and are saved.

| Strength | 64 Hz | 2 kHz | 7.5 kHz | 10 kHz | Above 16 kHz |
|---|---|---|---|---|---|
| Off | nothing changes | | | | |
| 25 % | +1.3 dB | +1.5 dB | -2.1 dB | -0.7 dB | +1.3 dB |
| 50 % | +2.7 dB | +2.9 dB | -4.1 dB | -1.5 dB | +2.7 dB |
| 75 % | +4.0 dB | +4.4 dB | -6.2 dB | -2.2 dB | +4.0 dB |
| 100 % | +5.3 dB | +5.8 dB | -8.2 dB | -2.9 dB | +5.3 dB |

At 100 % these are Rockbox's gains, with the whole cascade a further 0.5 dB down; a lower strength scales each of them in decibels. Rockbox's strength sets the delays of a band split instead, which is left out here, as is its precut setting. The precut takes the whole cascade down by its largest lift, so the level never exceeds the input's, and the plot shows the curves with it. Peaks that still line up above full scale go through a look-ahead limiter, which delays the output by about 0.7 ms. The filters are designed for 44.1 and 48 kHz; at other rates the audio passes through.

The Bass Enhancement row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise, with the strength as its caption. Off costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | strength |
