/*  dbqlp.c

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
#include "dbqlp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                      Double Bi-Quad Low-Pass                                          *
*                                                                                                       *
********************************************************************************************************/

void DBQLP::calc()
{
    float w0, cs, c, den;

    w0 = TWOPI * fc / (float)rate;
    cs = cos(w0);
    c = sin(w0) / (2.0 * Q);
    den = 1.0 + c;
    a0 = 0.5 * (1.0 - cs) / den;
    a1 = (1.0 - cs) / den;
    a2 = 0.5 * (1.0 - cs) / den;
    b1 = 2.0 * cs / den;
    b2 = (c - 1.0) / den;
    flush();
}

DBQLP::DBQLP(
    int _run,
    int _size,
    float* _in,
    float* _out,
    double _rate,
    double _fc,
    double _Q,
    double _gain,
    int _nstages
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    fc(_fc),
    Q(_Q),
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

void DBQLP::flush()
{
    for (int i = 0; i < nstages; i++)
    {
        x1[i] = x2[i] = y1[i] = y2[i] = 0.0;
    }
}

void DBQLP::execute()
{
    if (run)
    {
        int i, n;

        for (i = 0; i < size; i++)
        {
            x0[0] = gain * in[i];

            for (n = 0; n < nstages; n++)
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

            out[i] = y0[nstages - 1];
        }
    }
    else if (out != in)
    {
        std::copy(in, in + size, out);
    }
}

void DBQLP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void DBQLP::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void DBQLP::setSize(int _size)
{
    size = _size;
    flush();
}

} // namespace WDSP
