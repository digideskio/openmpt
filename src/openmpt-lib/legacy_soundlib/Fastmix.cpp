/*
 * OpenMPT
 *
 * Fastmix.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"
#ifdef _DEBUG
#include <math.h>
#endif

#include "../mixgraph/constants.hpp"
#include "../mixgraph/mixer.hpp"

#include "../tracker/eval.hpp"

#include "../pervasives/pervasives.hpp"

using namespace modplug::pervasives;
using namespace modplug::tracker;

// rewbs.resamplerConf
#include "../MainFrm.h"
#include "WindowedFIR.h"
// end  rewbs.resamplerConf

#pragma bss_seg(".modplug")

// Front Mix Buffer (Also room for interleaved rear mix)
int MixSoundBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*4];


float MixFloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*2];

#pragma bss_seg()



extern LONG gnDryROfsVol;
extern LONG gnDryLOfsVol;
extern LONG gnRvbROfsVol;
extern LONG gnRvbLOfsVol;

// 4x256 taps polyphase FIR resampling filter
extern short int gFastSinc[];
extern short int gKaiserSinc[]; // 8-taps polyphase
extern short int gDownsample13x[]; // 1.3x downsampling
extern short int gDownsample2x[]; // 2x downsampling


/////////////////////////////////////////////////////
// Mixing Macros

#define SNDMIX_BEGINSAMPLELOOP8\
    register modplug::tracker::modchannel_t * const pChn = pChannel;\
    sample_position = pChn->fractional_sample_position;\
    const signed char *p = (signed char *)(pChn->active_sample_data+pChn->sample_position);\
    if (pChn->flags & CHN_STEREO) p += pChn->sample_position;\
    int *pvol = pbuffer;\
    do {

#define SNDMIX_BEGINSAMPLELOOP16\
    register modplug::tracker::modchannel_t * const pChn = pChannel;\
    sample_position = pChn->fractional_sample_position;\
    const signed short *p = (signed short *)(pChn->active_sample_data+(pChn->sample_position*2));\
    if (pChn->flags & CHN_STEREO) p += pChn->sample_position;\
    int *pvol = pbuffer;\
    do {

#define SNDMIX_ENDSAMPLELOOP\
        sample_position += pChn->position_delta;\
    } while (pvol < pbufmax);\
    pChn->sample_position += sample_position >> 16;\
    pChn->fractional_sample_position = sample_position & 0xFFFF;

#define SNDMIX_ENDSAMPLELOOP8    SNDMIX_ENDSAMPLELOOP
#define SNDMIX_ENDSAMPLELOOP16    SNDMIX_ENDSAMPLELOOP

signed short CWindowedFIR::lut[WFIR_LUTLEN*WFIR_WIDTH]; // rewbs.resamplerConf
//////////////////////////////////////////////////////////////////////////////
// Mono

// No interpolation
#define SNDMIX_GETMONOVOL8NOIDO\
    int vol = p[sample_position >> 16] << 8;

#define SNDMIX_GETMONOVOL16NOIDO\
    int vol = p[sample_position >> 16];

// Linear Interpolation
#define SNDMIX_GETMONOVOL8LINEAR\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 8) & 0xFF;\
    int srcvol = p[poshi];\
    int destvol = p[poshi+1];\
    int vol = (srcvol<<8) + ((int)(poslo * (destvol - srcvol)));

#define SNDMIX_GETMONOVOL16LINEAR\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 8) & 0xFF;\
    int srcvol = p[poshi];\
    int destvol = p[poshi+1];\
    int vol = srcvol + ((int)(poslo * (destvol - srcvol)) >> 8);

// Cubic Spline
#define SNDMIX_GETMONOVOL8HQSRC\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 6) & 0x3FC;\
    int vol = (gFastSinc[poslo]*p[poshi-1] + gFastSinc[poslo+1]*p[poshi]\
         + gFastSinc[poslo+2]*p[poshi+1] + gFastSinc[poslo+3]*p[poshi+2]) >> 6;\

#define SNDMIX_GETMONOVOL16HQSRC\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 6) & 0x3FC;\
    int vol = (gFastSinc[poslo]*p[poshi-1] + gFastSinc[poslo+1]*p[poshi]\
         + gFastSinc[poslo+2]*p[poshi+1] + gFastSinc[poslo+3]*p[poshi+2]) >> 14;\

// 8-taps polyphase
#define SNDMIX_GETMONOVOL8KAISER\
    int poshi = sample_position >> 16;\
    const short int *poslo = (const short int *)(sinc+(sample_position&0xfff0));\
    int vol = (poslo[0]*p[poshi-3] + poslo[1]*p[poshi-2]\
         + poslo[2]*p[poshi-1] + poslo[3]*p[poshi]\
         + poslo[4]*p[poshi+1] + poslo[5]*p[poshi+2]\
         + poslo[6]*p[poshi+3] + poslo[7]*p[poshi+4]) >> 6;\

#define SNDMIX_GETMONOVOL16KAISER\
    int poshi = sample_position >> 16;\
    const short int *poslo = (const short int *)(sinc+(sample_position&0xfff0));\
    int vol = (poslo[0]*p[poshi-3] + poslo[1]*p[poshi-2]\
         + poslo[2]*p[poshi-1] + poslo[3]*p[poshi]\
         + poslo[4]*p[poshi+1] + poslo[5]*p[poshi+2]\
         + poslo[6]*p[poshi+3] + poslo[7]*p[poshi+4]) >> 14;\
// rewbs.resamplerConf
#define SNDMIX_GETMONOVOL8FIRFILTER \
    int poshi  = sample_position >> 16;\
    int poslo  = (sample_position & 0xFFFF);\
    int firidx = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol    = (CWindowedFIR::lut[firidx+0]*(int)p[poshi+1-4]);    \
        vol   += (CWindowedFIR::lut[firidx+1]*(int)p[poshi+2-4]);    \
        vol   += (CWindowedFIR::lut[firidx+2]*(int)p[poshi+3-4]);    \
        vol   += (CWindowedFIR::lut[firidx+3]*(int)p[poshi+4-4]);    \
        vol   += (CWindowedFIR::lut[firidx+4]*(int)p[poshi+5-4]);    \
        vol   += (CWindowedFIR::lut[firidx+5]*(int)p[poshi+6-4]);    \
        vol   += (CWindowedFIR::lut[firidx+6]*(int)p[poshi+7-4]);    \
        vol   += (CWindowedFIR::lut[firidx+7]*(int)p[poshi+8-4]);    \
        vol  >>= WFIR_8SHIFT;

#define SNDMIX_GETMONOVOL16FIRFILTER \
    int poshi  = sample_position >> 16;\
    int poslo  = (sample_position & 0xFFFF);\
    int firidx = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1   = (CWindowedFIR::lut[firidx+0]*(int)p[poshi+1-4]);    \
        vol1  += (CWindowedFIR::lut[firidx+1]*(int)p[poshi+2-4]);    \
        vol1  += (CWindowedFIR::lut[firidx+2]*(int)p[poshi+3-4]);    \
        vol1  += (CWindowedFIR::lut[firidx+3]*(int)p[poshi+4-4]);    \
    int vol2   = (CWindowedFIR::lut[firidx+4]*(int)p[poshi+5-4]);    \
        vol2  += (CWindowedFIR::lut[firidx+5]*(int)p[poshi+6-4]);    \
        vol2  += (CWindowedFIR::lut[firidx+6]*(int)p[poshi+7-4]);    \
        vol2  += (CWindowedFIR::lut[firidx+7]*(int)p[poshi+8-4]);    \
    int vol    = ((vol1>>1)+(vol2>>1)) >> (WFIR_16BITSHIFT-1);


// end rewbs.resamplerConf
#define SNDMIX_INITSINCTABLE\
    const char * const sinc = (const char *)(((pChannel->position_delta > 0x13000) || (pChannel->position_delta < -0x13000)) ?\
        (((pChannel->position_delta > 0x18000) || (pChannel->position_delta < -0x18000)) ? gDownsample2x : gDownsample13x) : gKaiserSinc);

/////////////////////////////////////////////////////////////////////////////
// Stereo

// No interpolation
#define SNDMIX_GETSTEREOVOL8NOIDO\
    int vol_l = p[(sample_position>>16)*2] << 8;\
    int vol_r = p[(sample_position>>16)*2+1] << 8;

#define SNDMIX_GETSTEREOVOL16NOIDO\
    int vol_l = p[(sample_position>>16)*2];\
    int vol_r = p[(sample_position>>16)*2+1];

// Linear Interpolation
#define SNDMIX_GETSTEREOVOL8LINEAR\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 8) & 0xFF;\
    int srcvol_l = p[poshi*2];\
    int vol_l = (srcvol_l<<8) + ((int)(poslo * (p[poshi*2+2] - srcvol_l)));\
    int srcvol_r = p[poshi*2+1];\
    int vol_r = (srcvol_r<<8) + ((int)(poslo * (p[poshi*2+3] - srcvol_r)));

#define SNDMIX_GETSTEREOVOL16LINEAR\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 8) & 0xFF;\
    int srcvol_l = p[poshi*2];\
    int vol_l = srcvol_l + ((int)(poslo * (p[poshi*2+2] - srcvol_l)) >> 8);\
    int srcvol_r = p[poshi*2+1];\
    int vol_r = srcvol_r + ((int)(poslo * (p[poshi*2+3] - srcvol_r)) >> 8);\

// Cubic Spline
#define SNDMIX_GETSTEREOVOL8HQSRC\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 6) & 0x3FC;\
    int vol_l = (gFastSinc[poslo]*p[poshi*2-2] + gFastSinc[poslo+1]*p[poshi*2]\
         + gFastSinc[poslo+2]*p[poshi*2+2] + gFastSinc[poslo+3]*p[poshi*2+4]) >> 6;\
    int vol_r = (gFastSinc[poslo]*p[poshi*2-1] + gFastSinc[poslo+1]*p[poshi*2+1]\
         + gFastSinc[poslo+2]*p[poshi*2+3] + gFastSinc[poslo+3]*p[poshi*2+5]) >> 6;\

#define SNDMIX_GETSTEREOVOL16HQSRC\
    int poshi = sample_position >> 16;\
    int poslo = (sample_position >> 6) & 0x3FC;\
    int vol_l = (gFastSinc[poslo]*p[poshi*2-2] + gFastSinc[poslo+1]*p[poshi*2]\
         + gFastSinc[poslo+2]*p[poshi*2+2] + gFastSinc[poslo+3]*p[poshi*2+4]) >> 14;\
    int vol_r = (gFastSinc[poslo]*p[poshi*2-1] + gFastSinc[poslo+1]*p[poshi*2+1]\
         + gFastSinc[poslo+2]*p[poshi*2+3] + gFastSinc[poslo+3]*p[poshi*2+5]) >> 14;\

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// 8-taps polyphase
#define SNDMIX_GETSTEREOVOL8KAISER\
    int poshi = sample_position >> 16;\
    const short int *poslo = (const short int *)(sinc+(sample_position&0xfff0));\
    int vol_l = (poslo[0]*p[poshi*2-6] + poslo[1]*p[poshi*2-4]\
         + poslo[2]*p[poshi*2-2] + poslo[3]*p[poshi*2]\
         + poslo[4]*p[poshi*2+2] + poslo[5]*p[poshi*2+4]\
         + poslo[6]*p[poshi*2+6] + poslo[7]*p[poshi*2+8]) >> 6;\
    int vol_r = (poslo[0]*p[poshi*2-5] + poslo[1]*p[poshi*2-3]\
         + poslo[2]*p[poshi*2-1] + poslo[3]*p[poshi*2+1]\
         + poslo[4]*p[poshi*2+3] + poslo[5]*p[poshi*2+5]\
         + poslo[6]*p[poshi*2+7] + poslo[7]*p[poshi*2+9]) >> 6;\

#define SNDMIX_GETSTEREOVOL16KAISER\
    int poshi = sample_position >> 16;\
    const short int *poslo = (const short int *)(sinc+(sample_position&0xfff0));\
    int vol_l = (poslo[0]*p[poshi*2-6] + poslo[1]*p[poshi*2-4]\
         + poslo[2]*p[poshi*2-2] + poslo[3]*p[poshi*2]\
         + poslo[4]*p[poshi*2+2] + poslo[5]*p[poshi*2+4]\
         + poslo[6]*p[poshi*2+6] + poslo[7]*p[poshi*2+8]) >> 14;\
    int vol_r = (poslo[0]*p[poshi*2-5] + poslo[1]*p[poshi*2-3]\
         + poslo[2]*p[poshi*2-1] + poslo[3]*p[poshi*2+1]\
         + poslo[4]*p[poshi*2+3] + poslo[5]*p[poshi*2+5]\
         + poslo[6]*p[poshi*2+7] + poslo[7]*p[poshi*2+9]) >> 14;\
// rewbs.resamplerConf
// fir interpolation
#define SNDMIX_GETSTEREOVOL8FIRFILTER \
    int poshi   = sample_position >> 16;\
    int poslo   = (sample_position & 0xFFFF);\
    int firidx  = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol_l   = (CWindowedFIR::lut[firidx+0]*(int)p[(poshi+1-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+1]*(int)p[(poshi+2-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+2]*(int)p[(poshi+3-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+3]*(int)p[(poshi+4-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+4]*(int)p[(poshi+5-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+5]*(int)p[(poshi+6-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+6]*(int)p[(poshi+7-4)*2  ]);   \
        vol_l  += (CWindowedFIR::lut[firidx+7]*(int)p[(poshi+8-4)*2  ]);   \
        vol_l >>= WFIR_8SHIFT; \
    int vol_r   = (CWindowedFIR::lut[firidx+0]*(int)p[(poshi+1-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+1]*(int)p[(poshi+2-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+2]*(int)p[(poshi+3-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+3]*(int)p[(poshi+4-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+4]*(int)p[(poshi+5-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+5]*(int)p[(poshi+6-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+6]*(int)p[(poshi+7-4)*2+1]);   \
        vol_r  += (CWindowedFIR::lut[firidx+7]*(int)p[(poshi+8-4)*2+1]);   \
        vol_r >>= WFIR_8SHIFT;

#define SNDMIX_GETSTEREOVOL16FIRFILTER \
    int poshi   = sample_position >> 16;\
    int poslo   = (sample_position & 0xFFFF);\
    int firidx  = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1_l  = (CWindowedFIR::lut[firidx+0]*(int)p[(poshi+1-4)*2  ]);   \
        vol1_l += (CWindowedFIR::lut[firidx+1]*(int)p[(poshi+2-4)*2  ]);   \
        vol1_l += (CWindowedFIR::lut[firidx+2]*(int)p[(poshi+3-4)*2  ]);   \
        vol1_l += (CWindowedFIR::lut[firidx+3]*(int)p[(poshi+4-4)*2  ]);   \
    int vol2_l  = (CWindowedFIR::lut[firidx+4]*(int)p[(poshi+5-4)*2  ]);   \
        vol2_l += (CWindowedFIR::lut[firidx+5]*(int)p[(poshi+6-4)*2  ]);   \
        vol2_l += (CWindowedFIR::lut[firidx+6]*(int)p[(poshi+7-4)*2  ]);   \
        vol2_l += (CWindowedFIR::lut[firidx+7]*(int)p[(poshi+8-4)*2  ]);   \
    int vol_l   = ((vol1_l>>1)+(vol2_l>>1)) >> (WFIR_16BITSHIFT-1); \
    int vol1_r  = (CWindowedFIR::lut[firidx+0]*(int)p[(poshi+1-4)*2+1]);   \
        vol1_r += (CWindowedFIR::lut[firidx+1]*(int)p[(poshi+2-4)*2+1]);   \
        vol1_r += (CWindowedFIR::lut[firidx+2]*(int)p[(poshi+3-4)*2+1]);   \
        vol1_r += (CWindowedFIR::lut[firidx+3]*(int)p[(poshi+4-4)*2+1]);   \
    int vol2_r  = (CWindowedFIR::lut[firidx+4]*(int)p[(poshi+5-4)*2+1]);   \
        vol2_r += (CWindowedFIR::lut[firidx+5]*(int)p[(poshi+6-4)*2+1]);   \
        vol2_r += (CWindowedFIR::lut[firidx+6]*(int)p[(poshi+7-4)*2+1]);   \
        vol2_r += (CWindowedFIR::lut[firidx+7]*(int)p[(poshi+8-4)*2+1]);   \
    int vol_r   = ((vol1_r>>1)+(vol2_r>>1)) >> (WFIR_16BITSHIFT-1);
//end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025


/////////////////////////////////////////////////////////////////////////////

#define SNDMIX_STOREMONOVOL\
    pvol[0] += vol * pChn->right_volume;\
    pvol[1] += vol * pChn->left_volume;\
    pvol += 2;

#define SNDMIX_STORESTEREOVOL\
    pvol[0] += vol_l * pChn->right_volume;\
    pvol[1] += vol_r * pChn->left_volume;\
    pvol += 2;

#define SNDMIX_STOREFASTMONOVOL\
    int v = vol * pChn->right_volume;\
    pvol[0] += v;\
    pvol[1] += v;\
    pvol += 2;

#define SNDMIX_RAMPMONOVOL\
    nRampLeftVol += pChn->left_ramp;\
    nRampRightVol += pChn->right_ramp;\
    pvol[0] += vol * (nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION);\
    pvol[1] += vol * (nRampLeftVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION);\
    pvol += 2;

#define SNDMIX_RAMPFASTMONOVOL\
    nRampRightVol += pChn->right_ramp;\
    int fastvol = vol * (nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION);\
    pvol[0] += fastvol;\
    pvol[1] += fastvol;\
    pvol += 2;

#define SNDMIX_RAMPSTEREOVOL\
    nRampLeftVol += pChn->left_ramp;\
    nRampRightVol += pChn->right_ramp;\
    pvol[0] += vol_l * (nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION);\
    pvol[1] += vol_r * (nRampLeftVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION);\
    pvol += 2;


///////////////////////////////////////////////////
// Resonant Filters

// Mono
#define MIX_BEGIN_FILTER\
    int fy1 = pChannel->nFilter_Y1;\
    int fy2 = pChannel->nFilter_Y2;\

#define MIX_END_FILTER\
    pChannel->nFilter_Y1 = fy1;\
    pChannel->nFilter_Y2 = fy2;

#define SNDMIX_PROCESSFILTER\
    int fy = (vol * pChn->nFilter_A0 + fy1 * pChn->nFilter_B0 + fy2 * pChn->nFilter_B1 + 4096) >> 13;\
    fy2 = fy1;\
    fy1 = fy - (vol & pChn->nFilter_HP);\
    vol = fy;\


// Stereo
#define MIX_BEGIN_STEREO_FILTER\
    int fy1 = pChannel->nFilter_Y1;\
    int fy2 = pChannel->nFilter_Y2;\
    int fy3 = pChannel->nFilter_Y3;\
    int fy4 = pChannel->nFilter_Y4;\

#define MIX_END_STEREO_FILTER\
    pChannel->nFilter_Y1 = fy1;\
    pChannel->nFilter_Y2 = fy2;\
    pChannel->nFilter_Y3 = fy3;\
    pChannel->nFilter_Y4 = fy4;\

#define SNDMIX_PROCESSSTEREOFILTER\
    int fy = (vol_l * pChn->nFilter_A0 + fy1 * pChn->nFilter_B0 + fy2 * pChn->nFilter_B1 + 4096) >> 13;\
    fy2 = fy1; fy1 = fy - (vol_l & pChn->nFilter_HP);\
    vol_l = fy;\
    fy = (vol_r * pChn->nFilter_A0 + fy3 * pChn->nFilter_B0 + fy4 * pChn->nFilter_B1 + 4096) >> 13;\
    fy4 = fy3; fy3 = fy - (vol_r & pChn->nFilter_HP);\
    vol_r = fy;\


//////////////////////////////////////////////////////////
// Interfaces

typedef VOID (MPPASMCALL * LPMIXINTERFACE)(modplug::tracker::voice_ty *, int *, int *);

#define BEGIN_MIX_INTERFACE(func)\
    VOID MPPASMCALL func(modplug::tracker::modchannel_t *pChannel, int *pbuffer, int *pbufmax)\
    {\
        LONG sample_position;

#define END_MIX_INTERFACE()\
        SNDMIX_ENDSAMPLELOOP\
    }

// Volume Ramps
#define BEGIN_RAMPMIX_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
        LONG nRampRightVol = pChannel->nRampRightVol;\
        LONG nRampLeftVol = pChannel->nRampLeftVol;

#define END_RAMPMIX_INTERFACE()\
        SNDMIX_ENDSAMPLELOOP\
        pChannel->nRampRightVol = nRampRightVol;\
        pChannel->right_volume = nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
        pChannel->nRampLeftVol = nRampLeftVol;\
        pChannel->left_volume = nRampLeftVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
    }

#define BEGIN_FASTRAMPMIX_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
        LONG nRampRightVol = pChannel->nRampRightVol;

#define END_FASTRAMPMIX_INTERFACE()\
        SNDMIX_ENDSAMPLELOOP\
        pChannel->nRampRightVol = nRampRightVol;\
        pChannel->nRampLeftVol = nRampRightVol;\
        pChannel->right_volume = nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
        pChannel->left_volume = pChannel->right_volume;\
    }


// Mono Resonant Filters
#define BEGIN_MIX_FLT_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
    MIX_BEGIN_FILTER


#define END_MIX_FLT_INTERFACE()\
    SNDMIX_ENDSAMPLELOOP\
    MIX_END_FILTER\
    }

#define BEGIN_RAMPMIX_FLT_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
        LONG nRampRightVol = pChannel->nRampRightVol;\
        LONG nRampLeftVol = pChannel->nRampLeftVol;\
        MIX_BEGIN_FILTER

#define END_RAMPMIX_FLT_INTERFACE()\
        SNDMIX_ENDSAMPLELOOP\
        MIX_END_FILTER\
        pChannel->nRampRightVol = nRampRightVol;\
        pChannel->right_volume = nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
        pChannel->nRampLeftVol = nRampLeftVol;\
        pChannel->left_volume = nRampLeftVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
    }

// Stereo Resonant Filters
#define BEGIN_MIX_STFLT_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
    MIX_BEGIN_STEREO_FILTER


#define END_MIX_STFLT_INTERFACE()\
    SNDMIX_ENDSAMPLELOOP\
    MIX_END_STEREO_FILTER\
    }

#define BEGIN_RAMPMIX_STFLT_INTERFACE(func)\
    BEGIN_MIX_INTERFACE(func)\
        LONG nRampRightVol = pChannel->nRampRightVol;\
        LONG nRampLeftVol = pChannel->nRampLeftVol;\
        MIX_BEGIN_STEREO_FILTER

#define END_RAMPMIX_STFLT_INTERFACE()\
        SNDMIX_ENDSAMPLELOOP\
        MIX_END_STEREO_FILTER\
        pChannel->nRampRightVol = nRampRightVol;\
        pChannel->right_volume = nRampRightVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
        pChannel->nRampLeftVol = nRampLeftVol;\
        pChannel->left_volume = nRampLeftVol >> modplug::mixgraph::VOLUME_RAMP_PRECISION;\
    }


/////////////////////////////////////////////////////
//


#ifdef ENABLE_AMD
extern void AMD_StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount, const float _i2fc);
extern void AMD_FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, UINT nCount, const float _f2ic);
extern void AMD_MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount, const float _i2fc);
extern void AMD_FloatToMonoMix(const float *pIn, int *pOut, UINT nCount, const float _f2ic);
#endif

#ifdef ENABLE_SSE
extern void SSE_StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount, const float _i2fc);
extern void SSE_MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount, const float _i2fc);
#endif


/////////////////////////////////////////////////////
// Mono samples functions

//XXXih: disabled legacy garbage
/*
BEGIN_MIX_INTERFACE(Mono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

#ifndef FASTSOUNDLIB
BEGIN_MIX_INTERFACE(Mono8BitHQMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8HQSRC
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitHQMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16HQSRC
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()
#else
#define Mono8BitHQMix    Mono8BitLinearMix
#define Mono16BitHQMix    Mono16BitLinearMix
#endif

// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Mono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

#ifndef FASTSOUNDLIB
BEGIN_RAMPMIX_INTERFACE(Mono8BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8HQSRC
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16HQSRC
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

#else
#define    Mono8BitHQRampMix        Mono8BitLinearRampMix
#define    Mono16BitHQRampMix        Mono16BitLinearRampMix
#endif

//////////////////////////////////////////////////////
// 8-taps polyphase resampling filter

#ifndef FASTSOUNDLIB

// Normal
BEGIN_MIX_INTERFACE(Mono8BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8KAISER
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16KAISER
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

// Ramp
BEGIN_RAMPMIX_INTERFACE(Mono8BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8KAISER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16KAISER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()


#else
#define Mono8BitKaiserMix            Mono8BitHQMix
#define Mono16BitKaiserMix            Mono16BitHQMix
#define Mono8BitKaiserRampMix    Mono8BitHQRampMix
#define Mono16BitKaiserRampMix    Mono16BitHQRampMix
#endif

// -> BEHAVIOUR_CHANGE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// rewbs.resamplerConf
//////////////////////////////////////////////////////
// FIR filter

#ifndef FASTSOUNDLIB

// Normal
BEGIN_MIX_INTERFACE(Mono8BitFIRFilterMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitFIRFilterMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

// Ramp
BEGIN_RAMPMIX_INTERFACE(Mono8BitFIRFilterRampMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitFIRFilterRampMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()


#else
#define Mono8BitFIRFilterMix            Mono8BitHQMix
#define Mono16BitFIRFilterMix            Mono16BitHQMix
#define Mono8BitFIRFilterRampMix    Mono8BitHQRampMix
#define Mono16BitFIRFilterRampMix    Mono16BitHQRampMix
#endif
//end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025

//////////////////////////////////////////////////////
// Fast mono mix for leftvol=rightvol (1 less imul)

BEGIN_MIX_INTERFACE(FastMono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

// Fast Ramps
BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Stereo samples

BEGIN_MIX_INTERFACE(Stereo8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo8BitHQMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8HQSRC
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitHQMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16HQSRC
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Stereo8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo8BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8HQSRC
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16HQSRC
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"

//////////////////////////////////////////////////////
// Stereo 8-taps polyphase resampling filter

BEGIN_MIX_INTERFACE(Stereo8BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8KAISER
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16KAISER
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

// Ramp
BEGIN_RAMPMIX_INTERFACE(Stereo8BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8KAISER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16KAISER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

// rewbs.resamplerConf
//////////////////////////////////////////////////////
// Stereo FIR Filter

BEGIN_MIX_INTERFACE(Stereo8BitFIRFilterMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitFIRFilterMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

// Ramp
BEGIN_RAMPMIX_INTERFACE(Stereo8BitFIRFilterRampMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitFIRFilterRampMix)
    //SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

// end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025



//////////////////////////////////////////////////////
// Resonant Filter Mix

#ifndef NO_FILTER

// Mono Filter Mix
BEGIN_MIX_FLT_INTERFACE(FilterMono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// rewbs.resamplerConf
//Cubic + reso filter:
BEGIN_MIX_FLT_INTERFACE(FilterMono8BitHQMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8HQSRC
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitHQMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16HQSRC
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

//Polyphase + reso filter:
BEGIN_MIX_FLT_INTERFACE(FilterMono8BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8KAISER
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16KAISER
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

// Enable FIR Filter with resonant filters

BEGIN_MIX_FLT_INTERFACE(FilterMono8BitFIRFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitFIRFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_PROCESSFILTER
    SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()
// end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025

// Filter + Ramp
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// rewbs.resamplerConf
//Cubic + reso filter + ramp:
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8HQSRC
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16HQSRC
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

//Polyphase + reso filter + ramp:
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8KAISER
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16KAISER
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

//FIR Filter + reso filter + ramp
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitFIRFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitFIRFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_PROCESSFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

// end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025

// Stereo Filter Mix
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// rewbs.resamplerConf
//Cubic stereo + reso filter
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitHQMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8HQSRC
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitHQMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16HQSRC
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

//Polyphase stereo + reso filter
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8KAISER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitKaiserMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16KAISER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

//FIR filter stereo + reso filter
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitFIRFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitFIRFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()


//end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025

// Stereo Filter + Ramp
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
// rewbs.resamplerConf
//Cubic stereo + ramp + reso filter
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8HQSRC
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitHQRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16HQSRC
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

//Polyphase stereo + ramp + reso filter
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8KAISER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitKaiserRampMix)
    SNDMIX_INITSINCTABLE
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16KAISER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

//FIR filter stereo + ramp + reso filter
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitFIRFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitFIRFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()
*/

