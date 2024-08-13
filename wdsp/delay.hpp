/*  delay.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2019 Warren Pratt, NR0V
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

#ifndef wdsp_delay_h
#define wdsp_delay_h

#include <vector>

#include "export.h"

#define WSDEL   1025    // number of supported whole sample delays

namespace WDSP {

class WDSP_API DELAY
{
public:
    int run;            // run
    int size;           // number of input samples per buffer
    float* in;         // input buffer
    float* out;        // output buffer
    int rate;           // samplerate
    float tdelta;      // delay increment required (seconds)
    float tdelay;      // delay requested (seconds)

    int L;              // interpolation factor
    int ncoef;          // number of coefficients
    int cpp;            // coefficients per phase
    float ft;          // normalized cutoff frequency
    std::vector<float> h;          // coefficients
    int snum;           // starting sample number (0 for sub-sample delay)
    int phnum;          // phase number

    int idx_in;         // index for input into ring
    int rsize;          // ring size in complex samples
    std::vector<float> ring;       // ring buffer

    float adelta;      // actual delay increment
    float adelay;      // actual delay

    DELAY(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        float tdelta,
        float tdelay
    );
    DELAY(const DELAY&) = delete;
    DELAY& operator=(DELAY& other) = delete;
    ~DELAY() = default;

    void flush();
    void execute();
    // Properties
    void setRun(int run);
    float setValue(float delay);        // returns actual delay in seconds
    void setBuffs(int size, float* in, float* out);
};

} // namespace WDSP

#endif
