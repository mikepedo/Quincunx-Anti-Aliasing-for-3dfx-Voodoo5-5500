/*
** qxpattern - Quincunx 1.0 test patterns for Voodoo5 5500 (stand-alone)
**
** Single-file version for MinGW: no Glide headers, no import library,
** no tlib.  It loads glide3x.dll at run time (first from the program's
** folder, then from system32), so it always tests the DLL actually
** installed on the machine.
**
** Build (MinGW, from this folder):
**     gcc -O2 -Wall -o qxpattern.exe qxpattern.c -lgdi32
**
** Usage:  qxpattern [-r 640x480] [-p aa2argb8888] [-o u|l]
**   -r   640x480, 800x600, 1024x768, 1280x1024, 1600x1200
**   -p   argb8888, aa2argb8888, rgb565, aa2rgb565
**   -o   u = upper-left origin (Glide games), l = lower-left (MesaFX)
**
** Keys:
**   1 flat 50% grey field          5 nine single-pixel impulses
**   2 alternating 1-pixel columns  6 jitter calibration (half-pixel edges)
**   3 alternating 1-pixel rows     7 moving scene
**   4 white rectangle              8 edge frame (rows/cols 0..2 per side)
**                                  9 nine impulses 2 pixels inside the edges
**                                  0 sample-position probe (use with the
**                                    FX_GLIDE_SCREENSHOT_KEY screenshot)
**   c  colour test for the Quincunx filter direction
**                                  q or ESC quit
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Glide 3 types and constants (values from glide.h / g3ext.h) ---- */

typedef unsigned long GrContext_t;
typedef void (__stdcall *GrProc)(void);

#define GR_RESOLUTION_640x480    0x7
#define GR_RESOLUTION_800x600    0x8
#define GR_RESOLUTION_1024x768   0xC
#define GR_RESOLUTION_1280x1024  0xD
#define GR_RESOLUTION_1600x1200  0xE
#define GR_REFRESH_60Hz          0x0
#define GR_COLORFORMAT_ABGR      0x1
#define GR_ORIGIN_UPPER_LEFT     0x0
#define GR_ORIGIN_LOWER_LEFT     0x1
#define GR_PIXFMT_RGB_565        0x0003
#define GR_PIXFMT_ARGB_8888      0x0005
#define GR_PIXFMT_AA_2_RGB_565   0x0006
#define GR_PIXFMT_AA_2_ARGB_8888 0x0008
#define GR_COMBINE_FUNCTION_LOCAL 0x1
#define GR_COMBINE_FACTOR_NONE    0x0
#define GR_COMBINE_LOCAL_CONSTANT 0x1
#define GR_COMBINE_OTHER_NONE     0x2
#define GR_PARAM_XY              0x01
#define GR_PARAM_ENABLE          0x01
#define GR_WDEPTH_MIN_MAX        0x27
#define GR_DEPTHBUFFER_DISABLE   0x0
#define GR_VERSION               0xa4

typedef void        (__stdcall *PFN_grGlideInit)(void);
typedef void        (__stdcall *PFN_grGlideShutdown)(void);
typedef void        (__stdcall *PFN_grSstSelect)(int);
typedef GrContext_t (__stdcall *PFN_grSstWinOpen)(unsigned long, int, int, int, int, int, int);
typedef GrContext_t (__stdcall *PFN_grSstWinOpenExt)(unsigned long, int, int, int, int, int, int, int);
typedef int         (__stdcall *PFN_grSstWinClose)(GrContext_t);
typedef unsigned long (__stdcall *PFN_grGet)(unsigned long, unsigned long, long *);
typedef const char *(__stdcall *PFN_grGetString)(unsigned long);
typedef void        (__stdcall *PFN_grVertexLayout)(unsigned long, long, unsigned long);
typedef void        (__stdcall *PFN_grColorCombine)(int, int, int, int, int);
typedef void        (__stdcall *PFN_grConstantColorValue)(unsigned long);
typedef void        (__stdcall *PFN_grDepthBufferMode)(int);
typedef void        (__stdcall *PFN_grClipWindow)(unsigned long, unsigned long, unsigned long, unsigned long);
typedef void        (__stdcall *PFN_grBufferClear)(unsigned long, unsigned char, unsigned long);
typedef void        (__stdcall *PFN_grBufferSwap)(unsigned long);
typedef void        (__stdcall *PFN_grDrawTriangle)(const void *, const void *, const void *);
typedef GrProc      (__stdcall *PFN_grGetProcAddress)(char *);