// end rewbs.resamplerConf
// -! BEHAVIOUR_CHANGE#0025

//XXXih: disabled legacy garbage
/*
#else
// Mono
#define FilterMono8BitMix                            Mono8BitMix
#define FilterMono16BitMix                            Mono16BitMix
#define FilterMono8BitLinearMix                    Mono8BitLinearMix
#define FilterMono16BitLinearMix            Mono16BitLinearMix
#define FilterMono8BitRampMix                    Mono8BitRampMix
#define FilterMono16BitRampMix                    Mono16BitRampMix
#define FilterMono8BitLinearRampMix            Mono8BitLinearRampMix
#define FilterMono16BitLinearRampMix    Mono16BitLinearRampMix
// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
#define FilterMono8BitHQMix                            Mono8BitHQMix
#define FilterMono16BitHQMix                    Mono16BitHQMix
#define FilterMono8BitKaiserMix                    Mono8BitKaiserMix
#define FilterMono16BitKaiserMix            Mono16BitKaiserMix
#define FilterMono8BitHQRampMix                    Mono8BitHQRampMix
#define FilterMono16BitHQRampMix            Mono16BitHQRampMix
#define FilterMono8BitKaiserRampMix            Mono8BitKaiserRampMix
#define FilterMono16BitKaiserRampMix    Mono16BitKaiserRampMix
#define FilterMono8BitFIRFilterRampMix            Mono8BitFIRFilterRampMix
#define FilterMono16BitFIRFilterRampMix    Mono16BitFIRFilterRampMix
// -! BEHAVIOUR_CHANGE#0025

// Stereo
#define FilterStereo8BitMix                            Stereo8BitMix
#define FilterStereo16BitMix                    Stereo16BitMix
#define FilterStereo8BitLinearMix            Stereo8BitLinearMix
#define FilterStereo16BitLinearMix            Stereo16BitLinearMix
#define FilterStereo8BitRampMix                    Stereo8BitRampMix
#define FilterStereo16BitRampMix            Stereo16BitRampMix
#define FilterStereo8BitLinearRampMix    Stereo8BitLinearRampMix
#define FilterStereo16BitLinearRampMix    Stereo16BitLinearRampMix
// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
#define FilterStereo8BitHQMix                    Stereo8BitHQMix
#define FilterStereo16BitHQMix                    Stereo16BitHQMix
#define FilterStereo8BitKaiserMix            Stereo8BitKaiserMix
#define FilterStereo16BitKaiserMix            Stereo16BitKaiserMix
#define FilterStereo8BitHQRampMix            Stereo8BitHQRampMix
#define FilterStereo16BitHQRampMix            Stereo16BitHQRampMix
#define FilterStereo8BitKaiserRampMix    Stereo8BitKaiserRampMix
#define FilterStereo16BitKaiserRampMix    Stereo16BitKaiserRampMix
#define FilterStereo8BitFIRFilterRampMix    Stereo8BitFIRFilterRampMix
#define FilterStereo16BitFIRFilterRampMix    Stereo16BitFIRFilterRampMix
// -! BEHAVIOUR_CHANGE#0025
#endif
*/

