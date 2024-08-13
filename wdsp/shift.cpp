/*  shift.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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
#include "shift.hpp"

namespace WDSP {

void SHIFT::calc()
{
    delta = TWOPI * shift / rate;
    cos_delta = cos (delta);
    sin_delta = sin (delta);
}

SHIFT::SHIFT (
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _fshift
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate((double) _rate),
    shift(_fshift),
    phase(0.0)
{
    calc();
}

void SHIFT::flush()
{
    phase = 0.0;
}

void SHIFT::execute()
{
    if (run)
    {
        double I1;
        double Q1;
        double t1;
        double t2;
        double cos_phase = cos (phase);
        double sin_phase = sin (phase);

        for (int i = 0; i < size; i++)
        {
            I1 = in[2 * i + 0];
            Q1 = in[2 * i + 1];
            out[2 * i + 0] = (float) (I1 * cos_phase - Q1 * sin_phase);
            out[2 * i + 1] = (float) (I1 * sin_phase + Q1 * cos_phase);
            t1 = cos_phase;
            t2 = sin_phase;
            cos_phase = t1 * cos_delta - t2 * sin_delta;
            sin_phase = t1 * sin_delta + t2 * cos_delta;
            phase += delta;

            if (phase >= TWOPI)
                phase -= TWOPI;

            if (phase <   0.0 )
                phase += TWOPI;
        }
    }
    else if (in != out)
    {
        std::copy( in,  in + size * 2, out);
    }
}

void SHIFT::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void SHIFT::setSamplerate(int _rate)
{
    rate = _rate;
    phase = 0.0;
    calc();
}

void SHIFT::setSize(int _size)
{
    size = _size;
    flush();
}

/********************************************************************************************************
*                                                                                                       *
*                                           Non static                                                  *
*                                                                                                       *
********************************************************************************************************/

void SHIFT::SetRun (int _run)
{
    run = _run;
}

void SHIFT::SetFreq(double _fshift)
{
    shift = _fshift;
    calc();
}

} // namespace WDSP
