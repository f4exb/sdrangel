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

    RESAMPLE (
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
    ~RESAMPLE();

    void flush();
    int execute();
    void setBuffers(float* in, float* out);
    void setSize(int size);
    void setInRate(int rate);
    void setOutRate(int rate);
    void setFCLow(double fc_low);
    void setBandwidth(double fc_low, double fc_high);
    // Exported calls
    static void* createV (int in_rate, int out_rate);
    static void executeV (float* input, float* output, int numsamps, int* outsamps, void* ptr);
    static void destroyV (void* ptr);

private:
    void calc_resample();
    void decalc_resample();
};

} // namespace WDSP

#endif
