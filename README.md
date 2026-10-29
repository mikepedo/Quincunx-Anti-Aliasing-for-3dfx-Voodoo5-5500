# Quincunx 1.1 — Glide3x 3.10.0.40409 for the Voodoo5 5500 (Windows XP)

Five-tap Quincunx anti-aliasing on the Voodoo5 5500, built into `glide3x.dll`. The Windows
display driver (2D, DirectDraw, Direct3D) is **not modified**. It works with Glide games and
with OpenGL games that run through MesaFX.

This release is build **3.10.0.40409**. It is based on the 3dfx Glide sources released on
SourceForge as 3.10.0.40408; the build number was raised to 40409 so that the modified DLL
can be told apart from the original.

Version 1.1 adds **Quincunx smoother**, brings the cost of **Quincunx sharper** down from
about 7% to about 3.5% compared with stock 2x AA, and restores the first row and column of
the screen. Measurements are in `RESULTS_1.1.md`.

---

## 1. How it works

In stock 2x AA on the 5500, each VSA-100 renders the whole image with its own sample, and
the master averages its own video stream with the slave's.

| Mode | Master | Slave | Pixel |
|---|---|---|---|
| **Quincunx sharper** | pixel centre, no filter | top-left corner, half-phase bilinear filter | ½ centre + ⅛ × 4 corners |
| **Quincunx smoother** | pixel centre, half-phase bilinear filter | as above | ⅛ × 4 centres + ⅛ × 4 corners |

The corners are shared between neighbouring pixels, so the rendering cost is that of 2x AA.
The only extra cost is the additional line the filter reads: on the slave for sharper, on
both chips for smoother.

In **smoother**, the mean of the 4 centres and the mean of the 4 corners sit a quarter of a
pixel apart. The resulting filter is regular (4 taps half a pixel apart on each axis), and
the whole image moves by ¼ pixel to the right and down, which is not visible on screen.

### Measured convention (for anyone changing the code)

- In Glide, each pixel's sample sits on the **integer** screen coordinate.
- The jitter values (`FX_GLIDE_AA2_OFFSET_*`, `FX_GLIDE_QX_*`) act as **vertex offsets**:
  +0.5 puts the sample at −0.5, on the top-left corner of the pixel.
- Scale: 1 unit = 1 pixel. Stock 2x AA samples at −0.25 and +0.25.

---

## 2. Selector

The 3dfx Tools key `SSTH3_SLI_AA_CONFIGURATION`:

| Value | Result |
|---|---|
| 0–8 | same as stock |
| **10** | **Quincunx sharper** (Voodoo5 5500) |
| **11** | **Quincunx smoother** (Voodoo5 5500) |

On any other board, or when the video path cannot be used (see §6), Glide falls back to
stock 2x AA and writes the reason to the log. Glide reads the key from the environment
variable first, then from `HKCU\SYSTEM\CurrentControlSet\Services\3dfxvs\Device0\glide`,
then from the same path under `HKLM`. The `.bat` profiles use environment variables.

Value 9 (4500) and the 1.0 meaning of 11 (6000) have been removed.

---

## 3. Building (Windows XP VM, VC6)

Same tools as for 1.0: Visual C++ 6, XP DDK 2600, DirectX 9 SDK, NASM, GNU make 3.81.

1. Starting from the **original** Glide 3.10.0.40408 tree, use one of the two methods:
   - **patch:** `patch -p1 < patches\quincunx-1.1-glide3x.patch`, from the folder that
     contains `glide3x`;
   - **full files:** copy `sources\3dfx_glide_sourceforge-3.10.0.40409\glide3x` over the
     tree and answer Yes to everything.

   **If you applied test builds 2–8 to your tree**, use the full files: they include
   `gglide.c`, which in 1.1 is the **original** one again and undoes the test-build changes.
   They also include `Makefile.win32`, which sets the build number to 40409.
2. From the `glide3x` folder:
   ```bat
   make -f Makefile.win32 realclean
   make -f Makefile.win32 FX_GLIDE_HW=h5 USE_X86=1 USE_3DNOW=1 USE_MMX=1 USE_SSE=1 USE_SSE2=1 TEXUS2=1
   ```
   `realclean` is required: it regenerates `fxbldno.h` with the new build number.
3. Check: `findstr /m "Quincunx 1.1:" h5\lib\glide3x.dll` and
   `findstr /m "3.10.00.40409" h5\lib\glide3x.dll` must both print the file name.

Files changed from the original: `glide3x\Makefile.win32` (build number), and `fxglide.h`,
`gpci.c`, `gsst.c` (in `h5\glide3\src`), `minihwc.c` and `minihwc.h` (in `h5\minihwc`).

### Test program

`tools\qxpattern.c` is a single file that loads `glide3x.dll` at start-up. Build it with
MinGW:

