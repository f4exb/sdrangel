/*  siphon.c

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
#include "meterlog10.hpp"
#include "siphon.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

void SIPHON::build_window (SIPHON *a)
{
    int i;
    float arg0, cosphi;
    float sum, scale;
    arg0 = 2.0 * PI / ((float)a->fftsize - 1.0);
    sum = 0.0;
    for (i = 0; i < a->fftsize; i++)
    {
        cosphi = cos (arg0 * (float)i);
        a->window[i] =  + 6.3964424114390378e-02
          + cosphi *  ( - 2.3993864599352804e-01
          + cosphi *  ( + 3.5015956323820469e-01
          + cosphi *  ( - 2.4774111897080783e-01
          + cosphi *  ( + 8.5438256055858031e-02
          + cosphi *  ( - 1.2320203369293225e-02
          + cosphi *  ( + 4.3778825791773474e-04 ))))));
        sum += a->window[i];
    }
    scale = 1.0 / sum;
    for (i = 0; i < a->fftsize; i++)
        a->window[i] *= scale;
}

SIPHON* SIPHON::create_siphon (
    int run,
    int position,
    int mode,
    int disp,
    int insize,
    float* in,
    int sipsize,
    int fftsize,
    int specmode
)
{
    SIPHON *a = new SIPHON;
    a->run = run;
    a->position = position;
    a->mode = mode;
    a->disp = disp;
    a->insize = insize;
    a->in = in;
    a->sipsize = sipsize;   // NOTE:  sipsize MUST BE A POWER OF TWO!!
    a->fftsize = fftsize;
    a->specmode = specmode;
    a->sipbuff = new float[a->sipsize * 2]; // (float *) malloc0 (a->sipsize * sizeof (complex));
    a->idx = 0;
    a->sipout  = new float[a->sipsize * 2]; // (float *) malloc0 (a->sipsize * sizeof (complex));
    a->specout = new float[a->fftsize * 2]; // (float *) malloc0 (a->fftsize * sizeof (complex));
    a->sipplan = fftwf_plan_dft_1d (a->fftsize, (fftwf_complex *)a->sipout, (fftwf_complex *)a->specout, FFTW_FORWARD, FFTW_PATIENT);
    a->window  = new float[a->fftsize * 2]; // (float *) malloc0 (a->fftsize * sizeof (complex));
    build_window (a);
    return a;
}

void SIPHON::destroy_siphon (SIPHON *a)
{
    fftwf_destroy_plan (a->sipplan);
    delete[] (a->window);
    delete[] (a->specout);
    delete[] (a->sipout);
    delete[] (a->sipbuff);
    delete (a);
}

void SIPHON::flush_siphon (SIPHON *a)
{
    std::fill(a->sipbuff, a->sipbuff + a->sipsize * 2, 0);
    std::fill(a->sipout,  a->sipout  + a->sipsize * 2, 0);
    std::fill(a->specout, a->specout + a->fftsize * 2, 0);
    a->idx = 0;
}

void SIPHON::xsiphon (SIPHON *a, int pos)
{
    int first, second;

    if (a->run && a->position == pos)
    {
        switch (a->mode)
        {
        case 0:
            if (a->insize >= a->sipsize)
                std::copy(&(a->in[2 * (a->insize - a->sipsize)]), &(a->in[2 * (a->insize - a->sipsize)]) + a->sipsize * 2, a->sipbuff);
            else
            {
                if (a->insize > (a->sipsize - a->idx))
                {
                    first = a->sipsize - a->idx;
                    second = a->insize - first;
                }
                else
                {
                    first = a->insize;
                    second = 0;
                }
                std::copy(a->in, a->in + first * 2, a->sipbuff + 2 * a->idx);
                std::copy(a->in + 2 * first, a->in + 2 * first + second * 2, a->sipbuff);
                if ((a->idx += a->insize) >= a->sipsize) a->idx -= a->sipsize;
            }
            break;
        case 1:
            // Spectrum0 (1, a->disp, 0, 0, a->in);
            break;
        }
    }
}

void SIPHON::setBuffers_siphon (SIPHON *a, float* in)
{
    a->in = in;
}

void SIPHON::setSamplerate_siphon (SIPHON *a, int)
{
    flush_siphon (a);
}

void SIPHON::setSize_siphon (SIPHON *a, int size)
{
    a->insize = size;
    flush_siphon (a);
}

void SIPHON::suck (SIPHON *a)
{
    if (a->outsize <= a->sipsize)
    {
        int mask = a->sipsize - 1;
        int j = (a->idx - a->outsize) & mask;
        int size = a->sipsize - j;
        if (size >= a->outsize)
            std::copy(&(a->sipbuff[2 * j]), &(a->sipbuff[2 * j]) + a->outsize * 2, a->sipout);
        else
        {
            std::copy(&(a->sipbuff[2 * j]), &(a->sipbuff[2 * j]) + size * 2, a->sipout);
            std::copy(a->sipbuff, a->sipbuff + (a->outsize - size) * 2, &(a->sipout[2 * size]));
        }
    }
}

void SIPHON::sip_spectrum (SIPHON *a)
{
    int i;
    for (i = 0; i < a->fftsize; i++)
    {
        a->sipout[2 * i + 0] *= a->window[i];
        a->sipout[2 * i + 1] *= a->window[i];
    }
    fftwf_execute (a->sipplan);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SIPHON::GetaSipF (RXA& rxa, float* out, int size)
{   // return raw samples as floats
    SIPHON *a=rxa.sip1.p;
    int i;
    a->outsize = size;
    suck (a);

    for (i = 0; i < size; i++) {
        out[i] = (float)a->sipout[2 * i + 0];
    }
}

void SIPHON::GetaSipF1 (RXA& rxa, float* out, int size)
{   // return raw samples as floats
    SIPHON *a=rxa.sip1.p;
    int i;
    a->outsize = size;
    suck (a);

    for (i = 0; i < size; i++)
    {
        out[2 * i + 0] = (float)a->sipout[2 * i + 0];
        out[2 * i + 1] = (float)a->sipout[2 * i + 1];
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SIPHON::SetSipPosition (TXA& txa, int pos)
{
    SIPHON *a = txa.sip1.p;
    a->position = pos;
}

void SIPHON::SetSipMode (TXA& txa, int mode)
{
    SIPHON *a = txa.sip1.p;
    a->mode = mode;
}

void SIPHON::SetSipDisplay (TXA& txa, int disp)
{
    SIPHON *a = txa.sip1.p;
    a->disp = disp;
}

void SIPHON::GetaSipF (TXA& txa, float* out, int size)
{   // return raw samples as floats
    SIPHON *a = txa.sip1.p;
    int i;
    a->outsize = size;
    suck (a);

    for (i = 0; i < size; i++) {
        out[i] = (float)a->sipout[2 * i + 0];
    }
}

void SIPHON::GetaSipF1 (TXA& txa, float* out, int size)
{   // return raw samples as floats
    SIPHON *a = txa.sip1.p;
    int i;
    a->outsize = size;
    suck (a);

    for (i = 0; i < size; i++)
    {
        out[2 * i + 0] = (float)a->sipout[2 * i + 0];
        out[2 * i + 1] = (float)a->sipout[2 * i + 1];
    }
}

void SIPHON::SetSipSpecmode (TXA& txa, int mode)
{
    SIPHON *a = txa.sip1.p;
    if (mode == 0)
        a->specmode = 0;
    else
        a->specmode = 1;
}

void SIPHON::GetSpecF1 (TXA& txa, float* out)
{   // return spectrum magnitudes in dB
    SIPHON *a = txa.sip1.p;
    int i, j, mid, m, n;
    a->outsize = a->fftsize;
    suck (a);
    sip_spectrum (a);
    mid = a->fftsize / 2;

    if (a->specmode != 1)
    {
        // swap the halves of the spectrum
        for (i = 0, j = mid; i < mid; i++, j++)
        {
            out[i] = (float)(10.0 * MemLog::mlog10 (a->specout[2 * j + 0] * a->specout[2 * j + 0] + a->specout[2 * j + 1] * a->specout[2 * j + 1] + 1.0e-60));
            out[j] = (float)(10.0 * MemLog::mlog10 (a->specout[2 * i + 0] * a->specout[2 * i + 0] + a->specout[2 * i + 1] * a->specout[2 * i + 1] + 1.0e-60));
        }
    }
    else
    {
        // mirror each half of the spectrum in-place
        for (i = 0, j = mid - 1, m = mid, n = a->fftsize - 1; i < mid; i++, j--, m++, n--)
        {
            out[i] = (float)(10.0 * MemLog::mlog10 (a->specout[2 * j + 0] * a->specout[2 * j + 0] + a->specout[2 * j + 1] * a->specout[2 * j + 1] + 1.0e-60));
            out[m] = (float)(10.0 * MemLog::mlog10 (a->specout[2 * n + 0] * a->specout[2 * n + 0] + a->specout[2 * n + 1] * a->specout[2 * n + 1] + 1.0e-60));
        }
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                       CALLS FOR EXTERNAL USE                                          *
*                                                                                                       *
********************************************************************************************************/

