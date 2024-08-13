/*  dbqbp.c

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
#include "dbqbp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                     Double Bi-Quad Band-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

void DBQBP::calc()
{
    double f0;
    double w0;
    double bw;
    double q;
    double sn;
    double cs;
    double c;
    double den;

    bw = f_high - f_low;
    f0 = (f_high + f_low) / 2.0;
    q = f0 / bw;
    w0 = TWOPI * f0 / rate;
    sn = sin(w0);
    cs = cos(w0);
    c = sn / (2.0 * q);
    den = 1.0 + c;
    a0 = +c / den;
    a1 = 0.0;
    a2 = -c / den;
    b1 = 2.0 * cs / den;
    b2 = (c - 1.0) / den;
    flush();
}

DBQBP::DBQBP(
    int _run,
    int _size,
    float* _in,
    float* _out,
    double _rate,
    double _f_low,
    double _f_high,
    double _gain,
    int _nstages
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    f_low(_f_low),
    f_high(_f_high),
    gain(_gain),
    nstages(_nstages)
{
    x0.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    x1.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    x2.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    y0.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    y1.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    y2.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    calc();
}

void DBQBP::flush()
{
    for (int i = 0; i < nstages; i++)
    {
        x1[i] = x2[i] = y1[i] = y2[i] = 0.0;
    }
}

void DBQBP::execute()
{
    if (run)
    {

        for (int i = 0; i < size; i++)
        {
            x0[0] = gain * in[i];

            for (int n = 0; n < nstages; n++)
            {
                if (n > 0)
                    x0[n] = y0[n - 1];

                y0[n] = a0 * x0[n]
                    + a1 * x1[n]
                    + a2 * x2[n]
                    + b1 * y1[n]
                    + b2 * y2[n];
                y2[n] = y1[n];
                y1[n] = y0[n];
                x2[n] = x1[n];
                x1[n] = x0[n];
            }

            out[i] = (float) y0[nstages - 1];
        }
    }
    else if (out != in)
    {
        std::copy(in, in + size, out);
    }
}

void DBQBP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void DBQBP::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void DBQBP::setSize(int _size)
{
    size = _size;
    flush();
}

} // namespace WDSP
