/*----------------------------------------------------------------------
  Quincunx 1.0 - video resolve on the slave VSA-100

  Requested by glide3 (_grQuincunxSetup) through bInfo->h3quincunx for
  the dual-chip, 2-sample, no-SLI, digital configuration, where every
  chip renders one full sample and the master averages its own PIPE with
  the slave PIPE arriving through the AA FIFO (cfgVideoCtrl0 on the
  master: PIPE_PLUS_AAFIFO, divide by 2).

  Only the slave video unit is changed: its overlay scaler is enabled
  in both directions with a 1:1 step and a half-pixel start phase, and
  its filter is set to bilinear.  The slave PIPE then carries the mean
  of the four samples around each pixel, and the stock mux produces

      OUT = 1/2 centre + 1/8 (four corners)

  Notes taken from this source tree:
  - after SLI/AA setup the slave snoops writes to the master's memBase0
    (cfgInitEnable: MEMBASE0_SNOOP_ENABLE, see hwcSetSLIAAMode in
    lin_mode.c), so a master register write also lands on the slave.
    Slave-only state is therefore written through slaveIOBase, after
    the last master video write of hwcInitVideo.
  - the slave geometry copy and the vidProcCfg disable/enable pulse
    follow the reference sequence in hwcSetSLIAAMode (lin_mode.c).
  - vidOverlayDudx/Dvdy are 0.20 and 20 bits wide, so 1.0 cannot be
    written: 0xFFFFF is used, and the start phase carries a small
    correction so that the fraction stays at one half across the line.

  Everything except the 1:1 step is overridable from the environment
  or the glide registry key, so the phase can be calibrated on the
  board without rebuilding:
    FX_GLIDE_QX_PHASE_X / FX_GLIDE_QX_PHASE_Y  start offset (0.19)
    FX_GLIDE_QX_FILTER   0..3 (3 = bilinear, 0 = point for diagnosis)
    FX_GLIDE_QX_LOG      path of a text log, e.g. C:\qx.log
  ----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdarg.h>

#define HWC_QX_DDA_STEP          0x000FFFFFUL   /* 1 - 2^-20 in 0.20 */
#define HWC_QX_HALF_PHASE        0x00040000UL   /* 0.5 in 0.19       */

typedef struct hwcQxSaved_s {
  hwcBoardInfo *owner;
  FxU32 vidProcCfg;
  FxU32 vidScreenSize;
  FxU32 vidOverlayStartCoords;
  FxU32 vidOverlayEndScreenCoord;
  FxU32 vidDesktopOverlayStride;
  FxU32 vidOverlayDudxOffsetSrcWidth;
  FxU32 vidOverlayDudx;
  FxU32 vidOverlayDvdy;
  FxU32 vidOverlayDvdyOffset;
  FxU32 vidPixelBufThold;
  FxU32 vidMaxRGBDelta;
  /* Quadrunx (experimental): the master scaler is changed too. */
  FxBool masterSaved;
  FxU32 mVidProcCfg;
  FxU32 mVidOverlayDudx;
  FxU32 mVidOverlayDvdy;
  FxU32 mVidOverlayDudxOffsetSrcWidth;
  FxU32 mVidOverlayDvdyOffset;
} hwcQxSaved;

static hwcQxSaved hwcQxState;

static void
hwcQxLog(const char *fmt, ...)
{
  const char *path = GETENV("FX_GLIDE_QX_LOG");
  FILE *fp;
  va_list args;

  if ((path == NULL) || (path[0] == '\0'))
    return;

  fp = fopen(path, "a");
  if (fp == NULL)
    return;

  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);
  fclose(fp);
}

static FxU32
hwcQxEnv(const char *name, FxU32 defVal)
{
  const char *str = GETENV(name);

  if ((str == NULL) || (str[0] == '\0'))
    return defVal;

  return (FxU32)strtoul(str, NULL, 0);
}

