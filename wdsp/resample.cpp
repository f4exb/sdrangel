/*  resample.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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
#include "resample.hpp"

namespace WDSP {

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR COMPLEX DOUBLE-PRECISION                              *
*                                                                                               *
************************************************************************************************/

void RESAMPLE::calc_resample (RESAMPLE *a)
{
    int x, y, z;
    int i, j, k;
    int min_rate;
    float full_rate;
    float fc_norm_high, fc_norm_low;
    float* impulse;
    a->fc = a->fcin;
    a->ncoef = a->ncoefin;
    x = a->in_rate;
    y = a->out_rate;
    while (y != 0)
    {
        z = y;
        y = x % y;
        x = z;
    }
    a->L = a->out_rate / x;
    a->M = a->in_rate / x;
    if (a->in_rate < a->out_rate) min_rate = a->in_rate;
    else min_rate = a->out_rate;
    if (a->fc == 0.0) a->fc = 0.45 * (float)min_rate;
    full_rate = (float)(a->in_rate * a->L);
    fc_norm_high = a->fc / full_rate;
    if (a->fc_low < 0.0)
        fc_norm_low = - fc_norm_high;
    else
        fc_norm_low = a->fc_low / full_rate;
    if (a->ncoef == 0) a->ncoef = (int)(140.0 * full_rate / min_rate);
    a->ncoef = (a->ncoef / a->L + 1) * a->L;
    a->cpp = a->ncoef / a->L;
    a->h = new double[a->ncoef]; // (float *)malloc0(a->ncoef * sizeof(float));
    impulse = FIR::fir_bandpass(a->ncoef, fc_norm_low, fc_norm_high, 1.0, 1, 0, a->gain * (float)a->L);
    i = 0;
    for (j = 0; j < a->L; j++)
        for (k = 0; k < a->ncoef; k += a->L)
            a->h[i++] = impulse[j + k];
    a->ringsize = a->cpp;
    a->ring = new double[a->ringsize]; // (float *)malloc0(a->ringsize * sizeof(complex));
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;
    delete[] (impulse);
}

void RESAMPLE::decalc_resample (RESAMPLE *a)
{
    delete[] (a->ring);
    delete[] (a->h);
}

RESAMPLE* RESAMPLE::create_resample (
    int run,
    int size,
    float* in,
    float* out,
    int in_rate,
    int out_rate,
    double fc,
    int ncoef,
    double gain
)
{
    RESAMPLE *a = new RESAMPLE;

    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->in_rate = in_rate;
    a->out_rate = out_rate;
    a->fcin = fc;
    a->fc_low = -1.0;       // could add to create_resample() parameters
    a->ncoefin = ncoef;
    a->gain = gain;
    calc_resample (a);
    return a;
}


void RESAMPLE::destroy_resample (RESAMPLE *a)
{
    decalc_resample (a);
    delete (a);
}


void RESAMPLE::flush_resample (RESAMPLE *a)
{
    std::fill(a->ring, a->ring + 2 * a->ringsize, 0);
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;
}


