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

#include <vector>

#include "comm.hpp"
#include "fircore.hpp"
#include "fir.hpp"
#include "fmmod.hpp"
#include "TXA.hpp"

namespace WDSP {

void FMMOD::calc()
{
    // ctcss gen
    tscale = 1.0 / (1.0 + ctcss_level);
    tphase = 0.0;
    tdelta = TWOPI * ctcss_freq / samplerate;
    // mod
    sphase = 0.0;
    sdelta = TWOPI * deviation / samplerate;
    // bandpass
    bp_fc = deviation + f_high;
}

FMMOD::FMMOD(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _dev,
    double _f_low,
    double _f_high,
    int _ctcss_run,
    double _ctcss_level,
    double _ctcss_freq,
    int _bp_run,
    int _nc,
    int _mp
)
{
    std::vector<float> impulse;
    run = _run;
    size = _size;
    in = _in;
    out = _out;
    samplerate = (float) _rate;
    deviation = _dev;
    f_low = _f_low;
    f_high = _f_high;
    ctcss_run = _ctcss_run;
    ctcss_level = _ctcss_level;
    ctcss_freq = _ctcss_freq;
    bp_run = _bp_run;
    nc = _nc;
    mp = _mp;
    calc();
    FIR::fir_bandpass(impulse, nc, -bp_fc, +bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
    p = new FIRCORE(size, out, out, mp, impulse);
}

FMMOD::~FMMOD()
{
    delete p;
}

void FMMOD::flush()
{
    tphase = 0.0;
    sphase = 0.0;
}

void FMMOD::execute()
{
    double dp;
    double magdp;
    double peak;
    if (run)
    {
        peak = 0.0;
        for (int i = 0; i < size; i++)
        {
            if (ctcss_run)
            {
                tphase += tdelta;
                if (tphase >= TWOPI) tphase -= TWOPI;
                out[2 * i + 0] = (float) (tscale * (in[2 * i + 0] + ctcss_level * cos (tphase)));
            }
            dp = out[2 * i + 0] * sdelta;
            sphase += dp;
            if (sphase >= TWOPI) sphase -= TWOPI;
            if (sphase <   0.0 ) sphase += TWOPI;
            out[2 * i + 0] = (float) (0.7071 * cos (sphase));
            out[2 * i + 1] = (float) (0.7071 * sin (sphase));
            if ((magdp = dp) < 0.0) magdp = - magdp;
            if (magdp > peak) peak = magdp;
        }

        if (bp_run)
            p->execute();
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void FMMOD::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
    calc();
    p->setBuffers(out, out);
}

void FMMOD::setSamplerate(int _rate)
{
    std::vector<float> impulse;
    samplerate = _rate;
    calc();
    FIR::fir_bandpass(impulse, nc, -bp_fc, +bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
    p->setImpulse(impulse, 1);
}

void FMMOD::setSize(int _size)
{
    std::vector<float> impulse;
    size = _size;
    calc();
    p->setSize(size);
    FIR::fir_bandpass(impulse, nc, -bp_fc, +bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
    p->setImpulse(impulse, 1);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMMOD::setDeviation(float _deviation)
{
    double _bp_fc = f_high + _deviation;
    std::vector<float> impulse;
    FIR::fir_bandpass (impulse, nc, -_bp_fc, +_bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
    p->setImpulse(impulse, 0);
    deviation = _deviation;
    // mod
    sphase = 0.0;
    sdelta = TWOPI * deviation / samplerate;
    // bandpass
    bp_fc = _bp_fc;
    p->setUpdate();
}

void FMMOD::setCTCSSFreq (float _freq)
{
    ctcss_freq = _freq;
    tphase = 0.0;
    tdelta = TWOPI * ctcss_freq / samplerate;
}

void FMMOD::setCTCSSRun (int _run)
{
    ctcss_run = _run;
}

void FMMOD::setNC(int _nc)
{
    std::vector<float> impulse;

    if (nc != _nc)
    {
        nc = _nc;
        FIR::fir_bandpass (impulse, nc, -bp_fc, +bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
        p->setNc(impulse);
    }
}

void FMMOD::setMP(int _mp)
{
    if (mp != _mp)
    {
        mp = _mp;
        p->setMp(mp);
    }
}

void FMMOD::setAFFreqs(float _low, float _high)
{
    std::vector<float> impulse;

    if (f_low != _low || f_high != _high)
    {
        f_low = _low;
        f_high = _high;
        bp_fc = deviation + f_high;
        FIR::fir_bandpass (impulse, nc, -bp_fc, +bp_fc, samplerate, 0, 1, 1.0 / (2 * size));
        p->setImpulse(impulse, 1);
    }
}

} // namespace WDSP
