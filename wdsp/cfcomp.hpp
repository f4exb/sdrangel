/*  cfcomp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V
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

#ifndef wdsp_cfcomp_h
#define wdsp_cfcomp_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API CFCOMP
{
public:
    int run;
    int position;
    int bsize;
    float* in;
    float* out;
    int fsize;
    int ovrlp;
    int incr;
    float* window;
    int iasize;
    float* inaccum;
    float* forfftin;
    float* forfftout;
    int msize;
    float* cmask;
    float* mask;
    int mask_ready;
    float* cfc_gain;
    float* revfftin;
    float* revfftout;
    float** save;
    int oasize;
    float* outaccum;
    float rate;
    int wintype;
    float pregain;
    float postgain;
    int nsamps;
    int iainidx;
    int iaoutidx;
    int init_oainidx;
    int oainidx;
    int oaoutidx;
    int saveidx;
    fftwf_plan Rfor;
    fftwf_plan Rrev;

    int comp_method;
    int nfreqs;
    float* F;
    float* G;
    float* E;
    float* fp;
    float* gp;
    float* ep;
    float* comp;
    float precomp;
    float precomplin;
    float* peq;
    int peq_run;
    float prepeq;
    float prepeqlin;
    float winfudge;

    float gain;
    float mtau;
    float mmult;
    // display stuff
    float dtau;
    float dmult;
    float* delta;
    float* delta_copy;
    float* cfc_gain_copy;

    static CFCOMP* create_cfcomp (
        int run,
        int position,
        int peq_run,
        int size,
        float* in,
        float* out,
        int fsize,
        int ovrlp,
        int rate,
        int wintype,
        int comp_method,
        int nfreqs,
        float precomp,
        float prepeq,
        float* F,
        float* G,
        float* E,
        float mtau,
        float dtau
    );
    static void destroy_cfcomp (CFCOMP *a);
    static void flush_cfcomp (CFCOMP *a);
    static void xcfcomp (CFCOMP *a, int pos);
    static void setBuffers_cfcomp (CFCOMP *a, float* in, float* out);
    static void setSamplerate_cfcomp (CFCOMP *a, int rate);
    static void setSize_cfcomp (CFCOMP *a, int size);
    // TXA Properties
    static void SetCFCOMPRun (TXA& txa, int run);
    static void SetCFCOMPPosition (TXA& txa, int pos);
    static void SetCFCOMPprofile (TXA& txa, int nfreqs, float* F, float* G, float *E);
    static void SetCFCOMPPrecomp (TXA& txa, float precomp);
    static void SetCFCOMPPeqRun (TXA& txa, int run);
    static void SetCFCOMPPrePeq (TXA& txa, float prepeq);
    static void GetCFCOMPDisplayCompression (TXA& txa, float* comp_values, int* ready);

private:
    static void calc_cfcwindow (CFCOMP *a);
    static int fCOMPcompare (const void *a, const void *b);
    static void calc_comp (CFCOMP *a);
    static void calc_cfcomp(CFCOMP *a);
    static void decalc_cfcomp(CFCOMP *a);
    static void calc_mask (CFCOMP *a);
};

} // namespace WDSP

#endif
