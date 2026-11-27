# Surround

Based on Rockbox's Haas surround (`lib/rbcodec/dsp/surround.c`) by Chiwen Chang, GPL-2.0-or-later.

Widens the stage for headphones with the Haas effect: the right channel arrives a few milliseconds after the left, which the ear reads as space, while a band you choose, where voices sit, stays in place. Runs at its own stage of the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md), after Fatigue Reduction and before Channels.

Settings > Surround plots what the right ear hears, the band kept in place and what arrives late, with the switch, Delay and Mix below and the four shape values under them. The wheel picks a value, Select edits it, holding Select resets it, Menu goes back. Changes apply immediately and are saved; switching off keeps every value, and the shape values stay as they are when the delay changes.

| Value | Range | Default |
|---|---|---|
| State | Off or On | Off |
| Delay | 5, 8, 10, 15 or 30 ms | 10 ms |
| Mix | 0 to 100 % in 5 % steps | 50 % |
| Emphasis | 0 to 99 % | 35 % |
| Low | 40 to 400 Hz in 40 Hz steps | 320 Hz |
| High | 600 to 8000 Hz in 200 Hz steps | 3400 Hz |
| Side only | Off or On | Off |

The right channel is split at Low and High: the band between them stays where it is, and what lies below and above it is delayed by the Delay, at the sample rate that plays. The delay pulls the image towards the left ear; Emphasis moves that much level from the leading channel to the lagging one, which brings it back. Side only applies all of this to the side signal alone, so anything in the centre of the mix stays put in both ears. Mix blends the result with the original; at 0 % nothing plays through the effect, which costs nothing per sample, as does Off.

The lagging channel can go up to 3.5 dB over at full Emphasis, and the delayed and kept bands can add up over full scale near the cutoffs; a look-ahead limiter catches what goes over and delays the output by about 0.7 ms.

The Surround row sits after Sound Check, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise. It says Off or On.

| | |
|---|---|
| Default | off |
| Memory | two delay lines of 30 ms, about 6 KB |
| Settings | the switch, delay, mix, emphasis, low, high and side only |
