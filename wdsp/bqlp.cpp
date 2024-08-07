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
#include "bqlp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                   Complex Bi-Quad Low-Pass                                            *
*                                                                                                       *
********************************************************************************************************/

void BQLP::calc()
{
    double w0;
    double cs;
    double c;
    double den;

    w0 = TWOPI * fc / rate;
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

BQLP::BQLP(
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
    x0.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    x1.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    x2.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    y0.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    y1.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    y2.resize(nstages * 2); // (float*)malloc0(nstages * sizeof(complex));
    calc();
}

void BQLP::flush()
{
    for (int i = 0; i < nstages; i++)
    {
        x1[2 * i + 0] = x2[2 * i + 0] = y1[2 * i + 0] = y2[2 * i + 0] = 0.0;
        x1[2 * i + 1] = x2[2 * i + 1] = y1[2 * i + 1] = y2[2 * i + 1] = 0.0;
    }
}

void BQLP::execute()
{
    if (run)
    {

        for (int i = 0; i < size; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                x0[j] = gain * in[2 * i + j];

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
        std::copy(in, in + size * 2, out);
    }
}

void BQLP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void BQLP::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void BQLP::setSize(int _size)
{
    size = _size;
    flush();
}

} // namespace WDSP