static void
hwcQuincunxRestore(hwcBoardInfo *bInfo)
{
#define FN_NAME "hwcQuincunxRestore"
  FxU32 cfg;

  if ((bInfo == NULL) || (hwcQxState.owner != bInfo))
    return;

  /* Quadrunx: master first (its writes may be snooped by the slave,
   * which is restored right after). */
  if (hwcQxState.masterSaved)
  {
    HWC_IO_STORE(bInfo->regInfo, vidProcCfg,
                 hwcQxState.mVidProcCfg & ~(SST_OVERLAY_HORIZ_SCALE_EN |
                                            SST_OVERLAY_VERT_SCALE_EN));
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDudx, hwcQxState.mVidOverlayDudx);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDvdy, hwcQxState.mVidOverlayDvdy);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDudxOffsetSrcWidth,
                 hwcQxState.mVidOverlayDudxOffsetSrcWidth);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDvdyOffset,
                 hwcQxState.mVidOverlayDvdyOffset);
    HWC_IO_STORE(bInfo->regInfo, vidProcCfg, hwcQxState.mVidProcCfg);
    hwcQxState.masterSaved = FXFALSE;
    hwcQxLog("Quincunx 1.1: master video state restored (master filter)\n");
  }

  if (bInfo->regInfo.slaveIOBase[0] != 0)
  {
    /* Stop the slave scaler first, then put back what we found. */
    HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidProcCfg, cfg);
    cfg &= ~(SST_OVERLAY_HORIZ_SCALE_EN |
             SST_OVERLAY_VERT_SCALE_EN |
             SST_OVERLAY_FILTER_MODE);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidProcCfg, cfg);

    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDudx,
                       hwcQxState.vidOverlayDudx);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDvdy,
                       hwcQxState.vidOverlayDvdy);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDvdyOffset,
                       hwcQxState.vidOverlayDvdyOffset);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDudxOffsetSrcWidth,
                       hwcQxState.vidOverlayDudxOffsetSrcWidth);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidScreenSize,
                       hwcQxState.vidScreenSize);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayStartCoords,
                       hwcQxState.vidOverlayStartCoords);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayEndScreenCoord,
                       hwcQxState.vidOverlayEndScreenCoord);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidDesktopOverlayStride,
                       hwcQxState.vidDesktopOverlayStride);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidPixelBufThold,
                       hwcQxState.vidPixelBufThold);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidMaxRGBDelta,
                       hwcQxState.vidMaxRGBDelta);
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidProcCfg,
                       hwcQxState.vidProcCfg);
  }

  hwcQxLog("Quincunx 1.1: slave video state restored\n");
  GDBG_INFO(80, "%s: slave video state restored\n", FN_NAME);
  hwcQxState.owner = NULL;
#undef FN_NAME
}

static FxBool hwcQuincunxApplyChecked(hwcBoardInfo *bInfo);

/* Clears h3quincunx when the video path cannot be used, so that glide3
 * puts the stock jitter back instead of rendering Quincunx samples
 * without the matching filter. */
static FxBool
hwcQuincunxApply(hwcBoardInfo *bInfo)
{
  FxBool active;

  if (!bInfo->h3quincunx)
    return FXFALSE;

  active = hwcQuincunxApplyChecked(bInfo);
  if (!active)
    bInfo->h3quincunx = 0;
  return active;
}

