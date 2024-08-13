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

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                           Complex Bi-Quad Peaking                                     *
*                                                                                                       *
********************************************************************************************************/

void SPEAK::calc()
{
    double ratio;
    double f_corr;
    double g_corr;
    double bw_corr;
    double bw_parm;
    double A;
    double f_min;

    switch (design)
    {
    case 0:
        ratio = bw / f;
        if (nstages == 4)
        {
            bw_parm = 2.4;
            f_corr  = 1.0 - 0.160 * ratio + 1.440 * ratio * ratio;
            g_corr = 1.0 - 1.003 * ratio + 3.990 * ratio * ratio;
        }
        else
        {
            bw_parm = 1.0;
            f_corr  = 1.0;
            g_corr = 1.0;
        }
        {
            double fn;
            double qk;
            double qr;
            double csn;
            fgain = gain / g_corr;
            fn = f / rate / f_corr;
            csn = cos (TWOPI * fn);
            qr = 1.0 - 3.0 * bw / rate * bw_parm;
            qk = (1.0 - 2.0 * qr * csn + qr * qr) / (2.0 * (1.0 - csn));
            a0 = 1.0 - qk;
            a1 = 2.0 * (qk - qr) * csn;
            a2 = qr * qr - qk;
            b1 = 2.0 * qr * csn;
            b2 = - qr * qr;
        }
        break;

    case 1:
        if (f < 200.0) f = 200.0;
        ratio = bw / f;
        if (nstages == 4)
        {
            bw_parm = 5.0;
            bw_corr = 1.13 * ratio - 0.956 * ratio * ratio;
            A = 2.5;
            f_min = 50.0;
        }
        else
        {
            bw_parm = 1.0;
            bw_corr  = 1.0;
            A = 2.5;
            f_min = 50.0;
        }
        {
            double w0;
            double sn;
            double c;
            double den;
            if (f < f_min) f = f_min;
            w0 = TWOPI * f / rate;
            sn = sin (w0);
            cbw = bw_corr * f;
            c = sn * sinh(0.5 * log((f + 0.5 * cbw * bw_parm) / (f - 0.5 * cbw * bw_parm)) * w0 / sn);
            den = 1.0 + c / A;
            a0 = (1.0 + c * A) / den;
            a1 = - 2.0 * cos (w0) / den;
            a2 = (1 - c * A) / den;
            b1 = - a1;
            b2 = - (1 - c / A ) / den;
            fgain = gain / pow (A * A, (double)nstages);
        }
        break;
    default:
        break;
    }
    flush();
}

SPEAK::SPEAK(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _f,
    double _bw,
    double _gain,
    int _nstages,
    int _design
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    f(_f),
    bw(_bw),
    gain(_gain),
    nstages(_nstages),
    design(_design)
{
    x0.resize(nstages * 2);
    x1.resize(nstages * 2);
    x2.resize(nstages * 2);
    y0.resize(nstages * 2);
    y1.resize(nstages * 2);
    y2.resize(nstages * 2);
    calc();
}

void SPEAK::flush()
{
    for (int i = 0; i < nstages; i++)
    {
        x1[2 * i + 0] = x2[2 * i + 0] = y1[2 * i + 0] = y2[2 * i + 0] = 0.0;
        x1[2 * i + 1] = x2[2 * i + 1] = y1[2 * i + 1] = y2[2 * i + 1] = 0.0;
    }
}

void SPEAK::execute()
{
    if (run)
    {
        for (int i = 0; i < size; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                x0[j] = fgain * in[2 * i + j];

                for (int n = 0; n < nstages; n++)
                {
                    if (n > 0)
                        x0[2 * n + j] = y0[2 * (n - 1) + j];
                    y0[2 * n + j] = a0 * x0[2 * n + j]
                        + a1 * x1[2 * n + j]
                        + a2 * x2[2 * n + j]
                        + b1 * y1[2 * n + j]
                        + b2 * y2[2 * n + j];
                    y2[2 * n + j] = y1[2 * n + j];
                    y1[2 * n + j] = y0[2 * n + j];
                    x2[2 * n + j] = x1[2 * n + j];
                    x1[2 * n + j] = x0[2 * n + j];
                }

                out[2 * i + j] = (float) y0[2 * (nstages - 1) + j];
            }
        }
    }
    else if (out != in)
    {
        std::copy( in,  in + size * 2, out);
    }
}

void SPEAK::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void SPEAK::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void SPEAK::setSize(int _size)
{
    size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SPEAK::setRun(int _run)
{
    run = _run;
}

void SPEAK::setFreq(double _freq)
{
    f = _freq;
    calc();
}

void SPEAK::setBandwidth(double _bw)
{
    bw = _bw;
    calc();
}

void SPEAK::setGain(double _gain)
{
    gain = _gain;
    calc();
}

} // namespace WDSP
