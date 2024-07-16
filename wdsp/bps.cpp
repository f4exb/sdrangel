/*  bandpass.c

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
#include "bps.hpp"
#include "fir.hpp"
#include "bandpass.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       Overlap-Save Bandpass                                           *
*                                                                                                       *
********************************************************************************************************/

void BPS::calc_bps (BPS *a)
{
    float* impulse;
    a->infilt = new float[2 * a->size * 2];
    a->product = new float[2 * a->size * 2];
    impulse = FIR::fir_bandpass(a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
    a->mults = FIR::fftcv_mults(2 * a->size, impulse);
    a->CFor = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->infilt, (fftwf_complex *)a->product, FFTW_FORWARD, FFTW_PATIENT);
    a->CRev = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->product, (fftwf_complex *)a->out, FFTW_BACKWARD, FFTW_PATIENT);
    delete[](impulse);
}

void BPS::decalc_bps (BPS *a)
{
    fftwf_destroy_plan(a->CRev);
    fftwf_destroy_plan(a->CFor);
    delete[] (a->mults);
    delete[] (a->product);
    delete[] (a->infilt);
}

BPS* BPS::create_bps (int run, int position, int size, float* in, float* out,
    float f_low, float f_high, int samplerate, int wintype, float gain)
{
    BPS *a = new BPS;
    a->run = run;
    a->position = position;
    a->size = size;
    a->samplerate = (float)samplerate;
    a->wintype = wintype;
    a->gain = gain;
    a->in = in;
    a->out = out;
    a->f_low = f_low;
    a->f_high = f_high;
    calc_bps (a);
    return a;
}

void BPS::destroy_bps (BPS *a)
{
    decalc_bps (a);
    delete a;
}

void BPS::flush_bps (BPS *a)
{
    std::fill(a->infilt, a->infilt + 2 * a->size * 2, 0);
}

void BPS::xbps (BPS *a, int pos)
{
    int i;
    float I, Q;
    if (a->run && pos == a->position)
    {
        std::copy(a->in, a->in + a->size * 2, &(a->infilt[2 * a->size]));
        fftwf_execute (a->CFor);
        for (i = 0; i < 2 * a->size; i++)
        {
            I = a->gain * a->product[2 * i + 0];
            Q = a->gain * a->product[2 * i + 1];
            a->product[2 * i + 0] = I * a->mults[2 * i + 0] - Q * a->mults[2 * i + 1];
            a->product[2 * i + 1] = I * a->mults[2 * i + 1] + Q * a->mults[2 * i + 0];
        }
        fftwf_execute (a->CRev);
        std::copy(&(a->infilt[2 * a->size]), &(a->infilt[2 * a->size]) + a->size * 2, a->infilt);
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void BPS::setBuffers_bps (BPS *a, float* in, float* out)
{
    decalc_bps (a);
    a->in = in;
    a->out = out;
    calc_bps (a);
}

void BPS::setSamplerate_bps (BPS *a, int rate)
{
    decalc_bps (a);
    a->samplerate = rate;
    calc_bps (a);
}

void BPS::setSize_bps (BPS *a, int size)
{
    decalc_bps (a);
    a->size = size;
    calc_bps (a);
}

void BPS::setFreqs_bps (BPS *a, float f_low, float f_high)
{
    decalc_bps (a);
    a->f_low = f_low;
    a->f_high = f_high;
    calc_bps (a);
}

/********************************************************************************************************
*                                                                                                       *
*                               Overlap-Save Bandpass:  RXA Properties                                  *
*                                                                                                       *
********************************************************************************************************/

void BPS::SetBPSRun (RXA& rxa, int run)
{
    rxa.bp1.p->run = run;
}

void BPS::SetBPSFreqs (RXA& rxa, float f_low, float f_high)
{
    float* impulse;
    BPS *a1;
    a1 = rxa.bps1.p;

    if ((f_low != a1->f_low) || (f_high != a1->f_high))
    {
        a1->f_low = f_low;
        a1->f_high = f_high;
        delete[] (a1->mults);
        impulse = FIR::fir_bandpass(a1->size + 1, f_low, f_high, a1->samplerate, a1->wintype, 1, 1.0 / (float)(2 * a1->size));
        a1->mults = FIR::fftcv_mults (2 * a1->size, impulse);
        delete[] (impulse);
    }
}

void BPS::SetBPSWindow (RXA& rxa, int wintype)
{
    float* impulse;
    BPS *a1;
    a1 = rxa.bps1.p;

    if ((a1->wintype != wintype))
    {
        a1->wintype = wintype;
        delete[] (a1->mults);
        impulse = FIR::fir_bandpass(a1->size + 1, a1->f_low, a1->f_high, a1->samplerate, a1->wintype, 1, 1.0 / (float)(2 * a1->size));
        a1->mults = FIR::fftcv_mults (2 * a1->size, impulse);
        delete[] (impulse);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/
// UNCOMMENT properties when pointers in place in txa
void BPS::SetBPSRun (TXA& txa, int run)
{
    txa.bp1.p->run = run;
}

void BPS::SetBPSFreqs (TXA& txa, float f_low, float f_high)
{
    float* impulse;
    BPS *a;
    a = txa.bps0.p;

    if ((f_low != a->f_low) || (f_high != a->f_high))
    {
        a->f_low = f_low;
        a->f_high = f_high;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass(a->size + 1, f_low, f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }

    a = txa.bps1.p;

    if ((f_low != a->f_low) || (f_high != a->f_high))
    {
        a->f_low = f_low;
        a->f_high = f_high;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass(a->size + 1, f_low, f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }

    a = txa.bps2.p;

    if ((f_low != a->f_low) || (f_high != a->f_high))
    {
        a->f_low = f_low;
        a->f_high = f_high;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass(a->size + 1, f_low, f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }
}

void BPS::SetBPSWindow (TXA& txa, int wintype)
{
    float* impulse;
    BPS *a;
    a = txa.bps0.p;

    if (a->wintype != wintype)
    {
        a->wintype = wintype;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass(a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }

    a = txa.bps1.p;

    if (a->wintype != wintype)
    {
        a->wintype = wintype;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass(a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }

    a = txa.bps2.p;

    if (a->wintype != wintype)
    {
        a->wintype = wintype;
        delete[] (a->mults);
        impulse = FIR::fir_bandpass (a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        a->mults = FIR::fftcv_mults (2 * a->size, impulse);
        delete[] (impulse);
    }
}

} // namespace WDSP
