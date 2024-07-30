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
#include "dsphp.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                    Double Single-Pole High-Pass                                       *
*                                                                                                       *
********************************************************************************************************/

void DSPHP::calc_dsphp(DSPHP *a)
{
    double g;
    a->x0 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->x1 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->y0 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->y1 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    g = exp(-TWOPI * a->fc / a->rate);
    a->b0 = +0.5 * (1.0 + g);
    a->b1 = -0.5 * (1.0 + g);
    a->a1 = -g;
}

DSPHP* DSPHP::create_dsphp(int run, int size, float* in, float* out, double rate, double fc, int nstages)
{
    DSPHP *a = new DSPHP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_dsphp(a);
    return a;
}

void DSPHP::decalc_dsphp(DSPHP *a)
{
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x1);
    delete[](a->x0);
}

void DSPHP::destroy_dsphp(DSPHP *a)
{
    decalc_dsphp(a);
    delete(a);
}

void DSPHP::flush_dsphp(DSPHP *a)
{
    memset(a->x0, 0, a->nstages * sizeof(float));
    memset(a->x1, 0, a->nstages * sizeof(float));
    memset(a->y0, 0, a->nstages * sizeof(float));
    memset(a->y1, 0, a->nstages * sizeof(float));
}

void DSPHP::xdsphp(DSPHP *a)
{
    if (a->run)
    {
        int i, n;

        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->in[i];

            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0)
                    a->x0[n] = a->y0[n - 1];

                a->y0[n] = a->b0 * a->x0[n]
                    + a->b1 * a->x1[n]
                    - a->a1 * a->y1[n];
                a->y1[n] = a->y0[n];
                a->x1[n] = a->x0[n];
            }

            a->out[i] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
    {
        memcpy(a->out, a->in, a->size * sizeof(float));
    }
}

void DSPHP::setBuffers_dsphp(DSPHP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void DSPHP::setSamplerate_dsphp(DSPHP *a, int rate)
{
    decalc_dsphp(a);
    a->rate = rate;
    calc_dsphp(a);
}

void DSPHP::setSize_dsphp(DSPHP *a, int size)
{
    a->size = size;
    flush_dsphp(a);
}

} // namespace WDSP
