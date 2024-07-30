/*  dbqbp.c

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
#include "dbqbp.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                     Double Bi-Quad Band-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

void DBQBP::calc_dbqbp(DBQBP *a)
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
    flush_dbqbp(a);
}

DBQBP* DBQBP::create_dbqbp(int run, int size, float* in, float* out, double rate, double f_low, double f_high, double gain, int nstages)
{
    DBQBP *a = new DBQBP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f_low = f_low;
    a->f_high = f_high;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->x1 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->x2 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->y0 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->y1 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    a->y2 = new double[a->nstages]; // (float*)malloc0(a->nstages * sizeof(float));
    calc_dbqbp(a);
    return a;
}

void DBQBP::destroy_dbqbp(DBQBP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void DBQBP::flush_dbqbp(DBQBP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[i] = a->x2[i] = a->y1[i] = a->y2[i] = 0.0;
    }
}

void DBQBP::xdbqbp(DBQBP *a)
{
    if (a->run)
    {
        int i, n;

        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->gain * a->in[i];

            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0)
                    a->x0[n] = a->y0[n - 1];

                a->y0[n] = a->a0 * a->x0[n]
                    + a->a1 * a->x1[n]
                    + a->a2 * a->x2[n]
                    + a->b1 * a->y1[n]
                    + a->b2 * a->y2[n];
                a->y2[n] = a->y1[n];
                a->y1[n] = a->y0[n];
                a->x2[n] = a->x1[n];
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

void DBQBP::setBuffers_dbqbp(DBQBP *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void DBQBP::setSamplerate_dbqbp(DBQBP *a, int rate)
{
    a->rate = rate;
    calc_dbqbp(a);
}

void DBQBP::setSize_dbqbp(DBQBP *a, int size)
{
    a->size = size;
    flush_dbqbp(a);
}

} // namespace WDSP