/////////////////////////////////////////////////////////////////////////////////////
//
// Mix function tables
//
//
// Index is as follow:
//    [b1-b0]        format (8-bit-mono, 16-bit-mono, 8-bit-stereo, 16-bit-stereo)
//    [b2]        ramp
//    [b3]        filter
//    [b5-b4]        src type
//

#define MIXNDX_16BIT    0x01
#define MIXNDX_STEREO    0x02
#define MIXNDX_RAMP            0x04
#define MIXNDX_FILTER    0x08

#define MIXNDX_LINEARSRC    0x10
#define MIXNDX_HQSRC            0x20
#define MIXNDX_KAISERSRC    0x30
#define MIXNDX_FIRFILTERSRC    0x40 // rewbs.resamplerConf

//XXXih: disabled legacy garbage
/*
//const LPMIXINTERFACE gpMixFunctionTable[4*16] =
const LPMIXINTERFACE gpMixFunctionTable[5*16] =    //rewbs.resamplerConf: increased to 5 to cope with FIR
{
    // No SRC
    Mono8BitMix,                            Mono16BitMix,                                Stereo8BitMix,                        Stereo16BitMix,
    Mono8BitRampMix,                    Mono16BitRampMix,                        Stereo8BitRampMix,                Stereo16BitRampMix,
    // No SRC, Filter
    FilterMono8BitMix,                    FilterMono16BitMix,                        FilterStereo8BitMix,        FilterStereo16BitMix,
    FilterMono8BitRampMix,            FilterMono16BitRampMix,                FilterStereo8BitRampMix,FilterStereo16BitRampMix,
    // Linear SRC
    Mono8BitLinearMix,                    Mono16BitLinearMix,                        Stereo8BitLinearMix,        Stereo16BitLinearMix,
    Mono8BitLinearRampMix,            Mono16BitLinearRampMix,                Stereo8BitLinearRampMix,Stereo16BitLinearRampMix,
    // Linear SRC, Filter
    FilterMono8BitLinearMix,    FilterMono16BitLinearMix,        FilterStereo8BitLinearMix,        FilterStereo16BitLinearMix,
    FilterMono8BitLinearRampMix,FilterMono16BitLinearRampMix,FilterStereo8BitLinearRampMix,FilterStereo16BitLinearRampMix,
    // HQ SRC
    Mono8BitHQMix,                            Mono16BitHQMix,                                Stereo8BitHQMix,                Stereo16BitHQMix,
    Mono8BitHQRampMix,                    Mono16BitHQRampMix,                        Stereo8BitHQRampMix,        Stereo16BitHQRampMix,
    // HQ SRC, Filter

// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
//    FilterMono8BitLinearMix,        FilterMono16BitLinearMix,        FilterStereo8BitLinearMix,        FilterStereo16BitLinearMix,
//    FilterMono8BitLinearRampMix,FilterMono16BitLinearRampMix,FilterStereo8BitLinearRampMix,FilterStereo16BitLinearRampMix,
    FilterMono8BitHQMix,            FilterMono16BitHQMix,                FilterStereo8BitHQMix,                FilterStereo16BitHQMix,
    FilterMono8BitHQRampMix,    FilterMono16BitHQRampMix,        FilterStereo8BitHQRampMix,        FilterStereo16BitHQRampMix,
// -! BEHAVIOUR_CHANGE#0025

    // Kaiser SRC
// -> CODE#0025
// -> DESC="enable polyphase resampling on stereo samples"
//    Mono8BitKaiserMix,                        Mono16BitKaiserMix,                        Stereo8BitHQMix,                Stereo16BitHQMix,
//    Mono8BitKaiserRampMix,                Mono16BitKaiserRampMix,                Stereo8BitHQRampMix,        Stereo16BitHQRampMix,
    Mono8BitKaiserMix,                    Mono16BitKaiserMix,                        Stereo8BitKaiserMix,        Stereo16BitKaiserMix,
    Mono8BitKaiserRampMix,            Mono16BitKaiserRampMix,                Stereo8BitKaiserRampMix,Stereo16BitKaiserRampMix,
// -! BEHAVIOUR_CHANGE#0025

    // Kaiser SRC, Filter
//    FilterMono8BitLinearMix,        FilterMono16BitLinearMix,        FilterStereo8BitLinearMix,        FilterStereo16BitLinearMix,
//    FilterMono8BitLinearRampMix,FilterMono16BitLinearRampMix,FilterStereo8BitLinearRampMix,FilterStereo16BitLinearRampMix,
    FilterMono8BitKaiserMix,    FilterMono16BitKaiserMix,        FilterStereo8BitKaiserMix,        FilterStereo16BitKaiserMix,
    FilterMono8BitKaiserRampMix,FilterMono16BitKaiserRampMix,FilterStereo8BitKaiserRampMix,FilterStereo16BitKaiserRampMix,

    // FIR Filter SRC
    Mono8BitFIRFilterMix,                    Mono16BitFIRFilterMix,                        Stereo8BitFIRFilterMix,        Stereo16BitFIRFilterMix,
    Mono8BitFIRFilterRampMix,            Mono16BitFIRFilterRampMix,                Stereo8BitFIRFilterRampMix,Stereo16BitFIRFilterRampMix,

    // FIR Filter SRC, Filter
    FilterMono8BitFIRFilterMix,    FilterMono16BitFIRFilterMix,        FilterStereo8BitFIRFilterMix,        FilterStereo16BitFIRFilterMix,
    FilterMono8BitFIRFilterRampMix,FilterMono16BitFIRFilterRampMix,FilterStereo8BitFIRFilterRampMix,FilterStereo16BitFIRFilterRampMix,

// -! BEHAVIOUR_CHANGE#0025
};

const LPMIXINTERFACE gpFastMixFunctionTable[2*16] =
{
    // No SRC
    FastMono8BitMix,                    FastMono16BitMix,                        Stereo8BitMix,                        Stereo16BitMix,
    FastMono8BitRampMix,            FastMono16BitRampMix,                Stereo8BitRampMix,                Stereo16BitRampMix,
    // No SRC, Filter
    FilterMono8BitMix,                    FilterMono16BitMix,                        FilterStereo8BitMix,        FilterStereo16BitMix,
    FilterMono8BitRampMix,            FilterMono16BitRampMix,                FilterStereo8BitRampMix,FilterStereo16BitRampMix,
    // Linear SRC
    FastMono8BitLinearMix,            FastMono16BitLinearMix,                Stereo8BitLinearMix,        Stereo16BitLinearMix,
    FastMono8BitLinearRampMix,    FastMono16BitLinearRampMix,        Stereo8BitLinearRampMix,Stereo16BitLinearRampMix,
    // Linear SRC, Filter
    FilterMono8BitLinearMix,    FilterMono16BitLinearMix,        FilterStereo8BitLinearMix,        FilterStereo16BitLinearMix,
    FilterMono8BitLinearRampMix,FilterMono16BitLinearRampMix,FilterStereo8BitLinearRampMix,FilterStereo16BitLinearRampMix,
};
*/