```bat
gcc -O2 -Wall -o qxpattern.exe qxpattern.c -lgdi32
```

Its on-screen messages are still in Italian.

---

## 4. Installation

1. Back up `C:\WINDOWS\system32\glide3x.dll`.
2. Copy the new `glide3x.dll` to `system32`, and to the game folder if the game ships its
   own copy.
3. OpenGL games need the MesaFX `opengl32.dll` in the game folder. It does not need to be
   rebuilt; the patch in `patches\optional\` only makes the 2x pixel format request
   explicit.

**To restore:** close the game and put the backed-up DLL back.

---

## 5. Use

- **Quake III Arena:** copy `profiles\Quake3A_sharper.bat`, `Quake3A_smoother.bat` and
  `Quake3A_2x.bat` into the game folder (`r_mode`: 3 = 640×480, 4 = 800×600,
  6 = 1024×768, 8 = 1280×1024).
- **Other games**, from a command prompt: `GAME_sharper.bat "C:\path\game.exe"` or
  `GAME_smoother.bat ...`.
- **Tests:** put `qxpattern.exe` and the `A_`, `D_`, `E_` and `PROBE_` profiles in `C:\qx\`
  (keys: 2 columns, 3 rows, 5 dots, 8 frame, c colours, 0 probe, q quit).

### What changes automatically when Quincunx is active

- **Video buffer threshold 48** on both chips (stock is 32, and the slave was left at 16).
  `SSTVB_PIXTHOLD`, if set, always wins.
- **`FX_GLIDE_AA_CLIP` off.** With stock AA, neither chip draws row 0 and column 0; with
  Quincunx the master samples exactly there. If you set it yourself, your value wins.

### Optional variables

| Variable | Default | Effect |
|---|---|---|
| `FX_GLIDE_QX_LOG` | (empty) | log file, e.g. `C:\qx\qx.log` |
| `FX_GLIDE_QX_FILTER` | 3 | filter: 3 bilinear, 0 point (diagnostics) |
| `FX_GLIDE_QX_SCALER` | 3 | bit0 horizontal scaler, bit1 vertical scaler |
| `FX_GLIDE_QX_THOLD` | 1 | 0 = do not copy the threshold to the slave |
| `FX_GLIDE_QX_CENTER_X` / `_Y` | 0.0 | master jitter |
| `FX_GLIDE_QX_CORNER_X` / `_Y` | 0.5 | slave jitter |
| `FX_GLIDE_QX_YFLIP` | 1 | flips Y with a lower-left origin (MesaFX) |
| `FX_GLIDE_QX_JITTER` | 1 | 0 = keep the stock jitter |
| `FX_GLIDE_QX_PHASE_X` / `_Y` | 0x40000 + half the size | scaler start phase |
| `FX_GLIDE_QX_ALLOW_2X` | 0 | 1 = also try in 2x video mode (untested) |

The defaults are the values verified on the board; there is no need to change them.

---

## 6. Known limits

- **Voodoo5 5500 only.** Verified up to 1280×1024. In the "2x" video mode (high pixel
  clocks, e.g. 1600×1200) Quincunx stays off: that mode could not be tested.
- **Bottom line:** the slave's filter reads one line below the screen, which does not exist,
  and mixes in a little of another buffer there. In games it is barely visible, and it shows
  that Quincunx is on. The overscan of the test builds fixed it, but it was left out by
  choice because of its cost (0.3 fps).
- **Left and top edges** (left and bottom with a lower-left origin): the corner sample falls
  outside the scene and takes a quarter of the background colour. This is geometry, not a
  bug.
- `grDisable(GR_AA_MULTI_SAMPLE)` during a game: the samples return to the centre but the
  filter stays on.

The plan for the next version is in `ROADMAP_1.2.md`.

---

## License

The Glide sources and the modified files are distributed under the **3dfx Glide Source
Code General Public License**, included as `LICENSE-GLIDE.txt`. Anyone who copies,
distributes or modifies them may do so only under that license. The software is provided
"as is", **without warranty of any kind**; see section 5 of the license.

Every file changed from the 3dfx sources carries a notice at the top saying that it was
modified and when; the changes themselves are marked "Quincunx" in the comments. Glide is a
trademark of 3dfx Interactive; this project is not affiliated with or endorsed by its
owner.

---

## Contents

```text
README.md, RESULTS_1.1.md, ROADMAP_1.2.md, LICENSE-GLIDE.txt
patches\quincunx-1.1-glide3x.patch         all changes, 1.0 included, against the original
patches\optional\quincunx-1.0-mesafx.patch
sources\...                                 full files (gglide.c is the original one)
profiles\*.bat                              test and game profiles
tools\qxpattern.c, tools\probe.py
tools\host-tests\                           logic tests (gcc on Linux: run.sh)
results\                                    1.0 sample probes D and E
```
