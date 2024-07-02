/*  anf.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2012, 2013 Warren Pratt, NR0V
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
#include "amd.hpp"
#include "snba.hpp"
#include "emnr.hpp"
#include "anr.hpp"
#include "anf.hpp"
#include "bandpass.hpp"
#include "RXA.hpp"

namespace WDSP {

ANF* ANF::create_anf(
    int run,
    int position,
    int buff_size,
    float *in_buff,
    float *out_buff,
    int dline_size,
    int n_taps,
    int delay,
    float two_mu,
    float gamma,
    float lidx,
    float lidx_min,
    float lidx_max,
    float ngamma,
    float den_mult,
    float lincr,
    float ldecr
)
{
    ANF *a = new ANF;
    a->run = run;
    a->position = position;
    a->buff_size = buff_size;
    a->in_buff = in_buff;
    a->out_buff = out_buff;
    a->dline_size = dline_size;
    a->mask = dline_size - 1;
    a->n_taps = n_taps;
    a->delay = delay;
    a->two_mu = two_mu;
    a->gamma = gamma;
    a->in_idx = 0;
    a->lidx = lidx;
    a->lidx_min = lidx_min;
    a->lidx_max = lidx_max;
    a->ngamma = ngamma;
    a->den_mult = den_mult;
    a->lincr = lincr;
    a->ldecr = ldecr;

    memset (a->d, 0, sizeof(float) * ANF_DLINE_SIZE);
    memset (a->w, 0, sizeof(float) * ANF_DLINE_SIZE);

    return a;
}

void ANF::destroy_anf (ANF *a)
{
    delete a;
}

void ANF::xanf(ANF *a, int position)
{
    int i, j, idx;
    float c0, c1;
    float y, error, sigma, inv_sigp;
    float nel, nev;
    if (a->run && (a->position == position))
    {
        for (i = 0; i < a->buff_size; i++)
        {
            a->d[a->in_idx] = a->in_buff[2 * i + 0];

            y = 0;
            sigma = 0;

            for (j = 0; j < a->n_taps; j++)
            {
                idx = (a->in_idx + j + a->delay) & a->mask;
                y += a->w[j] * a->d[idx];
                sigma += a->d[idx] * a->d[idx];
            }
            inv_sigp = 1.0 / (sigma + 1e-10);
            error = a->d[a->in_idx] - y;

            a->out_buff[2 * i + 0] = error;
            a->out_buff[2 * i + 1] = 0.0;

            if((nel = error * (1.0 - a->two_mu * sigma * inv_sigp)) < 0.0) nel = -nel;
            if((nev = a->d[a->in_idx] - (1.0 - a->two_mu * a->ngamma) * y - a->two_mu * error * sigma * inv_sigp) < 0.0) nev = -nev;
            if (nev < nel)
            {
                if ((a->lidx += a->lincr) > a->lidx_max) a->lidx = a->lidx_max;
            }
            else
            {
                if ((a->lidx -= a->ldecr) < a->lidx_min) a->lidx = a->lidx_min;
            }
            a->ngamma = a->gamma * (a->lidx * a->lidx) * (a->lidx * a->lidx) * a->den_mult;

            c0 = 1.0 - a->two_mu * a->ngamma;
            c1 = a->two_mu * error * inv_sigp;

            for (j = 0; j < a->n_taps; j++)
            {
                idx = (a->in_idx + j + a->delay) & a->mask;
                a->w[j] = c0 * a->w[j] + c1 * a->d[idx];
            }
            a->in_idx = (a->in_idx + a->mask) & a->mask;
        }
    }
    else if (a->in_buff != a->out_buff)
        memcpy (a->out_buff, a->in_buff, a->buff_size * sizeof (wcomplex));
}

void ANF::flush_anf (ANF *a)
{
    memset (a->d, 0, sizeof(float) * ANF_DLINE_SIZE);
    memset (a->w, 0, sizeof(float) * ANF_DLINE_SIZE);
    a->in_idx = 0;
}

void ANF::setBuffers_anf (ANF *a, float* in, float* out)
{
    a->in_buff = in;
    a->out_buff = out;
}

void ANF::setSamplerate_anf (ANF *a, int)
{
    flush_anf (a);
}

void ANF::setSize_anf (ANF *a, int size)
{
    a->buff_size = size;
    flush_anf (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void ANF::SetANFRun (RXA& rxa, int run)
{
    ANF *a = rxa.anf.p;
    if (a->run != run)
    {
        RXA::bp1Check (rxa, rxa.amd.p->run, rxa.snba.p->run,
            rxa.emnr.p->run, run, rxa.anr.p->run);
        rxa.csDSP.lock();
        a->run = run;
        RXA::bp1Set (rxa);
        flush_anf (a);
        rxa.csDSP.unlock();
    }
}


void ANF::SetANFVals (RXA& rxa, int taps, int delay, float gain, float leakage)
{
    rxa.csDSP.lock();
    rxa.anf.p->n_taps = taps;
    rxa.anf.p->delay = delay;
    rxa.anf.p->two_mu = gain;          //try two_mu = 1e-4
    rxa.anf.p->gamma = leakage;        //try gamma = 0.10
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

void ANF::SetANFTaps (RXA& rxa, int taps)
{
    rxa.csDSP.lock();
    rxa.anf.p->n_taps = taps;
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

void ANF::SetANFDelay (RXA& rxa, int delay)
{
    rxa.csDSP.lock();
    rxa.anf.p->delay = delay;
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

void ANF::SetANFGain (RXA& rxa, float gain)
{
    rxa.csDSP.lock();
    rxa.anf.p->two_mu = gain;
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

void ANF::SetANFLeakage (RXA& rxa, float leakage)
{
    rxa.csDSP.lock();
    rxa.anf.p->gamma = leakage;
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

void ANF::SetANFPosition (RXA& rxa, int position)
{
    rxa.csDSP.lock();
    rxa.anf.p->position = position;
    rxa.bp1.p->position = position;
    flush_anf (rxa.anf.p);
    rxa.csDSP.unlock();
}

} // namespace WDSP
