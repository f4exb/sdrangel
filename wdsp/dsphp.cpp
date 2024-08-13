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
#include "dsphp.hpp"

namespace WDSP {

/********************************************************************************************************
*                                                                                                       *
*                                    Double Single-Pole High-Pass                                       *
*                                                                                                       *
********************************************************************************************************/

void DSPHP::calc()
{
    double g;
    x0.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    x1.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    y0.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    y1.resize(nstages); // (float*)malloc0(nstages * sizeof(float));
    g = exp(-TWOPI * fc / rate);
    b0 = +0.5 * (1.0 + g);
    b1 = -0.5 * (1.0 + g);
    a1 = -g;
}

DSPHP::DSPHP(
    int _run,
    int _size,
    float* _in,
    float* _out,
    double _rate,
    double _fc,
    int _nstages
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate(_rate),
    fc(_fc),
    nstages(_nstages)
{
    calc();
}

void DSPHP::flush()
{
    std::fill(x0.begin(), x0.end(), 0);
    std::fill(x1.begin(), x1.end(), 0);
    std::fill(y0.begin(), y0.end(), 0);
    std::fill(y1.begin(), y1.end(), 0);
}

void DSPHP::execute()
{
    if (run)
    {
        for (int i = 0; i < size; i++)
        {
            x0[0] = in[i];

            for (int n = 0; n < nstages; n++)
            {
                if (n > 0)
                    x0[n] = y0[n - 1];

                y0[n] = b0 * x0[n]
                    + b1 * x1[n]
                    - a1 * y1[n];
                y1[n] = y0[n];
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

void DSPHP::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void DSPHP::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void DSPHP::setSize(int _size)
{
    size = _size;
}

} // namespace WDSP
