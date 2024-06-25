/*  compress.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2011, 2013 Warren Pratt, NR0V
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

#ifndef wdsp_compressor_h
#define wdsp_compressor_h

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API COMPRESSOR
{
public:
    int run;
    int buffsize;
    float *inbuff;
    float *outbuff;
    float gain;

    static COMPRESSOR* create_compressor (
        int run,
        int buffsize,
        float* inbuff,
        float* outbuff,
        float gain
    );
    static void destroy_compressor (COMPRESSOR *a);
    static void flush_compressor (COMPRESSOR *a);
    static void xcompressor (COMPRESSOR *a);
    static void setBuffers_compressor (COMPRESSOR *a, float* in, float* out);
    static void setSamplerate_compressor (COMPRESSOR *a, int rate);
    static void setSize_compressor (COMPRESSOR *a, int size);
    // TXA Properties
    static void SetCompressorRun (TXA& txa, int run);
    static void SetCompressorGain (TXA& txa, float gain);
};

} // namespace WDSP

#endif
