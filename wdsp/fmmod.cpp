/*  fmmod.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2023 Warren Pratt, NR0V
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
#include "fircore.hpp"
#include "fir.hpp"
#include "fmmod.hpp"
#include "TXA.hpp"

namespace WDSP {

void FMMOD::calc_fmmod (FMMOD *a)
{
    // ctcss gen
    a->tscale = 1.0 / (1.0 + a->ctcss_level);
    a->tphase = 0.0;
    a->tdelta = TWOPI * a->ctcss_freq / a->samplerate;
    // mod
    a->sphase = 0.0;
    a->sdelta = TWOPI * a->deviation / a->samplerate;
    // bandpass
    a->bp_fc = a->deviation + a->f_high;
}

FMMOD* FMMOD::create_fmmod (
    int run,
    int size,
    float* in,
    float* out,
    int rate,
    float dev,
    float f_low,
    float f_high,
    int ctcss_run,
    float ctcss_level,
    float ctcss_freq,
    int bp_run,
    int nc,
    int mp
)
{
    FMMOD *a = new FMMOD;
    float* impulse;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->samplerate = (float)rate;
    a->deviation = dev;
    a->f_low = f_low;
    a->f_high = f_high;
    a->ctcss_run = ctcss_run;
    a->ctcss_level = ctcss_level;
    a->ctcss_freq = ctcss_freq;
    a->bp_run = bp_run;
    a->nc = nc;
    a->mp = mp;
    calc_fmmod (a);
    impulse = FIR::fir_bandpass(a->nc, -a->bp_fc, +a->bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
    a->p = FIRCORE::create_fircore (a->size, a->out, a->out, a->nc, a->mp, impulse);
    delete[] (impulse);
    return a;
}

void FMMOD::destroy_fmmod (FMMOD *a)
{
    FIRCORE::destroy_fircore (a->p);
    delete (a);
}

void FMMOD::flush_fmmod (FMMOD *a)
{
    a->tphase = 0.0;
    a->sphase = 0.0;
}

void FMMOD::xfmmod (FMMOD *a)
{
    int i;
    float dp, magdp, peak;
    if (a->run)
    {
        peak = 0.0;
        for (i = 0; i < a->size; i++)
        {
            if (a->ctcss_run)
            {
                a->tphase += a->tdelta;
                if (a->tphase >= TWOPI) a->tphase -= TWOPI;
                a->out[2 * i + 0] = a->tscale * (a->in[2 * i + 0] + a->ctcss_level * cos (a->tphase));
            }
            dp = a->out[2 * i + 0] * a->sdelta;
            a->sphase += dp;
            if (a->sphase >= TWOPI) a->sphase -= TWOPI;
            if (a->sphase <   0.0 ) a->sphase += TWOPI;
            a->out[2 * i + 0] = 0.7071 * cos (a->sphase);
            a->out[2 * i + 1] = 0.7071 * sin (a->sphase);
            if ((magdp = dp) < 0.0) magdp = - magdp;
            if (magdp > peak) peak = magdp;
        }
        //print_deviation ("peakdev.txt", peak, a->samplerate);
        if (a->bp_run)
            FIRCORE::xfircore (a->p);
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void FMMOD::setBuffers_fmmod (FMMOD *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    calc_fmmod (a);
    FIRCORE::setBuffers_fircore (a->p, a->out, a->out);
}

void FMMOD::setSamplerate_fmmod (FMMOD *a, int rate)
{
    float* impulse;
    a->samplerate = rate;
    calc_fmmod (a);
    impulse = FIR::fir_bandpass(a->nc, -a->bp_fc, +a->bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

void FMMOD::setSize_fmmod (FMMOD *a, int size)
{
    float* impulse;
    a->size = size;
    calc_fmmod (a);
    FIRCORE::setSize_fircore (a->p, a->size);
    impulse = FIR::fir_bandpass(a->nc, -a->bp_fc, +a->bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
    FIRCORE::setImpulse_fircore (a->p, impulse, 1);
    delete[] (impulse);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMMOD::SetFMDeviation (TXA& txa, float deviation)
{
    FMMOD *a = txa.fmmod;
    float bp_fc = a->f_high + deviation;
    float* impulse = FIR::fir_bandpass (a->nc, -bp_fc, +bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
    FIRCORE::setImpulse_fircore (a->p, impulse, 0);
    delete[] (impulse);
    a->deviation = deviation;
    // mod
    a->sphase = 0.0;
    a->sdelta = TWOPI * a->deviation / a->samplerate;
    // bandpass
    a->bp_fc = bp_fc;
    FIRCORE::setUpdate_fircore (a->p);
}

void FMMOD::SetCTCSSFreq (TXA& txa, float freq)
{
    FMMOD *a;
    a = txa.fmmod;
    a->ctcss_freq = freq;
    a->tphase = 0.0;
    a->tdelta = TWOPI * a->ctcss_freq / a->samplerate;
}

void FMMOD::SetCTCSSRun (TXA& txa, int run)
{
    txa.fmmod->ctcss_run = run;
}

void FMMOD::SetFMNC (TXA& txa, int nc)
{
    FMMOD *a;
    float* impulse;
    a = txa.fmmod;

    if (a->nc != nc)
    {
        a->nc = nc;
        impulse = FIR::fir_bandpass (a->nc, -a->bp_fc, +a->bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
        FIRCORE::setNc_fircore (a->p, a->nc, impulse);
        delete[] (impulse);
    }
}

void FMMOD::SetFMMP (TXA& txa, int mp)
{
    FMMOD *a;
    a = txa.fmmod;
    if (a->mp != mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->p, a->mp);
    }
}

void FMMOD::SetFMAFFreqs (TXA& txa, float low, float high)
{
    FMMOD *a;
    float* impulse;
    a = txa.fmmod;

    if (a->f_low != low || a->f_high != high)
    {
        a->f_low = low;
        a->f_high = high;
        a->bp_fc = a->deviation + a->f_high;
        impulse = FIR::fir_bandpass (a->nc, -a->bp_fc, +a->bp_fc, a->samplerate, 0, 1, 1.0 / (2 * a->size));
        FIRCORE::setImpulse_fircore (a->p, impulse, 1);
        delete[] (impulse);
    }
}

} // namespace WDSP