/////////////////////////////////////////////////////////////////////////






UINT module_renderer::CreateStereoMix(int count)
//-----------------------------------------
{
    uint32_t nchused, nchmixed;

    if (!count) return 0;
    nchused = nchmixed = 0;
    for (UINT nChn=0; nChn<m_nMixChannels; nChn++)
    {
        const LPMIXINTERFACE *pMixFuncTable;
        modplug::tracker::voice_ty * const pChannel = &Chn[ChnMix[nChn]];
        LONG nSmpCount;
        int nsamples;

        size_t channel_i_care_about = pChannel->parent_channel ? (pChannel->parent_channel - 1) : (ChnMix[nChn]);
        auto herp = (channel_i_care_about > modplug::mixgraph::MAX_PHYSICAL_CHANNELS) ?
            (mixgraph.channel_bypass) :
            (mixgraph.channel_vertices[channel_i_care_about]);
        auto herp_left = herp->channels[0];
        auto herp_right = herp->channels[1];

        if (!pChannel->active_sample_data.generic) continue;
        //nFlags |= GetResamplingFlag(pChannel);
        nsamples = count;

        //Look for plugins associated with this implicit tracker channel.
        UINT nMixPlugin = GetBestPlugin(ChnMix[nChn], PRIORITISE_INSTRUMENT, RESPECT_MUTES);

        //rewbs.instroVSTi
/*            UINT nMixPlugin=0;
        if (pChannel->instrument && pChannel->pInstrument) {    // first try intrument VST
            if (!(pChannel->pInstrument->uFlags & ENV_MUTE))
                nMixPlugin = pChannel->instrument->nMixPlug;
        }
        if (!nMixPlugin && (nMasterCh > 0) && (nMasterCh <= m_nChannels)) {     // Then try Channel VST
            if(!(pChannel->dwFlags & CHN_NOFX))
                nMixPlugin = ChnSettings[nMasterCh-1].nMixPlugin;
        }
*/

        //end rewbs.instroVSTi
        //XXXih: vchan mix -> plugin prepopulated state heer
        /*
        if ((nMixPlugin > 0) && (nMixPlugin <= MAX_MIXPLUGINS))
        {
            PSNDMIXPLUGINSTATE pPlugin = m_MixPlugins[nMixPlugin-1].pMixState;
            if ((pPlugin) && (pPlugin->pMixBuffer))
            {
                pbuffer = pPlugin->pMixBuffer;
                pOfsR = &pPlugin->nVolDecayR;
                pOfsL = &pPlugin->nVolDecayL;
                if (!(pPlugin->dwFlags & MIXPLUG_MIXREADY))
                {
                    //modplug::mixer::stereo_fill(pbuffer, count, pOfsR, pOfsL);
                    pPlugin->dwFlags |= MIXPLUG_MIXREADY;
                }
            }
        }
        pbuffer = MixSoundBuffer;
        */
        nchused++;
        ////////////////////////////////////////////////////
    SampleLooping:
        UINT nrampsamples = nsamples;
        if (pChannel->nRampLength > 0)
        {
            if ((LONG)nrampsamples > pChannel->nRampLength) nrampsamples = pChannel->nRampLength;
        }
        nSmpCount = GetSampleCount(pChannel, nrampsamples, m_bITBidiMode);
        //nSmpCount = calc_contiguous_span_in_place_legacy(*pChannel, nrampsamples, m_bITBidiMode);

        if (nSmpCount <= 0) {
            // Stopping the channel
            pChannel->active_sample_data.generic = nullptr;
            pChannel->length = 0;
            pChannel->sample_position = 0;
            pChannel->fractional_sample_position = 0;
            pChannel->nRampLength = 0;

            pChannel->nROfs = pChannel->nLOfs = 0;
            bitset_remove(pChannel->flags, vflag_ty::ScrubBackwards);
            continue;
        }
        // Should we mix this channel ?
        UINT naddmix;
        if (
            ( (nchmixed >= m_nMaxMixChannels)
             && (!(deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK))
            )
         || ( (pChannel->nRampLength == 0)
             && ( (pChannel->left_volume == 0) && (pChannel->right_volume == 0) )
            )
           )
        {
            /*
            LONG delta = (pChannel->position_delta * (LONG)nSmpCount) + (LONG)pChannel->fractional_sample_position;
            pChannel->fractional_sample_position = delta & 0xFFFF;
            pChannel->sample_position += (delta >> 16);
            pChannel->nROfs = pChannel->nLOfs = 0;
            */
            advance_silently(*pChannel, nSmpCount);

            herp_left += nSmpCount;
            herp_right += nSmpCount;
            naddmix = 0;
        } else
        // Do mixing
        {
            // Choose function for mixing
            //LPMIXINTERFACE pMixFunc;
            //XXXih: disabled legacy garbage
            //pMixFunc = (pChannel->nRampLength) ? pMixFuncTable[nFlags|MIXNDX_RAMP] : pMixFuncTable[nFlags];

            int maxlen = nSmpCount;

            pChannel->nROfs = 0;
            pChannel->nLOfs = 0;

            modplug::mixgraph::resample_and_mix(herp_left, herp_right, pChannel, maxlen);

            herp_left += maxlen;
            herp_right += maxlen;

            naddmix = 1;
        }
        nsamples -= nSmpCount;
        if (pChannel->nRampLength)
        {
            pChannel->nRampLength -= nSmpCount;
            if (pChannel->nRampLength <= 0)
            {
                pChannel->nRampLength = 0;
                pChannel->right_volume = pChannel->nNewRightVol;
                pChannel->left_volume = pChannel->nNewLeftVol;
                pChannel->right_ramp = pChannel->left_ramp = 0;
                if (bitset_is_set(pChannel->flags, vflag_ty::NoteFade) && (!(pChannel->nFadeOutVol)))
                {
                    pChannel->length = 0;
                    pChannel->active_sample_data.generic = nullptr;
                }
            }
        }
        if (nsamples > 0) goto SampleLooping;
        nchmixed += naddmix;
    }
    return nchused;
}

