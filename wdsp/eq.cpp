/*  eq.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2017 Warren Pratt, NR0V
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

#include "comm.hpp"
#include "eq.hpp"
#include "firmin.hpp"
#include "fir.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

int EQP::fEQcompare (const void * a, const void * b)
{
    if (*(float*)a < *(float*)b)
        return -1;
    else if (*(float*)a == *(float*)b)
        return 0;
    else
        return 1;
}

float* EQP::eq_impulse (int N, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype)
{
    float* fp = new float[nfreqs + 2]; // (float *) malloc0 ((nfreqs + 2)   * sizeof (float));
    float* gp = new float[nfreqs + 2]; // (float *) malloc0 ((nfreqs + 2)   * sizeof (float));
    float* A  = new float[N / 2 + 1]; // (float *) malloc0 ((N / 2 + 1) * sizeof (float));
    float* sary = new float[2 * nfreqs]; // (float *) malloc0 (2 * nfreqs * sizeof (float));
    float gpreamp, f, frac;
    float* impulse;
    int i, j, mid;
    fp[0] = 0.0;
    fp[nfreqs + 1] = 1.0;
    gpreamp = G[0];
    for (i = 1; i <= nfreqs; i++)
    {
        fp[i] = 2.0 * F[i] / samplerate;
        if (fp[i] < 0.0) fp[i] = 0.0;
        if (fp[i] > 1.0) fp[i] = 1.0;
        gp[i] = G[i];
    }
    for (i = 1, j = 0; i <= nfreqs; i++, j+=2)
    {
        sary[j + 0] = fp[i];
        sary[j + 1] = gp[i];
    }
    qsort (sary, nfreqs, 2 * sizeof (float), fEQcompare);
    for (i = 1, j = 0; i <= nfreqs; i++, j+=2)
    {
        fp[i] = sary[j + 0];
        gp[i] = sary[j + 1];
    }
    gp[0] = gp[1];
    gp[nfreqs + 1] = gp[nfreqs];
    mid = N / 2;
    j = 0;
    if (N & 1)
    {
        for (i = 0; i <= mid; i++)
        {
            f = (float)i / (float)mid;
            while (f > fp[j + 1]) j++;
            frac = (f - fp[j]) / (fp[j + 1] - fp[j]);
            A[i] = pow (10.0, 0.05 * (frac * gp[j + 1] + (1.0 - frac) * gp[j] + gpreamp)) * scale;
        }
    }
    else
    {
        for (i = 0; i < mid; i++)
        {
            f = ((float)i + 0.5) / (float)mid;
            while (f > fp[j + 1]) j++;
            frac = (f - fp[j]) / (fp[j + 1] - fp[j]);
            A[i] = pow (10.0, 0.05 * (frac * gp[j + 1] + (1.0 - frac) * gp[j] + gpreamp)) * scale;
        }
    }
    if (ctfmode == 0)
    {
        int k, low, high;
        float lowmag, highmag, flow4, fhigh4;
        if (N & 1)
        {
            low = (int)(fp[1] * mid);
            high = (int)(fp[nfreqs] * mid + 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((float)low / (float)mid, 4.0);
            fhigh4 = pow((float)high / (float)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (float)k / (float)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-100) lowmag = 1.0e-100;
                A[k] = lowmag;
            }
            k = high;
            while (++k <= mid)
            {
                f = (float)k / (float)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-100) highmag = 1.0e-100;
                A[k] = highmag;
            }
        }
        else
        {
            low = (int)(fp[1] * mid - 0.5);
            high = (int)(fp[nfreqs] * mid - 0.5);
            lowmag = A[low];
            highmag = A[high];
            flow4 = pow((float)low / (float)mid, 4.0);
            fhigh4 = pow((float)high / (float)mid, 4.0);
            k = low;
            while (--k >= 0)
            {
                f = (float)k / (float)mid;
                lowmag *= (f * f * f * f) / flow4;
                if (lowmag < 1.0e-100) lowmag = 1.0e-100;
                A[k] = lowmag;
            }
            k = high;
            while (++k < mid)
            {
                f = (float)k / (float)mid;
                highmag *= fhigh4 / (f * f * f * f);
                if (highmag < 1.0e-100) highmag = 1.0e-100;
                A[k] = highmag;
            }
        }
    }
    if (N & 1)
        impulse = FIR::fir_fsamp_odd(N, A, 1, 1.0, wintype);
    else
        impulse = FIR::fir_fsamp(N, A, 1, 1.0, wintype);
    // print_impulse("eq.txt", N, impulse, 1, 0);
    delete[] (sary);
    delete[] (A);
    delete[] (gp);
    delete[] (fp);
    return impulse;
}

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Equalizer                                  *
*                                                                                                       *
********************************************************************************************************/

