# Possible work — Quincunx 1.2

1. **AA turned off during a game.** With `grDisable(GR_AA_MULTI_SAMPLE)` the samples return
   to the centre but the filter stays on: turn the scalers off too, and back on with
   `grEnable`.
2. **16-bit.** Stock 2x AA at 16-bit uses the master's 4×4 filter to remove dithering. Check
   how smoother behaves (it puts the bilinear filter on the master in its place), and whether
   sharper is fine as it is.
3. **Bottom line.** If it is ever needed: the overscan of the test builds works and costs
   0.3 fps (an extra column and row, the scaler fetching 16 more bytes per line, the row
   cleared once, and the clip window recalculated **only** when the origin really changes).
   Everything is in test builds 2–8.
4. **2x video mode** (above 1280×1024 or at high refresh rates): a CRT monitor is needed to
   test it; `FX_GLIDE_QX_ALLOW_2X=1` already exists.
5. **Voodoo4 4500** (one chip) and **Voodoo5 6000** (4 chips, its 2x mode uses banded SLI):
   these need a different hardware path.
