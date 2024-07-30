/*  phrot.c

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
#include "phrot.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Phase Rotator                                               *
*                                                                                                       *
********************************************************************************************************/

void PHROT::calc_phrot (PHROT *a)
{
    double g;
    a->x0 = new double[a->nstages]; // (float *) malloc0 (a->nstages * sizeof (float));
    a->x1 = new double[a->nstages]; // (float *) malloc0 (a->nstages * sizeof (float));
    a->y0 = new double[a->nstages]; // (float *) malloc0 (a->nstages * sizeof (float));
    a->y1 = new double[a->nstages]; // (float *) malloc0 (a->nstages * sizeof (float));
    g = tan (PI * a->fc / (float)a->rate);
    a->b0 = (g - 1.0) / (g + 1.0);
    a->b1 = 1.0;
    a->a1 = a->b0;
}

void PHROT::decalc_phrot (PHROT *a)
{
    delete[] (a->y1);
    delete[] (a->y0);
    delete[] (a->x1);
    delete[] (a->x0);
}

PHROT* PHROT::create_phrot (int run, int size, float* in, float* out, int rate, double fc, int nstages)
{
    PHROT *a = new PHROT;
    a->reverse = 0;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_phrot (a);
    return a;
}

void PHROT::destroy_phrot (PHROT *a)
{
    decalc_phrot (a);
    delete (a);
}

void PHROT::flush_phrot (PHROT *a)
{
    memset (a->x0, 0, a->nstages * sizeof (double));
    memset (a->x1, 0, a->nstages * sizeof (double));
    memset (a->y0, 0, a->nstages * sizeof (double));
    memset (a->y1, 0, a->nstages * sizeof (double));
}

void PHROT::xphrot (PHROT *a)
{
    if (a->reverse)
    {
        for (int i = 0; i < a->size; i++)
            a->in[2 * i + 0] = -a->in[2 * i + 0];
    }

    if (a->run)
    {
        int i, n;

        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->in[2 * i + 0];

            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0) a->x0[n] = a->y0[n - 1];
                a->y0[n] = a->b0 * a->x0[n]
                    + a->b1 * a->x1[n]
                    - a->a1 * a->y1[n];
                a->y1[n] = a->y0[n];
                a->x1[n] = a->x0[n];
            }

            a->out[2 * i + 0] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
    {
        std::copy( a->in,  a->in + a->size * 2, a->out);
    }
}

void PHROT::setBuffers_phrot (PHROT *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void PHROT::setSamplerate_phrot (PHROT *a, int rate)
{
    decalc_phrot (a);
    a->rate = rate;
    calc_phrot (a);
}

void PHROT::setSize_phrot (PHROT *a, int size)
{
    a->size = size;
    flush_phrot (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void PHROT::SetPHROTRun (TXA& txa, int run)
{
    PHROT *a = txa.phrot;
    a->run = run;

    if (a->run)
        flush_phrot (a);
}

void PHROT::SetPHROTCorner (TXA& txa, double corner)
{
    PHROT *a = txa.phrot;
    decalc_phrot (a);
    a->fc = corner;
    calc_phrot (a);
}

void PHROT::SetPHROTNstages (TXA& txa, int nstages)
{
    PHROT *a = txa.phrot;
    decalc_phrot (a);
    a->nstages = nstages;
    calc_phrot (a);
}

void PHROT::SetPHROTReverse (TXA& txa, int reverse)
{
    PHROT *a = txa.phrot;
    a->reverse = reverse;
}

} // namespace WDSP