UINT module_renderer::GetResamplingFlag(const modplug::tracker::voice_ty *pChannel)
//------------------------------------------------------------
{
    if (pChannel->instrument) {
        switch (pChannel->instrument->resampling_mode) {
            case SRCMODE_NEAREST:    return 0;
            case SRCMODE_LINEAR:    return MIXNDX_LINEARSRC;
            case SRCMODE_SPLINE:    return MIXNDX_HQSRC;
            case SRCMODE_POLYPHASE: return MIXNDX_KAISERSRC;
            case SRCMODE_FIRFILTER: return MIXNDX_FIRFILTERSRC;
//                    default: ;
        }
    }

    //didn't manage to get flag from instrument header, use channel flags.
        if (deprecated_global_sound_setup_bitmask & SNDMIX_SPLINESRCMODE)            return MIXNDX_HQSRC;
        if (deprecated_global_sound_setup_bitmask & SNDMIX_POLYPHASESRCMODE)    return MIXNDX_KAISERSRC;
        if (deprecated_global_sound_setup_bitmask & SNDMIX_FIRFILTERSRCMODE)    return MIXNDX_FIRFILTERSRC;
        return MIXNDX_LINEARSRC;
}


extern int gbInitPlugins;

void module_renderer::ProcessPlugins(UINT nCount)
//------------------------------------------
{
    // Setup float inputs
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        PSNDMIXPLUGIN pPlugin = &m_MixPlugins[iPlug];
        if ((pPlugin->pMixPlugin) && (pPlugin->pMixState)
         && (pPlugin->pMixState->pMixBuffer)
         && (pPlugin->pMixState->pOutBufferL)
         && (pPlugin->pMixState->pOutBufferR))
        {
            PSNDMIXPLUGINSTATE pState = pPlugin->pMixState;
            // Init plugins ?
            /*if (gbInitPlugins)
            {   //ToDo: do this in resume.
                pPlugin->pMixPlugin->Init(gdwMixingFreq, (gbInitPlugins & 2) ? TRUE : FALSE);
            }*/

            //We should only ever reach this point if the song is playing.
            if (!pPlugin->pMixPlugin->IsSongPlaying())
            {
                //Plugin doesn't know it is in a song that is playing;
                //we must have added it during playback. Initialise it!
                pPlugin->pMixPlugin->NotifySongPlaying(true);
                pPlugin->pMixPlugin->Resume();
            }


            // Setup float input
            if (pState->dwFlags & MIXPLUG_MIXREADY)
            {
                StereoMixToFloat(pState->pMixBuffer, pState->pOutBufferL, pState->pOutBufferR, nCount);
            } else
            if (pState->nVolDecayR|pState->nVolDecayL)
            {
                //modplug::mixer::stereo_fill(pState->pMixBuffer, nCount, &pState->nVolDecayR, &pState->nVolDecayL);
                //XXXih: indiana jones and the ancient mayan treasure
                StereoMixToFloat(pState->pMixBuffer, pState->pOutBufferL, pState->pOutBufferR, nCount);
            } else
            {
                memset(pState->pOutBufferL, 0, nCount*sizeof(FLOAT));
                memset(pState->pOutBufferR, 0, nCount*sizeof(FLOAT));
            }
            pState->dwFlags &= ~MIXPLUG_MIXREADY;
        }
    }
    // Convert mix buffer
    StereoMixToFloat(MixSoundBuffer, MixFloatBuffer, MixFloatBuffer+modplug::mixgraph::MIX_BUFFER_SIZE, nCount);
    FLOAT *pMixL = MixFloatBuffer;
    FLOAT *pMixR = MixFloatBuffer + modplug::mixgraph::MIX_BUFFER_SIZE;

    // Process Plugins
    //XXXih replace
    //XXXih replace
    //XXXih replace
    for (UINT iDoPlug=0; iDoPlug<MAX_MIXPLUGINS; iDoPlug++)
    {
        PSNDMIXPLUGIN pPlugin = &m_MixPlugins[iDoPlug];
        if ((pPlugin->pMixPlugin) && (pPlugin->pMixState)
         && (pPlugin->pMixState->pMixBuffer)
         && (pPlugin->pMixState->pOutBufferL)
         && (pPlugin->pMixState->pOutBufferR))
        {
            BOOL bMasterMix = FALSE;
            if (pMixL == pPlugin->pMixState->pOutBufferL)
            {
                bMasterMix = TRUE;
                pMixL = MixFloatBuffer;
                pMixR = MixFloatBuffer + modplug::mixgraph::MIX_BUFFER_SIZE;
            }
            IMixPlugin *pObject = pPlugin->pMixPlugin;
            PSNDMIXPLUGINSTATE pState = pPlugin->pMixState;
            FLOAT *pOutL = pMixL;
            FLOAT *pOutR = pMixR;

            if (pPlugin->Info.dwOutputRouting & 0x80)
            {
                UINT nOutput = pPlugin->Info.dwOutputRouting & 0x7f;
                if ((nOutput > iDoPlug) && (nOutput < MAX_MIXPLUGINS)
                 && (m_MixPlugins[nOutput].pMixState))
                {
                    PSNDMIXPLUGINSTATE pOutState = m_MixPlugins[nOutput].pMixState;

                    if( (pOutState->pOutBufferL) && (pOutState->pOutBufferR) )
                    {
                        pOutL = pOutState->pOutBufferL;
                        pOutR = pOutState->pOutBufferR;
                    }
                }
            }

            /*
            if (pPlugin->multiRouting) {
                int nOutput=0;
                for (int nOutput=0; nOutput<pPlugin->nOutputs/2; nOutput++) {
                    destinationPlug = pPlugin->multiRoutingDestinations[nOutput];
                    pOutState = m_MixPlugins[destinationPlug].pMixState;
                    pOutputs[2*nOutput] = pOutState->pOutBufferL;
                    pOutputs[2*(nOutput+1)] = pOutState->pOutBufferR;
                }

            }
*/

            if (pPlugin->Info.dwInputRouting & MIXPLUG_INPUTF_MASTEREFFECT)
            {
                if (!bMasterMix)
                {
                    FLOAT *pInL = pState->pOutBufferL;
                    FLOAT *pInR = pState->pOutBufferR;
                    for (UINT i=0; i<nCount; i++)
                    {
                        pInL[i] += pMixL[i];
                        pInR[i] += pMixR[i];
                        pMixL[i] = 0;
                        pMixR[i] = 0;
                    }
                }
                pMixL = pOutL;
                pMixR = pOutR;
            }

            if (pPlugin->Info.dwInputRouting & MIXPLUG_INPUTF_BYPASS)
            {
                const FLOAT * const pInL = pState->pOutBufferL;
                const FLOAT * const pInR = pState->pOutBufferR;
                for (UINT i=0; i<nCount; i++)
                {
                    pOutL[i] += pInL[i];
                    pOutR[i] += pInR[i];
                }
            } else
            {
                pObject->Process(pOutL, pOutR, nCount);
            }
        }
    }
    //XXXih replace
    //XXXih replace
    //XXXih replace
    FloatToStereoMix(pMixL, pMixR, MixSoundBuffer, nCount);
    gbInitPlugins = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Float <-> Int conversion
