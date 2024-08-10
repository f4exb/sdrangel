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
    float* fftin;          // fft input buffer
    float** fmask;         // frequency domain masks
    float** fftout;        // fftout delay line
    float* accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    float* maskgen;        // input for mask generation FFT
    fftwf_plan* pcfor;       // array of forward FFT plans
    fftwf_plan crev;         // reverse fft plan
    fftwf_plan* maskplan;    // plans for frequency domain masks

    static FIROPT* create_firopt (int run, int position, int size, float* in, float* out,
        int nc, float f_low, float f_high, int samplerate, int wintype, float gain);
    static void xfiropt (FIROPT *a, int pos);
    static void destroy_firopt (FIROPT *a);
    static void flush_firopt (FIROPT *a);
    static void setBuffers_firopt (FIROPT *a, float* in, float* out);
    static void setSamplerate_firopt (FIROPT *a, int rate);
    static void setSize_firopt (FIROPT *a, int size);
    static void setFreqs_firopt (FIROPT *a, float f_low, float f_high);

private:
    static void plan_firopt (FIROPT *a);
    static void calc_firopt (FIROPT *a);
    static void deplan_firopt (FIROPT *a);
};

} // namespace WDSP

#endif

