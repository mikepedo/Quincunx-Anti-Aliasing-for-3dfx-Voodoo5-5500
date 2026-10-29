/*-------------------------------------------------------------------
  Quincunx 1.0 - sample placement

  On a dual VSA-100 board running 2-sample AA without SLI each chip
  renders one complete sample and the digital AA mux averages the two
  video PIPEs.  Quincunx keeps chip 0 on the pixel centre and moves
  chip 1 onto the pixel corner; minihwc then turns chip 1's overlay
  scaler into a half-phase bilinear filter, so its PIPE carries the
  average of the four corners around every pixel:

      OUT = 1/2 centre + 1/8 (four corners)

  The jitter values use the same string format as FX_GLIDE_AA2_OFFSET_*
  and can be overridden for calibration on real hardware.

  Measured on a Voodoo5 5500 (read-back of both chips, 1/16 pixel steps):
  in Glide screen space a pixel's sample sits on the integer coordinate,
  and the AA offsets act as vertex offsets, so a value of +0.5 places the
  sample at -0.5, i.e. on the pixel's top-left corner.  With centre 0.0
  and corner 0.5 the slave's bilinear half-phase filter averages the four
  corners symmetrically around the master sample (checked on screen with
  the qxpattern colour test).
  -------------------------------------------------------------------*/
static FxBool qxJitterRowSaved = FXFALSE;
static int    qxJitterRowIndex = 0;
static FxU32  qxJitterSavedX[8];
static FxU32  qxJitterSavedY[8];

static FxU32
_grQxJitterValue(const char *envName, const char *defVal, FxBool negate)
{
  const char *envStr = GETENV(envName);
  float value = (float)atof((envStr != NULL) ? envStr : defVal);

  if (negate)
    value = -value;

  /* Same encoding as GLIDE_34GETENV in gpci.c (3.4, +0.5 bias). */
  return (FxU32)((((signed char)(value * 16.0f)) + 8) & 0x7f);
}

/* Quincunx 1.1: FX_GLIDE_AA_CLIP (on by default) keeps both chips off
 * row 0 and column 0 whenever AA is on.  With Quincunx the master samples
 * exactly there, so the whole first line was lost (seen on hardware with a
 * lower-left origin: the bottom line of the qxpattern frame disappeared).
 * It is turned off for Quincunx opens unless the user sets it explicitly. */
static FxBool qxAaClipSaved = FXFALSE;
static FxBool qxAaClipValue;

/* Put back the jitter row and the AA clip changed by a Quincunx open. */
static void
_grQuincunxRestoreJitter(void)
{
  int i;

  if (qxAaClipSaved)
  {
    _GlideRoot.environment.aaClip = qxAaClipValue;
    qxAaClipSaved = FXFALSE;
  }

  if (!qxJitterRowSaved)
    return;

  for (i = 0; i < 8; i++)
  {
    _GlideRoot.environment.aaXOffset[qxJitterRowIndex][i] = qxJitterSavedX[i];
    _GlideRoot.environment.aaYOffset[qxJitterRowIndex][i] = qxJitterSavedY[i];
  }
  qxJitterRowSaved = FXFALSE;
}

