/*  ammod.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2017 Warren Pratt, NR0V
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

#include <cmath>

#include "ammod.hpp"
#include "comm.hpp"
#include "TXA.hpp"

namespace WDSP {

AMMOD::AMMOD(
    int _run,
    int _mode,
    int _size,
    float* _in,
    float* _out,
    double _c_level
)
{
    run = _run;
    mode = _mode;
    size = _size;
    in = _in;
    out = _out;
    c_level = _c_level;
    a_level = 1.0 - c_level;
    mult = 1.0 / sqrt (2.0);
}

void AMMOD::flush()
{
    // Nothing to flush
}

void AMMOD::execute()
{
    if (run)
    {
        int i;
        switch (mode)
        {
        case 0: // AM
            for (i = 0; i < size; i++)
                out[2 * i + 0] = out[2 * i + 1] = (float) (mult * (c_level + a_level * in[2 * i + 0]));
            break;
        case 1: // DSB
            for (i = 0; i < size; i++)
                out[2 * i + 0] = out[2 * i + 1] = (float) (mult * in[2 * i + 0]);
            break;
        case 2: // SSB w/Carrier
            for (i = 0; i < size; i++)
            {
                out[2 * i + 0] = (float) (mult * c_level + a_level * in[2 * i + 0]);
                out[2 * i + 1] = (float) (mult * c_level + a_level * in[2 * i + 1]);
            }
            break;
        default:
            break;
        }
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void AMMOD::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void AMMOD::setSamplerate(int)
{
    // Nothing to do
}

void AMMOD::setSize(int _size)
{
    size = _size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void AMMOD::setAMCarrierLevel(double _c_level)
{
    c_level = _c_level;
    a_level = 1.0 - _c_level;
}

} // namespace WDSP
