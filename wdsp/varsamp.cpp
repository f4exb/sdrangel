/*  varsamp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V
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
#include "fir.hpp"
#include "varsamp.hpp"

namespace WDSP {

void VARSAMP::calc_varsamp (VARSAMP *a)
{
    double min_rate, max_rate, norm_rate;
    double fc_norm_high, fc_norm_low;
    a->nom_ratio = (double)a->out_rate / (double)a->in_rate;
    a->cvar = a->var * a->nom_ratio;
    a->inv_cvar = 1.0 / a->cvar;
    a->old_inv_cvar = a->inv_cvar;
    a->dicvar = 0.0;
    a->delta = fabs (1.0 / a->cvar - 1.0);
    a->fc = a->fcin;
    if (a->out_rate >= a->in_rate)
    {
        min_rate = (double)a->in_rate;
        max_rate = (double)a->out_rate;
        norm_rate = min_rate;
    }
    else
    {
        min_rate = (double)a->out_rate;
        max_rate = (double)a->in_rate;
        norm_rate = max_rate;
    }
    if (a->fc == 0.0) a->fc = 0.95 * 0.45 * min_rate;
    fc_norm_high = a->fc / norm_rate;
    if (a->fc_low < 0.0)
        fc_norm_low = - fc_norm_high;
    else
        fc_norm_low = a->fc_low / norm_rate;
    a->rsize = (int)(140.0 * norm_rate / min_rate);
    a->ncoef = a->rsize + 1;
    a->ncoef += (a->R - 1) * (a->ncoef - 1);
    a->h = FIR::fir_bandpass(a->ncoef, fc_norm_low, fc_norm_high, (double)a->R, 1, 0, (double)a->R * a->gain);
    // print_impulse ("imp.txt", a->ncoef, a->h, 0, 0);
    a->ring = new double[a->rsize * 2]; // (double *)malloc0(a->rsize * sizeof(complex));
    a->idx_in = a->rsize - 1;
    a->h_offset = 0.0;
    a->hs = new double[a->rsize]; // (double *)malloc0 (a->rsize * sizeof (double));
    a->isamps = 0.0;
}

void VARSAMP::decalc_varsamp (VARSAMP *a)
{
    delete[] (a->hs);
    delete[] (a->ring);
    delete[] (a->h);
}

VARSAMP* VARSAMP::create_varsamp (
    int run,
    int size,
    double* in,
    double* out,
    int in_rate,
    int out_rate,
    double fc,
    double fc_low,
    int R,
    double gain,
    double var,
    int varmode
)
{
    VARSAMP *a = new VARSAMP;

    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->in_rate = in_rate;
    a->out_rate = out_rate;
    a->fcin = fc;
    a->fc_low = fc_low;
    a->R = R;
    a->gain = gain;
    a->var = var;
    a->varmode = varmode;
    calc_varsamp (a);
    return a;
}

void VARSAMP::destroy_varsamp (VARSAMP *a)
{
    decalc_varsamp (a);
    delete (a);
}

void VARSAMP::flush_varsamp (VARSAMP *a)
{
    memset (a->ring, 0, a->rsize * sizeof (wcomplex));
    a->idx_in = a->rsize - 1;
    a->h_offset = 0.0;
    a->isamps = 0.0;
}

void VARSAMP::hshift (VARSAMP *a)
{
    int i, j, k;
    int hidx;
    double frac, pos;
    pos = (double)a->R * a->h_offset;
    hidx = (int)(pos);
    frac = pos - (double)hidx;
    for (i = a->rsize - 1, j = hidx, k = hidx + 1; i >= 0; i--, j += a->R, k += a->R)
        a->hs[i] = a->h[j] + frac * (a->h[k] - a->h[j]);
}

int VARSAMP::xvarsamp (VARSAMP *a, double var)
{
    int outsamps = 0;
    uint64_t* picvar;
    uint64_t N;
    a->var = var;
    a->old_inv_cvar = a->inv_cvar;
    a->cvar = a->var * a->nom_ratio;
    a->inv_cvar = 1.0 / a->cvar;
    if (a->varmode)
    {
        a->dicvar = (a->inv_cvar - a->old_inv_cvar) / (double)a->size;
        a->inv_cvar = a->old_inv_cvar;
    }
    else            a->dicvar = 0.0;
    if (a->run)
    {
        int i, j;
        int idx_out;
        double I, Q;
        for (i = 0; i < a->size; i++)
        {
            a->ring[2 * a->idx_in + 0] = a->in[2 * i + 0];
            a->ring[2 * a->idx_in + 1] = a->in[2 * i + 1];
            a->inv_cvar += a->dicvar;
            picvar = (uint64_t*)(&a->inv_cvar);
            N = *picvar & 0xffffffffffff0000;
            a->inv_cvar = static_cast<double>(N);
            a->delta = 1.0 - a->inv_cvar;
            while (a->isamps < 1.0)
            {
                I = 0.0;
                Q = 0.0;
                hshift (a);
                a->h_offset += a->delta;
                while (a->h_offset >= 1.0) a->h_offset -= 1.0;
                while (a->h_offset <  0.0) a->h_offset += 1.0;
                for (j = 0; j < a->rsize; j++)
                {
                    if ((idx_out = a->idx_in + j) >= a->rsize) idx_out -= a->rsize;
                    I += a->hs[j] * a->ring[2 * idx_out + 0];
                    Q += a->hs[j] * a->ring[2 * idx_out + 1];
                }
                a->out[2 * outsamps + 0] = I;
                a->out[2 * outsamps + 1] = Q;
                outsamps++;
                a->isamps += a->inv_cvar;
            }
            a->isamps -= 1.0;
            if (--a->idx_in < 0) a->idx_in = a->rsize - 1;
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    return outsamps;
}

void VARSAMP::setBuffers_varsamp (VARSAMP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void VARSAMP::setSize_varsamp (VARSAMP *a, int size)
{
    a->size = size;
    flush_varsamp (a);
}

void VARSAMP::setInRate_varsamp (VARSAMP *a, int rate)
{
    decalc_varsamp (a);
    a->in_rate = rate;
    calc_varsamp (a);
}

void VARSAMP::setOutRate_varsamp (VARSAMP *a, int rate)
{
    decalc_varsamp (a);
    a->out_rate = rate;
    calc_varsamp (a);
}

void VARSAMP::setFCLow_varsamp (VARSAMP *a, double fc_low)
{
    if (fc_low != a->fc_low)
    {
        decalc_varsamp (a);
        a->fc_low = fc_low;
        calc_varsamp (a);
    }
}

void VARSAMP::setBandwidth_varsamp (VARSAMP *a, double fc_low, double fc_high)
{
    if (fc_low != a->fc_low || fc_high != a->fcin)
    {
        decalc_varsamp (a);
        a->fc_low = fc_low;
        a->fcin = fc_high;
        calc_varsamp (a);
    }
}

// exported calls

void* VARSAMP::create_varsampV (int in_rate, int out_rate, int R)
{
    return (void *)create_varsamp (1, 0, 0, 0, in_rate, out_rate, 0.0, -1.0, R, 1.0, 1.0, 1);
}

void VARSAMP::xvarsampV (double* input, double* output, int numsamps, double var, int* outsamps, void* ptr)
{
    VARSAMP *a = (VARSAMP*) ptr;
    a->in = input;
    a->out = output;
    a->size = numsamps;
    *outsamps = xvarsamp(a, var);
}

void VARSAMP::destroy_varsampV (void* ptr)
{
    destroy_varsamp ( (VARSAMP*) ptr );
}

} // namespace WDSP
