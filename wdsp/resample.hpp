/*  resample.h

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

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR COMPLEX DOUBLE-PRECISION                              *
*                                                                                               *
************************************************************************************************/

#ifndef wdsp_resample_h
#define wdsp_resample_h

#include "export.h"

namespace WDSP {

class WDSP_API RESAMPLE
{
public:
    int run;            // run
    int size;           // number of input samples per buffer
    float* in;         // input buffer for resampler
    float* out;        // output buffer for resampler
    int in_rate;
    int out_rate;
    double fcin;
    double fc;
    double fc_low;
    double gain;
    int idx_in;         // index for input into ring
    int ncoefin;
    int ncoef;          // number of coefficients
    int L;              // interpolation factor
    int M;              // decimation factor
    double* h;          // coefficients
    int ringsize;       // number of complex pairs the ring buffer holds
    double* ring;       // ring buffer
    int cpp;            // coefficients of the phase
    int phnum;          // phase number

    static RESAMPLE* create_resample (
        int run,
        int size,
        float* in,
        float* out,
        int in_rate,
        int out_rate,
        double fc,
        int ncoef,
        double gain
    );
    static void destroy_resample (RESAMPLE *a);
    static void flush_resample (RESAMPLE *a);
    static int xresample (RESAMPLE *a);
    static void setBuffers_resample (RESAMPLE *a, float* in, float* out);
    static void setSize_resample(RESAMPLE *a, int size);
    static void setInRate_resample(RESAMPLE *a, int rate);
    static void setOutRate_resample(RESAMPLE *a, int rate);
    static void setFCLow_resample (RESAMPLE *a, float fc_low);
    static void setBandwidth_resample (RESAMPLE *a, float fc_low, float fc_high);
    // Exported calls
    static void* create_resampleV (int in_rate, int out_rate);
    static void xresampleV (float* input, float* output, int numsamps, int* outsamps, void* ptr);
    static void destroy_resampleV (void* ptr);

private:
    static void calc_resample (RESAMPLE *a);
    static void decalc_resample (RESAMPLE *a);
};

#endif

/************************************************************************************************
*                                                                                               *
*                             VERSION FOR NON-COMPLEX FLOATS                                    *
*                                                                                               *
************************************************************************************************/

#ifndef wdsp_resampleF_h
#define wdsp_resampleF_h

#include "export.h"

class WDSP_API RESAMPLEF
{
public:
    int run;            // run
    int size;           // number of input samples per buffer
    float* in;          // input buffer for resampler
    float* out;         // output buffer for resampler
    int idx_in;         // index for input into ring
    int ncoef;          // number of coefficients
    int L;              // interpolation factor
    int M;              // decimation factor
    float* h;          // coefficients
    int ringsize;       // number of values the ring buffer holds
    float* ring;       // ring buffer
    int cpp;            // coefficients of the phase
    int phnum;          // phase number

    static RESAMPLEF* create_resampleF (int run, int size, float* in, float* out, int in_rate, int out_rate);
    static void destroy_resampleF (RESAMPLEF *a);
    static void flush_resampleF (RESAMPLEF *a);
    static int xresampleF (RESAMPLEF *a);
    // Exported calls
    static void* create_resampleFV (int in_rate, int out_rate);
    static void xresampleFV (float* input, float* output, int numsamps, int* outsamps, void* ptr);
    static void destroy_resampleFV (void* ptr);
};

} // namespace WDSP

#endif
