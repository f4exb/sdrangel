/*  iir.c

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
#include "iir.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Bi-Quad Notch                                               *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::calc_snotch (SNOTCH *a)
{
    double fn, qk, qr, csn;
    fn = a->f / (double)a->rate;
    csn = cos (TWOPI * fn);
    qr = 1.0 - 3.0 * a->bw;
    qk = (1.0 - 2.0 * qr * csn + qr * qr) / (2.0 * (1.0 - csn));
    a->a0 = + qk;
    a->a1 = - 2.0 * qk * csn;
    a->a2 = + qk;
    a->b1 = + 2.0 * qr * csn;
    a->b2 = - qr * qr;
    flush_snotch (a);
}

SNOTCH* SNOTCH::create_snotch (int run, int size, double* in, double* out, int rate, double f, double bw)
{
    SNOTCH *a = new SNOTCH;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f = f;
    a->bw = bw;
    calc_snotch (a);
    return a;
}

void SNOTCH::destroy_snotch (SNOTCH *a)
{
    delete (a);
}

void SNOTCH::flush_snotch (SNOTCH *a)
{
    a->x1 = a->x2 = a->y1 = a->y2 = 0.0;
}

void SNOTCH::xsnotch (SNOTCH *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i;
        for (i = 0; i < a->size; i++)
        {
            a->x0 = a->in[2 * i + 0];
            a->out[2 * i + 0] = a->a0 * a->x0 + a->a1 * a->x1 + a->a2 * a->x2 + a->b1 * a->y1 + a->b2 * a->y2;
            a->y2 = a->y1;
            a->y1 = a->out[2 * i + 0];
            a->x2 = a->x1;
            a->x1 = a->x0;
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    a->cs_update.unlock();
}

void SNOTCH::setBuffers_snotch (SNOTCH *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void SNOTCH::setSamplerate_snotch (SNOTCH *a, int rate)
{
    a->rate = rate;
    calc_snotch (a);
}

void SNOTCH::setSize_snotch (SNOTCH *a, int size)
{
    a->size = size;
    flush_snotch (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SNOTCH::SetSNCTCSSFreq (SNOTCH *a, double freq)
{
    a->cs_update.lock();
    a->f = freq;
    calc_snotch (a);
    a->cs_update.unlock();
}

void SNOTCH::SetSNCTCSSRun (SNOTCH *a, int run)
{
    a->cs_update.lock();
    a->run = run;
    a->cs_update.unlock();
}


/********************************************************************************************************
*                                                                                                       *
*                                           Complex Bi-Quad Peaking                                     *
*                                                                                                       *
********************************************************************************************************/

void SPEAK::calc_speak (SPEAK *a)
{
    double ratio;
    double f_corr, g_corr, bw_corr, bw_parm, A, f_min;

    switch (a->design)
    {
    case 0:
        ratio = a->bw / a->f;
        switch (a->nstages)
        {
        case 4:
            bw_parm = 2.4;
            f_corr  = 1.0 - 0.160 * ratio + 1.440 * ratio * ratio;
            g_corr = 1.0 - 1.003 * ratio + 3.990 * ratio * ratio;
            break;
        default:
            bw_parm = 1.0;
            f_corr  = 1.0;
            g_corr = 1.0;
            break;
        }
        {
            double fn, qk, qr, csn;
            a->fgain = a->gain / g_corr;
            fn = a->f / (double)a->rate / f_corr;
            csn = cos (TWOPI * fn);
            qr = 1.0 - 3.0 * a->bw / (double)a->rate * bw_parm;
            qk = (1.0 - 2.0 * qr * csn + qr * qr) / (2.0 * (1.0 - csn));
            a->a0 = 1.0 - qk;
            a->a1 = 2.0 * (qk - qr) * csn;
            a->a2 = qr * qr - qk;
            a->b1 = 2.0 * qr * csn;
            a->b2 = - qr * qr;
        }
        break;

    case 1:
        if (a->f < 200.0) a->f = 200.0;
        ratio = a->bw / a->f;
        switch (a->nstages)
        {
        case 4:
            bw_parm = 5.0;
            bw_corr = 1.13 * ratio - 0.956 * ratio * ratio;
            A = 2.5;
            f_min = 50.0;
            break;
        default:
            bw_parm = 1.0;
            bw_corr  = 1.0;
            g_corr = 1.0;
            A = 2.5;
            f_min = 50.0;
            break;
        }
        {
            double w0, sn, c, den;
            if (a->f < f_min) a->f = f_min;
            w0 = TWOPI * a->f / (double)a->rate;
            sn = sin (w0);
            a->cbw = bw_corr * a->f;
            c = sn * sinh(0.5 * log((a->f + 0.5 * a->cbw * bw_parm) / (a->f - 0.5 * a->cbw * bw_parm)) * w0 / sn);
            den = 1.0 + c / A;
            a->a0 = (1.0 + c * A) / den;
            a->a1 = - 2.0 * cos (w0) / den;
            a->a2 = (1 - c * A) / den;
            a->b1 = - a->a1;
            a->b2 = - (1 - c / A ) / den;
            a->fgain = a->gain / pow (A * A, (double)a->nstages);
        }
        break;
    }
    flush_speak (a);
}

