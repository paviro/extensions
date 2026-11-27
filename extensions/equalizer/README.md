# Equalizer

Partly adapted from the RepriseOS core's [Custom EQ](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/custom_eq.c), [EQ audio path](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/eq_audio.c), [EQ canvas](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/eq_canvas.c) and [filter design](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/eq_coefficients.c), by lgreitis and paviro.

Apple's EQ rounds each of its three filter stages to 16 bits, so boosting presets distort loud passages. Equalizer runs the same presets through its own filter chain with 42 dB of headroom between stages, and lowers each preset by its peak gain. A look-ahead limiter at the end catches the peaks the precut misses and delays the output by about 0.7 ms. Off and Flat pass audio through untouched.

The EQ list gains a Custom preset after Apple's. While Custom is selected, an Edit EQ row shapes it. The row sits after EQ, in Settings > Sound while the Sound Menu extension runs and in Settings otherwise. The wheel picks a band, Select steps into its parameters and then edits one, Menu goes back. Holding Select resets the highlighted band or parameter. Changes apply immediately. Moving the Hold switch shows the response curve, precut included. Like Flat, Custom is not replaced by a track's own EQ setting. Each of the five bands has:

| Parameter | Range | Default |
|---|---|---|
| Type | off, peaking, low shelf, high shelf | low shelf, peaking, high shelf, off, off |
| Frequency | 20 Hz to 20 kHz | 60 Hz, 1 kHz, 8 kHz, 250 Hz, 4 kHz |
| Q | 0.3 to 4.0 | 0.7 |
| Gain | -12 to +12 dB | 0 dB |
| Precut, for the whole preset | automatic (the chain's peak gain) or fixed down to -10 dB | automatic |

Enabled during playback, it takes over at the next track or preset change. Disabling it with Custom selected sets the EQ to Off.

On first start the extension imports the Custom EQ values the earlier core kept in its settings store (the CEQ record) and shows the Edit EQ row when Custom was the selected preset. Custom EQ values that exist only in the pre-store CFWEQ file start from the defaults.

| | |
|---|---|
| Default | on |
| Memory | image only |
| Settings | the Custom preset and whether the Edit EQ row shows |
