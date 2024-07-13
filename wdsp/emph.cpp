/*  emph.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2016, 2023 Warren Pratt, NR0V
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
#include "emph.hpp"
#include "fcurve.hpp"
#include "fircore.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                               Partitioned Overlap-Save FM Pre-Emphasis                                *
*                                                                                                       *
********************************************************************************************************/

EMPHP* EMPHP::create_emphp (int run, int position, int size, int nc, int mp, float* in, float* out, int rate, int ctype, float f_low, float f_high)
{
    EMPHP *a = new EMPHP;
    float* impulse;
    a->run = run;
    a->position = position;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->ctype = ctype;
    a->f_low = f_low;
    a->f_high = f_high;
    impulse = FCurve::fc_impulse (a->nc, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    a->p = FIRCORE::create_fircore (a->size, a->in, a->out, a->nc, a->mp, impulse);
    delete[] (impulse);
    return a;
}

void EMPHP::destroy_emphp (EMPHP *a)
{
    FIRCORE::destroy_fircore (a->p);
    delete (a);
}

void EMPHP::flush_emphp (EMPHP *a)
{
    FIRCORE::flush_fircore (a->p);
}

void EMPHP::xemphp (EMPHP *a, int position)
{
    if (a->run && a->position == position)
        FIRCORE::xfircore (a->p);
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void EMPHP::setBuffers_emphp (EMPHP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    FIRCORE::setBuffers_fircore (a->p, a->in, a->out);
}

void EMPHP::setSamplerate_emphp (EMPHP *a, int rate)
{
    float* impulse;
    a->rate = rate;
    impulse = FCurve::fc_impulse (a->nc, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void EMPHP::setSize_emphp (EMPHP *a, int size)
{
    float* impulse;
    a->size = size;
    FIRCORE::setSize_fircore (a->p, a->size);
    impulse = FCurve::fc_impulse (a->nc, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                       Partitioned Overlap-Save FM Pre-Emphasis:  TXA Properties                       *
*                                                                                                       *
********************************************************************************************************/

void EMPHP::SetFMEmphPosition (TXA& txa, int position)
{
    txa.csDSP.lock();
    txa.preemph.p->position = position;
    txa.csDSP.unlock();
}

void EMPHP::SetFMEmphMP (TXA& txa, int mp)
{
    EMPHP *a;
    a = txa.preemph.p;
    if (a->mp != mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

void EMPHP::SetFMEmphNC (TXA& txa, int nc)
{
    EMPHP *a;
    float* impulse;
    txa.csDSP.lock();
    a = txa.preemph.p;
    if (a->nc != nc)
    {
        a->nc = nc;
        impulse = FCurve::fc_impulse (a->nc, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
    txa.csDSP.unlock();
}

void EMPHP::SetFMPreEmphFreqs (TXA& txa, float low, float high)
{
    EMPHP *a;
    float* impulse;
    txa.csDSP.lock();
    a = txa.preemph.p;
    if (a->f_low != low || a->f_high != high)
    {
        a->f_low = low;
        a->f_high = high;
        impulse = FCurve::fc_impulse (a->nc, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
        FIRCORE::setImpulse_fircore (a->p, impulse, 1);
        delete[] (impulse);
    }
    txa.csDSP.unlock();
}

/********************************************************************************************************
*                                                                                                       *
*                                       Overlap-Save FM Pre-Emphasis                                    *
*                                                                                                       *
********************************************************************************************************/

void EMPH::calc_emph (EMPH *a)
{
    a->infilt = new float[2 * a->size * 2]; // (float *)malloc0(2 * a->size * sizeof(complex));
    a->product = new float[2 * a->size * 2]; // (float *)malloc0(2 * a->size * sizeof(complex));
    a->mults = FCurve::fc_mults(a->size, a->f_low, a->f_high, -20.0 * log10(a->f_high / a->f_low), 0.0, a->ctype, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    a->CFor = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->infilt, (fftwf_complex *)a->product, FFTW_FORWARD, FFTW_PATIENT);
    a->CRev = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->product, (fftwf_complex *)a->out, FFTW_BACKWARD, FFTW_PATIENT);
}

void EMPH::decalc_emph (EMPH *a)
{
    fftwf_destroy_plan(a->CRev);
    fftwf_destroy_plan(a->CFor);
    delete[] (a->mults);
    delete[] (a->product);
    delete[] (a->infilt);
}

EMPH* EMPH::create_emph (int run, int position, int size, float* in, float* out, int rate, int ctype, float f_low, float f_high)
{
    EMPH *a = new EMPH;
    a->run = run;
    a->position = position;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = (float)rate;
    a->ctype = ctype;
    a->f_low = f_low;
    a->f_high = f_high;
    calc_emph (a);
    return a;
}

void EMPH::destroy_emph (EMPH *a)
{
    decalc_emph (a);
    delete (a);
}

void EMPH::flush_emph (EMPH *a)
{
    memset (a->infilt, 0, 2 * a->size * sizeof (wcomplex));
}

void EMPH::xemph (EMPH *a, int position)
{
    int i;
    float I, Q;
    if (a->run && a->position == position)
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

void EMPH::setBuffers_emph (EMPH *a, float* in, float* out)
{
    decalc_emph (a);
    a->in = in;
    a->out = out;
    calc_emph (a);
}

void EMPH::setSamplerate_emph (EMPH *a, int rate)
{
    decalc_emph (a);
    a->rate = rate;
    calc_emph (a);
}

void EMPH::setSize_emph (EMPH *a, int size)
{
    decalc_emph(a);
    a->size = size;
    calc_emph(a);
}

/********************************************************************************************************
*                                                                                                       *
*                               Overlap-Save FM Pre-Emphasis:  TXA Properties                           *
*                                                                                                       *
********************************************************************************************************/
/*  // Uncomment when needed
PORT
void SetTXAFMEmphPosition (int channel, int position)
{
    ch.csDSP.lock();
    txa.preemph.p->position = position;
    ch.csDSP.unlock();
}
*/

} // namespace WDSP