SPEAK* SPEAK::create_speak (int run, int size, double* in, double* out, int rate, double f, double bw, double gain, int nstages, int design)
{
    SPEAK *a = new SPEAK;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f = f;
    a->bw = bw;
    a->gain = gain;
    a->nstages = nstages;
    a->design = design;
    a->x0 = new double[a->nstages * 2]; // (double *) malloc0 (a->nstages * sizeof (complex));
    a->x1 = new double[a->nstages * 2]; // (double *) malloc0 (a->nstages * sizeof (complex));
    a->x2 = new double[a->nstages * 2]; //(double *) malloc0 (a->nstages * sizeof (complex));
    a->y0 = new double[a->nstages * 2]; // (double *) malloc0 (a->nstages * sizeof (complex));
    a->y1 = new double[a->nstages * 2]; // (double *) malloc0 (a->nstages * sizeof (complex));
    a->y2 = new double[a->nstages * 2]; // (double *) malloc0 (a->nstages * sizeof (complex));
    calc_speak (a);
    return a;
}

void SPEAK::destroy_speak (SPEAK *a)
{
    delete[] (a->y2);
    delete[] (a->y1);
    delete[] (a->y0);
    delete[] (a->x2);
    delete[] (a->x1);
    delete[] (a->x0);
    delete (a);
}

void SPEAK::flush_speak (SPEAK *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[2 * i + 0] = a->x2[2 * i + 0] = a->y1[2 * i + 0] = a->y2[2 * i + 0] = 0.0;
        a->x1[2 * i + 1] = a->x2[2 * i + 1] = a->y1[2 * i + 1] = a->y2[2 * i + 1] = 0.0;
    }
}

void SPEAK::xspeak (SPEAK *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, j, n;
        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->fgain * a->in[2 * i + j];
                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0) a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];
                    a->y0[2 * n + j]    = a->a0 * a->x0[2 * n + j]
                                        + a->a1 * a->x1[2 * n + j]
                                        + a->a2 * a->x2[2 * n + j]
                                        + a->b1 * a->y1[2 * n + j]
                                        + a->b2 * a->y2[2 * n + j];
                    a->y2[2 * n + j] = a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
                    a->x2[2 * n + j] = a->x1[2 * n + j];
                    a->x1[2 * n + j] = a->x0[2 * n + j];
                }
                a->out[2 * i + j] = a->y0[2 * (a->nstages - 1) + j];
            }
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    a->cs_update.unlock();
}