static PFN_grGlideInit          p_grGlideInit;
static PFN_grGlideShutdown      p_grGlideShutdown;
static PFN_grSstSelect          p_grSstSelect;
static PFN_grSstWinOpen         p_grSstWinOpen;
static PFN_grSstWinClose        p_grSstWinClose;
static PFN_grGet                p_grGet;
static PFN_grGetString          p_grGetString;
static PFN_grVertexLayout       p_grVertexLayout;
static PFN_grColorCombine       p_grColorCombine;
static PFN_grConstantColorValue p_grConstantColorValue;
static PFN_grDepthBufferMode    p_grDepthBufferMode;
static PFN_grClipWindow         p_grClipWindow;
static PFN_grBufferClear        p_grBufferClear;
static PFN_grBufferSwap         p_grBufferSwap;
static PFN_grDrawTriangle       p_grDrawTriangle;
static PFN_grGetProcAddress     p_grGetProcAddress;

static HMODULE glideDll;

/* glide3x.dll exports __stdcall names, decorated (_name@bytes) when built
 * with Visual C++.  Try the decorated name first, then the plain one. */
static FARPROC glideProc(const char *name, int argBytes)
{
  char decorated[96];
  FARPROC proc;

  sprintf(decorated, "_%s@%d", name, argBytes);
  proc = GetProcAddress(glideDll, decorated);
  if (proc == NULL)
    proc = GetProcAddress(glideDll, name);
  if (proc == NULL)
    printf("glide3x.dll: funzione %s non trovata\n", name);
  return proc;
}

#define LOAD(fn, bytes) \
  ((p_##fn = (PFN_##fn)(void *)glideProc(#fn, bytes)) != NULL)

static int loadGlide(void)
{
  char path[MAX_PATH];
  int ok = 1;

  glideDll = LoadLibraryA("glide3x.dll");
  if (glideDll == NULL) {
    printf("Impossibile caricare glide3x.dll\n");
    return 0;
  }
  if (GetModuleFileNameA(glideDll, path, sizeof(path)))
    printf("Uso: %s\n", path);

  ok &= LOAD(grGlideInit, 0);
  ok &= LOAD(grGlideShutdown, 0);
  ok &= LOAD(grSstSelect, 4);
  ok &= LOAD(grSstWinOpen, 28);
  ok &= LOAD(grSstWinClose, 4);
  ok &= LOAD(grGet, 12);
  ok &= LOAD(grGetString, 4);
  ok &= LOAD(grVertexLayout, 12);
  ok &= LOAD(grColorCombine, 20);
  ok &= LOAD(grConstantColorValue, 4);
  ok &= LOAD(grDepthBufferMode, 4);
  ok &= LOAD(grClipWindow, 16);
  ok &= LOAD(grBufferClear, 12);
  ok &= LOAD(grBufferSwap, 4);
  ok &= LOAD(grDrawTriangle, 12);
  ok &= LOAD(grGetProcAddress, 4);
  return ok;
}

/* ---- window and keyboard ---- */

static volatile int lastKey = 0;
static volatile int quitRequested = 0;

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_CHAR:
    lastKey = (int)wParam;
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE)
      lastKey = 27;
    return 0;
  case WM_CLOSE:
    quitRequested = 1;
    return 0;
  case WM_SETCURSOR:
    SetCursor(NULL);
    return TRUE;
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND makeWindow(void)
{
  WNDCLASSA wc;
  HWND hwnd;

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc   = wndProc;
  wc.hInstance     = GetModuleHandleA(NULL);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = "qxpattern";
  RegisterClassA(&wc);

  hwnd = CreateWindowExA(WS_EX_TOPMOST, "qxpattern", "qxpattern",
                         WS_POPUP | WS_VISIBLE, 0, 0,
                         GetSystemMetrics(SM_CXSCREEN),
                         GetSystemMetrics(SM_CYSCREEN),
                         NULL, NULL, wc.hInstance, NULL);
  if (hwnd != NULL) {
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
  }
  return hwnd;
}

static void pumpMessages(void)
{
  MSG msg;
  while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT)
      quitRequested = 1;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
}

/* ---- patterns (same as the tests/qxpattern.c version) ---- */

typedef struct { float x, y; } QxVertex;

static float scrW = 640.0f;
static float scrH = 480.0f;
static int   lowerLeft = 0;

static float qxY(float y)
{
  return lowerLeft ? (scrH - y) : y;
}

