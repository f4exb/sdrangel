/*
 * codec2_fft.c
 *
 *  Created on: 24.09.2016
 *      Author: danilo
 */

#include "codec2_fft.h"

#ifdef USE_KISS_FFT
#include "_kiss_fft_guts.h"
#endif

namespace FreeDV
{

#ifdef USE_KISS_FFT
#else
#if 0
// caching constants in RAM did not seem to have an effect on performance
// TODO: Decide what to with this code
#define FFT_INIT_CACHE_SIZE 4
const arm_cfft_instance_f32* fft_init_cache[FFT_INIT_CACHE_SIZE];

static const arm_cfft_instance_f32* arm_fft_instance2ram(const arm_cfft_instance_f32* in)
{

    arm_cfft_instance_f32* out = malloc(sizeof(arm_cfft_instance_f32));

    if (out) {
        memcpy(out,in,sizeof(arm_cfft_instance_f32));
        out->pBitRevTable = malloc(out->bitRevLength * sizeof(uint16_t));
        out->pTwiddle = malloc(out->fftLen * sizeof(float32_t));
        memcpy((void*)out->pBitRevTable,in->pBitRevTable,out->bitRevLength * sizeof(uint16_t));
        memcpy((void*)out->pTwiddle,in->pTwiddle,out->fftLen * sizeof(float32_t));
    }
    return out;
}


static const arm_cfft_instance_f32* arm_fft_cache_get(const arm_cfft_instance_f32* romfft)
{
    const arm_cfft_instance_f32* retval = NULL;
    static int used = 0;
    for (int i = 0; fft_init_cache[i] != NULL && i < used; i++)
    {
        if (romfft->fftLen == fft_init_cache[i]->fftLen)
        {
            retval = fft_init_cache[i];
            break;
        }
    }
    if (retval == NULL && used < FFT_INIT_CACHE_SIZE)
    {
         retval = arm_fft_instance2ram(romfft);
         fft_init_cache[used++] = retval;
    }
    if (retval == NULL)
    {
        retval = romfft;
    }
    return retval;
}
#endif
#endif

void codec2_fft_free(codec2_fft_cfg cfg)
{
#ifdef USE_KISS_FFT
    KISS_FFT_FREE(cfg);
#else
    FREE(cfg);
#endif
}

codec2_fft_cfg codec2_fft_alloc(int nfft, int inverse_fft, void* mem, std::size_t* lenmem)
{
    codec2_fft_cfg retval;
#ifdef USE_KISS_FFT
    retval = kiss_fft_alloc(nfft, inverse_fft, mem, lenmem);
#else
    retval = MALLOC(sizeof(codec2_fft_struct));
    retval->inverse  = inverse_fft;
    switch(nfft)
    {
    case 128:
        retval->instance = &arm_cfft_sR_f32_len128;
        break;
    case 256:
        retval->instance = &arm_cfft_sR_f32_len256;
        break;
    case 512:
        retval->instance = &arm_cfft_sR_f32_len512;
        break;
//    case 1024:
//        retval->instance = &arm_cfft_sR_f32_len1024;
//        break;
    default:
        abort();
    }
    // retval->instance = arm_fft_cache_get(retval->instance);
#endif
    return retval;
}

codec2_fftr_cfg codec2_fftr_alloc(int nfft, int inverse_fft, void* mem, std::size_t* lenmem)
{
    codec2_fftr_cfg retval;
#ifdef USE_KISS_FFT
    retval = kiss_fftr_alloc(nfft, inverse_fft, mem, lenmem);
#else
    retval = MALLOC(sizeof(codec2_fftr_struct));
    retval->inverse  = inverse_fft;
    retval->instance = MALLOC(sizeof(arm_rfft_fast_instance_f32));
    arm_rfft_fast_init_f32(retval->instance,nfft);
    // memcpy(&retval->instance->Sint,arm_fft_cache_get(&retval->instance->Sint),sizeof(arm_cfft_instance_f32));
#endif
    return retval;
}
void codec2_fftr_free(codec2_fftr_cfg cfg)
{
#ifdef USE_KISS_FFT
    KISS_FFT_FREE(cfg);
#else
    FREE(cfg->instance);
    FREE(cfg);
#endif
}

// there is a little overhead for inplace kiss_fft but this is
// on the powerful platforms like the Raspberry or even x86 PC based ones
// not noticeable
// the reduced usage of RAM and increased performance on STM32 platforms
// should be worth it.
void codec2_fft_inplace(codec2_fft_cfg cfg, codec2_fft_cpx* inout)
{

#ifdef USE_KISS_FFT
    kiss_fft_cpx in[512];
    // decide whether to use the local stack based buffer for in
    // or to allow kiss_fft to allocate RAM
    // second part is just to play safe since first method
    // is much faster and uses less RAM
    if (cfg->nfft <= 512)
    {
        memcpy(in,inout,cfg->nfft*sizeof(kiss_fft_cpx));
        kiss_fft(cfg, in, (kiss_fft_cpx*)inout);
    }
    else
    {
        kiss_fft(cfg, (kiss_fft_cpx*)inout, (kiss_fft_cpx*)inout);
    }
#else
    arm_cfft_f32(cfg->instance,(float*)inout,cfg->inverse,1);
    if (cfg->inverse)
    {
        arm_scale_f32((float*)inout,cfg->instance->fftLen,(float*)inout,cfg->instance->fftLen*2);
    }

#endif
}

} // FreeDV
