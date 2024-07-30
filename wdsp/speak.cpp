/*  speak.cpp

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
#include "speak.hpp"
#include "RXA.hpp"
#include "TXA.hpp"

namespace WDSP {

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

SPEAK* SPEAK::create_speak (
    int run,
    int size,
    float* in,
    float* out,
    int rate,
    double f,
    double bw,
    double gain,
    int nstages,
    int design
)
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
    a->x0 = new double[a->nstages * 2]; // (float *) malloc0 (a->nstages * sizeof (complex));
    a->x1 = new double[a->nstages * 2]; // (float *) malloc0 (a->nstages * sizeof (complex));
    a->x2 = new double[a->nstages * 2]; //(float *) malloc0 (a->nstages * sizeof (complex));
    a->y0 = new double[a->nstages * 2]; // (float *) malloc0 (a->nstages * sizeof (complex));
    a->y1 = new double[a->nstages * 2]; // (float *) malloc0 (a->nstages * sizeof (complex));
    a->y2 = new double[a->nstages * 2]; // (float *) malloc0 (a->nstages * sizeof (complex));
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
                    if (n > 0)
                        a->x0[2 * n + j] = a->y0[2 * (n - 1) + j];
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
    {
        std::copy( a->in,  a->in + a->size * 2, a->out);
    }
}

void SPEAK::setBuffers_speak (SPEAK *a, float* in, float* out)
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
    SPEAK *a = rxa.speak;
    a->run = run;
}

void SPEAK::SetSPCWFreq (RXA& rxa, double freq)
{
    SPEAK *a = rxa.speak;
    a->f = freq;
    calc_speak (a);
}

void SPEAK::SetSPCWBandwidth (RXA& rxa, double bw)
{
    SPEAK *a = rxa.speak;
    a->bw = bw;
    calc_speak (a);
}

void SPEAK::SetSPCWGain (RXA& rxa, double gain)
{
    SPEAK *a = rxa.speak;
    a->gain = gain;
    calc_speak (a);
}

} // namespace WDSP
