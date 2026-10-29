/* Host harness for the gsst.c Quincunx jitter setup (stub GC). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int FxU32; typedef int FxI32; typedef int FxBool;
#define FXTRUE 1
#define FXFALSE 0
#define GR_ORIGIN_UPPER_LEFT 0
#define GR_ORIGIN_LOWER_LEFT 1
#define IS_NAPALM(id) ((id) >= 0x4 && (id) <= 0x1f)
#define GDBG_INFO(...) ((void)0)
typedef struct { FxU32 deviceID; } PciInfo;
typedef struct { PciInfo pciInfo; FxU32 h3analogSli, h3quincunx; } BInfo;
typedef struct { FxU32 origin; } State;
typedef struct { BInfo *bInfo; int chipCount, grPixelSample, grSamplesPerChip, sliCount, sampleOffsetIndex; State state; } GrGC;
struct { struct { FxU32 quincunxMode; FxBool aaClip; FxU32 aaXOffset[15][8], aaYOffset[15][8]; } environment; } _GlideRoot;
static const char *envN[8], *envV[8]; static int envC;
static const char *GETENV(const char *n){int i;for(i=0;i<envC;i++)if(!strcmp(envN[i],n))return envV[i];return NULL;}
#include "gsstblock.inc"
static int fails;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
static FxU32 enc(float v){return (FxU32)((((signed char)(v*16.0f))+8)&0x7f);}
int main(void){
  BInfo b; GrGC gc; int i;
  memset(&b,0,sizeof b); memset(&gc,0,sizeof gc);
  b.pciInfo.deviceID=0x9; gc.bInfo=&b; gc.chipCount=2; gc.grPixelSample=2; gc.grSamplesPerChip=1; gc.sliCount=1; gc.sampleOffsetIndex=5;
  for(i=0;i<8;i++){_GlideRoot.environment.aaXOffset[5][i]=100+i;_GlideRoot.environment.aaYOffset[5][i]=200+i;}
  /* off */
  _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"off by default"); CHECK(_GlideRoot.environment.aaXOffset[5][2]==102,"table untouched when off");
  /* on, upper-left */
  _GlideRoot.environment.quincunxMode=10; gc.state.origin=GR_ORIGIN_UPPER_LEFT;
  _grQuincunxSetup(&gc); CHECK(b.h3quincunx==1,"enabled");
  for(i=0;i<8;i++){int chip=(i>>1)&1;
    CHECK(_GlideRoot.environment.aaXOffset[5][i]==(chip?enc(0.5f):enc(0.0f)),"x jitter");
    CHECK(_GlideRoot.environment.aaYOffset[5][i]==(chip?enc(0.5f):enc(0.0f)),"y jitter UL");}
  printf("UL: centre=%02x corner=%02x\n",_GlideRoot.environment.aaXOffset[5][0],_GlideRoot.environment.aaXOffset[5][2]);
  /* lower-left flips Y only */
  gc.state.origin=GR_ORIGIN_LOWER_LEFT; _grQuincunxSetup(&gc);
  CHECK(_GlideRoot.environment.aaYOffset[5][2]==enc(-0.5f),"y flipped for LL");
  CHECK(_GlideRoot.environment.aaXOffset[5][2]==enc(0.5f),"x not flipped for LL");
  printf("LL: cornerY=%02x\n",_GlideRoot.environment.aaYOffset[5][2]);
  /* YFLIP=0 keeps +0.5 */
  envN[0]="FX_GLIDE_QX_YFLIP";envV[0]="0";envC=1; _grQuincunxSetup(&gc);
  CHECK(_GlideRoot.environment.aaYOffset[5][2]==enc(0.5f),"yflip override"); envC=0;
  /* JITTER=0 restores the original table */
  envN[0]="FX_GLIDE_QX_JITTER";envV[0]="0";envC=1; _grQuincunxSetup(&gc);
  CHECK(b.h3quincunx==1,"still enabled with stock jitter");
  for(i=0;i<8;i++) CHECK(_GlideRoot.environment.aaXOffset[5][i]==(FxU32)(100+i) && _GlideRoot.environment.aaYOffset[5][i]==(FxU32)(200+i),"original table back"); envC=0;
  /* modes 9 and 11 stay stock */
  _GlideRoot.environment.quincunxMode=11; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==2,"11 = smoother"); CHECK(_GlideRoot.environment.aaXOffset[5][0]==enc(0.0f)&&_GlideRoot.environment.aaXOffset[5][2]==enc(0.5f),"smoother: same jitter as sharper");
  _GlideRoot.environment.quincunxMode=9; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"9 has no path");
  for(i=0;i<8;i++) CHECK(_GlideRoot.environment.aaXOffset[5][i]==(FxU32)(100+i),"11 table restored");
  /* ineligible configs */
  _GlideRoot.environment.quincunxMode=10;
  gc.sliCount=2; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"SLI rejected"); gc.sliCount=1;
  b.h3analogSli=1; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"analog rejected"); b.h3analogSli=0;
  gc.grSamplesPerChip=2; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"2/chip rejected"); gc.grSamplesPerChip=1;
  gc.chipCount=4; _grQuincunxSetup(&gc); CHECK(b.h3quincunx==0,"4 chip rejected");
  printf(fails?"%d FAILURES\n":"All gsst Quincunx checks passed\n",fails); return fails!=0;
}
