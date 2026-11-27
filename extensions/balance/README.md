# Balance

Shifts the sound towards one ear by turning the other down, for headphones and for hearing that differs between the ears. Runs on the shared [output stage](https://github.com/paviro/revola/blob/main/docs/sdk/audio.md) after the Compressor, so every other effect sees both channels at full level.

Settings > Balance draws the stereo image as a lobe in a half disc, the ears at its arms: the arm on the quieter ear's side closes towards the centre as that ear is turned down, so the image leans towards the louder ear. The position sits below. The wheel moves it, holding Select returns to Center, Menu goes back. Each step from the centre turns the quieter ear down by one decibel, up to 20 dB; the last step on each side plays that ear only. The louder ear is never changed, so nothing can clip and nothing gets louder.

| Position | What plays |
|---|---|
| L only | the left ear alone |
| L 20 dB to L 1 dB | the right ear that much quieter |
| Center | nothing changes |
| R 1 dB to R 20 dB | the left ear that much quieter |
| R only | the right ear alone |

Changes apply immediately and are saved. The Balance row sits after Volume Limit, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise, with the position as its caption. Center costs nothing per sample.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | position |
