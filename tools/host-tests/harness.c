/* Host-side harness for the Quincunx 1.0 minihwc block.
 * Uses the real h5 register headers and hwcio.h macros, with the
 * master and slave IO apertures backed by ordinary memory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <3dfx.h>
#include <h3regs.h>
#include <h3defs.h>
#include <minihwc.h>

/* hwcio.h needs these name tables only for debug output. */
char *ioRegNames[256];
char *cmdAGPRegNames[256];
char *waxRegNames[256];
char *sstRegNames[256];
#include "hwcio.h"

#ifndef GDBG_INFO
#define GDBG_INFO(...) ((void)0)
#endif

static const char *envNames[16];
static const char *envVals[16];
static int envCount;
static const char *testGetenv(const char *name)
{
  int i;
  for (i = 0; i < envCount; i++)
    if (strcmp(envNames[i], name) == 0)
      return envVals[i];
  return NULL;
}
#undef GETENV
#define GETENV(a) testGetenv(a)

#include "qxblock.inc"

static SstIORegs master, slave;
static hwcBoardInfo board;
static int failures;

#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

static void setupStock(FxU32 w, FxU32 h, FxU32 pixFmt)
{
  memset((void *)&master, 0, sizeof(master));
  memset(&board, 0, sizeof(board));
  /* What hwcInitVideo + SLI/AA leave behind for 2-chip 2x, 32bpp. */
  master.vidProcCfg = SST_VIDEO_PROCESSOR_EN | SST_OVERLAY_EN |
                      SST_OVERLAY_TILED_EN | pixFmt | SST_OVERLAY_FILTER_POINT;
  master.vidScreenSize = (h << SST_VIDEO_SCREEN_HEIGHT_SHIFT) | w;
  master.vidOverlayStartCoords = 0;
  master.vidOverlayEndScreenCoord = ((h - 1) << SST_OVERLAY_Y_SHIFT) | (w - 1);
  master.vidDesktopOverlayStride = (20 << 16) | 20;
  master.vidOverlayDudxOffsetSrcWidth = ((w << 2) << 19);
  master.vidOverlayDudx = 0;
  master.vidOverlayDvdy = 0;
  master.vidOverlayDvdyOffset = 0;
  slave = master;                 /* snooped copy */
  slave.vidMaxRGBDelta = 0x100810;

  board.pciInfo.deviceID = 0x0009;   /* Napalm */
  board.pciInfo.numChips = 2;
  board.h3pixelSample = 2;
  board.h3nwaySli = 1;
  board.h3analogSli = 0;
  board.h3quincunx = 1;
  board.buffInfo.enable2ndbuffer = FXFALSE;
  board.vidInfo.xRes = w;
  board.vidInfo.yRes = h;
  board.regInfo.ioMemBase = (unsigned long)&master;
  board.regInfo.slaveIOBase[0] = (unsigned long)&slave;
  hwcQxState.owner = NULL;
}

static void testActive(FxU32 w, FxU32 h)
{
  SstIORegs masterBefore, slaveBefore;
  FxU32 expPhaseX = (0x40000 + (w >> 1)) & SST_OVERLAY_DELTA_OFFSET;
  FxU32 expPhaseY = (0x40000 + (h >> 1)) & SST_OVERLAY_DELTA_OFFSET;
  char label[64];

  setupStock(w, h, SST_OVERLAY_PIXEL_RGB32U);
  masterBefore = master;
  slaveBefore = slave;
  sprintf(label, "%ux%u apply", (unsigned)w, (unsigned)h);
  CHECK(hwcQuincunxApply(&board) == FXTRUE, label);
  CHECK(memcmp((void *)&master, (void *)&masterBefore, sizeof(master)) == 0, "master untouched");
  CHECK((slave.vidProcCfg & (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN)) ==
        (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN), "slave scale on");
  CHECK((slave.vidProcCfg & SST_OVERLAY_FILTER_MODE) == SST_OVERLAY_FILTER_BILINEAR, "slave bilinear");
  CHECK(slave.vidProcCfg & SST_VIDEO_PROCESSOR_EN, "slave video enabled");
  CHECK((slave.vidProcCfg & SST_OVERLAY_PIXEL_FORMAT) == SST_OVERLAY_PIXEL_RGB32U, "slave format kept");
  CHECK(slave.vidOverlayDudx == 0xFFFFF && slave.vidOverlayDvdy == 0xFFFFF, "slave 1:1 step");
  CHECK((slave.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_DELTA_OFFSET) == expPhaseX, "phase X");
  CHECK((slave.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_FETCH_SIZE) ==
        (master.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_FETCH_SIZE), "fetch size kept");
  CHECK(slave.vidOverlayDvdyOffset == expPhaseY, "phase Y");
  CHECK(slave.vidScreenSize == master.vidScreenSize &&
        slave.vidOverlayEndScreenCoord == master.vidOverlayEndScreenCoord, "geometry");
  CHECK(slave.vidMaxRGBDelta == 0x100810, "vidMaxRGBDelta untouched");
  CHECK(hwcQxState.owner == &board, "owner set");

  hwcQuincunxRestore(&board);
  CHECK(memcmp((void *)&slave, (void *)&slaveBefore, sizeof(slave)) == 0, "slave restored exactly");
  CHECK(memcmp((void *)&master, (void *)&masterBefore, sizeof(master)) == 0, "master untouched after restore");
  CHECK(hwcQxState.owner == NULL, "owner cleared");
  printf("%4ux%-4u phaseX=%05x phaseY=%05x\n", (unsigned)w, (unsigned)h,
         (unsigned)expPhaseX, (unsigned)expPhaseY);
}

