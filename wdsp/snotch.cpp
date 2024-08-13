/*  iir.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014, 2022, 2023 Warren Pratt, NR0V
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
#include "snotch.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Bi-Quad Notch                                               *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::calc()
{
    double fn;
    double qk;
    double qr;
    double csn;
    fn = f / rate;
    csn = cos (TWOPI * fn);
    qr = 1.0 - 3.0 * bw;
    qk = (1.0 - 2.0 * qr * csn + qr * qr) / (2.0 * (1.0 - csn));
    a0 = + qk;
    a1 = - 2.0 * qk * csn;
    a2 = + qk;
    b1 = + 2.0 * qr * csn;
    b2 = - qr * qr;
    flush();
}

SNOTCH::SNOTCH(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _f,
    double _bw
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    f(_f),
    bw(_bw)
{
    calc();
}

void SNOTCH::flush()
{
    x1 = x2 = y1 = y2 = 0.0;
}

void SNOTCH::execute()
{
    if (run)
    {
        for (int i = 0; i < size; i++)
        {
            x0 = in[2 * i + 0];
            out[2 * i + 0] = (float) (a0 * x0 + a1 * x1 + a2 * x2 + b1 * y1 + b2 * y2);
            y2 = y1;
            y1 = out[2 * i + 0];
            x2 = x1;
            x1 = x0;
        }
    }
    else if (out != in)
    {
        std::copy( in,  in + size * 2, out);
    }
}

void SNOTCH::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void SNOTCH::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void SNOTCH::setSize(int _size)
{
    size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::setFreq(double _freq)
{
    f = _freq;
    calc();
}

void SNOTCH::setRun(int _run)
{
    run = _run;
}

} // namespace WDSP
