/*  eq.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Equalizer                                  *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_eqp_h
#define wdsp_eqp_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class RXA;
class TXA;

class WDSP_API EQP
{
public:
    int run;
    int size;
    int nc;
    int mp;
    float* in;
    float* out;
    int nfreqs;
    float* F;
    float* G;
    int ctfmode;
    int wintype;
    float samplerate;
    FIRCORE *p;

    static EQP* create_eqp (
        int run,
        int size,
        int nc,
        int mp,
        float *in,
        float *out,
        int nfreqs,
        float* F,
        float* G,
        int ctfmode,
        int wintype,
        int samplerate
    );
    static float* eq_impulse (int N, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype);
    static void destroy_eqp (EQP *a);
    static void flush_eqp (EQP *a);
    static void xeqp (EQP *a);
    static void setBuffers_eqp (EQP *a, float* in, float* out);
    static void setSamplerate_eqp (EQP *a, int rate);
    static void setSize_eqp (EQP *a, int size);
    // RXA
    static void SetEQRun (RXA& rxa, int run);
    static void SetEQNC (RXA& rxa, int nc);
    static void SetEQMP (RXA& rxa, int mp);
    static void SetEQProfile (RXA& rxa, int nfreqs, float* F, float* G);
    static void SetEQCtfmode (RXA& rxa, int mode);
    static void SetEQWintype (RXA& rxa, int wintype);
    static void SetGrphEQ (RXA& rxa, int *rxeq);
    static void SetGrphEQ10 (RXA& rxa, int *rxeq);
    // TXA
    static void SetEQRun (TXA& txa, int run);
    static void SetEQNC (TXA& txa, int nc);
    static void SetEQMP (TXA& txa, int mp);
    static void SetEQProfile (TXA& txa, int nfreqs, float* F, float* G);
    static void SetEQCtfmode (TXA& txa, int mode);
    static void SetEQWintype (TXA& txa, int wintype);
    static void SetGrphEQ (TXA& txa, int *txeq);
    static void SetGrphEQ10 (TXA& txa, int *txeq);

    static float* eq_mults (int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype);

private:
    static int fEQcompare (const void * a, const void * b);
};

} // namespace WDSP

#endif



/********************************************************************************************************
*                                                                                                       *
*                                           Overlap-Save Equalizer                                      *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_eq_h
#define wdsp_eq_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API EQ
{
public:
    int run;
    int size;
    float* in;
    float* out;
    int nfreqs;
    float* F;
    float* G;
    float* infilt;
    float* product;
    float* mults;
    float scale;
    int ctfmode;
    int wintype;
    float samplerate;
    fftwf_plan CFor;
    fftwf_plan CRev;

    static EQ* create_eq (int run, int size, float *in, float *out, int nfreqs, float* F, float* G, int ctfmode, int wintype, int samplerate);
    // static float* eq_mults (int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype);
    static void destroy_eq (EQ *a);
    static void flush_eq (EQ *a);
    static void xeq (EQ *a);
    static void setBuffers_eq (EQ *a, float* in, float* out);
    static void setSamplerate_eq (EQ *a, int rate);
    static void setSize_eq (EQ *a, int size);

private:
    static void calc_eq (EQ *a);
    static void decalc_eq (EQ *a);
};

} // namespace WDSP

#endif