//


float module_renderer::m_nMaxSample = 0;

VOID module_renderer::StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount)
//-----------------------------------------------------------------------------------------
{

#ifdef ENABLE_MMX
    if (deprecated_global_sound_setup_bitmask & SNDMIX_ENABLEMMX)
    {
        if (deprecated_global_system_info & SYSMIX_SSE)
        {
#ifdef ENABLE_SSE
        SSE_StereoMixToFloat(pSrc, pOut1, pOut2, nCount, m_pConfig->getIntToFloat());
#endif
            return;
        }
        if (deprecated_global_system_info & SYSMIX_3DNOW)
        {
#ifdef ENABLE_AMD
        AMD_StereoMixToFloat(pSrc, pOut1, pOut2, nCount, m_pConfig->getIntToFloat());
#endif
            return;
        }
    }
#endif

}


VOID module_renderer::FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, UINT nCount)
//---------------------------------------------------------------------------------------------
{
    if (deprecated_global_sound_setup_bitmask & SNDMIX_ENABLEMMX)
    {
        if (deprecated_global_system_info & SYSMIX_3DNOW)
        {
#ifdef ENABLE_AMDNOW
            AMD_FloatToStereoMix(pIn1, pIn2, pOut, nCount, m_pConfig->getFloatToInt());
#endif
            return;
        }
    }
}


