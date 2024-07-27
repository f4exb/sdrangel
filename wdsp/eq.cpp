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
#include "eqp.hpp"
#include "fircore.hpp"
#include "fir.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Overlap-Save Equalizer                                      *
*                                                                                                       *
********************************************************************************************************/


float* EQ::eq_mults (int size, int nfreqs, float* F, float* G, float samplerate, float scale, int ctfmode, int wintype)
{
    float* impulse = EQP::eq_impulse (size + 1, nfreqs, F, G, samplerate, scale, ctfmode, wintype);
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
    a->mults = EQ::eq_mults(a->size, a->nfreqs, a->F, a->G, a->samplerate, a->scale, a->ctfmode, a->wintype);
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
    std::fill(a->infilt, a->infilt + 2 * a->size * 2, 0);
}

void EQ::xeq (EQ *a)
{
    int i;
    float I, Q;
    if (a->run)
    {
        std::copy(a->in, a->in + a->size * 2, &(a->infilt[2 * a->size]));
        fftwf_execute (a->CFor);
        for (i = 0; i < 2 * a->size; i++)
        {
            I = a->product[2 * i + 0];
            Q = a->product[2 * i + 1];
            a->product[2 * i + 0] = I * a->mults[2 * i + 0] - Q * a->mults[2 * i + 1];
            a->product[2 * i + 1] = I * a->mults[2 * i + 1] + Q * a->mults[2 * i + 0];
        }
        fftwf_execute (a->CRev);
        std::copy(&(a->infilt[2 * a->size]), &(a->infilt[2 * a->size]) + a->size * 2, a->infilt);
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
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
    rxa.eq->run = run;
    ch.csDSP.unlock();
}

PORT
void SetRXAEQProfile (int channel, int nfreqs, float* F, float* G)
{
    EQ a;
    ch.csDSP.lock();
    a = rxa.eq;
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
    a = rxa.eq;
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
    a = rxa.eq;
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
    a = rxa.eq;
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
    a = rxa.eq;
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
    txa.eq->run = run;
    ch.csDSP.unlock();
}

PORT
void SetTXAEQProfile (int channel, int nfreqs, float* F, float* G)
{
    EQ a;
    ch.csDSP.lock();
    a = txa.eq;
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
    a = txa.eq;
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
    a = txa.eq;
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
    a = txa.eq;
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
    a = txa.eq;
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