static FxBool
hwcQuincunxApplyChecked(hwcBoardInfo *bInfo)
{
#define FN_NAME "hwcQuincunxApply"
  FxU32 mCfg, mSize, mStart, mEnd, mStride, mDudxOfs, mDudx, mDvdy, mDvdyOfs;
  FxU32 sCfg, sCfgNew, filter, scaler, scaleBits;
  FxU32 width, height;
  FxU32 phaseX, phaseY, packedDudx, phaseYOfs;
  FxU32 stepX, stepY, mThold, sThold, sDelta, newDelta;
  FxU32 rb[11];
  FxU32 mCfgNew = 0;
  FxBool ok;
  /* Variant chosen in glide3 from SSTH3_SLI_AA_CONFIGURATION, stored in
   * h3quincunx:
   *   1 sharper  (10): master centre, unfiltered; slave corner, filtered
   *   2 smoother (11): as sharper, and the master scaler filters too
   * "quad" below means: the master scaler is programmed as well. */
  FxBool quad = (bInfo->h3quincunx == 2);
  const char *modeName = quad ? " SMOOTHER" : " SHARPER";

  if (!bInfo->h3quincunx)
    return FXFALSE;

  /* One owner at a time; re-entry from the same board starts clean. */
  if (hwcQxState.owner != NULL)
  {
    if (hwcQxState.owner != bInfo)
      return FXFALSE;
    hwcQuincunxRestore(bInfo);
  }

  hwcQxLog("Quincunx 1.1: apply%s, chips=%lu samples=%lu sli=%lu analog=%lu 2nd=%lu\n",
           modeName,
           (unsigned long)bInfo->pciInfo.numChips,
           (unsigned long)bInfo->h3pixelSample,
           (unsigned long)bInfo->h3nwaySli,
           (unsigned long)bInfo->h3analogSli,
           (unsigned long)bInfo->buffInfo.enable2ndbuffer);

  if (!IS_NAPALM(bInfo->pciInfo.deviceID) ||
      (bInfo->pciInfo.numChips != 2) ||
      (bInfo->h3pixelSample != 2) ||
      (bInfo->h3nwaySli > 1) ||
      (bInfo->h3analogSli != 0) ||
      (bInfo->buffInfo.enable2ndbuffer) ||
      (bInfo->regInfo.slaveIOBase[0] == 0))
  {
    hwcQxLog("Quincunx 1.1: not eligible, stock 2x AA kept\n");
    return FXFALSE;
  }

  HWC_IO_LOAD(bInfo->regInfo, vidProcCfg, mCfg);
  HWC_IO_LOAD(bInfo->regInfo, vidScreenSize, mSize);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayStartCoords, mStart);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayEndScreenCoord, mEnd);
  HWC_IO_LOAD(bInfo->regInfo, vidDesktopOverlayStride, mStride);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, mDudxOfs);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudx, mDudx);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDvdy, mDvdy);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDvdyOffset, mDvdyOfs);

  width  = (mSize & SST_VIDEO_SCREEN_WIDTH)  >> SST_VIDEO_SCREEN_WIDTH_SHIFT;
  height = (mSize & SST_VIDEO_SCREEN_HEIGHT) >> SST_VIDEO_SCREEN_HEIGHT_SHIFT;

  hwcQxLog("Quincunx 1.1: master cfg=%08lx size=%08lx (%lux%lu) start=%08lx end=%08lx stride=%08lx dudxOfs=%08lx dudx=%08lx dvdy=%08lx dvdyOfs=%08lx\n",
           (unsigned long)mCfg, (unsigned long)mSize,
           (unsigned long)width, (unsigned long)height,
           (unsigned long)mStart, (unsigned long)mEnd, (unsigned long)mStride,
           (unsigned long)mDudxOfs, (unsigned long)mDudx, (unsigned long)mDvdy,
           (unsigned long)mDvdyOfs);

  /* The master must show a full-screen, unscaled, tiled overlay.
   * The desktop surface may stay enabled underneath (the XP driver keeps
   * it on, measured on a Voodoo5 5500: vidProcCfg 036c0181), but without
   * chroma keying, so the overlay covers the whole screen. */
  {
    const char *why = NULL;

    if ((width == 0) || (height == 0) ||
        (width != bInfo->vidInfo.xRes) || (height != bInfo->vidInfo.yRes))
      why = "screen size differs from the Glide resolution";
    else if (!(mCfg & SST_OVERLAY_EN) || !(mCfg & SST_OVERLAY_TILED_EN))
      why = "overlay off or not tiled";
    else if (mCfg & (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN))
      why = "master overlay already scaled";
    else if (mCfg & (SST_HALF_MODE | SST_INTERLACED_EN))
      why = "half or interlaced video mode";
    else if ((mCfg & SST_VIDEO_2X_MODE_EN) && !hwcQxEnv("FX_GLIDE_QX_ALLOW_2X", 0))
      why = "2x video mode (set FX_GLIDE_QX_ALLOW_2X=1 to try it)";
    else if (mCfg & SST_OVERLAY_STEREO_EN)
      why = "stereo overlay";
    else if (mCfg & SST_CHROMA_EN)
      why = "chroma key on (overlay does not cover the desktop)";
    else if (((mStart & (SST_OVERLAY_X | SST_OVERLAY_Y)) != 0) ||
             (((mEnd & SST_OVERLAY_X) >> SST_OVERLAY_X_SHIFT) != (width - 1)) ||
             (((mEnd & SST_OVERLAY_Y) >> SST_OVERLAY_Y_SHIFT) != (height - 1)))
      why = "overlay window is not full screen";

    if (why != NULL)
    {
      hwcQxLog("Quincunx 1.1: master video state not as expected (%s), stock 2x AA kept\n", why);
      return FXFALSE;
    }
  }

  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidProcCfg, sCfg);
  if ((sCfg & SST_OVERLAY_PIXEL_FORMAT) != (mCfg & SST_OVERLAY_PIXEL_FORMAT))
  {
    hwcQxLog("Quincunx 1.1: slave pixel format %08lx differs from master %08lx, stock 2x AA kept\n",
             (unsigned long)(sCfg & SST_OVERLAY_PIXEL_FORMAT),
             (unsigned long)(mCfg & SST_OVERLAY_PIXEL_FORMAT));
    return FXFALSE;
  }

  /* Save the slave before the first write. */
  hwcQxState.vidProcCfg = sCfg;
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidScreenSize, hwcQxState.vidScreenSize);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayStartCoords, hwcQxState.vidOverlayStartCoords);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayEndScreenCoord, hwcQxState.vidOverlayEndScreenCoord);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidDesktopOverlayStride, hwcQxState.vidDesktopOverlayStride);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, hwcQxState.vidOverlayDudxOffsetSrcWidth);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDudx, hwcQxState.vidOverlayDudx);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDvdy, hwcQxState.vidOverlayDvdy);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDvdyOffset, hwcQxState.vidOverlayDvdyOffset);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidPixelBufThold, sThold);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidMaxRGBDelta, sDelta);
  HWC_IO_LOAD(bInfo->regInfo, vidPixelBufThold, mThold);
  hwcQxState.vidPixelBufThold = sThold;
  hwcQxState.vidMaxRGBDelta = sDelta;
  hwcQxState.masterSaved = FXFALSE;
  hwcQxState.owner = bInfo;

  hwcQxLog("Quincunx 1.1: slave before cfg=%08lx size=%08lx start=%08lx end=%08lx stride=%08lx dudxOfs=%08lx dudx=%08lx dvdy=%08lx dvdyOfs=%08lx\n",
           (unsigned long)hwcQxState.vidProcCfg, (unsigned long)hwcQxState.vidScreenSize,
           (unsigned long)hwcQxState.vidOverlayStartCoords,
           (unsigned long)hwcQxState.vidOverlayEndScreenCoord,
           (unsigned long)hwcQxState.vidDesktopOverlayStride,
           (unsigned long)hwcQxState.vidOverlayDudxOffsetSrcWidth,
           (unsigned long)hwcQxState.vidOverlayDudx,
           (unsigned long)hwcQxState.vidOverlayDvdy,
           (unsigned long)hwcQxState.vidOverlayDvdyOffset);

  /* Start phase: one half plus extent/2 in 0.19, which offsets the
   * 2^-20 per pixel shortfall of the 0xFFFFF step over the line. */
  phaseX = hwcQxEnv("FX_GLIDE_QX_PHASE_X", HWC_QX_HALF_PHASE + (width  >> 1));
  phaseY = hwcQxEnv("FX_GLIDE_QX_PHASE_Y", HWC_QX_HALF_PHASE + (height >> 1));
  filter = hwcQxEnv("FX_GLIDE_QX_FILTER", 3) & 0x3;

  /* 1.1 test knobs (defaults = the verified 1.0 setup):
   *   FX_GLIDE_QX_SCALER    bit0 horizontal, bit1 vertical scaler (3)
   *   FX_GLIDE_QX_DUDX/DVDY scaler steps, 0.20 (0xFFFFF)
   *   FX_GLIDE_QX_THOLD     1 = copy the master vidPixelBufThold (1)
   *   FX_GLIDE_QX_MAXDELTA  slave vidMaxRGBDelta for the 2x2 filter
   *   FX_GLIDE_QX_ALLOW_2X  1 = also try in 2x video mode (0) */
  scaler = hwcQxEnv("FX_GLIDE_QX_SCALER", 3) & 0x3;
  scaleBits = ((scaler & 1) ? SST_OVERLAY_HORIZ_SCALE_EN : 0) |
              ((scaler & 2) ? SST_OVERLAY_VERT_SCALE_EN : 0);
  stepX = hwcQxEnv("FX_GLIDE_QX_DUDX", HWC_QX_DDA_STEP) & SST_OVERLAY_DELTA;
  stepY = hwcQxEnv("FX_GLIDE_QX_DVDY", HWC_QX_DDA_STEP) & SST_OVERLAY_DELTA;
  newDelta = hwcQxEnv("FX_GLIDE_QX_MAXDELTA", sDelta);

  packedDudx = (mDudxOfs & ~SST_OVERLAY_DELTA_OFFSET) |
               (phaseX & SST_OVERLAY_DELTA_OFFSET);
  phaseYOfs  = phaseY & SST_OVERLAY_DELTA_OFFSET;

  sCfgNew = (sCfg & ~(SST_OVERLAY_FILTER_MODE |
                      SST_OVERLAY_HORIZ_SCALE_EN |
                      SST_OVERLAY_VERT_SCALE_EN)) |
            scaleBits |
            (filter << SST_OVERLAY_FILTER_MODE_SHIFT);

  /* Quadrunx: the master overlay gets the same scaler and filter.  Done
   * before the slave-only writes, so whatever the slave snoops from these
   * is overwritten by its own values below.  No disable pulse here: the
   * master also scans out the desktop. */
  if (quad)
  {
    hwcQxState.mVidProcCfg = mCfg;
    hwcQxState.mVidOverlayDudx = mDudx;
    hwcQxState.mVidOverlayDvdy = mDvdy;
    hwcQxState.mVidOverlayDudxOffsetSrcWidth = mDudxOfs;
    hwcQxState.mVidOverlayDvdyOffset = mDvdyOfs;
    hwcQxState.masterSaved = FXTRUE;

    mCfgNew = (mCfg & ~(SST_OVERLAY_FILTER_MODE |
                        SST_OVERLAY_HORIZ_SCALE_EN |
                        SST_OVERLAY_VERT_SCALE_EN)) |
              scaleBits |
              (filter << SST_OVERLAY_FILTER_MODE_SHIFT);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDudx, stepX);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDvdy, stepY);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, packedDudx);
    HWC_IO_STORE(bInfo->regInfo, vidOverlayDvdyOffset, phaseYOfs);
    HWC_IO_STORE(bInfo->regInfo, vidProcCfg, mCfgNew);
  }

  /* Geometry identical to the master (same set as hwcSetSLIAAMode). */
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidScreenSize, mSize);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayStartCoords, mStart);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayEndScreenCoord, mEnd);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidDesktopOverlayStride, mStride);

  /* 1:1 bilinear DDA with half-pixel start phase. */
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDudx, stepX);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDvdy, stepY);

  /* The master's FIFO threshold is written before snooping is set up,
   * so the slave would otherwise keep its power-on value. */
  /* Braces needed: the HWC_IO_* macros expand to two statements. */
  if (hwcQxEnv("FX_GLIDE_QX_THOLD", 1)) {
    HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidPixelBufThold, mThold);
  }
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidMaxRGBDelta, newDelta);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, packedDudx);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidOverlayDvdyOffset, phaseYOfs);

  /* Pulse the video processor so the new setup takes effect,
   * as the reference SLI/AA code does after changing the size. */
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidProcCfg, sCfgNew & ~SST_VIDEO_PROCESSOR_EN);
  HWC_IO_STORE_SLAVE(1, bInfo->regInfo, vidProcCfg, sCfgNew | (sCfg & SST_VIDEO_PROCESSOR_EN));

  /* Read back the slave, and prove the master was not touched. */
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidProcCfg, rb[0]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDudx, rb[1]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDvdy, rb[2]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, rb[3]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidOverlayDvdyOffset, rb[4]);
  HWC_IO_LOAD(bInfo->regInfo, vidProcCfg, rb[5]);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudx, rb[6]);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDvdy, rb[7]);
  HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, rb[8]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidPixelBufThold, rb[9]);
  HWC_IO_LOAD_SLAVE(1, bInfo->regInfo, vidMaxRGBDelta, rb[10]);
  hwcQxLog("Quincunx 1.1: thold master=%08lx slave before=%08lx after=%08lx | maxRGBDelta slave before=%08lx after=%08lx | scaler=%lu stepX=%05lx stepY=%05lx\n",
           (unsigned long)mThold, (unsigned long)sThold, (unsigned long)rb[9],
           (unsigned long)sDelta, (unsigned long)rb[10],
           (unsigned long)scaler, (unsigned long)stepX, (unsigned long)stepY);

  hwcQxLog("Quincunx 1.1: slave after cfg=%08lx dudx=%08lx dvdy=%08lx dudxOfs=%08lx dvdyOfs=%08lx | master cfg=%08lx dudx=%08lx dvdy=%08lx dudxOfs=%08lx\n",
           (unsigned long)rb[0], (unsigned long)rb[1], (unsigned long)rb[2],
           (unsigned long)rb[3], (unsigned long)rb[4], (unsigned long)rb[5],
           (unsigned long)rb[6], (unsigned long)rb[7], (unsigned long)rb[8]);

  ok = ((rb[0] & (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN |
                  SST_OVERLAY_FILTER_MODE)) ==
        (sCfgNew & (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN |
                    SST_OVERLAY_FILTER_MODE))) &&
       ((rb[1] & SST_OVERLAY_DELTA) == stepX) &&
       ((rb[2] & SST_OVERLAY_DELTA) == stepY) &&
       (rb[3] == packedDudx) &&
       ((rb[4] & SST_OVERLAY_DELTA_OFFSET) == phaseYOfs) &&
       (quad ? ((rb[5] == mCfgNew) &&
                ((rb[6] & SST_OVERLAY_DELTA) == stepX) &&
                ((rb[7] & SST_OVERLAY_DELTA) == stepY) &&
                (rb[8] == packedDudx))
             : ((rb[5] == mCfg) &&
                (rb[6] == mDudx) &&
                (rb[7] == mDvdy) &&
                (rb[8] == mDudxOfs)));

  if (!ok)
  {
    hwcQxLog("Quincunx 1.1: readback mismatch, rolling back to stock 2x AA\n");
    hwcQuincunxRestore(bInfo);
    return FXFALSE;
  }

  hwcQxLog("Quincunx 1.1: ACTIVE%s %lux%lu phaseX=%05lx phaseY=%05lx filter=%lu\n",
           modeName,
           (unsigned long)width, (unsigned long)height,
           (unsigned long)(phaseX & SST_OVERLAY_DELTA_OFFSET),
           (unsigned long)phaseYOfs, (unsigned long)filter);
  GDBG_INFO(80, "%s: Quincunx 1.1 active\n", FN_NAME);
  return FXTRUE;
#undef FN_NAME
}
