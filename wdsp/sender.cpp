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
#include "bufferprobe.hpp"

namespace WDSP {

SENDER::SENDER(int _run, int _flag, int _mode, int _size, float* _in) :
    run(_run),
    flag(_flag),
    mode(_mode),
    size(_size),
    in(_in),
    spectrumProbe(nullptr)
{
}

void SENDER::flush()
{
    // There is no internal data to be reset
}

void SENDER::execute()
{
    if (run && flag && (mode == 0) && spectrumProbe) {
        spectrumProbe->proceed(in, size);
    }
}

void SENDER::setBuffers(float* _in)
{
    in = _in;
}

void SENDER::setSamplerate(int)
{
    flush();
}

void SENDER::setSize(int _size)
{
    size = _size;
}

/********************************************************************************************************
*                                                                                                       *
*                                           Public Properties                                           *
*                                                                                                       *
********************************************************************************************************/

void SENDER::SetSpectrum(int _flag, BufferProbe *_spectrumProbe)
{
    flag = _flag;
    spectrumProbe = _spectrumProbe;
}

} // namespace WDSP
