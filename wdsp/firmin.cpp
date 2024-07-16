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

void FIRMIN::calc_firmin (FIRMIN *a)
{
    a->h = FIR::fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain);
    a->rsize = a->nc;
    a->mask = a->rsize - 1;
    a->ring = new float[a->rsize * 2]; // (float *) malloc0 (a->rsize * sizeof (complex));
    a->idx = 0;
}

FIRMIN* FIRMIN::create_firmin (int run, int position, int size, float* in, float* out,
    int nc, float f_low, float f_high, int samplerate, int wintype, float gain)
{
    FIRMIN *a = new FIRMIN;
    a->run = run;
    a->position = position;
    a->size = size;
    a->in = in;
    a->out = out;
    a->nc = nc;
    a->f_low = f_low;
    a->f_high = f_high;
    a->samplerate = samplerate;
    a->wintype = wintype;
    a->gain = gain;
    calc_firmin (a);
    return a;
}

void FIRMIN::destroy_firmin (FIRMIN *a)
{
    delete[] (a->ring);
    delete[] (a->h);
    delete (a);
}

void FIRMIN::flush_firmin (FIRMIN *a)
{
    std::fill(a->ring, a->ring + a->rsize * 2, 0);
    a->idx = 0;
}

void FIRMIN::xfirmin (FIRMIN *a, int pos)
{
    if (a->run && a->position == pos)
    {
        int i, j, k;
        for (i = 0; i < a->size; i++)
        {
            a->ring[2 * a->idx + 0] = a->in[2 * i + 0];
            a->ring[2 * a->idx + 1] = a->in[2 * i + 1];
            a->out[2 * i + 0] = 0.0;
            a->out[2 * i + 1] = 0.0;
            k = a->idx;
            for (j = 0; j < a->nc; j++)
            {
                a->out[2 * i + 0] += a->h[2 * j + 0] * a->ring[2 * k + 0] - a->h[2 * j + 1] * a->ring[2 * k + 1];
                a->out[2 * i + 1] += a->h[2 * j + 0] * a->ring[2 * k + 1] + a->h[2 * j + 1] * a->ring[2 * k + 0];
                k = (k + a->mask) & a->mask;
            }
            a->idx = (a->idx + 1) & a->mask;
        }
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
}

void FIRMIN::setBuffers_firmin (FIRMIN *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void FIRMIN::setSamplerate_firmin (FIRMIN *a, int rate)
{
    a->samplerate = (float)rate;
    calc_firmin (a);
}

void FIRMIN::setSize_firmin (FIRMIN *a, int size)
{
    a->size = size;
}

void FIRMIN::setFreqs_firmin (FIRMIN *a, float f_low, float f_high)
{
    a->f_low = f_low;
    a->f_high = f_high;
    calc_firmin (a);
}

} // namespace WDSP
