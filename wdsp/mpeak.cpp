/*  mpeak.c

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
#include "mpeak.hpp"
#include "speak.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                       Complex Multiple Peaking                                        *
*                                                                                                       *
********************************************************************************************************/

void MPEAK::calc_mpeak (MPEAK *a)
{
    int i;
    a->tmp = new float[a->size * 2]; // (float *) malloc0 (a->size * sizeof (complex));
    a->mix = new float[a->size * 2]; // (float *) malloc0 (a->size * sizeof (complex));
    for (i = 0; i < a->npeaks; i++)
    {
        a->pfil[i] = SPEAK::create_speak (
            1,
            a->size,
            a->in,
            a->tmp,
            a->rate,
            a->f[i],
            a->bw[i],
            a->gain[i],
            a->nstages,
            1
        );
    }
}

void MPEAK::decalc_mpeak (MPEAK *a)
{
    int i;
    for (i = 0; i < a->npeaks; i++)
        SPEAK::destroy_speak (a->pfil[i]);
    delete[] (a->mix);
    delete[] (a->tmp);
}

MPEAK* MPEAK::create_mpeak (
    int run,
    int size,
    float* in,
    float* out,
    int rate,
    int npeaks,
    int* enable,
    double* f,
    double* bw,
    double* gain,
    int nstages
)
{
    MPEAK *a = new MPEAK;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->npeaks = npeaks;
    a->nstages = nstages;
    a->enable = new int[a->npeaks]; // (int *) malloc0 (a->npeaks * sizeof (int));
    a->f    = new double[a->npeaks]; // (float *) malloc0 (a->npeaks * sizeof (float));
    a->bw   = new double[a->npeaks]; // (float *) malloc0 (a->npeaks * sizeof (float));
    a->gain = new double[a->npeaks]; // (float *) malloc0 (a->npeaks * sizeof (float));
    memcpy (a->enable, enable, a->npeaks * sizeof (int));
    memcpy (a->f, f, a->npeaks * sizeof (double));
    memcpy (a->bw, bw, a->npeaks * sizeof (double));
    memcpy (a->gain, gain, a->npeaks * sizeof (double));
    a->pfil = new SPEAK*[a->npeaks]; // (SPEAK *) malloc0 (a->npeaks * sizeof (SPEAK));
    calc_mpeak (a);
    return a;
}

void MPEAK::destroy_mpeak (MPEAK *a)
{
    decalc_mpeak (a);
    delete[] (a->pfil);
    delete[] (a->gain);
    delete[] (a->bw);
    delete[] (a->f);
    delete[] (a->enable);
    delete (a);
}

void MPEAK::flush_mpeak (MPEAK *a)
{
    int i;
    for (i = 0; i < a->npeaks; i++)
        SPEAK::flush_speak (a->pfil[i]);
}

void MPEAK::xmpeak (MPEAK *a)
{
    if (a->run)
    {
        int i, j;
        std::fill(a->mix, a->mix + a->size * 2, 0);

        for (i = 0; i < a->npeaks; i++)
        {
            if (a->enable[i])
            {
                SPEAK::xspeak (a->pfil[i]);
                for (j = 0; j < 2 * a->size; j++)
                    a->mix[j] += a->tmp[j];
            }
        }

        std::copy(a->mix, a->mix + a->size * 2, a->out);
    }
    else if (a->in != a->out)
    {
        std::copy( a->in,  a->in + a->size * 2, a->out);
    }
}

void MPEAK::setBuffers_mpeak (MPEAK *a, float* in, float* out)
{
    decalc_mpeak (a);
    a->in = in;
    a->out = out;
    calc_mpeak (a);
}

void MPEAK::setSamplerate_mpeak (MPEAK *a, int rate)
{
    decalc_mpeak (a);
    a->rate = rate;
    calc_mpeak (a);
}

void MPEAK::setSize_mpeak (MPEAK *a, int size)
{
    decalc_mpeak (a);
    a->size = size;
    calc_mpeak (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void MPEAK::SetmpeakRun (RXA& rxa, int run)
{
    MPEAK *a = rxa.mpeak;
    a->run = run;
}

void MPEAK::SetmpeakNpeaks (RXA& rxa, int npeaks)
{
    MPEAK *a = rxa.mpeak;
    a->npeaks = npeaks;
}

void MPEAK::SetmpeakFilEnable (RXA& rxa, int fil, int enable)
{
    MPEAK *a = rxa.mpeak;
    a->enable[fil] = enable;
}

void MPEAK::SetmpeakFilFreq (RXA& rxa, int fil, double freq)
{
    MPEAK *a = rxa.mpeak;
    a->f[fil] = freq;
    a->pfil[fil]->f = freq;
    SPEAK::calc_speak(a->pfil[fil]);
}

void MPEAK::SetmpeakFilBw (RXA& rxa, int fil, double bw)
{
    MPEAK *a = rxa.mpeak;
    a->bw[fil] = bw;
    a->pfil[fil]->bw = bw;
    SPEAK::calc_speak(a->pfil[fil]);
}

void MPEAK::SetmpeakFilGain (RXA& rxa, int fil, double gain)
{
    MPEAK *a = rxa.mpeak;
    a->gain[fil] = gain;
    a->pfil[fil]->gain = gain;
    SPEAK::calc_speak(a->pfil[fil]);
}

} // namespace WDSP