VOID module_renderer::MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount)
//------------------------------------------------------------------------
{
    if (deprecated_global_sound_setup_bitmask & SNDMIX_ENABLEMMX)
    {
        if (deprecated_global_system_info & SYSMIX_SSE)
        {
#ifdef ENABLE_SSE
        SSE_MonoMixToFloat(pSrc, pOut, nCount, m_pConfig->getIntToFloat());
#endif
            return;
        }
        if (deprecated_global_system_info & SYSMIX_3DNOW)
        {
#ifdef ENABLE_AMDNOW
            AMD_MonoMixToFloat(pSrc, pOut, nCount, m_pConfig->getIntToFloat());
#endif
            return;
        }
    }

}


VOID module_renderer::FloatToMonoMix(const float *pIn, int *pOut, UINT nCount)
//-----------------------------------------------------------------------
{
    if (deprecated_global_sound_setup_bitmask & SNDMIX_ENABLEMMX)
    {
        if (deprecated_global_system_info & SYSMIX_3DNOW)
        {
#ifdef ENABLE_AMDNOW
            AMD_FloatToMonoMix(pIn, pOut, nCount, m_pConfig->getFloatToInt());
#endif
            return;
        }
    }
}



//////////////////////////////////////////////////////////////////////////
// Noise Shaping (Dither)

