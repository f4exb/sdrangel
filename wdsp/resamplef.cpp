/*  resample.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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
#include "fir.hpp"
#include "resamplef.hpp"

namespace WDSP {

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR NON-COMPLEX FLOATS                                    *
*                                                                                               *
************************************************************************************************/

RESAMPLEF* RESAMPLEF::create_resampleF (
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _in_rate,
    int _out_rate
)
{
    auto *a = new RESAMPLEF;
    int x;
    int y;
    int z;
    int i;
    int min_rate;
    float full_rate;
    float fc;
    float fc_norm;
    std::vector<float> impulse;
    a->run = _run;
    a->size = _size;
    a->in = _in;
    a->out = _out;
    x = _in_rate;
    y = _out_rate;

    while (y != 0)
    {
        z = y;
        y = x % y;
        x = z;
    }

    a->L = _out_rate / x;
    a->M = _in_rate / x;

    a->L = a->L <= 0 ? 1 : a->L;
    a->M = a->M <= 0 ? 1 : a->M;

    if (_in_rate < _out_rate)
        min_rate = _in_rate;
    else
        min_rate = _out_rate;

    fc = 0.45f * (float)min_rate;
    full_rate = (float)(_in_rate * a->L);
    fc_norm = fc / full_rate;
    a->ncoef = (int)(60.0 / fc_norm);
    a->ncoef = (a->ncoef / a->L + 1) * a->L;
    a->cpp = a->ncoef / a->L;
    a->h = new float[a->ncoef];
    FIR::fir_bandpass (impulse, a->ncoef, -fc_norm, +fc_norm, 1.0, 1, 0, (float)a->L);
    i = 0;

    for (int j = 0; j < a->L; j ++)
    {
        for (int k = 0; k < a->ncoef; k += a->L)
            a->h[i++] = impulse[j + k];
    }

    a->ringsize = a->cpp;
    a->ring = new float[a->ringsize];
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;

    return a;
}

void RESAMPLEF::destroy_resampleF (RESAMPLEF *a)
{
    delete[] a->ring;
    delete[] a->h;
    delete a;
}

void RESAMPLEF::flush_resampleF (RESAMPLEF *a)
{
    memset (a->ring, 0, a->ringsize * sizeof (float));
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;
}

int RESAMPLEF::xresampleF (RESAMPLEF *a)
{
    int outsamps = 0;

    if (a->run)
    {
        int n;
        int idx_out;
        float I;

        for (int i = 0; i < a->size; i++)
        {
            a->ring[a->idx_in] = a->in[i];

            while (a->phnum < a->L)
            {
                I = 0.0;
                n = a->cpp * a->phnum;

                for (int j = 0; j < a->cpp; j++)
                {
                    if ((idx_out = a->idx_in + j) >= a->ringsize)
                        idx_out -= a->ringsize;

                    I += a->h[n + j] * a->ring[idx_out];
                }

                a->out[outsamps] = I;

                outsamps++;
                a->phnum += a->M;
            }

            a->phnum -= a->L;

            if (--a->idx_in < 0)
                a->idx_in = a->ringsize - 1;
        }
    }
    else if (a->in != a->out)
    {
        memcpy (a->out, a->in, a->size * sizeof (float));
    }

    return outsamps;
}

// Exported calls


void* RESAMPLEF::create_resampleFV (int _in_rate, int _out_rate)
{
    return (void *) create_resampleF (1, 0, nullptr, nullptr, _in_rate, _out_rate);
}


void RESAMPLEF::xresampleFV (float* _input, float* _output, int _numsamps, int* _outsamps, void* _ptr)
{
    auto *a = (RESAMPLEF*) _ptr;
    a->in = _input;
    a->out = _output;
    a->size = _numsamps;
    *_outsamps = xresampleF(a);
}


void RESAMPLEF::destroy_resampleFV (void* _ptr)
{
    destroy_resampleF ( (RESAMPLEF*) _ptr );
}

} // namespace WDSP