static void
_grQuincunxSetup(GrGC *gc)
{
#define FN_NAME "_grQuincunxSetup"
  const char *envStr;
  int i;

  /* Undo the jitter of a previous Quincunx open in this process. */
  _grQuincunxRestoreJitter();

  gc->bInfo->h3quincunx = 0;

  if (_GlideRoot.environment.quincunxMode == 0)
    return;

  /* 10 = Quincunx sharper, 11 = Quincunx smoother (Voodoo5 5500 only). */
  if ((_GlideRoot.environment.quincunxMode != 10) &&
      (_GlideRoot.environment.quincunxMode != 11))
  {
    GDBG_INFO(80, "%s: Quincunx %d has no hardware path yet, running stock 2x AA\n",
              FN_NAME, _GlideRoot.environment.quincunxMode);
    return;
  }

  /* Dual chip, 2 samples, one sample per chip, no SLI, digital mux. */
  if (!IS_NAPALM(gc->bInfo->pciInfo.deviceID) ||
      (gc->chipCount != 2) ||
      (gc->grPixelSample != 2) ||
      (gc->grSamplesPerChip != 1) ||
      (gc->sliCount != 1) ||
      (gc->bInfo->h3analogSli != 0) ||
      (gc->sampleOffsetIndex < 0) || (gc->sampleOffsetIndex > 14))
  {
    GDBG_INFO(80, "%s: configuration not eligible (chips=%d samples=%d perChip=%d sli=%d analog=%d)\n",
              FN_NAME, gc->chipCount, gc->grPixelSample, gc->grSamplesPerChip,
              gc->sliCount, gc->bInfo->h3analogSli);
    return;
  }

  /* Quincunx 1.1 variants (h3quincunx, read by minihwc):
   *   1 sharper  (10): master centre unfiltered + slave corner filtered
   *                    = 1/2 centre + 1/8 x 4 corners (the 1.0 kernel)
   *   2 smoother (11): as sharper, but the master scaler filters too, so the
   *                    master gives the mean of 4 centres and the slave the
   *                    mean of 4 corners.  The two means sit a quarter pixel
   *                    apart: an even 4-tap box per axis (taps 0.5 px apart),
   *                    and the whole image moves by a quarter pixel.
   * Both render like 2x AA; the smoother adds the filter read on the master
   * (Quake III 1024x768x32 on a 5500: 2x 31.5, sharper 30.4 fps; smoother
   * 28.3 fps, measured with the overscan of the test builds).
   *
   * The slave's filter reads one row below the screen, which does not
   * exist: the bottom line mixes in a little of another buffer.  An
   * overscan row/column fixed it (tests 2-8) but was dropped by choice for
   * the 0.3 fps it cost. */
  gc->bInfo->h3quincunx = (_GlideRoot.environment.quincunxMode == 11) ? 2 : 1;
  GDBG_INFO(80, "%s: Quincunx %s requested for this open\n", FN_NAME,
            (gc->bInfo->h3quincunx == 2) ? "smoother" : "sharper");

  if (GETENV("FX_GLIDE_AA_CLIP") == NULL)
  {
    qxAaClipValue = _GlideRoot.environment.aaClip;
    qxAaClipSaved = FXTRUE;
    _GlideRoot.environment.aaClip = FXFALSE;
  }

  /* FX_GLIDE_QX_JITTER=0 keeps the stock/FX_GLIDE_AA2_* jitter, which is
   * what the zero-jitter diagnostic profiles need. */
  envStr = GETENV("FX_GLIDE_QX_JITTER");
  if ((envStr != NULL) && (atol(envStr) == 0))
    return;

  {
    /* Default: invert Y when the application uses a lower-left origin,
     * so the corner stays the top-left corner on the screen. */
    FxBool flipY = (gc->state.origin == GR_ORIGIN_LOWER_LEFT);
    FxU32 centreX, centreY, cornerX, cornerY;

    envStr = GETENV("FX_GLIDE_QX_YFLIP");
    if (envStr != NULL)
      flipY = (atol(envStr) != 0) && (gc->state.origin == GR_ORIGIN_LOWER_LEFT);

    centreX = _grQxJitterValue("FX_GLIDE_QX_CENTER_X", "0.0", FXFALSE);
    centreY = _grQxJitterValue("FX_GLIDE_QX_CENTER_Y", "0.0", flipY);
    cornerX = _grQxJitterValue("FX_GLIDE_QX_CORNER_X", "0.5", FXFALSE);
    cornerY = _grQxJitterValue("FX_GLIDE_QX_CORNER_Y", "0.5", flipY);

    qxJitterRowIndex = gc->sampleOffsetIndex;
    for (i = 0; i < 8; i++)
    {
      qxJitterSavedX[i] = _GlideRoot.environment.aaXOffset[qxJitterRowIndex][i];
      qxJitterSavedY[i] = _GlideRoot.environment.aaYOffset[qxJitterRowIndex][i];

      /* _grAAOffsetValue: chip c uses entries 2c and 2c+1 (mod 8). */
      if (((i >> 1) & 1) == 0)
      {
        _GlideRoot.environment.aaXOffset[qxJitterRowIndex][i] = centreX;
        _GlideRoot.environment.aaYOffset[qxJitterRowIndex][i] = centreY;
      }
      else
      {
        _GlideRoot.environment.aaXOffset[qxJitterRowIndex][i] = cornerX;
        _GlideRoot.environment.aaYOffset[qxJitterRowIndex][i] = cornerY;
      }
    }
    qxJitterRowSaved = FXTRUE;

    GDBG_INFO(80, "%s: jitter row %d centre=(%02x,%02x) corner=(%02x,%02x) flipY=%d\n",
              FN_NAME, qxJitterRowIndex, centreX, centreY, cornerX, cornerY, flipY);
  }
#undef FN_NAME
}
