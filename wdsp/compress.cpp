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

COMPRESSOR* COMPRESSOR::create_compressor (
                int run,
                int buffsize,
                float* inbuff,
                float* outbuff,
                float gain )
{
    COMPRESSOR *a = new COMPRESSOR;
    a->run = run;
    a->inbuff = inbuff;
    a->outbuff = outbuff;
    a->buffsize = buffsize;
    a->gain = gain;
    return a;
}

void COMPRESSOR::destroy_compressor (COMPRESSOR *a)
{
    delete (a);
}

void COMPRESSOR::flush_compressor (COMPRESSOR *)
{
}

void COMPRESSOR::xcompressor (COMPRESSOR *a)
{
    int i;
    float mag;
    if (a->run)
        for (i = 0; i < a->buffsize; i++)
        {
            mag = sqrt(a->inbuff[2 * i + 0] * a->inbuff[2 * i + 0] + a->inbuff[2 * i + 1] * a->inbuff[2 * i + 1]);
            if (a->gain * mag > 1.0)
                a->outbuff[2 * i + 0] = a->inbuff[2 * i + 0] / mag;
            else
                a->outbuff[2 * i + 0] = a->inbuff[2 * i + 0] * a->gain;
            a->outbuff[2 * i + 1] = 0.0;
        }
    else if (a->inbuff != a->outbuff)
        std::copy(a->inbuff, a->inbuff + a->buffsize * 2, a->outbuff);
}

void COMPRESSOR::setBuffers_compressor (COMPRESSOR *a, float* in, float* out)
{
    a->inbuff = in;
    a->outbuff = out;
}

void COMPRESSOR::setSamplerate_compressor (COMPRESSOR *, int)
{
}

void COMPRESSOR::setSize_compressor (COMPRESSOR *a, int size)
{
    a->buffsize = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void COMPRESSOR::SetCompressorRun (TXA& txa, int run)
{
    if (txa.compressor.p->run != run)
    {
        txa.compressor.p->run = run;
        TXA::SetupBPFilters (txa);
    }
}

void COMPRESSOR::SetCompressorGain (TXA& txa, float gain)
{
    txa.compressor.p->gain = pow (10.0, gain / 20.0);
}

} // namespace WDSP