static void qxTriangle(float x0, float y0, float x1, float y1,
                       float x2, float y2, unsigned long color)
{
  QxVertex a, b, c;
  a.x = x0; a.y = qxY(y0);
  b.x = x1; b.y = qxY(y1);
  c.x = x2; c.y = qxY(y2);
  p_grConstantColorValue(color);
  p_grDrawTriangle(&a, &b, &c);
}

/* Fill [x0,x1) x [y0,y1) in screen pixels, y measured from the top. */
static void qxRect(float x0, float y0, float x1, float y1, unsigned long color)
{
  qxTriangle(x0, y0, x1, y0, x1, y1, color);
  qxTriangle(x0, y0, x1, y1, x0, y1, color);
}

static void patternFlat(void)
{
  qxRect(scrW * 0.25f, scrH * 0.25f, scrW * 0.75f, scrH * 0.75f, 0x808080);
}

static void patternColumns(void)
{
  int x, x0 = (int)(scrW * 0.25f), x1 = (int)(scrW * 0.75f);
  for (x = x0; x < x1; x += 2)
    qxRect((float)x, scrH * 0.25f, (float)(x + 1), scrH * 0.75f, 0xffffff);
}

static void patternRows(void)
{
  int y, y0 = (int)(scrH * 0.25f), y1 = (int)(scrH * 0.75f);
  for (y = y0; y < y1; y += 2)
    qxRect(scrW * 0.25f, (float)y, scrW * 0.75f, (float)(y + 1), 0xffffff);
}

static void patternRectangle(void)
{
  qxRect((float)(int)(scrW * 0.3f), (float)(int)(scrH * 0.3f),
         (float)(int)(scrW * 0.7f), (float)(int)(scrH * 0.7f), 0xffffff);
}

static void patternImpulses(void)
{
  int xs[3], ys[3], i, j;
  int w = (int)scrW, h = (int)scrH;

  xs[0] = 0; xs[1] = w / 2; xs[2] = w - 1;
  ys[0] = 0; ys[1] = h / 2; ys[2] = h - 1;
  for (j = 0; j < 3; j++)
    for (i = 0; i < 3; i++)
      qxRect((float)xs[i], (float)ys[j],
             (float)(xs[i] + 1), (float)(ys[j] + 1), 0xffffff);
}

static void patternImpulsesInset(void)
{
  int xs[3], ys[3], i, j;
  int w = (int)scrW, h = (int)scrH;

  xs[0] = 2; xs[1] = w / 2; xs[2] = w - 3;
  ys[0] = 2; ys[1] = h / 2; ys[2] = h - 3;
  for (j = 0; j < 3; j++)
    for (i = 0; i < 3; i++)
      qxRect((float)xs[i], (float)ys[j],
             (float)(xs[i] + 1), (float)(ys[j] + 1), 0xffffff);
}

/* Sample-position probe.  48 strips whose edge moves by 1/16 pixel each,
 * from -1 to +2 pixels.  In a Glide AA screenshot (both chips averaged)
 * column 100 / row 300 then show where each chip samples inside a pixel:
 *   X: strip j covers rows 40+4j..42+4j, left edge at x = 100 + (j-16)/16
 *   Y: strip j covers cols 300+4j..302+4j, top edge at y = 300 + (j-16)/16 */
static void patternProbe(void)
{
  int j;
  for (j = 0; j < 48; j++) {
    float k = (float)(j - 16) / 16.0f;
    qxRect(100.0f + k, (float)(40 + 4 * j), 110.0f + k, (float)(43 + 4 * j), 0xffffff);
    qxRect((float)(300 + 4 * j), 300.0f + k, (float)(303 + 4 * j), 310.0f + k, 0xffffff);
  }
}

/* Quincunx filter-direction test.
 * Measured on a Voodoo5 5500 (probe screenshots): in Glide screen space a
 * pixel's sample sits on the integer coordinate, and with Quincunx the
 * master samples there while the slave samples half a pixel up-left.
 * Green lines cover only master samples (|d| < 0.25 around an integer),
 * red lines only slave samples (half-integer).  On screen, with the filter
 * in the right direction, every green line turns yellowish and has one red
 * line below it (or to its right); with the filter one texel off it stays
 * pure green with two red lines after it. */