static void testRejected(const char *what)
{
  SstIORegs slaveBefore = slave;
  CHECK(hwcQuincunxApply(&board) == FXFALSE, what);
  CHECK(memcmp((void *)&slave, (void *)&slaveBefore, sizeof(slave)) == 0, "no slave write when rejected");
  CHECK(hwcQxState.owner == NULL, "no owner when rejected");
  CHECK(board.h3quincunx == 0, "request cleared when rejected");
}

int main(void)
{
  static const FxU32 modes[][2] = {
    {640, 480}, {800, 600}, {1024, 768}, {1280, 1024}, {1600, 1200}
  };
  unsigned i;

  for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    testActive(modes[i][0], modes[i][1]);

  /* Real master state read on a Voodoo5 5500 under XP (qx.log, 640x480):
   * cfg=036c0181 = processor, desktop (RGB32, tiled), overlay (RGB32, tiled). */
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  master.vidProcCfg = 0x036c0181UL;
  master.vidOverlayDudxOffsetSrcWidth = 0x50000000UL;
  slave = master;
  CHECK(hwcQuincunxApply(&board) == FXTRUE, "XP master state 036c0181 accepted");
  CHECK((slave.vidProcCfg & SST_DESKTOP_EN) != 0, "slave desktop bit kept");
  CHECK((slave.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_FETCH_SIZE) == 0x50000000UL, "fetch size kept (XP)");
  CHECK((slave.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_DELTA_OFFSET) == 0x40140, "phase X (XP)");
  CHECK(board.h3quincunx == 1, "request kept when active");
  hwcQuincunxRestore(&board);
  CHECK(slave.vidProcCfg == 0x036c0181UL, "XP slave cfg restored");

  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); master.vidProcCfg |= SST_CHROMA_EN; testRejected("chroma key on");

  setupStock(1024, 768, SST_OVERLAY_PIXEL_RGB565D);
  CHECK(hwcQuincunxApply(&board) == FXTRUE, "16bpp dithered accepted");
  hwcQuincunxRestore(&board);

  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.h3quincunx = 0;   testRejected("not requested");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.h3analogSli = 1;  testRejected("analog SLI");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.h3nwaySli = 2;    testRejected("SLI on");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.buffInfo.enable2ndbuffer = FXTRUE; testRejected("2 samples per chip");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.pciInfo.numChips = 4; testRejected("4 chips");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.regInfo.slaveIOBase[0] = 0; testRejected("no slave mapping");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); master.vidProcCfg |= SST_HALF_MODE; testRejected("half mode");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); master.vidProcCfg |= SST_OVERLAY_HORIZ_SCALE_EN; testRejected("master scaled");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.vidInfo.xRes = 512; testRejected("app res != screen");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  slave.vidProcCfg = (slave.vidProcCfg & ~SST_OVERLAY_PIXEL_FORMAT) | SST_OVERLAY_PIXEL_RGB565D;
  testRejected("slave format mismatch");

  /* Environment overrides for calibration. */
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  envNames[0] = "FX_GLIDE_QX_PHASE_X"; envVals[0] = "0x0";
  envNames[1] = "FX_GLIDE_QX_FILTER";  envVals[1] = "0";
  envCount = 2;
  CHECK(hwcQuincunxApply(&board) == FXTRUE, "override apply");
  CHECK((slave.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_DELTA_OFFSET) == 0, "phase X override");
  CHECK((slave.vidProcCfg & SST_OVERLAY_FILTER_MODE) == SST_OVERLAY_FILTER_POINT, "filter override");
  hwcQuincunxRestore(&board);
  envCount = 0;

  /* Re-apply on the same board restores first, then applies again. */
  setupStock(800, 600, SST_OVERLAY_PIXEL_RGB32U);
  {
    SstIORegs slaveStock = slave;
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "first apply");
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "second apply");
    hwcQuincunxRestore(&board);
    CHECK(memcmp((void *)&slave, (void *)&slaveStock, sizeof(slave)) == 0, "stock after double apply");
  }

  /* Smoother: same video programming on both chips. */
  setupStock(800, 600, SST_OVERLAY_PIXEL_RGB32U);
  board.h3quincunx = 2;
  {
    SstIORegs masterStock = master;
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "smoother apply");
    CHECK((master.vidProcCfg & SST_OVERLAY_FILTER_MODE) == SST_OVERLAY_FILTER_BILINEAR, "smoother master bilinear");
    CHECK(master.vidOverlayDvdyOffset == slave.vidOverlayDvdyOffset, "smoother same phase Y");
    hwcQuincunxRestore(&board);
    CHECK(memcmp((void *)&master, (void *)&masterStock, sizeof(master)) == 0, "smoother master restored");
  }
  /* Sharper (1) never touches the master: covered by testActive. */
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); board.h3quincunx = 2; master.vidProcCfg |= SST_CHROMA_EN;
  {
    SstIORegs masterStock = master;
    testRejected("smoother chroma key on");
    CHECK(memcmp((void *)&master, (void *)&masterStock, sizeof(master)) == 0, "smoother rejected: master untouched");
  }

  /* Smoother: full register check on the master, exact restore. */
  setupStock(1024, 768, SST_OVERLAY_PIXEL_RGB32U);
  master.vidProcCfg = 0x036c0181UL; slave = master; board.h3quincunx = 2;
  {
    SstIORegs masterStock = master, slaveStock = slave;
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "smoother apply (XP cfg)");
    CHECK(master.vidProcCfg == (0x036c0181UL | 0x3c000UL), "smoother master cfg = stock + scaler + bilinear");
    CHECK(slave.vidProcCfg == master.vidProcCfg, "smoother slave cfg = master cfg");
    CHECK(master.vidOverlayDudx == 0xFFFFF && master.vidOverlayDvdy == 0xFFFFF, "smoother master 1:1 step");
    CHECK(master.vidOverlayDudxOffsetSrcWidth == slave.vidOverlayDudxOffsetSrcWidth, "smoother same phase X / fetch");
    CHECK((master.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_FETCH_SIZE) == (masterStock.vidOverlayDudxOffsetSrcWidth & SST_OVERLAY_FETCH_SIZE), "fetch size unchanged");
    CHECK(board.h3quincunx == 2, "smoother request kept");
    hwcQuincunxRestore(&board);
    CHECK(memcmp((void *)&master, (void *)&masterStock, sizeof(master)) == 0, "smoother master restored exactly");
    CHECK(memcmp((void *)&slave, (void *)&slaveStock, sizeof(slave)) == 0, "smoother slave restored exactly");
  }

  /* 1.1 test knobs */
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  master.vidPixelBufThold = 0x20820; slave.vidPixelBufThold = 0x3ffff;
  {
    SstIORegs slaveStock = slave;
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "thold apply");
    CHECK(slave.vidPixelBufThold == 0x20820, "thold copied to slave");
    hwcQuincunxRestore(&board);
    CHECK(memcmp((void *)&slave, (void *)&slaveStock, sizeof(slave)) == 0, "thold restored");
  }
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  envNames[0] = "FX_GLIDE_QX_SCALER"; envVals[0] = "1";
  envNames[1] = "FX_GLIDE_QX_FILTER"; envVals[1] = "1";
  envNames[2] = "FX_GLIDE_QX_MAXDELTA"; envVals[2] = "0xffffff";
  envNames[3] = "FX_GLIDE_QX_DVDY"; envVals[3] = "0x80000";
  envCount = 4;
  {
    SstIORegs slaveStock = slave;
    CHECK(hwcQuincunxApply(&board) == FXTRUE, "knob apply");
    CHECK((slave.vidProcCfg & (SST_OVERLAY_HORIZ_SCALE_EN | SST_OVERLAY_VERT_SCALE_EN)) == SST_OVERLAY_HORIZ_SCALE_EN, "H only");
    CHECK((slave.vidProcCfg & SST_OVERLAY_FILTER_MODE) == SST_OVERLAY_FILTER_2X2, "2x2 filter");
    CHECK(slave.vidMaxRGBDelta == 0xffffff, "maxdelta override");
    CHECK(slave.vidOverlayDvdy == 0x80000, "dvdy override");
    hwcQuincunxRestore(&board);
    CHECK(memcmp((void *)&slave, (void *)&slaveStock, sizeof(slave)) == 0, "knobs restored");
  }
  envCount = 0;
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); master.vidProcCfg |= SST_VIDEO_2X_MODE_EN; testRejected("2x mode without opt-in");
  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U); master.vidProcCfg |= SST_VIDEO_2X_MODE_EN;
  envNames[0] = "FX_GLIDE_QX_ALLOW_2X"; envVals[0] = "1"; envCount = 1;
  CHECK(hwcQuincunxApply(&board) == FXTRUE, "2x mode with opt-in");
  hwcQuincunxRestore(&board); envCount = 0;

  setupStock(640, 480, SST_OVERLAY_PIXEL_RGB32U);
  master.vidPixelBufThold = 0x20820; slave.vidPixelBufThold = 0x3ffff;
  envNames[0] = "FX_GLIDE_QX_THOLD"; envVals[0] = "0"; envCount = 1;
  CHECK(hwcQuincunxApply(&board) == FXTRUE, "thold off apply");
  CHECK(slave.vidPixelBufThold == 0x3ffff, "thold not copied when off");
  hwcQuincunxRestore(&board); envCount = 0;

  printf(failures ? "%d FAILURES\n" : "All minihwc Quincunx checks passed\n", failures);
  return failures ? 1 : 0;
}