#pragma warning(disable:4731) // ebp modified

void MPPASMCALL X86_Dither(int *pBuffer, UINT nSamples, UINT nBits)
//-----------------------------------------------------------------
{
    static int gDitherA, gDitherB;

    __asm {
    mov esi, pBuffer    // esi = pBuffer+i
    mov eax, nSamples    // ebp = i
    mov ecx, nBits            // ecx = number of bits of noise
    mov edi, gDitherA    // Noise generation
    mov ebx, gDitherB
    add ecx, modplug::mixgraph::MIXING_ATTENUATION+1
    push ebp
    mov ebp, eax
noiseloop:
    rol edi, 1
    mov eax, dword ptr [esi]
    xor edi, 0x10204080
    add esi, 4
    lea edi, [ebx*4+edi+0x78649E7D]
    mov edx, edi
    rol edx, 16
    lea edx, [edx*4+edx]
    add ebx, edx
    mov edx, ebx
    sar edx, cl
    add eax, edx
/*
    int a = 0, b = 0;
    for (UINT i=0; i<len; i++)
    {
        a = (a << 1) | (((uint32_t)a) >> (uint8_t)31);
        a ^= 0x10204080;
        a += 0x78649E7D + (b << 2);
        b += ((a << 16) | (a >> 16)) * 5;
        int c = a + b;
        p[i] = ((signed char)c ) >> 1;
    }
*/
    dec ebp
    mov dword ptr [esi-4], eax
    jnz noiseloop
    pop ebp
    mov gDitherA, edi
    mov gDitherB, ebx
    }
}

VOID MPPASMCALL X86_MonoFromStereo(int *pMixBuf, UINT nSamples)
//-------------------------------------------------------------
{
    _asm {
    mov ecx, nSamples
    mov esi, pMixBuf
    mov edi, esi
stloop:
    mov eax, dword ptr [esi]
    mov edx, dword ptr [esi+4]
    add edi, 4
    add esi, 8
    add eax, edx
    sar eax, 1
    dec ecx
    mov dword ptr [edi-4], eax
    jnz stloop
    }
}
