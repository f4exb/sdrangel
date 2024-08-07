/*  send.h

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

// 'send' copies samples from the sample stream and proactively pushes those samples to external
//  functions by calling them.  As such, the call and parameters are somewhat specific to the
//  need for the data.  Nevertheless, this code is written in somewhat of a generic form to
//  facilitate adding additional functions in the future.

#ifndef wdsp_sender_h
#define wdsp_sender_h

#include "export.h"

namespace WDSP {

class BufferProbe;

class WDSP_API SENDER
{
public:
    int run;            // 0 = send OFF, 1 = send ON
    int flag;           // secondary 'run'; AND'd with 'run'
    int mode;           // selects the specific processing and function call
    int size;           // size of the data buffer (complex samples)
    float* in;         // buffer from which to take the data
    BufferProbe *spectrumProbe; // this is the data handler actually

    SENDER(int run, int flag, int mode, int size, float* in);
    ~SENDER() = default;

    void flush();
    void execute();
    void setBuffers(float* in);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void SetSpectrum(int flag, BufferProbe *spectrumProbe);
};

} // namespace WDSP

#endif
