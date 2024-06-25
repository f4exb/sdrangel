/*  snb.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

#ifndef wdsp_snba_h
#define wdsp_snba_h

#include "export.h"

namespace WDSP {

class RESAMPLE;
class RXA;

class WDSP_API SNBA
{
public:
    int run;
    float* in;
    float* out;
    int inrate;
    int internalrate;
    int bsize;
    int xsize;
    int ovrlp;
    int incr;
    int iasize;
    int iainidx;
    int iaoutidx;
    float* inaccum;
    float* xbase;
    float* xaux;
    int nsamps;
    int oasize;
    int oainidx;
    int oaoutidx;
    int init_oaoutidx;
    float* outaccum;

    int resamprun;
    int isize;
    RESAMPLE *inresamp;
    RESAMPLE *outresamp;
    float* inbuff;
    float* outbuff;
    struct _exec
    {
        int asize;
        float* a;
        float* v;
        int* detout;
        float* savex;
        float* xHout;
        int* unfixed;
        int npasses;
    } exec;
    struct _det
    {
        float k1;
        float k2;
        int b;
        int pre;
        int post;
        float* vp;
        float* vpwr;
    } sdet;
    struct _scan
    {
        float pmultmin;
    } scan;
    struct _wrk
    {
        int xHat_a1rows_max;
        int xHat_a2cols_max;
        float* xHat_r;
        float* xHat_ATAI;
        float* xHat_A1;
        float* xHat_A2;
        float* xHat_P1;
        float* xHat_P2;
        float* trI_y;
        float* trI_v;
        float* dR_z;
        float* asolve_r;
        float* asolve_z;
    } wrk;
    float out_low_cut;
    float out_high_cut;

    static SNBA* create_snba (
        int run,
        float* in,
        float* out,
        int inrate,
        int internalrate,
        int bsize,
        int ovrlp,
        int xsize,
        int asize,
        int npasses,
        float k1,
        float k2,
        int b,
        int pre,
        int post,
        float pmultmin,
        float out_low_cut,
        float out_high_cut
    );
    static void destroy_snba (SNBA *d);
    static void flush_snba (SNBA *d);
    static void xsnba (SNBA *d);
    static void setBuffers_snba (SNBA *a, float* in, float* out);
    static void setSamplerate_snba (SNBA *a, int rate);
    static void setSize_snba (SNBA *a, int size);
    // RXA
    static void SetSNBAOutputBandwidth (RXA& rxa, float flow, float fhigh);
    static void SetSNBARun (RXA& rxa, int run);
    static void SetSNBAovrlp (RXA& rxa, int ovrlp);
    static void SetSNBAasize (RXA& rxa, int size);
    static void SetSNBAnpasses (RXA& rxa, int npasses);
    static void SetSNBAk1 (RXA& rxa, float k1);
    static void SetSNBAk2 (RXA& rxa, float k2);
    static void SetSNBAbridge (RXA& rxa, int bridge);
    static void SetSNBApresamps (RXA& rxa, int presamps);
    static void SetSNBApostsamps (RXA& rxa, int postsamps);
    static void SetSNBApmultmin (RXA& rxa, float pmultmin);

private:
    static void calc_snba (SNBA *d);
    static void decalc_snba (SNBA *d);
    static void ATAc0 (int n, int nr, float* A, float* r);
    static void multA1TA2(float* a1, float* a2, int m, int n, int q, float* c);
    static void multXKE(float* a, float* xk, int m, int q, int p, float* vout);
    static void multAv(float* a, float* v, int m, int q, float* vout);
    static void xHat(
        int xusize,
        int asize,
        float* xk,
        float* a,
        float* xout,
        float* r,
        float* ATAI,
        float* A1,
        float* A2,
        float* P1,
        float* P2,
        float* trI_y,
        float* trI_v,
        float* dR_z
    );
    static void invf(int xsize, int asize, float* a, float* x, float* v);
    static void det(SNBA *d, int asize, float* v, int* detout);
    static int scanFrame(
        int xsize,
        int pval,
        float pmultmin,
        int* det,
        int* bimp,
        int* limp,
        int* befimp,
        int* aftimp,
        int* p_opt,
        int* next
    );
    static void execFrame(SNBA *d, float* x);
};


class NBP;
class NOTCHDB;

class WDSP_API BPSNBA
{
public:
    int run;                        // run the filter
    int run_notches;                // use the notches, vs straight bandpass
    int position;                   // position in the processing pipeline
    int size;                       // buffer size
    int nc;                         // number of filter coefficients
    int mp;                         // minimum phase flag
    float* in;                     // input buffer
    float* out;                    // output buffer
    int rate;                       // sample rate
    float* buff;                   // internal buffer
    NBP *bpsnba;                    // pointer to the notched bandpass filter, nbp
    float f_low;                   // low cutoff frequency
    float f_high;                  // high cutoff frequency
    float abs_low_freq;            // lowest positive freq supported by SNB
    float abs_high_freq;           // highest positive freq supported by SNG
    int wintype;                    // filter window type
    float gain;                    // filter gain
    int autoincr;                   // use auto increment for notch width
    int maxpb;                      // maximum passband segments supported
    NOTCHDB* ptraddr;               // pointer to address of NOTCH DATABASE

    static BPSNBA* create_bpsnba (
        int run,
        int run_notches,
        int position,
        int size,
        int nc,
        int mp,
        float* in,
        float* out,
        int rate,
        float abs_low_freq,
        float abs_high_freq,
        float f_low,
        float f_high,
        int wintype,
        float gain,
        int autoincr,
        int maxpb,
        NOTCHDB* ptraddr
    );
    static void destroy_bpsnba (BPSNBA *a);
    static void flush_bpsnba (BPSNBA *a);
    static void setBuffers_bpsnba (BPSNBA *a, float* in, float* out);
    static void setSamplerate_bpsnba (BPSNBA *a, int rate);
    static void setSize_bpsnba (BPSNBA *a, int size);
    static void xbpsnbain (BPSNBA *a, int position);
    static void xbpsnbaout (BPSNBA *a, int position);
    static void recalc_bpsnba_filter (BPSNBA *a, int update);
    // RXA
    static void BPSNBASetNC (RXA& rxa, int nc);
    static void BPSNBASetMP (RXA& rxa, int mp);
    static void bpsnbaCheck (RXA& rxa, int mode, int notch_run);
    static void bpsnbaSet (RXA& rxa);

private:
    static void calc_bpsnba (BPSNBA *a);
    static void decalc_bpsnba (BPSNBA *a);
};

} // namespace WDSP

#endif
