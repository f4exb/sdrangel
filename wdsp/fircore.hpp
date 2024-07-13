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

#include <QRecursiveMutex>
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
    float* impulse;        // impulse response of filter
    float* imp;
    int nfor;               // number of buffers in delay line
    float* fftin;          // fft input buffer
    float*** fmask;        // frequency domain masks
    float** fftout;        // fftout delay line
    float* accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    float* maskgen;        // input for mask generation FFT
    fftwf_plan* pcfor;       // array of forward FFT plans
    fftwf_plan crev;         // reverse fft plan
    fftwf_plan** maskplan;   // plans for frequency domain masks
    QRecursiveMutex update;
    int cset;
    int mp;
    int masks_ready;

    static FIRCORE* create_fircore (int size, float* in, float* out,
        int nc, int mp, float* impulse);
    static void xfircore (FIRCORE *a);
    static void destroy_fircore (FIRCORE *a);
    static void flush_fircore (FIRCORE *a);
    static void setBuffers_fircore (FIRCORE *a, float* in, float* out);
    static void setSize_fircore (FIRCORE *a, int size);
    static void setImpulse_fircore (FIRCORE *a, float* impulse, int update);
    static void setNc_fircore (FIRCORE *a, int nc, float* impulse);
    static void setMp_fircore (FIRCORE *a, int mp);
    static void setUpdate_fircore (FIRCORE *a);

private:
    static void plan_fircore (FIRCORE *a);
    static void calc_fircore (FIRCORE *a, int flip);
    static void deplan_fircore (FIRCORE *a);
};

} // namespace WDSP

#endif