void SPEAK::setBuffers_speak (SPEAK *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void SPEAK::setSamplerate_speak (SPEAK *a, int rate)
{
    a->rate = rate;
    calc_speak (a);
}

void SPEAK::setSize_speak (SPEAK *a, int size)
{
    a->size = size;
    flush_speak (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SPEAK::SetSPCWRun (RXA& rxa, int run)
{
    SPEAK *a = rxa.speak.p;
    a->cs_update.lock();
    a->run = run;
    a->cs_update.unlock();
}

void SPEAK::SetSPCWFreq (RXA& rxa, double freq)
{
    SPEAK *a = rxa.speak.p;
    a->cs_update.lock();
    a->f = freq;
    calc_speak (a);
    a->cs_update.unlock();
}

void SPEAK::SetSPCWBandwidth (RXA& rxa, double bw)
{
    SPEAK *a = rxa.speak.p;
    a->cs_update.lock();
    a->bw = bw;
    calc_speak (a);
    a->cs_update.unlock();
}

void SPEAK::SetSPCWGain (RXA& rxa, double gain)
{
    SPEAK *a = rxa.speak.p;
    a->cs_update.lock();
    a->gain = gain;
    calc_speak (a);
    a->cs_update.unlock();
}

/********************************************************************************************************
*                                                                                                       *
*                                       Complex Multiple Peaking                                        *
*                                                                                                       *
********************************************************************************************************/

void MPEAK::calc_mpeak (MPEAK *a)
{
    int i;
    a->tmp = new double[a->size * 2]; // (double *) malloc0 (a->size * sizeof (complex));
    a->mix = new double[a->size * 2]; // (double *) malloc0 (a->size * sizeof (complex));
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

MPEAK* MPEAK::create_mpeak (int run, int size, double* in, double* out, int rate, int npeaks, int* enable, double* f, double* bw, double* gain, int nstages)
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
    a->f    = new double[a->npeaks]; // (double *) malloc0 (a->npeaks * sizeof (double));
    a->bw   = new double[a->npeaks]; // (double *) malloc0 (a->npeaks * sizeof (double));
    a->gain = new double[a->npeaks]; // (double *) malloc0 (a->npeaks * sizeof (double));
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
    a->cs_update.lock();
    if (a->run)
    {
        int i, j;
        memset (a->mix, 0, a->size * sizeof (wcomplex));
        for (i = 0; i < a->npeaks; i++)
        {
            if (a->enable[i])
            {
                SPEAK::xspeak (a->pfil[i]);
                for (j = 0; j < 2 * a->size; j++)
                    a->mix[j] += a->tmp[j];
            }
        }
        memcpy (a->out, a->mix, a->size * sizeof (wcomplex));
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    a->cs_update.unlock();
}

void MPEAK::setBuffers_mpeak (MPEAK *a, double* in, double* out)
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
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->run = run;
    a->cs_update.unlock();
}

void MPEAK::SetmpeakNpeaks (RXA& rxa, int npeaks)
{
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->npeaks = npeaks;
    a->cs_update.unlock();
}

void MPEAK::SetmpeakFilEnable (RXA& rxa, int fil, int enable)
{
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->enable[fil] = enable;
    a->cs_update.unlock();
}

void MPEAK::SetmpeakFilFreq (RXA& rxa, int fil, double freq)
{
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->f[fil] = freq;
    a->pfil[fil]->f = freq;
    SPEAK::calc_speak(a->pfil[fil]);
    a->cs_update.unlock();
}

void MPEAK::SetmpeakFilBw (RXA& rxa, int fil, double bw)
{
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->bw[fil] = bw;
    a->pfil[fil]->bw = bw;
    SPEAK::calc_speak(a->pfil[fil]);
    a->cs_update.unlock();
}

void MPEAK::SetmpeakFilGain (RXA& rxa, int fil, double gain)
{
    MPEAK *a = rxa.mpeak.p;
    a->cs_update.lock();
    a->gain[fil] = gain;
    a->pfil[fil]->gain = gain;
    SPEAK::calc_speak(a->pfil[fil]);
    a->cs_update.unlock();
}


/********************************************************************************************************
*                                                                                                       *
*                                           Phase Rotator                                               *
*                                                                                                       *
********************************************************************************************************/

void PHROT::calc_phrot (PHROT *a)
{
    double g;
    a->x0 = new double[a->nstages]; // (double *) malloc0 (a->nstages * sizeof (double));
    a->x1 = new double[a->nstages]; // (double *) malloc0 (a->nstages * sizeof (double));
    a->y0 = new double[a->nstages]; // (double *) malloc0 (a->nstages * sizeof (double));
    a->y1 = new double[a->nstages]; // (double *) malloc0 (a->nstages * sizeof (double));
    g = tan (PI * a->fc / (double)a->rate);
    a->b0 = (g - 1.0) / (g + 1.0);
    a->b1 = 1.0;
    a->a1 = a->b0;
}

void PHROT::decalc_phrot (PHROT *a)
{
    delete[] (a->y1);
    delete[] (a->y0);
    delete[] (a->x1);
    delete[] (a->x0);
}

PHROT* PHROT::create_phrot (int run, int size, double* in, double* out, int rate, double fc, int nstages)
{
    PHROT *a = new PHROT;
    a->reverse = 0;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_phrot (a);
    return a;
}

void PHROT::destroy_phrot (PHROT *a)
{
    decalc_phrot (a);
    delete (a);
}

void PHROT::flush_phrot (PHROT *a)
{
    memset (a->x0, 0, a->nstages * sizeof (double));
    memset (a->x1, 0, a->nstages * sizeof (double));
    memset (a->y0, 0, a->nstages * sizeof (double));
    memset (a->y1, 0, a->nstages * sizeof (double));
}

void PHROT::xphrot (PHROT *a)
{
    a->cs_update.lock();
    if (a->reverse)
    {
        for (int i = 0; i < a->size; i++)
            a->in[2 * i + 0] = -a->in[2 * i + 0];
    }
    if (a->run)
    {
        int i, n;
        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->in[2 * i + 0];
            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0) a->x0[n] = a->y0[n - 1];
                a->y0[n]    = a->b0 * a->x0[n]
                            + a->b1 * a->x1[n]
                            - a->a1 * a->y1[n];
                a->y1[n] = a->y0[n];
                a->x1[n] = a->x0[n];
            }
            a->out[2 * i + 0] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
    a->cs_update.unlock();
}

void PHROT::setBuffers_phrot (PHROT *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void PHROT::setSamplerate_phrot (PHROT *a, int rate)
{
    decalc_phrot (a);
    a->rate = rate;
    calc_phrot (a);
}

void PHROT::setSize_phrot (PHROT *a, int size)
{
    a->size = size;
    flush_phrot (a);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void PHROT::SetPHROTRun (TXA& txa, int run)
{
    PHROT *a = txa.phrot.p;
    a->cs_update.lock();
    a->run = run;
    if (a->run) flush_phrot (a);
    a->cs_update.unlock();
}

void PHROT::SetPHROTCorner (TXA& txa, double corner)
{
    PHROT *a = txa.phrot.p;
    a->cs_update.lock();
    decalc_phrot (a);
    a->fc = corner;
    calc_phrot (a);
    a->cs_update.unlock();
}

void PHROT::SetPHROTNstages (TXA& txa, int nstages)
{
    PHROT *a = txa.phrot.p;
    a->cs_update.lock();
    decalc_phrot (a);
    a->nstages = nstages;
    calc_phrot (a);
    a->cs_update.unlock();
}

void PHROT::SetPHROTReverse (TXA& txa, int reverse)
{
    PHROT *a = txa.phrot.p;
    a->cs_update.lock();
    a->reverse = reverse;
    a->cs_update.unlock();
}

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Low-Pass                                            *
*                                                                                                       *
********************************************************************************************************/

void BQLP::calc_bqlp(BQLP *a)
{
    double w0, cs, c, den;
    w0 = TWOPI * a->fc / (double)a->rate;
    cs = cos(w0);
    c = sin(w0) / (2.0 * a->Q);
    den = 1.0 + c;
    a->a0 = 0.5 * (1.0 - cs) / den;
    a->a1 = (1.0 - cs) / den;
    a->a2 = 0.5 * (1.0 - cs) / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_bqlp(a);
}

BQLP* BQLP::create_bqlp(int run, int size, double* in, double* out, double rate, double fc, double Q, double gain, int nstages)
{
    BQLP *a = new BQLP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->Q = Q;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->x2 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y2 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    calc_bqlp(a);
    return a;
}

void BQLP::destroy_bqlp(BQLP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void BQLP::flush_bqlp(BQLP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[2 * i + 0] = a->x2[2 * i + 0] = a->y1[2 * i + 0] = a->y2[2 * i + 0] = 0.0;
        a->x1[2 * i + 1] = a->x2[2 * i + 1] = a->y1[2 * i + 1] = a->y2[2 * i + 1] = 0.0;
    }
}

void BQLP::xbqlp(BQLP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, j, n;
        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->gain * a->in[2 * i + j];
                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0) a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];
                    a->y0[2 * n + j] = a->a0 * a->x0[2 * n + j]
                        + a->a1 * a->x1[2 * n + j]
                        + a->a2 * a->x2[2 * n + j]
                        + a->b1 * a->y1[2 * n + j]
                        + a->b2 * a->y2[2 * n + j];
                    a->y2[2 * n + j] = a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
                    a->x2[2 * n + j] = a->x1[2 * n + j];
                    a->x1[2 * n + j] = a->x0[2 * n + j];
                }
                a->out[2 * i + j] = a->y0[2 * (a->nstages - 1) + j];
            }
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(wcomplex));
    a->cs_update.unlock();
}

