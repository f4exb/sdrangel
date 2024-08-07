/*  compress.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2011, 2013, 2017 Warren Pratt, NR0V
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

This software is based upon the algorithm described by Peter Martinez, G3PLX,
in the January 2010 issue of RadCom magazine.

*/

#include "comm.hpp"
#include "compress.hpp"
#include "TXA.hpp"

namespace WDSP {

COMPRESSOR::COMPRESSOR(
    int _run,
    int _buffsize,
    float* _inbuff,
    float* _outbuff,
    double _gain
) :
    run(_run),
    buffsize(_buffsize),
    inbuff(_inbuff),
    outbuff(_outbuff),
    gain(_gain)
{}

void COMPRESSOR::flush()
{
    // Nothing to do
}

void COMPRESSOR::execute()
{
    double mag;
    if (run)
        for (int i = 0; i < buffsize; i++)
        {
            mag = sqrt(inbuff[2 * i + 0] * inbuff[2 * i + 0] + inbuff[2 * i + 1] * inbuff[2 * i + 1]);
            if (gain * mag > 1.0)
                outbuff[2 * i + 0] = (float) (inbuff[2 * i + 0] / mag);
            else
                outbuff[2 * i + 0] = (float) (inbuff[2 * i + 0] * gain);
            outbuff[2 * i + 1] = 0.0;
        }
    else if (inbuff != outbuff)
        std::copy(inbuff, inbuff + buffsize * 2, outbuff);
}

void COMPRESSOR::setBuffers(float* _in, float* _out)
{
    inbuff = _in;
    outbuff = _out;
}

void COMPRESSOR::setSamplerate(int)
{
    // Nothing to do
}

void COMPRESSOR::setSize(int _size)
{
    buffsize = _size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void COMPRESSOR::setGain(float _gain)
{
    gain = pow (10.0, _gain / 20.0);
}

} // namespace WDSP
