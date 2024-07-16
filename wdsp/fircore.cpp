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
#include "fircore.hpp"

namespace WDSP {

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
        std::copy(a->impulse, a->impulse + a->nc * 2, a->imp);

    for (i = 0; i < a->nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        std::copy(&(a->imp[2 * a->size * i]), &(a->imp[2 * a->size * i]) + a->size * 2, &(a->maskgen[2 * a->size]));
        fftwf_execute (a->maskplan[1 - a->cset][i]);
    }

    a->masks_ready = 1;

    if (flip)
    {
        a->cset = 1 - a->cset;
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
    std::copy(impulse, impulse + a->nc * 2, a->impulse);
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
    std::fill(a->fftin, a->fftin + 2 * a->size * 2, 0);
    for (i = 0; i < a->nfor; i++)
        std::fill(a->fftout[i], a->fftout[i] + 2 * a->size * 2, 0);
    a->buffidx = 0;
}

void FIRCORE::xfircore (FIRCORE *a)
{
    int i, j, k;
    std::copy(a->in, a->in + a->size * 2, &(a->fftin[2 * a->size]));
    fftwf_execute (a->pcfor[a->buffidx]);
    k = a->buffidx;
    std::fill(a->accum, a->accum + 2 * a->size * 2, 0);

    for (j = 0; j < a->nfor; j++)
    {
        for (i = 0; i < 2 * a->size; i++)
        {
            a->accum[2 * i + 0] += a->fftout[k][2 * i + 0] * a->fmask[a->cset][j][2 * i + 0] - a->fftout[k][2 * i + 1] * a->fmask[a->cset][j][2 * i + 1];
            a->accum[2 * i + 1] += a->fftout[k][2 * i + 0] * a->fmask[a->cset][j][2 * i + 1] + a->fftout[k][2 * i + 1] * a->fmask[a->cset][j][2 * i + 0];
        }

        k = (k + a->idxmask) & a->idxmask;
    }

    a->buffidx = (a->buffidx + 1) & a->idxmask;
    fftwf_execute (a->crev);
    std::copy(&(a->fftin[2 * a->size]), &(a->fftin[2 * a->size]) + a->size * 2, a->fftin);
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
    std::copy(impulse, impulse + a->nc * 2, a->impulse);
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
    std::copy(impulse, impulse + a->nc * 2, a->impulse);
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
        a->cset = 1 - a->cset;
        a->masks_ready = 0;
    }
}

} // namespace WDSP
