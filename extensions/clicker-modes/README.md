# Clicker Modes

Based on the RepriseOS core's [clicker modes](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/clicker.c), by Alex Ball and lgreitis.

Settings > Clicker cycles Off, Speaker, Headphones and Both instead of Off and On. Apple's click engine already handles all four values; the extension adds the missing two to the row and labels them correctly. Every mode except Off shows Apple's On picture.

Without the extension, a saved Speaker or Headphones still routes the click, but the stock row swaps their labels ("Speaker" for Headphones), picks the picture by the speaker alone, and Select goes to Both.

| | |
|---|---|
| Default | on |
| Memory | image only |
| Settings | none; the mode is Apple's preference |
