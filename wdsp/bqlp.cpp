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
#include "bqlp.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Low-Pass                                            *
*                                                                                                       *
********************************************************************************************************/

void BQLP::calc_bqlp(BQLP *a)
{
    double w0, cs, c, den;
    w0 = TWOPI * a->fc / (double)a->rate;
    cs = cos(w0);
    c = sin(w0) / (2.0 * a->Q);
    den = 1.0 + c;
    a->a0 = 0.5 * (1.0 - cs) / den;
    a->a1 = (1.0 - cs) / den;
    a->a2 = 0.5 * (1.0 - cs) / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_bqlp(a);
}

BQLP* BQLP::create_bqlp(int run, int size, float* in, float* out, double rate, double fc, double Q, double gain, int nstages)
{
    BQLP *a = new BQLP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->Q = Q;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->x2 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y2 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    calc_bqlp(a);
    return a;
}

void BQLP::destroy_bqlp(BQLP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void BQLP::flush_bqlp(BQLP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[2 * i + 0] = a->x2[2 * i + 0] = a->y1[2 * i + 0] = a->y2[2 * i + 0] = 0.0;
        a->x1[2 * i + 1] = a->x2[2 * i + 1] = a->y1[2 * i + 1] = a->y2[2 * i + 1] = 0.0;
    }
}

void BQLP::xbqlp(BQLP *a)
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

void BQLP::setBuffers_bqlp(BQLP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void BQLP::setSamplerate_bqlp(BQLP *a, int rate)
{
    a->rate = rate;
    calc_bqlp(a);
}

void BQLP::setSize_bqlp(BQLP *a, int size)
{
    a->size = size;
    flush_bqlp(a);
}


} // namespace WDSP