EQP* EQP::create_eqp (
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
)
{
    // NOTE:  'nc' must be >= 'size'
    EQP *a = new EQP;
    float* impulse;
    a->run = run;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->in = in;
    a->out = out;
    a->nfreqs = nfreqs;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    a->ctfmode = ctfmode;
    a->wintype = wintype;
    a->samplerate = (float)samplerate;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    a->p = FIRCORE::create_fircore (a->size, a->in, a->out, a->nc, a->mp, impulse);
    delete[] (impulse);
    return a;
}

void EQP::destroy_eqp (EQP *a)
{
    FIRCORE::destroy_fircore (a->p);
    delete (a);
}

void EQP::flush_eqp (EQP *a)
{
    FIRCORE::flush_fircore (a->p);
}

void EQP::xeqp (EQP *a)
{
    if (a->run)
        FIRCORE::xfircore (a->p);
    else
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void EQP::setBuffers_eqp (EQP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    FIRCORE::setBuffers_fircore (a->p, a->in, a->out);
}

void EQP::setSamplerate_eqp (EQP *a, int rate)
{
    float* impulse;
    a->samplerate = rate;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::setSize_eqp (EQP *a, int size)
{
    float* impulse;
    a->size = size;
    FIRCORE::setSize_fircore (a->p, a->size);
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                           Partitioned Overlap-Save Equalizer:  RXA Properties                         *
*                                                                                                       *
********************************************************************************************************/

void EQP::SetEQRun (RXA& rxa, int run)
{
    rxa.csDSP.lock();
    rxa.eqp.p->run = run;
    rxa.csDSP.unlock();
}

void EQP::SetEQNC (RXA& rxa, int nc)
{
    EQP *a;
    float* impulse;
    rxa.csDSP.lock();
    a = rxa.eqp.p;
    if (a->nc != nc)
    {
        a->nc = nc;
        impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
    rxa.csDSP.unlock();
}

void EQP::SetEQMP (RXA& rxa, int mp)
{
    EQP *a;
    a = rxa.eqp.p;
    if (a->mp != mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

void EQP::SetEQProfile (RXA& rxa, int nfreqs, const float* F, const float* G)
{
    EQP *a;
    float* impulse;
    a = rxa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = nfreqs;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G,
        a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetEQCtfmode (RXA& rxa, int mode)
{
    EQP *a;
    float* impulse;
    a = rxa.eqp.p;
    a->ctfmode = mode;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetEQWintype (RXA& rxa, int wintype)
{
    EQP *a;
    float* impulse;
    a = rxa.eqp.p;
    a->wintype = wintype;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetGrphEQ (RXA& rxa, int *rxeq)
{   // three band equalizer (legacy compatibility)
    EQP *a;
    float* impulse;
    a = rxa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 4;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1] =  150.0;
    a->F[2] =  400.0;
    a->F[3] = 1500.0;
    a->F[4] = 6000.0;
    a->G[0] = (float)rxeq[0];
    a->G[1] = (float)rxeq[1];
    a->G[2] = (float)rxeq[1];
    a->G[3] = (float)rxeq[2];
    a->G[4] = (float)rxeq[3];
    a->ctfmode = 0;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetGrphEQ10 (RXA& rxa, int *rxeq)
{   // ten band equalizer (legacy compatibility)
    EQP *a;
    float* impulse;
    int i;
    a = rxa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 10;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1]  =    32.0;
    a->F[2]  =    63.0;
    a->F[3]  =   125.0;
    a->F[4]  =   250.0;
    a->F[5]  =   500.0;
    a->F[6]  =  1000.0;
    a->F[7]  =  2000.0;
    a->F[8]  =  4000.0;
    a->F[9]  =  8000.0;
    a->F[10] = 16000.0;
    for (i = 0; i <= a->nfreqs; i++)
        a->G[i] = (float)rxeq[i];
    a->ctfmode = 0;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    // print_impulse ("rxeq.txt", a->nc, impulse, 1, 0);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                           Partitioned Overlap-Save Equalizer:  TXA Properties                         *
*                                                                                                       *
********************************************************************************************************/

void EQP::SetEQRun (TXA& txa, int run)
{
    txa.csDSP.lock();
    txa.eqp.p->run = run;
    txa.csDSP.unlock();
}

void EQP::SetEQNC (TXA& txa, int nc)
{
    EQP *a;
    float* impulse;
    txa.csDSP.lock();
    a = txa.eqp.p;
    if (a->nc != nc)
    {
        a->nc = nc;
        impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
    txa.csDSP.unlock();
}

void EQP::SetEQMP (TXA& txa, int mp)
{
    EQP *a;
    a = txa.eqp.p;
    if (a->mp != mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

void EQP::SetEQProfile (TXA& txa, int nfreqs, const float* F, const float* G)
{
    EQP *a;
    float* impulse;
    a = txa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = nfreqs;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetEQCtfmode (TXA& txa, int mode)
{
    EQP *a;
    float* impulse;
    a = txa.eqp.p;
    a->ctfmode = mode;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetEQWintype (TXA& txa, int wintype)
{
    EQP *a;
    float* impulse;
    a = txa.eqp.p;
    a->wintype = wintype;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetGrphEQ (TXA& txa, int *txeq)
{   // three band equalizer (legacy compatibility)
    EQP *a;
    float* impulse;
    a = txa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 4;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1] =  150.0;
    a->F[2] =  400.0;
    a->F[3] = 1500.0;
    a->F[4] = 6000.0;
    a->G[0] = (float)txeq[0];
    a->G[1] = (float)txeq[1];
    a->G[2] = (float)txeq[1];
    a->G[3] = (float)txeq[2];
    a->G[4] = (float)txeq[3];
    a->ctfmode = 0;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EQP::SetGrphEQ10 (TXA& txa, int *txeq)
{   // ten band equalizer (legacy compatibility)
    EQP *a;
    float* impulse;
    int i;
    a = txa.eqp.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 10;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1]  =    32.0;
    a->F[2]  =    63.0;
    a->F[3]  =   125.0;
    a->F[4]  =   250.0;
    a->F[5]  =   500.0;
    a->F[6]  =  1000.0;
    a->F[7]  =  2000.0;
    a->F[8]  =  4000.0;
    a->F[9]  =  8000.0;
    a->F[10] = 16000.0;
    for (i = 0; i <= a->nfreqs; i++)
        a->G[i] = (float)txeq[i];
    a->ctfmode = 0;
    impulse = eq_impulse (a->nc, a->nfreqs, a->F, a->G, a->samplerate, 1.0 / (2.0 * a->size), a->ctfmode, a->wintype);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                                           Overlap-Save Equalizer                                      *
*                                                                                                       *
********************************************************************************************************/


float* EQP::eq_mults (int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype)
{
    float* impulse = eq_impulse (size + 1, nfreqs, F, G, samplerate, scale, ctfmode, wintype);
    float* mults = FIR::fftcv_mults(2 * size, impulse);
    delete[] (impulse);
    return mults;
}

void EQ::calc_eq (EQ *a)
{
    a->scale = 1.0 / (float)(2 * a->size);
    a->infilt = new float[2 * a->size * 2]; // (float *)malloc0(2 * a->size * sizeof(complex));
    a->product = new float[2 * a->size * 2]; // (float *)malloc0(2 * a->size * sizeof(complex));
    a->CFor = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->infilt, (fftwf_complex *)a->product, FFTW_FORWARD, FFTW_PATIENT);
    a->CRev = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->product, (fftwf_complex *)a->out, FFTW_BACKWARD, FFTW_PATIENT);
    a->mults = EQP::eq_mults(a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
}

void EQ::decalc_eq (EQ *a)
{
    fftwf_destroy_plan(a->CRev);
    fftwf_destroy_plan(a->CFor);
    delete[] (a->mults);
    delete[] (a->product);
    delete[] (a->infilt);
}

EQ* EQ::create_eq (int run, int size, float *in, float *out, int nfreqs, float* F, float* G, int ctfmode, int wintype, int samplerate)
{
    EQ *a = new EQ;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->nfreqs = nfreqs;
    a->F = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = new float[a->nfreqs + 1]; // (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    a->ctfmode = ctfmode;
    a->wintype = wintype;
    a->samplerate = (float)samplerate;
    calc_eq (a);
    return a;
}

void EQ::destroy_eq (EQ *a)
{
    decalc_eq (a);
    delete[] (a->G);
    delete[] (a->F);
    delete[] (a);
}

void EQ::flush_eq (EQ *a)
{
    memset (a->infilt, 0, 2 * a->size * sizeof (wcomplex));
}

void EQ::xeq (EQ *a)
{
    int i;
    float I, Q;
    if (a->run)
    {
        memcpy (&(a->infilt[2 * a->size]), a->in, a->size * sizeof (wcomplex));
        fftwf_execute (a->CFor);
        for (i = 0; i < 2 * a->size; i++)
        {
            I = a->product[2 * i + 0];
            Q = a->product[2 * i + 1];
            a->product[2 * i + 0] = I * a->mults[2 * i + 0] - Q * a->mults[2 * i + 1];
            a->product[2 * i + 1] = I * a->mults[2 * i + 1] + Q * a->mults[2 * i + 0];
        }
        fftwf_execute (a->CRev);
        memcpy (a->infilt, &(a->infilt[2 * a->size]), a->size * sizeof(wcomplex));
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void EQ::setBuffers_eq (EQ *a, float* in, float* out)
{
    decalc_eq (a);
    a->in = in;
    a->out = out;
    calc_eq (a);
}

void EQ::setSamplerate_eq (EQ *a, int rate)
{
    decalc_eq (a);
    a->samplerate = rate;
    calc_eq (a);
}

void EQ::setSize_eq (EQ *a, int size)
{
    decalc_eq (a);
    a->size = size;
    calc_eq (a);
}

/********************************************************************************************************
*                                                                                                       *
*                               Overlap-Save Equalizer:  RXA Properties                                 *
*                                                                                                       *
********************************************************************************************************/
/*  // UNCOMMENT properties when a pointer is in place in rxa
PORT
void SetRXAEQRun (int channel, int run)
{
    ch.csDSP.lock();
    rxa.eq.p->run = run;
    ch.csDSP.unlock();
}

PORT
void SetRXAEQProfile (int channel, int nfreqs, float* F, float* G)
{
    EQ a;
    ch.csDSP.lock();
    a = rxa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = nfreqs;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetRXAEQCtfmode (int channel, int mode)
{
    EQ a;
    ch.csDSP.lock();
    a = rxa.eq.p;
    a->ctfmode = mode;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetRXAEQWintype (int channel, int wintype)
{
    EQ a;
    ch.csDSP.lock();
    a = rxa.eq.p;
    a->wintype = wintype;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetRXAGrphEQ (int channel, int *rxeq)
{   // three band equalizer (legacy compatibility)
    EQ a;
    ch.csDSP.lock();
    a = rxa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 4;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1] =  150.0;
    a->F[2] =  400.0;
    a->F[3] = 1500.0;
    a->F[4] = 6000.0;
    a->G[0] = (float)rxeq[0];
    a->G[1] = (float)rxeq[1];
    a->G[2] = (float)rxeq[1];
    a->G[3] = (float)rxeq[2];
    a->G[4] = (float)rxeq[3];
    a->ctfmode = 0;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetRXAGrphEQ10 (int channel, int *rxeq)
{   // ten band equalizer (legacy compatibility)
    EQ a;
    int i;
    ch.csDSP.lock();
    a = rxa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 10;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1]  =    32.0;
    a->F[2]  =    63.0;
    a->F[3]  =   125.0;
    a->F[4]  =   250.0;
    a->F[5]  =   500.0;
    a->F[6]  =  1000.0;
    a->F[7]  =  2000.0;
    a->F[8]  =  4000.0;
    a->F[9]  =  8000.0;
    a->F[10] = 16000.0;
    for (i = 0; i <= a->nfreqs; i++)
        a->G[i] = (float)rxeq[i];
    a->ctfmode = 0;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}
*/
/********************************************************************************************************
*                                                                                                       *
*                               Overlap-Save Equalizer:  TXA Properties                                 *
*                                                                                                       *
********************************************************************************************************/
/*  // UNCOMMENT properties when a pointer is in place in rxa
PORT
void SetTXAEQRun (int channel, int run)
{
    ch.csDSP.lock();
    txa.eq.p->run = run;
    ch.csDSP.unlock();
}

PORT
void SetTXAEQProfile (int channel, int nfreqs, float* F, float* G)
{
    EQ a;
    ch.csDSP.lock();
    a = txa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = nfreqs;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    memcpy (a->F, F, (nfreqs + 1) * sizeof (float));
    memcpy (a->G, G, (nfreqs + 1) * sizeof (float));
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetTXAEQCtfmode (int channel, int mode)
{
    EQ a;
    ch.csDSP.lock();
    a = txa.eq.p;
    a->ctfmode = mode;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetTXAEQMethod (int channel, int wintype)
{
    EQ a;
    ch.csDSP.lock();
    a = txa.eq.p;
    a->wintype = wintype;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetTXAGrphEQ (int channel, int *txeq)
{   // three band equalizer (legacy compatibility)
    EQ a;
    ch.csDSP.lock();
    a = txa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 4;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1] =  150.0;
    a->F[2] =  400.0;
    a->F[3] = 1500.0;
    a->F[4] = 6000.0;
    a->G[0] = (float)txeq[0];
    a->G[1] = (float)txeq[1];
    a->G[2] = (float)txeq[1];
    a->G[3] = (float)txeq[2];
    a->G[4] = (float)txeq[3];
    a->ctfmode = 0;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}

PORT
void SetTXAGrphEQ10 (int channel, int *txeq)
{   // ten band equalizer (legacy compatibility)
    EQ a;
    int i;
    ch.csDSP.lock();
    a = txa.eq.p;
    delete[] (a->G);
    delete[] (a->F);
    a->nfreqs = 10;
    a->F = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->G = (float *) malloc0 ((a->nfreqs + 1) * sizeof (float));
    a->F[1]  =    32.0;
    a->F[2]  =    63.0;
    a->F[3]  =   125.0;
    a->F[4]  =   250.0;
    a->F[5]  =   500.0;
    a->F[6]  =  1000.0;
    a->F[7]  =  2000.0;
    a->F[8]  =  4000.0;
    a->F[9]  =  8000.0;
    a->F[10] = 16000.0;
    for (i = 0; i <= a->nfreqs; i++)
        a->G[i] = (float)txeq[i];
    a->ctfmode = 0;
    delete[] (a->mults);
    a->mults = eq_mults (a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
    ch.csDSP.unlock();
}
*/

} // namespace WDSP
