/*  firmin.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2016 Warren Pratt, NR0V
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

/********************************************************************************************************
*                                                                                                       *
*                                           Time-Domain FIR                                             *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_firmin_h
#define wdsp_firmin_h

#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API FIRMIN
{
public:
    int run;                // run control
    int position;           // position at which to execute
    int size;               // input/output buffer size, power of two
    float* in;             // input buffer
    float* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two
    float f_low;           // low cutoff frequency
    float f_high;          // high cutoff frequency
    std::vector<float> ring;           // internal complex ring buffer
    std::vector<float> h;              // complex filter coefficients
    int rsize;              // ring size, number of complex samples, power of two
    int mask;               // mask to update indexes
    int idx;                // ring input/output index
    float samplerate;      // sample rate
    int wintype;            // filter window type
    float gain;            // filter gain

    FIRMIN(
        int run,
        int position,
        int size,
        float* in,
        float* out,
        int nc,
        float f_low,
        float f_high,
        int samplerate,
        int wintype,
        float gain
    );
    FIRMIN(const FIRMIN&) = delete;
    FIRMIN& operator=(const FIRMIN& other) = delete;
    ~FIRMIN() = default;

    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setFreqs(float f_low, float f_high);

private:
    void calc();
};

} // namespace WDSP

#endif
