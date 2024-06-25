/*  firmin.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2016 Warren Pratt, NR0V
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
#include "firmin.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Time-Domain FIR                                             *
*                                                                                                       *
********************************************************************************************************/

void FIRMIN::calc_firmin (FIRMIN *a)
{
    a->h = FIR::fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain);
    a->rsize = a->nc;
    a->mask = a->rsize - 1;
    a->ring = new float[a->rsize * 2]; // (float *) malloc0 (a->rsize * sizeof (complex));
    a->idx = 0;
}

FIRMIN* FIRMIN::create_firmin (int run, int position, int size, float* in, float* out,
    int nc, float f_low, float f_high, int samplerate, int wintype, float gain)
{
    FIRMIN *a = new FIRMIN;
    a->run = run;
    a->position = position;
    a->size = size;
    a->in = in;
    a->out = out;
    a->nc = nc;
    a->f_low = f_low;
    a->f_high = f_high;
    a->samplerate = samplerate;
    a->wintype = wintype;
    a->gain = gain;
    calc_firmin (a);
    return a;
}

void FIRMIN::destroy_firmin (FIRMIN *a)
{
    delete[] (a->ring);
    delete[] (a->h);
    delete (a);
}

void FIRMIN::flush_firmin (FIRMIN *a)
{
    memset (a->ring, 0, a->rsize * sizeof (wcomplex));
    a->idx = 0;
}

void FIRMIN::xfirmin (FIRMIN *a, int pos)
{
    if (a->run && a->position == pos)
    {
        int i, j, k;
        for (i = 0; i < a->size; i++)
        {
            a->ring[2 * a->idx + 0] = a->in[2 * i + 0];
            a->ring[2 * a->idx + 1] = a->in[2 * i + 1];
            a->out[2 * i + 0] = 0.0;
            a->out[2 * i + 1] = 0.0;
            k = a->idx;
            for (j = 0; j < a->nc; j++)
            {
                a->out[2 * i + 0] += a->h[2 * j + 0] * a->ring[2 * k + 0] - a->h[2 * j + 1] * a->ring[2 * k + 1];
                a->out[2 * i + 1] += a->h[2 * j + 0] * a->ring[2 * k + 1] + a->h[2 * j + 1] * a->ring[2 * k + 0];
                k = (k + a->mask) & a->mask;
            }
            a->idx = (a->idx + 1) & a->mask;
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void FIRMIN::setBuffers_firmin (FIRMIN *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
}

void FIRMIN::setSamplerate_firmin (FIRMIN *a, int rate)
{
    a->samplerate = (float)rate;
    calc_firmin (a);
}

void FIRMIN::setSize_firmin (FIRMIN *a, int size)
{
    a->size = size;
}

void FIRMIN::setFreqs_firmin (FIRMIN *a, float f_low, float f_high)
{
    a->f_low = f_low;
    a->f_high = f_high;
    calc_firmin (a);
}

/********************************************************************************************************
*                                                                                                       *
*                               Standalone Partitioned Overlap-Save Bandpass                            *
*                                                                                                       *
********************************************************************************************************/

void FIROPT::plan_firopt (FIROPT *a)
{
    // must call for change in 'nc', 'size', 'out'
    int i;
    a->nfor = a->nc / a->size;
    a->buffidx = 0;
    a->idxmask = a->nfor - 1;
    a->fftin = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
    a->fftout = new float*[a->nfor]; //  (float **) malloc0 (a->nfor * sizeof (float *));
    a->fmask = new float*[a->nfor]; //  (float **) malloc0 (a->nfor * sizeof (float *));
    a->maskgen = new float[2 * a->size * 2]; //  (float *) malloc0 (2 * a->size * sizeof (complex));
    a->pcfor = new fftwf_plan[a->nfor]; // (fftwf_plan *) malloc0 (a->nfor * sizeof (fftwf_plan));
    a->maskplan = new fftwf_plan[a->nfor]; // (fftwf_plan *) malloc0 (a->nfor * sizeof (fftwf_plan));
    for (i = 0; i < a->nfor; i++)
    {
        a->fftout[i] = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
        a->fmask[i] = new float[2 * a->size * 2]; //  (float *) malloc0 (2 * a->size * sizeof (complex));
        a->pcfor[i] = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->fftin, (fftwf_complex *)a->fftout[i], FFTW_FORWARD, FFTW_PATIENT);
        a->maskplan[i] = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->maskgen, (fftwf_complex *)a->fmask[i], FFTW_FORWARD, FFTW_PATIENT);
    }
    a->accum = new float[2 * a->size * 2]; //  (float *) malloc0 (2 * a->size * sizeof (complex));
    a->crev = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->accum, (fftwf_complex *)a->out, FFTW_BACKWARD, FFTW_PATIENT);
}