/*
#define MAX_EXT_SIPHONS (2)                                 // maximum number of Siphons called from outside wdsp
__declspec (align (16)) SIPHON psiphon[MAX_EXT_SIPHONS];    // array of pointers for Siphons used EXTERNAL to wdsp


PORT
void create_siphonEXT (int id, int run, int insize, int sipsize, int fftsize, int specmode)
{
    psiphon[id] = create_siphon (run, 0, 0, 0, insize, 0, sipsize, fftsize, specmode);
}

PORT
void destroy_siphonEXT (int id)
{
    destroy_siphon (psiphon[id]);
}

PORT
void flush_siphonEXT (int id)
{
    flush_siphon (psiphon[id]);
}

PORT
void xsiphonEXT (int id, float* buff)
{
    SIPHON a = psiphon[id];
    a->in = buff;
    xsiphon (a, 0);
}

PORT
void GetaSipF1EXT (int id, float* out, int size)
{   // return raw samples as floats
    SIPHON a = psiphon[id];
    int i;
    a->update.lock();
    a->outsize = size;
    suck (a);
    a->update.unlock();
    for (i = 0; i < size; i++)
    {
        out[2 * i + 0] = (float)a->sipout[2 * i + 0];
        out[2 * i + 1] = (float)a->sipout[2 * i + 1];
    }
}

PORT
void SetSiphonInsize (int id, int size)
{
    SIPHON a = psiphon[id];
    a->update.lock();
    a->insize = size;
    a->update.unlock();
}
*/

} // namespace WDSP

