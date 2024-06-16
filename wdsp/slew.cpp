/*  slew.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2023 Warren Pratt, NR0V
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
#include "slew.hpp"
#include "TXA.hpp"

namespace WDSP {

enum _USLEW
{
    BEGIN,
    WAIT,
    UP,
    ON
};

void USLEW::calc_uslew (USLEW *a)
{
    int i;
    double delta, theta;
    a->runmode = 0;
    a->state = BEGIN;
    a->count = 0;
    a->ndelup = (int)(a->tdelay * a->rate);
    a->ntup = (int)(a->tupslew * a->rate);
    a->cup = new double[a->ntup + 1]; // (double *) malloc0 ((a->ntup + 1) * sizeof (double));
    delta = PI / (double)a->ntup;
    theta = 0.0;
    for (i = 0; i <= a->ntup; i++)
    {
        a->cup[i] = 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
    *a->ch_upslew &= ~((long)1); // InterlockedBitTestAndReset (a->ch_upslew, 0);
}

void USLEW::decalc_uslew (USLEW *a)
{
    delete[] (a->cup);
}

USLEW* USLEW::create_uslew (TXA *txa, std::atomic<long> *ch_upslew, int size, double* in, double* out, double rate, double tdelay, double tupslew)
{
    USLEW *a = new USLEW;
    a->txa = txa;
    a->ch_upslew = ch_upslew;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->tdelay = tdelay;
    a->tupslew = tupslew;
    calc_uslew (a);
    return a;
}

void USLEW::destroy_uslew (USLEW *a)
{
    decalc_uslew (a);
    delete (a);
}

void USLEW::flush_uslew (USLEW *a)
{
    a->state = BEGIN;
    a->runmode = 0;
    *a->ch_upslew &= ~1L; //InterlockedBitTestAndReset (a->ch_upslew, 0);
}

void USLEW::xuslew (USLEW *a)
{
    if (!a->runmode && TXA::UslewCheck (*a->txa))
        a->runmode = 1;

    long upslew = *a->ch_upslew;
    *a->ch_upslew = 1L;
    if (a->runmode && upslew) //_InterlockedAnd (a->ch_upslew, 1))
    {
        int i;
        double I, Q;
        for (i = 0; i < a->size; i++)
        {
            I = a->in[2 * i + 0];
            Q = a->in[2 * i + 1];
            switch (a->state)
            {
            case BEGIN:
                a->out[2 * i + 0] = 0.0;
                a->out[2 * i + 1] = 0.0;
                if ((I != 0.0) || (Q != 0.0))
                {
                    if (a->ndelup > 0)
                    {
                        a->state = WAIT;
                        a->count = a->ndelup;
                    }
                    else if (a->ntup > 0)
                    {
                        a->state = UP;
                        a->count = a->ntup;
                    }
                    else
                        a->state = ON;
                }
                break;
            case WAIT:
                a->out[2 * i + 0] = 0.0;
                a->out[2 * i + 1] = 0.0;
                if (a->count-- == 0)
                {
                    if (a->ntup > 0)
                    {
                        a->state = UP;
                        a->count = a->ntup;
                    }
                    else
                        a->state = ON;
                }
                break;
            case UP:
                a->out[2 * i + 0] = I * a->cup[a->ntup - a->count];
                a->out[2 * i + 1] = Q * a->cup[a->ntup - a->count];
                if (a->count-- == 0)
                    a->state = ON;
                break;
            case ON:
                a->out[2 * i + 0] = I;
                a->out[2 * i + 1] = Q;
                *a->ch_upslew &= ~((long)1); // InterlockedBitTestAndReset (a->ch_upslew, 0);
                a->runmode = 0;
                break;
            }
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->size * sizeof (dcomplex));
}

void USLEW::setBuffers_uslew (USLEW *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void USLEW::setSamplerate_uslew (USLEW *a, int rate)
{
    decalc_uslew (a);
    a->rate = rate;
    calc_uslew (a);
}

void USLEW::setSize_uslew (USLEW *a, int size)
{
    a->size = size;
    flush_uslew (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                            TXA Properties                                             *
*                                                                                                       *
********************************************************************************************************/

void USLEW::SetuSlewTime (TXA& txa, double time)
{
    // NOTE:  'time' is in seconds
    txa.csDSP.lock();
    USLEW *a = txa.uslew.p;
    decalc_uslew (a);
    a->tupslew = time;
    calc_uslew (a);
    txa.csDSP.unlock();
}

} // namespace WDSP