void BQLP::setBuffers_bqlp(BQLP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void BQLP::setSamplerate_bqlp(BQLP *a, int rate)
{
    a->rate = rate;
    calc_bqlp(a);
}

void BQLP::setSize_bqlp(BQLP *a, int size)
{
    a->size = size;
    flush_bqlp(a);
}

/********************************************************************************************************
*                                                                                                       *
*                                      Double Bi-Quad Low-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

void DBQLP::calc_dbqlp(BQLP *a)
{
    double w0, cs, c, den;
    w0 = TWOPI * a->fc / (double)a->rate;
    cs = cos(w0);
    c = sin(w0) / (2.0 * a->Q);
    den = 1.0 + c;
    a->a0 = 0.5 * (1.0 - cs) / den;
    a->a1 = (1.0 - cs) / den;
    a->a2 = 0.5 * (1.0 - cs) / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_dbqlp(a);
}

BQLP* DBQLP::create_dbqlp(int run, int size, double* in, double* out, double rate, double fc, double Q, double gain, int nstages)
{
    BQLP *a = new BQLP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->Q = Q;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->x1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->x2 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y2 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    calc_dbqlp(a);
    return a;
}

void DBQLP::destroy_dbqlp(BQLP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void DBQLP::flush_dbqlp(BQLP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[i] = a->x2[i] = a->y1[i] = a->y2[i] = 0.0;
    }
}

void DBQLP::xdbqlp(BQLP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, n;
        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->gain * a->in[i];
            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0) a->x0[n] = a->y0[n - 1];
                a->y0[n] = a->a0 * a->x0[n]
                    + a->a1 * a->x1[n]
                    + a->a2 * a->x2[n]
                    + a->b1 * a->y1[n]
                    + a->b2 * a->y2[n];
                a->y2[n] = a->y1[n];
                a->y1[n] = a->y0[n];
                a->x2[n] = a->x1[n];
                a->x1[n] = a->x0[n];
            }
            a->out[i] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(double));
    a->cs_update.unlock();
}

void DBQLP::setBuffers_dbqlp(BQLP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void DBQLP::setSamplerate_dbqlp(BQLP *a, int rate)
{
    a->rate = rate;
    calc_dbqlp(a);
}

void DBQLP::setSize_dbqlp(BQLP *a, int size)
{
    a->size = size;
    flush_dbqlp(a);
}


/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Band-Pass                                           *
*                                                                                                       *
********************************************************************************************************/

void BQBP::calc_bqbp(BQBP *a)
{
    double f0, w0, bw, q, sn, cs, c, den;
    bw = a->f_high - a->f_low;
    f0 = (a->f_high + a->f_low) / 2.0;
    q = f0 / bw;
    w0 = TWOPI * f0 / a->rate;
    sn = sin(w0);
    cs = cos(w0);
    c = sn / (2.0 * q);
    den = 1.0 + c;
    a->a0 = +c / den;
    a->a1 = 0.0;
    a->a2 = -c / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_bqbp(a);
}

BQBP* BQBP::create_bqbp(int run, int size, double* in, double* out, double rate, double f_low, double f_high, double gain, int nstages)
{
    BQBP *a = new BQBP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f_low = f_low;
    a->f_high = f_high;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->x2 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y2 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    calc_bqbp(a);
    return a;
}

void BQBP::destroy_bqbp(BQBP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void BQBP::flush_bqbp(BQBP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[2 * i + 0] = a->x2[2 * i + 0] = a->y1[2 * i + 0] = a->y2[2 * i + 0] = 0.0;
        a->x1[2 * i + 1] = a->x2[2 * i + 1] = a->y1[2 * i + 1] = a->y2[2 * i + 1] = 0.0;
    }
}

void BQBP::xbqbp(BQBP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, j, n;
        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->gain * a->in[2 * i + j];
                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0) a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];
                    a->y0[2 * n + j] = a->a0 * a->x0[2 * n + j]
                        + a->a1 * a->x1[2 * n + j]
                        + a->a2 * a->x2[2 * n + j]
                        + a->b1 * a->y1[2 * n + j]
                        + a->b2 * a->y2[2 * n + j];
                    a->y2[2 * n + j] = a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
                    a->x2[2 * n + j] = a->x1[2 * n + j];
                    a->x1[2 * n + j] = a->x0[2 * n + j];
                }
                a->out[2 * i + j] = a->y0[2 * (a->nstages - 1) + j];
            }
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(wcomplex));
    a->cs_update.unlock();
}

