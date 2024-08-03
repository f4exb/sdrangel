/*  unit.hpp

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

#ifndef wdsp_unit_h
#define wdsp_unit_h

#include "export.h"

namespace WDSP {

class WDSP_API Unit
{
public:
    int in_rate;                // input samplerate
    int out_rate;               // output samplerate
    int dsp_rate;               // sample rate for mainstream dsp processing
    int dsp_size;               // number complex samples processed per buffer in mainstream dsp processing
    int dsp_insize;             // size (complex samples) of the input buffer
    int dsp_outsize;            // size (complex samples) of the output buffer
    int state;                  // 0 for unit OFF; 1 for unit ON

    Unit(
        int _in_rate,                // input samplerate
        int _out_rate,               // output samplerate
        int _dsp_rate,               // sample rate for mainstream dsp processing
        int _dsp_size                // number complex samples processed per buffer in mainstream dsp processing
    );
    ~Unit();

    void flushBuffers();
    void setBuffersInputSamplerate(int _in_rate);
    void setBuffersOutputSamplerate(int _out_rate);
    void setBuffersDSPSamplerate(int _dsp_rate);
    void setBuffersDSPBuffsize(int _dsp_size);

protected:
    float* inbuff;
    float* midbuff;
    float* outbuff;
};

} // namespace WDSP

#endif
