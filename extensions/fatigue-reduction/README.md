# Fatigue Reduction

Ported from Rockbox's auditory fatigue reduction (`lib/rbcodec/dsp/afr.c`) by Chiwen Chang, GPL-2.0-or-later.

Takes the edge off the upper mids and treble with four fixed filters, for long listening: a bell at 8 kHz with a shelf above it, a bell at 3.15 kHz and a low shelf below 200 Hz. Runs at its own stage of the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md), after Bass Enhancement and before Surround.

Settings > Fatigue Reduction plots the response of the chosen strength, with the strength below. The wheel picks a strength, holding Select resets it to Off, Menu goes back. Changes apply immediately and are saved.

The cut each filter is set to:

| Strength | 8 kHz bell | 3.15 kHz bell | Low shelf |
|---|---|---|---|
| Off | nothing changes | | |
| Weak | -2.9 dB | -1.3 dB | 0 dB |
| Moderate | -5.8 dB | -2.9 dB | -0.5 dB |
| Strong | -7.2 dB | -4.8 dB | -0.5 dB |

The filters are designed for 44.1 and 48 kHz; at other rates the audio passes through. Every filter is a cut, but the cuts can ring a little over full scale on hard transients, so a look-ahead limiter catches what still goes over and delays the output by about 0.7 ms.

The Fatigue Reduction row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise, with the strength as its caption. Off costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | strength |