void BQBP::setBuffers_bqbp(BQBP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void BQBP::setSamplerate_bqbp(BQBP *a, int rate)
{
    a->rate = rate;
    calc_bqbp(a);
}

void BQBP::setSize_bqbp(BQBP *a, int size)
{
    a->size = size;
    flush_bqbp(a);
}

/********************************************************************************************************
*                                                                                                       *
*                                     Double Bi-Quad Band-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

void BQBP::calc_dbqbp(BQBP *a)
{
    double f0, w0, bw, q, sn, cs, c, den;
    bw = a->f_high - a->f_low;
    f0 = (a->f_high + a->f_low) / 2.0;
    q = f0 / bw;
    w0 = TWOPI * f0 / a->rate;
    sn = sin(w0);
    cs = cos(w0);
    c = sn / (2.0 * q);
    den = 1.0 + c;
    a->a0 = +c / den;
    a->a1 = 0.0;
    a->a2 = -c / den;
    a->b1 = 2.0 * cs / den;
    a->b2 = (c - 1.0) / den;
    flush_dbqbp(a);
}

BQBP* BQBP::create_dbqbp(int run, int size, double* in, double* out, double rate, double f_low, double f_high, double gain, int nstages)
{
    BQBP *a = new BQBP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->f_low = f_low;
    a->f_high = f_high;
    a->gain = gain;
    a->nstages = nstages;
    a->x0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->x1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->x2 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y2 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    calc_dbqbp(a);
    return a;
}

void BQBP::destroy_dbqbp(BQBP *a)
{
    delete[](a->y2);
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x2);
    delete[](a->x1);
    delete[](a->x0);
    delete(a);
}

void BQBP::flush_dbqbp(BQBP *a)
{
    int i;
    for (i = 0; i < a->nstages; i++)
    {
        a->x1[i] = a->x2[i] = a->y1[i] = a->y2[i] = 0.0;
    }
}

void BQBP::xdbqbp(BQBP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, n;
        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->gain * a->in[i];
            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0) a->x0[n] = a->y0[n - 1];
                a->y0[n] = a->a0 * a->x0[n]
                    + a->a1 * a->x1[n]
                    + a->a2 * a->x2[n]
                    + a->b1 * a->y1[n]
                    + a->b2 * a->y2[n];
                a->y2[n] = a->y1[n];
                a->y1[n] = a->y0[n];
                a->x2[n] = a->x1[n];
                a->x1[n] = a->x0[n];
            }
            a->out[i] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(double));
    a->cs_update.unlock();
}

void BQBP::setBuffers_dbqbp(BQBP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void BQBP::setSamplerate_dbqbp(BQBP *a, int rate)
{
    a->rate = rate;
    calc_dbqbp(a);
}

void BQBP::setSize_dbqbp(BQBP *a, int size)
{
    a->size = size;
    flush_dbqbp(a);
}

/********************************************************************************************************
*                                                                                                       *
*                                    Complex Single-Pole High-Pass                                      *
*                                                                                                       *
********************************************************************************************************/

void SPHP::calc_sphp(SPHP *a)
{
    double g;
    a->x0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->x1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y0 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    a->y1 = new double[a->nstages * 2]; // (double*)malloc0(a->nstages * sizeof(complex));
    g = exp(-TWOPI * a->fc / a->rate);
    a->b0 = +0.5 * (1.0 + g);
    a->b1 = -0.5 * (1.0 + g);
    a->a1 = -g;
}

SPHP* SPHP::create_sphp(int run, int size, double* in, double* out, double rate, double fc, int nstages)
{
    SPHP *a = new SPHP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_sphp(a);
    return a;
}

void SPHP::decalc_sphp(SPHP *a)
{
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x1);
    delete[](a->x0);
}

