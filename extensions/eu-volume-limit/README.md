# EU Volume Limit

By paviro.

Gives an EU iPod the volume behaviour of a non-EU one. With the EU Volume Limit switch in Settings > Volume Limit off, the volume goes 15% higher; with it on, the EU cap stays. Other iPods are unchanged.

| | Stock EU iPod | With the extension |
|---|---|---|
| Ceiling with the switch off | 222 | 256 on Apple's scale, 15% higher |
| Ceiling with the switch on | 168 | 168 |
| Default volume limit | 222 | 256; a saved limit is kept |
| High-volume warning | yes | never |
| Saved volume lowered at startup | yes | never |
| Volume lowered after 20 hours of listening | yes | never |

Apple checks two region flags, bit 0 and bit 7, and accepts a stored override where it checks bit 7. Stock shows the switch only for bit 7 and holds a bit-0 iPod at 222 without one; the extension shows it for either. It patches seven constants in 2.0.5.

Turned off, Apple's behaviour returns at once, except the startup clamp, which returns at the next boot. The current volume and saved limit are kept.

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | none; Apple stores the switch |
