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

#include "sphp.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                    Complex Single-Pole High-Pass                                      *
*                                                                                                       *
********************************************************************************************************/

void SPHP::calc_sphp(SPHP *a)
{
    double g;
    a->x0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (float*)malloc0(a->nstages * sizeof(complex));
    g = exp(-TWOPI * a->fc / a->rate);
    a->b0 = +0.5 * (1.0 + g);
    a->b1 = -0.5 * (1.0 + g);
    a->a1 = -g;
}

SPHP* SPHP::create_sphp(int run, int size, float* in, float* out, double rate, double fc, int nstages)
{
    SPHP *a = new SPHP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_sphp(a);
    return a;
}

void SPHP::decalc_sphp(SPHP *a)
{
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x1);
    delete[](a->x0);
}

void SPHP::destroy_sphp(SPHP *a)
{
    decalc_sphp(a);
    delete(a);
}

void SPHP::flush_sphp(SPHP *a)
{
    std::fill(a->x0, a->x0 + a->nstages * 2, 0);
    std::fill(a->x1, a->x0 + a->nstages * 2, 0);
    std::fill(a->y0, a->x0 + a->nstages * 2, 0);
    std::fill(a->y1, a->x0 + a->nstages * 2, 0);
}

void SPHP::xsphp(SPHP *a)
{
    if (a->run)
    {
        int i, j, n;
        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->in[2 * i + j];

                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0)
                        a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];

                    a->y0[2 * n + j] = a->b0 * a->x0[2 * n + j]
                        + a->b1 * a->x1[2 * n + j]
                        - a->a1 * a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
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

void SPHP::setBuffers_sphp(SPHP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void SPHP::setSamplerate_sphp(SPHP *a, int rate)
{
    decalc_sphp(a);
    a->rate = rate;
    calc_sphp(a);
}

void SPHP::setSize_sphp(SPHP *a, int size)
{
    a->size = size;
    flush_sphp(a);
}

} // namespace WDSP
