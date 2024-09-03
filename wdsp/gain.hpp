/*  gain.h

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

#ifndef wdsp_gain_h
#define wdsp_gain_h

#include "export.h"

namespace WDSP {

class WDSP_API GAIN
{
public:
    int run;
    int* prun;
    int size;
    float* in;
    float* out;
    float Igain;
    float Qgain;

    static GAIN* create_gain (int run, int* prun, int size, float* in, float* out, float Igain, float Qgain);
    static void destroy_gain (GAIN *a);
    static void flush_gain (GAIN *a);
    static void xgain (GAIN *a);
    static void setBuffers_gain (GAIN *a, float* in, float* out);
    static void setSamplerate_gain (GAIN *a, int rate);
    static void setSize_gain (GAIN *a, int size);
    // TXA Properties
    // POINTER-BASED Properties
    static void pSetTXOutputLevel (GAIN *a, float level);
    static void pSetTXOutputLevelRun (GAIN *a, int run);
    static void pSetTXOutputLevelSize (GAIN *a, int size);
};

} // namespace WDSP

#endif