void FIROPT::calc_firopt (FIROPT *a)
{
    // call for change in frequency, rate, wintype, gain
    // must also call after a call to plan_firopt()
    int i;
    float* impulse = FIR::fir_bandpass (a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain);
    a->buffidx = 0;
    for (i = 0; i < a->nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        memcpy (&(a->maskgen[2 * a->size]), &(impulse[2 * a->size * i]), a->size * sizeof(wcomplex));
        fftwf_execute (a->maskplan[i]);
    }
    delete[] (impulse);
}

FIROPT* FIROPT::create_firopt (int run, int position, int size, float* in, float* out,
    int nc, float f_low, float f_high, int samplerate, int wintype, float gain)
{
    FIROPT *a = new FIROPT;
    a->run = run;
    a->position = position;
    a->size = size;
    a->in = in;
    a->out = out;
    a->nc = nc;
    a->f_low = f_low;
    a->f_high = f_high;
    a->samplerate = samplerate;
    a->wintype = wintype;
    a->gain = gain;
    plan_firopt (a);
    calc_firopt (a);
    return a;
}

void FIROPT::deplan_firopt (FIROPT *a)
{
    int i;
    fftwf_destroy_plan (a->crev);
    delete[] (a->accum);
    for (i = 0; i < a->nfor; i++)
    {
        delete[] (a->fftout[i]);
        delete[] (a->fmask[i]);
        fftwf_destroy_plan (a->pcfor[i]);
        fftwf_destroy_plan (a->maskplan[i]);
    }
    delete[] (a->maskplan);
    delete[] (a->pcfor);
    delete[] (a->maskgen);
    delete[] (a->fmask);
    delete[] (a->fftout);
    delete[] (a->fftin);
}

void FIROPT::destroy_firopt (FIROPT *a)
{
    deplan_firopt (a);
    delete (a);
}

void FIROPT::flush_firopt (FIROPT *a)
{
    int i;
    memset (a->fftin, 0, 2 * a->size * sizeof (wcomplex));
    for (i = 0; i < a->nfor; i++)
        memset (a->fftout[i], 0, 2 * a->size * sizeof (wcomplex));
    a->buffidx = 0;
}