static void patternColour(void)
{
  int n;
  for (n = 0; n < 30; n++) {
    float q = (float)(40 + 6 * n);
    float pp = (float)(360 + 6 * n);
    qxRect(100.0f, q - 0.25f, 300.0f, q + 0.25f, 0x00ff00);
    qxRect(100.0f, q + 0.25f, 300.0f, q + 0.75f, 0x0000ff);
    qxRect(pp - 0.25f, 240.0f, pp + 0.25f, 440.0f, 0x00ff00);
    qxRect(pp + 0.25f, 240.0f, pp + 0.75f, 440.0f, 0x0000ff);
  }
}

static void patternCalibration(void)
{
  /* Half-pixel edges: with correct Quincunx the faint fringe is outside
   * on the RIGHT and BOTTOM.  See TEST_PROTOCOL.md, step E. */
  qxRect((float)(int)(scrW * 0.3f) + 0.5f, (float)(int)(scrH * 0.3f) + 0.5f,
         (float)(int)(scrW * 0.7f) + 0.5f, (float)(int)(scrH * 0.7f) + 0.5f,
         0xffffff);
}

/* Edge frame: on every side the outermost line is white, the next one
 * red, the third green.  Shows exactly which rows/columns reach the
 * monitor and whether the image is shifted, and in which direction. */
static void patternFrame(void)
{
  static const unsigned long col[3] = { 0xffffff, 0x0000ff, 0x00ff00 };
  float w = scrW, h = scrH;
  int i;

  for (i = 0; i < 3; i++) {
    float a = (float)i;
    qxRect(a + 8.0f, a, w - a - 8.0f, a + 1.0f, col[i]);        /* top    */
    qxRect(a + 8.0f, h - a - 1.0f, w - a - 8.0f, h - a, col[i]); /* bottom */
    qxRect(a, a + 8.0f, a + 1.0f, h - a - 8.0f, col[i]);        /* left   */
    qxRect(w - a - 1.0f, a + 8.0f, w - a, h - a - 8.0f, col[i]); /* right  */
  }
  /* A white 2x2 block in the very centre as a fixed reference. */
  qxRect((float)((int)w / 2 - 1), (float)((int)h / 2 - 1),
         (float)((int)w / 2 + 1), (float)((int)h / 2 + 1), 0xffffff);
}

static void patternScene(float t)
{
  int i;
  float cx = scrW * 0.5f, cy = scrH * 0.5f;
  float r = (scrH < scrW ? scrH : scrW) * 0.45f;

  for (i = 0; i < 24; i++) {
    float a  = t * 0.2f + (float)i * (6.2831853f / 24.0f);
    float a2 = a + 0.035f;
    qxTriangle(cx, cy,
               cx + r * (float)cos(a),  cy + r * (float)sin(a),
               cx + r * (float)cos(a2), cy + r * (float)sin(a2),
               (i & 1) ? 0xffffff : 0x40c0ff);
  }
  qxTriangle(0.0f, scrH * 0.05f, scrW, scrH * 0.05f + 6.0f,
             0.0f, scrH * 0.05f + 3.0f, 0xffff00);
  qxTriangle(scrW * 0.05f, 0.0f, scrW * 0.05f + 6.0f, scrH,
             scrW * 0.05f + 3.0f, 0.0f, 0xffff00);
}

/* ---- options ---- */

static int parseResolution(const char *s)
{
  if (!strcmp(s, "640x480"))   { scrW = 640;  scrH = 480;  return GR_RESOLUTION_640x480; }
  if (!strcmp(s, "800x600"))   { scrW = 800;  scrH = 600;  return GR_RESOLUTION_800x600; }
  if (!strcmp(s, "1024x768"))  { scrW = 1024; scrH = 768;  return GR_RESOLUTION_1024x768; }
  if (!strcmp(s, "1280x1024")) { scrW = 1280; scrH = 1024; return GR_RESOLUTION_1280x1024; }
  if (!strcmp(s, "1600x1200")) { scrW = 1600; scrH = 1200; return GR_RESOLUTION_1600x1200; }
  return -1;
}

static int parsePixelFormat(const char *s)
{
  if (!strcmp(s, "argb8888"))    return GR_PIXFMT_ARGB_8888;
  if (!strcmp(s, "aa2argb8888")) return GR_PIXFMT_AA_2_ARGB_8888;
  if (!strcmp(s, "rgb565"))      return GR_PIXFMT_RGB_565;
  if (!strcmp(s, "aa2rgb565"))   return GR_PIXFMT_AA_2_RGB_565;
  return -1;
}

