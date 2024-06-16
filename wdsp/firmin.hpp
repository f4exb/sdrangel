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

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API FIRMIN
{
public:
    int run;                // run control
    int position;           // position at which to execute
    int size;               // input/output buffer size, power of two
    double* in;             // input buffer
    double* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two
    double f_low;           // low cutoff frequency
    double f_high;          // high cutoff frequency
    double* ring;           // internal complex ring buffer
    double* h;              // complex filter coefficients
    int rsize;              // ring size, number of complex samples, power of two
    int mask;               // mask to update indexes
    int idx;                // ring input/output index
    double samplerate;      // sample rate
    int wintype;            // filter window type
    double gain;            // filter gain

    static FIRMIN* create_firmin (int run, int position, int size, double* in, double* out,
        int nc, double f_low, double f_high, int samplerate, int wintype, double gain);
    static void destroy_firmin (FIRMIN *a);
    static void flush_firmin (FIRMIN *a);
    static void xfirmin (FIRMIN *a, int pos);
    static void setBuffers_firmin (FIRMIN *a, double* in, double* out);
    static void setSamplerate_firmin (FIRMIN *a, int rate);
    static void setSize_firmin (FIRMIN *a, int size);
    static void setFreqs_firmin (FIRMIN *a, double f_low, double f_high);

private:
    static void calc_firmin (FIRMIN *a);
};

} // namespace WDSP

#endif


/********************************************************************************************************
*                                                                                                       *
*                               Standalone Partitioned Overlap-Save Bandpass                            *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_firopt_h
#define wdsp_firopt_h

#include "fftw3.h"
#include "export.h"

namespace WDSP {

class WDSP_API FIROPT
{
    int run;                // run control
    int position;           // position at which to execute
    int size;               // input/output buffer size, power of two
    double* in;             // input buffer
    double* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two, >= size
    double f_low;           // low cutoff frequency
    double f_high;          // high cutoff frequency
    double samplerate;      // sample rate
    int wintype;            // filter window type
    double gain;            // filter gain
    int nfor;               // number of buffers in delay line
    double* fftin;          // fft input buffer
    double** fmask;         // frequency domain masks
    double** fftout;        // fftout delay line
    double* accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    double* maskgen;        // input for mask generation FFT
    fftw_plan* pcfor;       // array of forward FFT plans
    fftw_plan crev;         // reverse fft plan
    fftw_plan* maskplan;    // plans for frequency domain masks

    static FIROPT* create_firopt (int run, int position, int size, double* in, double* out,
        int nc, double f_low, double f_high, int samplerate, int wintype, double gain);
    static void xfiropt (FIROPT *a, int pos);
    static void destroy_firopt (FIROPT *a);
    static void flush_firopt (FIROPT *a);
    static void setBuffers_firopt (FIROPT *a, double* in, double* out);
    static void setSamplerate_firopt (FIROPT *a, int rate);
    static void setSize_firopt (FIROPT *a, int size);
    static void setFreqs_firopt (FIROPT *a, double f_low, double f_high);

private:
    static void plan_firopt (FIROPT *a);
    static void calc_firopt (FIROPT *a);
    static void deplan_firopt (FIROPT *a);
};

} // namespace WDSP

#endif


/********************************************************************************************************
*                                                                                                       *
*                                   Partitioned Overlap-Save Filter Kernel                              *
*                                                                                                       *
********************************************************************************************************/

#ifndef wdsp_fircore_h
#define wdsp_fircore_h

#include <QRecursiveMutex>
#include "export.h"

namespace WDSP {

class WDSP_API FIRCORE
{
public:
    int size;               // input/output buffer size, power of two
    double* in;             // input buffer
    double* out;            // output buffer, can be same as input
    int nc;                 // number of filter coefficients, power of two, >= size
    double* impulse;        // impulse response of filter
    double* imp;
    int nfor;               // number of buffers in delay line
    double* fftin;          // fft input buffer
    double*** fmask;        // frequency domain masks
    double** fftout;        // fftout delay line
    double* accum;          // frequency domain accumulator
    int buffidx;            // fft out buffer index
    int idxmask;            // mask for index computations
    double* maskgen;        // input for mask generation FFT
    fftw_plan* pcfor;       // array of forward FFT plans
    fftw_plan crev;         // reverse fft plan
    fftw_plan** maskplan;   // plans for frequency domain masks
    QRecursiveMutex update;
    int cset;
    int mp;
    int masks_ready;

    static FIRCORE* create_fircore (int size, double* in, double* out,
        int nc, int mp, double* impulse);
    static void xfircore (FIRCORE *a);
    static void destroy_fircore (FIRCORE *a);
    static void flush_fircore (FIRCORE *a);
    static void setBuffers_fircore (FIRCORE *a, double* in, double* out);
    static void setSize_fircore (FIRCORE *a, int size);
    static void setImpulse_fircore (FIRCORE *a, double* impulse, int update);
    static void setNc_fircore (FIRCORE *a, int nc, double* impulse);
    static void setMp_fircore (FIRCORE *a, int mp);
    static void setUpdate_fircore (FIRCORE *a);

private:
    static void plan_fircore (FIRCORE *a);
    static void calc_fircore (FIRCORE *a, int flip);
    static void deplan_fircore (FIRCORE *a);
};

} // namespace WDSP

#endif
