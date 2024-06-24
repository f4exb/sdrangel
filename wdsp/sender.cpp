/*  sender.c

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
#include "sender.hpp"
#include "RXA.hpp"
#include "bufferprobe.hpp"

namespace WDSP {

SENDER* SENDER::create_sender (int run, int flag, int mode, int size, double* in)
{
    SENDER *a = new SENDER;
    a->run = run;
    a->flag = flag;
    a->mode = mode;
    a->size = size;
    a->in = in;
    a->spectrumProbe = nullptr;
    return a;
}

void SENDER::destroy_sender (SENDER *a)
{
    delete (a);
}

void SENDER::flush_sender (SENDER *)
{
}

void SENDER::xsender (SENDER *a)
{
    if (a->run && a->flag)
    {
        switch (a->mode)
        {
        case 0:
            {
                if (a->spectrumProbe) {
                    a->spectrumProbe->proceed(a->in, a->size);
                }
                break;
            }
        }
    }
}

void SENDER::setBuffers_sender (SENDER *a, double* in)
{
    a->in = in;
}

void SENDER::setSamplerate_sender (SENDER *a, int)
{
    flush_sender (a);
}

void SENDER::setSize_sender (SENDER *a, int size)
{
    a->size = size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void SENDER::SetSpectrum (RXA& rxa, int flag, BufferProbe *spectrumProbe)
{
    SENDER *a;
    rxa.csDSP.lock();
    a = rxa.sender.p;
    a->flag = flag;
    a->spectrumProbe = spectrumProbe;
    rxa.csDSP.unlock();
}

} // namespace WDSP
