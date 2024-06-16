/*  delay.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2019 Warren Pratt, NR0V
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
#include "delay.hpp"
#include "fir.hpp"

namespace WDSP {

DELAY* create_delay (int run, int size, double* in, double* out, int rate, double tdelta, double tdelay)
{
    DELAY *a = new DELAY;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->tdelta = tdelta;
    a->tdelay = tdelay;
    a->L = (int)(0.5 + 1.0 / (a->tdelta * (double)a->rate));
    a->adelta = 1.0 / (a->rate * a->L);
    a->ft = 0.45 / (double)a->L;
    a->ncoef = (int)(60.0 / a->ft);
    a->ncoef = (a->ncoef / a->L + 1) * a->L;
    a->cpp = a->ncoef / a->L;
    a->phnum = (int)(0.5 + a->tdelay / a->adelta);
    a->snum = a->phnum / a->L;
    a->phnum %= a->L;
    a->idx_in = 0;
    a->adelay = a->adelta * (a->snum * a->L + a->phnum);
    a->h = FIR::fir_bandpass (a->ncoef,-a->ft, +a->ft, 1.0, 1, 0, (double)a->L);
    a->rsize = a->cpp + (WSDEL - 1);
    a->ring = new double[a->rsize * 2]; // (double *) malloc0 (a->rsize * sizeof (complex));
    return a;
}

void DELAY::destroy_delay (DELAY *a)
{
    delete[] (a->ring);
    delete[] (a->h);
    delete (a);
}

void DELAY::flush_delay (DELAY *a)
{
    memset (a->ring, 0, a->cpp * sizeof (dcomplex));
    a->idx_in = 0;
}

void DELAY::xdelay (DELAY *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, j, k, idx, n;
        double Itmp, Qtmp;
        for (i = 0; i < a->size; i++)
        {
            a->ring[2 * a->idx_in + 0] = a->in[2 * i + 0];
            a->ring[2 * a->idx_in + 1] = a->in[2 * i + 1];
            Itmp = 0.0;
            Qtmp = 0.0;
            if ((n = a->idx_in + a->snum) >= a->rsize) n -= a->rsize;
            for (j = 0, k = a->L - 1 - a->phnum; j < a->cpp; j++, k+= a->L)
            {
                if ((idx = n + j) >= a->rsize) idx -= a->rsize;
                Itmp += a->ring[2 * idx + 0] * a->h[k];
                Qtmp += a->ring[2 * idx + 1] * a->h[k];
            }
            a->out[2 * i + 0] = Itmp;
            a->out[2 * i + 1] = Qtmp;
            if (--a->idx_in < 0) a->idx_in = a->rsize - 1;
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->size * sizeof (dcomplex));
    a->cs_update.unlock();
}

/********************************************************************************************************
*                                                                                                       *
*                                             Properties                                                *
*                                                                                                       *
********************************************************************************************************/

void DELAY::SetDelayRun (DELAY *a, int run)
{
    a->cs_update.lock();
    a->run = run;
    a->cs_update.unlock();
}

double DELAY::SetDelayValue (DELAY *a, double tdelay)
{
    double adelay;
    a->cs_update.lock();
    a->tdelay = tdelay;
    a->phnum = (int)(0.5 + a->tdelay / a->adelta);
    a->snum = a->phnum / a->L;
    a->phnum %= a->L;
    a->adelay = a->adelta * (a->snum * a->L + a->phnum);
    adelay = a->adelay;
    a->cs_update.unlock();
    return adelay;
}

void DELAY::SetDelayBuffs (DELAY *a, int size, double* in, double* out)
{
    a->cs_update.lock();
    a->size = size;
    a->in = in;
    a->out = out;
    a->cs_update.unlock();
}

} // namespace WDSP

