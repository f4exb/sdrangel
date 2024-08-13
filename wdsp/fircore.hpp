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
*                                   Partitioned Overlap-Save Filter Kernel                              *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_fircore_h
#define wdsp_fircore_h

#include <array>
#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API FIRCORE
{
public:
    int size;               // input/output buffer size, power of two
    float* in;             // input buffer
    float* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two, >= size
    std::vector<float> impulse;        // impulse response of filter
    std::vector<float> imp;
    int nfor;               // number of buffers in delay line
    std::vector<float> fftin;          // fft input buffer
    std::array<std::vector<std::vector<float>>, 2> fmask;        // frequency domain masks
    std::vector<std::vector<float>> fftout;        // fftout delay line
    std::vector<float> accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    std::vector<float> maskgen;        // input for mask generation FFT
    std::vector<fftwf_plan> pcfor;       // array of forward FFT plans
    fftwf_plan crev;         // reverse fft plan
    std::array<std::vector<fftwf_plan>, 2> maskplan;   // plans for frequency domain masks
    int cset;
    int mp;
    int masks_ready;

    FIRCORE(
        int size,
        float* in,
        float* out,
        int mp,
        const std::vector<float>& impulse
    );
    FIRCORE(const FIRCORE&) = delete;
    FIRCORE& operator=(const FIRCORE& other) = delete;
    ~FIRCORE();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSize(int size);
    void setImpulse(const std::vector<float>& impulse, int update);
    void setNc(const std::vector<float>& impulse);
    void setMp(int mp);
    void setUpdate();

private:
    void plan();
    void calc(int flip);
    void deplan();
};

} // namespace WDSP

#endif