int RESAMPLE::xresample (RESAMPLE *a)
{
    int outsamps = 0;
    if (a->run)
    {
        int i, j, n;
        int idx_out;
        double I, Q;

        for (i = 0; i < a->size; i++)
        {
            a->ring[2 * a->idx_in + 0] = a->in[2 * i + 0];
            a->ring[2 * a->idx_in + 1] = a->in[2 * i + 1];
            while (a->phnum < a->L)
            {
                I = 0.0;
                Q = 0.0;
                n = a->cpp * a->phnum;
                for (j = 0; j < a->cpp; j++)
                {
                    if ((idx_out = a->idx_in + j) >= a->ringsize) idx_out -= a->ringsize;
                    I += a->h[n + j] * a->ring[2 * idx_out + 0];
                    Q += a->h[n + j] * a->ring[2 * idx_out + 1];
                }
                a->out[2 * outsamps + 0] = I;
                a->out[2 * outsamps + 1] = Q;
                outsamps++;
                a->phnum += a->M;
            }
            a->phnum -= a->L;
            if (--a->idx_in < 0) a->idx_in = a->ringsize - 1;
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    return outsamps;
}

void RESAMPLE::setBuffers_resample(RESAMPLE *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void RESAMPLE::setSize_resample(RESAMPLE *a, int size)
{
    a->size = size;
    flush_resample (a);
}

void RESAMPLE::setInRate_resample(RESAMPLE *a, int rate)
{
    decalc_resample (a);
    a->in_rate = rate;
    calc_resample (a);
}

void RESAMPLE::setOutRate_resample(RESAMPLE *a, int rate)
{
    decalc_resample (a);
    a->out_rate = rate;
    calc_resample (a);
}

void RESAMPLE::setFCLow_resample (RESAMPLE *a, float fc_low)
{
    if (fc_low != a->fc_low)
    {
        decalc_resample (a);
        a->fc_low = fc_low;
        calc_resample (a);
    }
}

void RESAMPLE::setBandwidth_resample (RESAMPLE *a, float fc_low, float fc_high)
{
    if (fc_low != a->fc_low || fc_high != a->fcin)
    {
        decalc_resample (a);
        a->fc_low = fc_low;
        a->fcin = fc_high;
        calc_resample (a);
    }
}

// exported calls


void* RESAMPLE::create_resampleV (int in_rate, int out_rate)
{
    return (void *)create_resample (1, 0, 0, 0, in_rate, out_rate, 0.0, 0, 1.0);
}


void RESAMPLE::xresampleV (float* input, float* output, int numsamps, int* outsamps, void* ptr)
{
    RESAMPLE *a = (RESAMPLE*) ptr;
    a->in = input;
    a->out = output;
    a->size = numsamps;
    *outsamps = xresample(a);
}


void RESAMPLE::destroy_resampleV (void* ptr)
{
    destroy_resample ( (RESAMPLE*) ptr );
}

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR NON-COMPLEX FLOATS                                    *
*                                                                                               *
************************************************************************************************/

RESAMPLEF* RESAMPLEF::create_resampleF ( int run, int size, float* in, float* out, int in_rate, int out_rate)
{
    RESAMPLEF *a = new RESAMPLEF;
    int x, y, z;
    int i, j, k;
    int min_rate;
    float full_rate;
    float fc;
    float fc_norm;
    float* impulse;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    x = in_rate;
    y = out_rate;
    while (y != 0)
    {
        z = y;
        y = x % y;
        x = z;
    }
    a->L = out_rate / x;
    a->M = in_rate / x;
    if (in_rate < out_rate) min_rate = in_rate;
    else min_rate = out_rate;
    fc = 0.45 * (float)min_rate;
    full_rate = (float)(in_rate * a->L);
    fc_norm = fc / full_rate;
    a->ncoef = (int)(60.0 / fc_norm);
    a->ncoef = (a->ncoef / a->L + 1) * a->L;
    a->cpp = a->ncoef / a->L;
    a->h = new float[a->ncoef]; // (float *) malloc0 (a->ncoef * sizeof (float));
    impulse = FIR::fir_bandpass (a->ncoef, -fc_norm, +fc_norm, 1.0, 1, 0, (float)a->L);
    i = 0;
    for (j = 0; j < a->L; j ++)
        for (k = 0; k < a->ncoef; k += a->L)
            a->h[i++] = impulse[j + k];
    a->ringsize = a->cpp;
    a->ring = new float[a->ringsize]; //(float *) malloc0 (a->ringsize * sizeof (float));
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;
    delete[] (impulse);
    return a;
}

void RESAMPLEF::destroy_resampleF (RESAMPLEF *a)
{
    delete[] (a->ring);
    delete[] (a->h);
    delete (a);
}

void RESAMPLEF::flush_resampleF (RESAMPLEF *a)
{
    memset (a->ring, 0, a->ringsize * sizeof (float));
    a->idx_in = a->ringsize - 1;
    a->phnum = 0;
}

int RESAMPLEF::xresampleF (RESAMPLEF *a)
{
    int outsamps = 0;
    if (a->run)
    {
        int i, j, n;
        int idx_out;
        float I;

        for (i = 0; i < a->size; i++)
        {
            a->ring[a->idx_in] = (float)a->in[i];

            while (a->phnum < a->L)
            {
                I = 0.0;
                n = a->cpp * a->phnum;
                for (j = 0; j < a->cpp; j++)
                {
                    if ((idx_out = a->idx_in + j) >= a->ringsize) idx_out -= a->ringsize;
                    I += a->h[n + j] * a->ring[idx_out];
                }
                a->out[outsamps] = (float)I;

                outsamps++;
                a->phnum += a->M;
            }
            a->phnum -= a->L;
            if (--a->idx_in < 0) a->idx_in = a->ringsize - 1;
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (float));
    return outsamps;
}

// Exported calls


void* RESAMPLEF::create_resampleFV (int in_rate, int out_rate)
{
    return (void *) create_resampleF (1, 0, 0, 0, in_rate, out_rate);
}


void RESAMPLEF::xresampleFV (float* input, float* output, int numsamps, int* outsamps, void* ptr)
{
    RESAMPLEF *a = (RESAMPLEF*) ptr;
    a->in = input;
    a->out = output;
    a->size = numsamps;
    *outsamps = xresampleF(a);
}


void RESAMPLEF::destroy_resampleFV (void* ptr)
{
    destroy_resampleF ( (RESAMPLEF*) ptr );
}

} // namespace WDSP
