# Results — Quincunx 1.1 (26–29 October 2026)

Voodoo5 5500 PCI (Mac version flashed with a PC BIOS), Windows XP, VGA output, Philips LCD
monitor. Glide built with VC6 in the XP VM. Quake III Arena, timedemo, maximum detail.

## Performance (1024×768, 32-bit)

| Configuration | fps |
|---|---|
| Stock 2x AA, `SSTVB_PIXTHOLD` 48 | 31.5 |
| Stock 2x AA, stock threshold | 31.3 |
| Quincunx 1.0 | 29.0 |
| **Quincunx 1.1 sharper** | **30.4** |
| **Quincunx 1.1 smoother** | **28.4** |
| Quincunx sharper with a point filter (diagnostics) | 31.5 |

At 16-bit, Quincunx and stock 2x AA both run at 45.9 fps (1.0 measurement).

## Where the 1.0 cost came from

- **Slave threshold.** Glide writes `vidPixelBufThold` only to the master, before the slave
  starts copying the master's writes, so the slave stayed at its power-on value of 16
  (`00010410`). Copying the threshold: 29.0 → 30.2. With 48 on both chips: 30.4. At 56 the
  image breaks up.
- **Filter read.** The rest is the second line the filter reads. It costs the same with any
  filter other than point, including 2×2 and even with the scalers off, and goes away only
  with the point filter (31.5).

## Other checks

| Test | Result |
|---|---|
| Slave 2×2 filter at 32-bit (T3/T4) | does no averaging at all: ruled out |
| Horizontal-only scaler (T2) | works, same cost: ruled out as a saving |
| 1280×1024 | Quincunx active and stable |
| 2x video mode | not reachable with this monitor (1600×1200 does not start even without AA) |
| Filter on the master (smoother) | keys 2 and 3 uniform: the master filter reaches the output |
| Stock AA clip turned off | row 0 and column 0 come back (key 5: 9 dots out of 9) |

## Left out of 1.1

- **Quadrunx** (both chips on the corner, both filtered): it works, but it is only a 2×2 box
  filter on one sample per pixel. Compared with stock 4x AA with the jitters moved to the
  corners (same image, twice the cost), the quality was judged very poor.
- **Overscan** (an extra column and row for the filter): it removed the stray dot on the
  right edge and the dirty bottom line, at a cost of 0.3 fps (30.1 against 30.4). Left out by
  choice.

## A lesson from tests 3–8

A 15% drop (30.4 → 25.9) looked for a long time like an effect of the overscan on memory.
Tests D (texture memory, spacing between buffers, addresses) and E (the overscan pieces one
at a time) ruled all of that out. The cause was a clip-window recalculation added to
`grSstOrigin`, a function Glide also runs every time it revalidates `fbzMode`, that is many
times per frame. It was fixed in test8 (30.1 with the overscan) and is not in 1.1.