void SPHP::destroy_sphp(SPHP *a)
{
    decalc_sphp(a);
    delete(a);
}

void SPHP::flush_sphp(SPHP *a)
{
    memset(a->x0, 0, a->nstages * sizeof(wcomplex));
    memset(a->x1, 0, a->nstages * sizeof(wcomplex));
    memset(a->y0, 0, a->nstages * sizeof(wcomplex));
    memset(a->y1, 0, a->nstages * sizeof(wcomplex));
}

void SPHP::xsphp(SPHP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, j, n;
        for (i = 0; i < a->size; i++)
        {
            for (j = 0; j < 2; j++)
            {
                a->x0[j] = a->in[2 * i + j];
                for (n = 0; n < a->nstages; n++)
                {
                    if (n > 0) a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];
                    a->y0[2 * n + j] = a->b0 * a->x0[2 * n + j]
                        + a->b1 * a->x1[2 * n + j]
                        - a->a1 * a->y1[2 * n + j];
                    a->y1[2 * n + j] = a->y0[2 * n + j];
                    a->x1[2 * n + j] = a->x0[2 * n + j];
                }
                a->out[2 * i + j] = a->y0[2 * (a->nstages - 1) + j];
            }
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(wcomplex));
    a->cs_update.unlock();
}

void SPHP::setBuffers_sphp(SPHP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void SPHP::setSamplerate_sphp(SPHP *a, int rate)
{
    decalc_sphp(a);
    a->rate = rate;
    calc_sphp(a);
}

void SPHP::setSize_sphp(SPHP *a, int size)
{
    a->size = size;
    flush_sphp(a);
}

/********************************************************************************************************
*                                                                                                       *
*                                    Double Single-Pole High-Pass                                       *
*                                                                                                       *
********************************************************************************************************/

void SPHP::calc_dsphp(SPHP *a)
{
    double g;
    a->x0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->x1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y0 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    a->y1 = new double[a->nstages]; // (double*)malloc0(a->nstages * sizeof(double));
    g = exp(-TWOPI * a->fc / a->rate);
    a->b0 = +0.5 * (1.0 + g);
    a->b1 = -0.5 * (1.0 + g);
    a->a1 = -g;
}

SPHP* SPHP::create_dsphp(int run, int size, double* in, double* out, double rate, double fc, int nstages)
{
    SPHP *a = new SPHP;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = rate;
    a->fc = fc;
    a->nstages = nstages;
    calc_dsphp(a);
    return a;
}

void SPHP::decalc_dsphp(SPHP *a)
{
    delete[](a->y1);
    delete[](a->y0);
    delete[](a->x1);
    delete[](a->x0);
}

void SPHP::destroy_dsphp(SPHP *a)
{
    decalc_dsphp(a);
    delete(a);
}

void SPHP::flush_dsphp(SPHP *a)
{
    memset(a->x0, 0, a->nstages * sizeof(double));
    memset(a->x1, 0, a->nstages * sizeof(double));
    memset(a->y0, 0, a->nstages * sizeof(double));
    memset(a->y1, 0, a->nstages * sizeof(double));
}

void SPHP::xdsphp(SPHP *a)
{
    a->cs_update.lock();
    if (a->run)
    {
        int i, n;
        for (i = 0; i < a->size; i++)
        {
            a->x0[0] = a->in[i];
            for (n = 0; n < a->nstages; n++)
            {
                if (n > 0) a->x0[n] = a->y0[n - 1];
                a->y0[n] = a->b0 * a->x0[n]
                    + a->b1 * a->x1[n]
                    - a->a1 * a->y1[n];
                a->y1[n] = a->y0[n];
                a->x1[n] = a->x0[n];
            }
            a->out[i] = a->y0[a->nstages - 1];
        }
    }
    else if (a->out != a->in)
        memcpy(a->out, a->in, a->size * sizeof(double));
    a->cs_update.unlock();
}

void SPHP::setBuffers_dsphp(SPHP *a, double* in, double* out)
{
    a->in = in;
    a->out = out;
}

void SPHP::setSamplerate_dsphp(SPHP *a, int rate)
{
    decalc_dsphp(a);
    a->rate = rate;
    calc_dsphp(a);
}

void SPHP::setSize_dsphp(SPHP *a, int size)
{
    a->size = size;
    flush_dsphp(a);
}

} // namespace WDSP
