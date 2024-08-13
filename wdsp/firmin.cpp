/*  firmin.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2016 Warren Pratt, NR0V
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
#include "fir.hpp"
#include "firmin.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Time-Domain FIR                                             *
*                                                                                                       *
********************************************************************************************************/

void FIRMIN::calc()
{
    FIR::fir_bandpass (h, nc, f_low, f_high, samplerate, wintype, 1, gain);
    rsize = nc;
    mask = rsize - 1;
    ring.resize(rsize * 2);
    idx = 0;
}

FIRMIN::FIRMIN(
    int _run,
    int _position,
    int _size,
    float* _in,
    float* _out,
    int _nc,
    float _f_low,
    float _f_high,
    int _samplerate,
    int _wintype,
    float _gain
)
{
    run = _run;
    position = _position;
    size = _size;
    in = _in;
    out = _out;
    nc = _nc;
    f_low = _f_low;
    f_high = _f_high;
    samplerate = (float) _samplerate;
    wintype = _wintype;
    gain = _gain;
    calc();
}

void FIRMIN::flush()
{
    std::fill(ring.begin(), ring.end(), 0);
    idx = 0;
}

void FIRMIN::execute(int _pos)
{
    if (run && position == _pos)
    {
        int k;
        for (int i = 0; i < size; i++)
        {
            ring[2 * idx + 0] = in[2 * i + 0];
            ring[2 * idx + 1] = in[2 * i + 1];
            out[2 * i + 0] = 0.0;
            out[2 * i + 1] = 0.0;
            k = idx;
            for (int j = 0; j < nc; j++)
            {
                out[2 * i + 0] += h[2 * j + 0] * ring[2 * k + 0] - h[2 * j + 1] * ring[2 * k + 1];
                out[2 * i + 1] += h[2 * j + 0] * ring[2 * k + 1] + h[2 * j + 1] * ring[2 * k + 0];
                k = (k + mask) & mask;
            }
            idx = (idx + 1) & mask;
        }
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void FIRMIN::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void FIRMIN::setSamplerate(int _rate)
{
    samplerate = (float) _rate;
    calc();
}

void FIRMIN::setSize(int _size)
{
    size = _size;
}

void FIRMIN::setFreqs(float _f_low, float _f_high)
{
    f_low = _f_low;
    f_high = _f_high;
    calc();
}

} // namespace WDSP
