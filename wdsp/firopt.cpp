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
#include "firopt.hpp"

namespace WDSP {

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
    a->fftin = new float[2 * a->size * 2];
    a->fftout = new float*[a->nfor];
    a->fmask = new float*[a->nfor];
    a->maskgen = new float[2 * a->size * 2];
    a->pcfor = new fftwf_plan[a->nfor];
    a->maskplan = new fftwf_plan[a->nfor];
    for (i = 0; i < a->nfor; i++)
    {
        a->fftout[i] = new float[2 * a->size * 2];
        a->fmask[i] = new float[2 * a->size * 2];
        a->pcfor[i] = fftwf_plan_dft_1d(
            2 * a->size,
            (fftwf_complex *)a->fftin,
            (fftwf_complex *)a->fftout[i],
            FFTW_FORWARD,
            FFTW_PATIENT
        );
        a->maskplan[i] = fftwf_plan_dft_1d(
            2 * a->size,
            (fftwf_complex *)a->maskgen,
            (fftwf_complex *)a->fmask[i],
            FFTW_FORWARD,
            FFTW_PATIENT
        );
    }
    a->accum = new float[2 * a->size * 2];
    a->crev = fftwf_plan_dft_1d(
        2 * a->size,
        (fftwf_complex *)a->accum,
        (fftwf_complex *)a->out,
        FFTW_BACKWARD,
        FFTW_PATIENT
    );
}

void FIROPT::calc_firopt (FIROPT *a)
{
    // call for change in frequency, rate, wintype, gain
    // must also call after a call to plan_firopt()
    std::vector<float> impulse;
    FIR::fir_bandpass (impulse, a->nc, a->f_low, a->f_high, a->samplerate, a->wintype, 1, a->gain);
    a->buffidx = 0;
    for (int i = 0; i < a->nfor; i++)
    {
        // I right-justified the impulse response => take output from left side of output buff, discard right side
        // Be careful about flipping an asymmetrical impulse response.
        std::copy(&(impulse[2 * a->size * i]), &(impulse[2 * a->size * i]) + a->size * 2, &(a->maskgen[2 * a->size]));
        fftwf_execute (a->maskplan[i]);
    }
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
    a->samplerate = (float) samplerate;
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
    std::fill(a->fftin, a->fftin + 2 * a->size * 2, 0);
    for (int i = 0; i < a->nfor; i++)
        std::fill(a->fftout[i], a->fftout[i] + 2 * a->size * 2, 0);
    a->buffidx = 0;
}

void FIROPT::xfiropt (FIROPT *a, int pos)
{
    if (a->run && (a->position == pos))
    {
        int k;
        std::copy(a->in, a->in + a->size * 2, &(a->fftin[2 * a->size]));
        fftwf_execute (a->pcfor[a->buffidx]);
        k = a->buffidx;
        std::fill(a->accum, a->accum + 2 * a->size * 2, 0);
        for (int j = 0; j < a->nfor; j++)
        {
            for (int i = 0; i < 2 * a->size; i++)
            {
                a->accum[2 * i + 0] += a->fftout[k][2 * i + 0] * a->fmask[j][2 * i + 0] - a->fftout[k][2 * i + 1] * a->fmask[j][2 * i + 1];
                a->accum[2 * i + 1] += a->fftout[k][2 * i + 0] * a->fmask[j][2 * i + 1] + a->fftout[k][2 * i + 1] * a->fmask[j][2 * i + 0];
            }
            k = (k + a->idxmask) & a->idxmask;
        }
        a->buffidx = (a->buffidx + 1) & a->idxmask;
        fftwf_execute (a->crev);
        std::copy(&(a->fftin[2 * a->size]), &(a->fftin[2 * a->size]) + a->size * 2, a->fftin);
    }
    else if (a->in != a->out)
        std::copy( a->in,  a->in + a->size * 2, a->out);
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
    a->samplerate = (float) rate;
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


} // namespace WDSP
