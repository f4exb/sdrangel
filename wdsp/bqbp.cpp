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
#include "bqbp.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Band-Pass                                           *
*                                                                                                       *
********************************************************************************************************/

void BQBP::calc_bqbp(BQBP *a)
{
    double f0, w0, bw, q, sn, cs, c, den;
    bw = a->f_high - a->f_low;
    f0 = (a->f_high + a->f_low) / 2.0;
    q = f0 / bw;
    w0 = TWOPI * f0 / a->rate;
    sn = sin(w0);
    cs = cos(w0);
    c = sn / (2.0 * q);
    den = 1.0 + c;
    a->a0 = +c / den;
    a->a1 = 0.0;
    a->a2 = -c / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_bqbp(a);
}

BQBP* BQBP::create_bqbp(int run, int size, float* in, float* out, double rate, double f_low, double f_high, double gain, int nstages)
{
    BQBP *a = new BQBP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f_low = f_low;
    a->f_high = f_high;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->x2 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y2 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    calc_bqbp(a);
    return a;
}

void BQBP::destroy_bqbp(BQBP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void BQBP::flush_bqbp(BQBP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[2 * i + 0] = a->x2[2 * i + 0] = a->y1[2 * i + 0] = a->y2[2 * i + 0] = 0.0;
        a->x1[2 * i + 1] = a->x2[2 * i + 1] = a->y1[2 * i + 1] = a->y2[2 * i + 1] = 0.0;
    }
}

void BQBP::xbqbp(BQBP *a)
{
    if (a->run)
    {
        int i, j, n;

        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->gain * a->in[2 * i + j];

                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0)
                        a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];

                    a->y0[2 * n + j] = a->a0 * a->x0[2 * n + j]
                        + a->a1 * a->x1[2 * n + j]
                        + a->a2 * a->x2[2 * n + j]
                        + a->b1 * a->y1[2 * n + j]
                        + a->b2 * a->y2[2 * n + j];
                    a->y2[2 * n + j] = a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
                    a->x2[2 * n + j] = a->x1[2 * n + j];
                    a->x1[2 * n + j] = a->x0[2 * n + j];
                }

                a->out[2 * i + j] = a->y0[2 * (a->nstages - 1) + j];
            }
        }
    }
    else if (a->out != a->in)
    {
        std::copy(a->in, a->in + a->size * 2, a->out);
    }
}

void BQBP::setBuffers_bqbp(BQBP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void BQBP::setSamplerate_bqbp(BQBP *a, int rate)
{
    a->rate = rate;
    calc_bqbp(a);
}

void BQBP::setSize_bqbp(BQBP *a, int size)
{
    a->size = size;
    flush_bqbp(a);
}

} // namespace WDSP
