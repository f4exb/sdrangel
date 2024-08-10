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
*                               Standalone Partitioned Overlap-Save Bandpass                            *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_firopt_h
#define wdsp_firopt_h

#include <vector>

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API FIROPT
{
public:
    int run;                // run control
    int position;           // position at which to execute
    int size;               // input/output buffer size, power of two
    float* in;             // input buffer
    float* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two, >= size
    float f_low;           // low cutoff frequency
    float f_high;          // high cutoff frequency
    float samplerate;      // sample rate
    int wintype;            // filter window type
    float gain;            // filter gain
    int nfor;               // number of buffers in delay line
    std::vector<float> fftin;          // fft input buffer
    std::vector<std::vector<float>> fmask;         // frequency domain masks
    std::vector<std::vector<float>> fftout;        // fftout delay line
    std::vector<float> accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    std::vector<float> maskgen;        // input for mask generation FFT
    std::vector<fftwf_plan> pcfor;       // array of forward FFT plans
    fftwf_plan crev;         // reverse fft plan
    std::vector<fftwf_plan> maskplan;    // plans for frequency domain masks

    FIROPT(
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
    FIROPT(const FIROPT&) = delete;
    FIROPT& operator=(const FIROPT& other) = delete;
    ~FIROPT();

    void destroy();
    void flush();
    void execute(int pos);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    void setFreqs(float f_low, float f_high);

private:
    void plan();
    void calc();
    void deplan();
};

} // namespace WDSP

#endif