int main(int argc, char **argv)
{
  int resolution = GR_RESOLUTION_640x480;
  int pixelformat = 0;
  int pattern = 1;
  int i;
  float t = 0.0f;
  long wrange[2];
  HWND hwnd;
  GrContext_t gc = 0;
  PFN_grSstWinOpenExt p_grSstWinOpenExt = NULL;

  for (i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "-r")) {
      resolution = parseResolution(argv[i + 1]);
    } else if (!strcmp(argv[i], "-p")) {
      pixelformat = parsePixelFormat(argv[i + 1]);
    } else if (!strcmp(argv[i], "-o")) {
      lowerLeft = (argv[i + 1][0] == 'l' || argv[i + 1][0] == 'L');
    } else {
      resolution = -1;
    }
    if (resolution < 0 || pixelformat < 0) {
      printf("Uso: qxpattern [-r 640x480|800x600|1024x768|1280x1024|1600x1200]\n"
             "                 [-p argb8888|aa2argb8888|rgb565|aa2rgb565] [-o u|l]\n");
      return 1;
    }
  }

  printf("qxpattern - schemi di prova Quincunx 1.0\n");
  if (!loadGlide())
    return 1;

  printf("Risoluzione %dx%d, origine %s\n", (int)scrW, (int)scrH,
         lowerLeft ? "in basso a sinistra" : "in alto a sinistra");
  printf("Tasti: 1 piatto  2 colonne  3 righe  4 rettangolo  5 impulsi\n");
  printf("       6 calibrazione jitter  7 scena in movimento  8 cornice bordi\n       9 impulsi a 2 pixel dai bordi  0 sonda campioni\n       c test colori del filtro  q esci\n");
  printf("Premi Invio per iniziare.\n");
  getchar();

  hwnd = makeWindow();
  if (hwnd == NULL) {
    printf("Impossibile creare la finestra.\n");
    return 1;
  }
  pumpMessages();

  p_grGlideInit();
  p_grSstSelect(0);
  printf("%s\n", p_grGetString(GR_VERSION));

  if (pixelformat != 0)
    p_grSstWinOpenExt = (PFN_grSstWinOpenExt)(void *)p_grGetProcAddress("grSstWinOpenExt");

  if (p_grSstWinOpenExt != NULL) {
    gc = p_grSstWinOpenExt((unsigned long)hwnd, resolution, GR_REFRESH_60Hz,
                           GR_COLORFORMAT_ABGR,
                           lowerLeft ? GR_ORIGIN_LOWER_LEFT : GR_ORIGIN_UPPER_LEFT,
                           pixelformat, 2, 1);
  } else {
    if (pixelformat != 0)
      printf("grSstWinOpenExt non disponibile: apro senza formato pixel.\n");
    gc = p_grSstWinOpen((unsigned long)hwnd, resolution, GR_REFRESH_60Hz,
                        GR_COLORFORMAT_ABGR,
                        lowerLeft ? GR_ORIGIN_LOWER_LEFT : GR_ORIGIN_UPPER_LEFT,
                        2, 1);
  }
  if (!gc) {
    printf("Impossibile aprire il contesto Glide a schermo intero.\n");
    p_grGlideShutdown();
    DestroyWindow(hwnd);
    return 1;
  }

  p_grGet(GR_WDEPTH_MIN_MAX, 8, wrange);
  p_grVertexLayout(GR_PARAM_XY, 0, GR_PARAM_ENABLE);
  p_grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                   GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, 0);
  p_grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
  p_grClipWindow(0, 0, (unsigned long)scrW, (unsigned long)scrH);

  while (!quitRequested) {
    pumpMessages();

    if (lastKey) {
      int key = lastKey;
      lastKey = 0;
      if (key >= '0' && key <= '9')
        pattern = key - '0';
      else if (key == 'c' || key == 'C')
        pattern = 10;
      else if (key == 'q' || key == 'Q' || key == 27)
        break;
    }

    p_grBufferClear(0x000000, 0, (unsigned long)wrange[1]);
    switch (pattern) {
    case 1:  patternFlat();        break;
    case 2:  patternColumns();     break;
    case 3:  patternRows();        break;
    case 4:  patternRectangle();   break;
    case 5:  patternImpulses();    break;
    case 6:  patternCalibration(); break;
    case 8:  patternFrame();       break;
    case 9:  patternImpulsesInset(); break;
    case 0:  patternProbe();       break;
    case 10: patternColour();      break;
    default: patternScene(t);      break;
    }
    p_grBufferSwap(1);
    t += 1.0f / 60.0f;
  }

  p_grSstWinClose(gc);
  p_grGlideShutdown();
  DestroyWindow(hwnd);
  FreeLibrary(glideDll);
  return 0;
}
