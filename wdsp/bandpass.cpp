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
#include "bandpass.hpp"
#include "fir.hpp"
#include "fircore.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Bandpass                                   *
*                                                                                                       *
********************************************************************************************************/

BANDPASS* BANDPASS::create_bandpass (
    int run,
    int position,
    int size,
    int nc,
    int mp,
    float* in,
    float* out,
    double f_low,
    double f_high,
    int samplerate,
    int wintype,
    double gain
)
{
    // NOTE:  'nc' must be >= 'size'
    BANDPASS *a = new BANDPASS;
    a->run = run;
    a->position = position;
    a->size = size;
    a->nc = nc;
    a->mp = mp;
    a->in = in;
    a->out = out;
    a->f_low = f_low;
    a->f_high = f_high;
    a->samplerate = samplerate;
    a->wintype = wintype;
    a->gain = gain;
    float* impulse = FIR::fir_bandpass (
        a->nc,
        a->f_low,
        a->f_high,
        a->samplerate,
        a->wintype,
        1,
        a->gain / (double)(2 * a->size)
    );
    a->p = FIRCORE::create_fircore (a->size, a->in, a->out, a->nc, a->mp, impulse);
    delete[] impulse;
    return a;
}

void BANDPASS::destroy_bandpass (BANDPASS *a)
{
    FIRCORE::destroy_fircore (a->p);
    delete a;
}

void BANDPASS::flush_bandpass (BANDPASS *a)
{
    FIRCORE::flush_fircore (a->p);
}

void BANDPASS::xbandpass (BANDPASS *a, int pos)
{
    if (a->run && a->position == pos)
        FIRCORE::xfircore (a->p);
    else if (a->out != a->in)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void BANDPASS::setBuffers_bandpass (BANDPASS *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    FIRCORE::setBuffers_fircore (a->p, a->in, a->out);
}

void BANDPASS::setSamplerate_bandpass (BANDPASS *a, int rate)
{
    a->samplerate = rate;
    float* impulse = FIR::fir_bandpass (
        a->nc,
        a->f_low,
        a->f_high,
        a->samplerate,
        a->wintype,
        1,
        a->gain / (float)(2 * a->size)
    );
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] impulse;
}

void BANDPASS::setSize_bandpass (BANDPASS *a, int size)
{
    // NOTE:  'size' must be <= 'nc'
    a->size = size;
    FIRCORE::setSize_fircore (a->p, a->size);
    // recalc impulse because scale factor is a function of size
    float* impulse = FIR::fir_bandpass (
        a->nc,
        a->f_low,
        a->f_high,
        a->samplerate,
        a->wintype,
        1,
        a->gain / (float)(2 * a->size)
    );
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void BANDPASS::setGain_bandpass (BANDPASS *a, double gain, int update)
{
    a->gain = gain;
    float* impulse = FIR::fir_bandpass (
        a->nc,
        a->f_low,
        a->f_high,
        a->samplerate,
        a->wintype,
        1,
        a->gain / (double)(2 * a->size)
    );
    FIRCORE::setImpulse_fircore (a->p, impulse, update);
    delete[] (impulse);
}

void BANDPASS::CalcBandpassFilter (BANDPASS *a, double f_low, double f_high, double gain)
{
    if ((a->f_low != f_low) || (a->f_high != f_high) || (a->gain != gain))
    {
        a->f_low = f_low;
        a->f_high = f_high;
        a->gain = gain;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setImpulse_fircore (a->p, impulse, 1);
        delete[] (impulse);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void BANDPASS::SetBandpassFreqs (RXA& rxa, double f_low, double f_high)
{
    BANDPASS *a = rxa.bp1;

    if ((f_low != a->f_low) || (f_high != a->f_high))
    {
        float* impulse = FIR::fir_bandpass (
            a->nc,
            f_low,
            f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );

        FIRCORE::setImpulse_fircore (a->p, impulse, 0);
        delete[] (impulse);
        a->f_low = f_low;
        a->f_high = f_high;
        FIRCORE::setUpdate_fircore (a->p);
    }
}

void BANDPASS::SetBandpassNC (RXA& rxa, int nc)
{
    // NOTE:  'nc' must be >= 'size'
    BANDPASS *a;
    a = rxa.bp1;

    if (nc != a->nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
}

void BANDPASS::SetBandpassMP (RXA& rxa, int mp)
{
    BANDPASS *a;
    a = rxa.bp1;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

//PORT
//void SetTXABandpassFreqs (int channel, float f_low, float f_high)
//{
//  float* impulse;
//  BANDPASS a;
//  a = txa.bp0;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->p, impulse, 1);
//      delete[] (impulse);
//  }
//  a = txa.bp1;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->p, impulse, 1);
//      delete[] (impulse);
//  }
//  a = txa.bp2;
//  if ((f_low != a->f_low) || (f_high != a->f_high))
//  {
//      a->f_low = f_low;
//      a->f_high = f_high;
//      impulse = fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain / (float)(2 * a->size));
//      setImpulse_fircore (a->p, impulse, 1);
//      delete[] (impulse);
//  }
//}

void BANDPASS::SetBandpassNC (TXA& txa, int nc)
{
    // NOTE:  'nc' must be >= 'size'
    BANDPASS *a;
    a = txa.bp0;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }

    a = txa.bp1;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }

    a = txa.bp2;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
}

void BANDPASS::SetBandpassMP (TXA& txa, int mp)
{
    BANDPASS *a;
    a = txa.bp0;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }

    a = txa.bp1;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }

    a = txa.bp2;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

} // namespace WDSP
