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
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Bi-Quad Notch                                               *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::calc_snotch (SNOTCH *a)
{
    double fn, qk, qr, csn;
    fn = a->f / (float)a->rate;
    csn = cos (TWOPI * fn);
    qr = 1.0 - 3.0 * a->bw;
    qk = (1.0 - 2.0 * qr * csn + qr * qr) / (2.0 * (1.0 - csn));
    a->a0 = + qk;
    a->a1 = - 2.0 * qk * csn;
    a->a2 = + qk;
    a->b1 = + 2.0 * qr * csn;
    a->b2 = - qr * qr;
    flush_snotch (a);
}

SNOTCH* SNOTCH::create_snotch (int run, int size, float* in, float* out, int rate, double f, double bw)
{
    SNOTCH *a = new SNOTCH;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f = f;
    a->bw = bw;
    calc_snotch (a);
    return a;
}

void SNOTCH::destroy_snotch (SNOTCH *a)
{
    delete (a);
}

void SNOTCH::flush_snotch (SNOTCH *a)
{
    a->x1 = a->x2 = a->y1 = a->y2 = 0.0;
}

void SNOTCH::xsnotch (SNOTCH *a)
{
    if (a->run)
    {
        int i;
        for (i = 0; i < a->size; i++)
        {
            a->x0 = a->in[2 * i + 0];
            a->out[2 * i + 0] = a->a0 * a->x0 + a->a1 * a->x1 + a->a2 * a->x2 + a->b1 * a->y1 + a->b2 * a->y2;
            a->y2 = a->y1;
            a->y1 = a->out[2 * i + 0];
            a->x2 = a->x1;
            a->x1 = a->x0;
        }
    }
    else if (a->out != a->in)
    {
        std::copy( a->in,  a->in + a->size * 2, a->out);
    }
}

void SNOTCH::setBuffers_snotch (SNOTCH *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void SNOTCH::setSamplerate_snotch (SNOTCH *a, int rate)
{
    a->rate = rate;
    calc_snotch (a);
}

void SNOTCH::setSize_snotch (SNOTCH *a, int size)
{
    a->size = size;
    flush_snotch (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::SetSNCTCSSFreq (SNOTCH *a, double freq)
{
    a->f = freq;
    calc_snotch (a);
}

void SNOTCH::SetSNCTCSSRun (SNOTCH *a, int run)
{
    a->run = run;
}

} // namespace WDSP