void FIROPT::xfiropt (FIROPT *a, int pos)
{
    if (a->run && (a->position == pos))
    {
        int i, j, k;
        memcpy (&(a->fftin[2 * a->size]), a->in, a->size * sizeof (wcomplex));
        fftwf_execute (a->pcfor[a->buffidx]);
        k = a->buffidx;
        memset (a->accum, 0, 2 * a->size * sizeof (wcomplex));
        for (j = 0; j < a->nfor; j++)
        {
            for (i = 0; i < 2 * a->size; i++)
            {
                a->accum[2 * i + 0] += a->fftout[k][2 * i + 0] * a->fmask[j][2 * i + 0] - a->fftout[k][2 * i + 1] * a->fmask[j][2 * i + 1];
                a->accum[2 * i + 1] += a->fftout[k][2 * i + 0] * a->fmask[j][2 * i + 1] + a->fftout[k][2 * i + 1] * a->fmask[j][2 * i + 0];
            }
            k = (k + a->idxmask) & a->idxmask;
        }
        a->buffidx = (a->buffidx + 1) & a->idxmask;
        fftwf_execute (a->crev);
        memcpy (a->fftin, &(a->fftin[2 * a->size]), a->size * sizeof(wcomplex));
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void FIROPT::setBuffers_firopt (FIROPT *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    deplan_firopt (a);
    plan_firopt (a);
    calc_firopt (a);
}

void FIROPT::setSamplerate_firopt (FIROPT *a, int rate)
{
    a->samplerate = rate;
    calc_firopt (a);
}

void FIROPT::setSize_firopt (FIROPT *a, int size)
{
    a->size = size;
    deplan_firopt (a);
    plan_firopt (a);
    calc_firopt (a);
}

void FIROPT::setFreqs_firopt (FIROPT *a, float f_low, float f_high)
{
    a->f_low = f_low;
    a->f_high = f_high;
    calc_firopt (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Filter Kernel                              *
*                                                                                                       *
********************************************************************************************************/


void FIRCORE::plan_fircore (FIRCORE *a)
{
    // must call for change in 'nc', 'size', 'out'
    int i;
    a->nfor = a->nc / a->size;
    a->cset = 0;
    a->buffidx = 0;
    a->idxmask = a->nfor - 1;
    a->fftin = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
    a->fftout   = new float*[a->nfor]; // (float **) malloc0 (a->nfor * sizeof (float *));
    a->fmask    = new float**[2]; // (float ***) malloc0 (2 * sizeof (float **));
    a->fmask[0] = new float*[a->nfor]; // (float **) malloc0 (a->nfor * sizeof (float *));
    a->fmask[1] = new float*[a->nfor]; // (float **) malloc0 (a->nfor * sizeof (float *));
    a->maskgen = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
    a->pcfor = new fftwf_plan[a->nfor]; // (fftwf_plan *) malloc0 (a->nfor * sizeof (fftwf_plan));
    a->maskplan    = new fftwf_plan*[2]; // (fftwf_plan **) malloc0 (2 * sizeof (fftwf_plan *));
    a->maskplan[0] = new fftwf_plan[a->nfor]; // (fftwf_plan *) malloc0 (a->nfor * sizeof (fftwf_plan));
    a->maskplan[1] = new fftwf_plan[a->nfor]; // (fftwf_plan *) malloc0 (a->nfor * sizeof (fftwf_plan));
    for (i = 0; i < a->nfor; i++)
    {
        a->fftout[i]   = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
        a->fmask[0][i] = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
        a->fmask[1][i] = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
        a->pcfor[i] = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->fftin, (fftwf_complex *)a->fftout[i], FFTW_FORWARD, FFTW_PATIENT);
        a->maskplan[0][i] = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->maskgen, (fftwf_complex *)a->fmask[0][i], FFTW_FORWARD, FFTW_PATIENT);
        a->maskplan[1][i] = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->maskgen, (fftwf_complex *)a->fmask[1][i], FFTW_FORWARD, FFTW_PATIENT);
    }
    a->accum = new float[2 * a->size * 2]; // (float *) malloc0 (2 * a->size * sizeof (complex));
    a->crev = fftwf_plan_dft_1d(2 * a->size, (fftwf_complex *)a->accum, (fftwf_complex *)a->out, FFTW_BACKWARD, FFTW_PATIENT);
    a->masks_ready = 0;
}

void FIRCORE::calc_fircore (FIRCORE *a, int flip)
{
    // call for change in frequency, rate, wintype, gain
    // must also call after a call to plan_firopt()
    int i;
    if (a->mp)
        FIR::mp_imp (a->nc, a->impulse, a->imp, 16, 0);
    else
        memcpy (a->imp, a->impulse, a->nc * sizeof (wcomplex));
    for (i = 0; i < a->nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        memcpy (&(a->maskgen[2 * a->size]), &(a->imp[2 * a->size * i]), a->size * sizeof(wcomplex));
        fftwf_execute (a->maskplan[1 - a->cset][i]);
    }
    a->masks_ready = 1;
    if (flip)
    {
        a->update.lock();
        a->cset = 1 - a->cset;
        a->update.unlock();
        a->masks_ready = 0;
    }
}

FIRCORE* FIRCORE::create_fircore (int size, float* in, float* out, int nc, int mp, float* impulse)
{
    FIRCORE *a = new FIRCORE;
    a->size = size;
    a->in = in;
    a->out = out;
    a->nc = nc;
    a->mp = mp;
    // InitializeCriticalSectionAndSpinCount (&a->update, 2500);
    plan_fircore (a);
    a->impulse = new float[a->nc * 2]; // (float *) malloc0 (a->nc * sizeof (complex));
    a->imp     = new float[a->nc * 2]; // (float *) malloc0 (a->nc * sizeof (complex));
    memcpy (a->impulse, impulse, a->nc * sizeof (wcomplex));
    calc_fircore (a, 1);
    return a;
}

void FIRCORE::deplan_fircore (FIRCORE *a)
{
    int i;
    fftwf_destroy_plan (a->crev);
    delete[] (a->accum);
    for (i = 0; i < a->nfor; i++)
    {
        delete[] (a->fftout[i]);
        delete[] (a->fmask[0][i]);
        delete[] (a->fmask[1][i]);
        fftwf_destroy_plan (a->pcfor[i]);
        fftwf_destroy_plan (a->maskplan[0][i]);
        fftwf_destroy_plan (a->maskplan[1][i]);
    }
    delete[] (a->maskplan[0]);
    delete[] (a->maskplan[1]);
    delete[] (a->maskplan);
    delete[] (a->pcfor);
    delete[] (a->maskgen);
    delete[] (a->fmask[0]);
    delete[] (a->fmask[1]);
    delete[] (a->fmask);
    delete[] (a->fftout);
    delete[] (a->fftin);
}

void FIRCORE::destroy_fircore (FIRCORE *a)
{
    deplan_fircore (a);
    delete[] (a->imp);
    delete[] (a->impulse);
    delete (a);
}

void FIRCORE::flush_fircore (FIRCORE *a)
{
    int i;
    memset (a->fftin, 0, 2 * a->size * sizeof (wcomplex));
    for (i = 0; i < a->nfor; i++)
        memset (a->fftout[i], 0, 2 * a->size * sizeof (wcomplex));
    a->buffidx = 0;
}

void FIRCORE::xfircore (FIRCORE *a)
{
    int i, j, k;
    memcpy (&(a->fftin[2 * a->size]), a->in, a->size * sizeof (wcomplex));
    fftwf_execute (a->pcfor[a->buffidx]);
    k = a->buffidx;
    memset (a->accum, 0, 2 * a->size * sizeof (wcomplex));
    a->update.lock();
    for (j = 0; j < a->nfor; j++)
    {
        for (i = 0; i < 2 * a->size; i++)
        {
            a->accum[2 * i + 0] += a->fftout[k][2 * i + 0] * a->fmask[a->cset][j][2 * i + 0] - a->fftout[k][2 * i + 1] * a->fmask[a->cset][j][2 * i + 1];
            a->accum[2 * i + 1] += a->fftout[k][2 * i + 0] * a->fmask[a->cset][j][2 * i + 1] + a->fftout[k][2 * i + 1] * a->fmask[a->cset][j][2 * i + 0];
        }
        k = (k + a->idxmask) & a->idxmask;
    }
    a->update.unlock();
    a->buffidx = (a->buffidx + 1) & a->idxmask;
    fftwf_execute (a->crev);
    memcpy (a->fftin, &(a->fftin[2 * a->size]), a->size * sizeof(wcomplex));
}

void FIRCORE::setBuffers_fircore (FIRCORE *a, float* in, float* out)
{
    a->in = in;
    a->out = out;
    deplan_fircore (a);
    plan_fircore (a);
    calc_fircore (a, 1);
}

void FIRCORE::setSize_fircore (FIRCORE *a, int size)
{
    a->size = size;
    deplan_fircore (a);
    plan_fircore (a);
    calc_fircore (a, 1);
}

void FIRCORE::setImpulse_fircore (FIRCORE *a, float* impulse, int update)
{
    memcpy (a->impulse, impulse, a->nc * sizeof (wcomplex));
    calc_fircore (a, update);
}

void FIRCORE::setNc_fircore (FIRCORE *a, int nc, float* impulse)
{
    // because of FFT planning, this will probably cause a glitch in audio if done during dataflow
    deplan_fircore (a);
    delete[] (a->impulse);
    delete[] (a->imp);
    a->nc = nc;
    plan_fircore (a);
    a->imp     = new float[a->nc * 2]; // (float *) malloc0 (a->nc * sizeof (complex));
    a->impulse = new float[a->nc * 2]; // (float *) malloc0 (a->nc * sizeof (complex));
    memcpy (a->impulse, impulse, a->nc * sizeof (wcomplex));
    calc_fircore (a, 1);
}

void FIRCORE::setMp_fircore (FIRCORE *a, int mp)
{
    a->mp = mp;
    calc_fircore (a, 1);
}

void FIRCORE::setUpdate_fircore (FIRCORE *a)
{
    if (a->masks_ready)
    {
        a->update.lock();
        a->cset = 1 - a->cset;
        a->update.unlock();
        a->masks_ready = 0;
    }
}

} // namespace WDSP
