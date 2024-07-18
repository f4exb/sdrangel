/*  anf.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2012, 2013 Warren Pratt, NR0V
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

#ifndef wdsp_anf_h
#define wdsp_anf_h

#include "export.h"

#define ANF_DLINE_SIZE 2048

namespace WDSP {

class RXA;

class WDSP_API ANF
{
public:
    int run;
    int position;
    int buff_size;
    float *in_buff;
    float *out_buff;
    int dline_size;
    int mask;
    int n_taps;
    int delay;
    double two_mu;
    double gamma;
    double d [ANF_DLINE_SIZE];
    double w [ANF_DLINE_SIZE];
    int in_idx;
    double lidx;
    double lidx_min;
    double lidx_max;
    double ngamma;
    double den_mult;
    double lincr;
    double ldecr;

    static ANF* create_anf(
        int run,
        int position,
        int buff_size,
        float *in_buff,
        float *out_buff,
        int dline_size,
        int n_taps,
        int delay,
        double two_mu,
        double gamma,
        double lidx,
        double lidx_min,
        double lidx_max,
        double ngamma,
        double den_mult,
        double lincr,
        double ldecr
    );
    static void destroy_anf (ANF *a);
    static void flush_anf (ANF *a);
    static void xanf (ANF *a, int position);
    static void setBuffers_anf (ANF *a, float* in, float* out);
    static void setSamplerate_anf (ANF *a, int rate);
    static void setSize_anf (ANF *a, int size);
    // RXA Properties
    static void SetANFRun (RXA& rxa, int setit);
    static void SetANFVals (RXA& rxa, int taps, int delay, double gain, double leakage);
    static void SetANFTaps (RXA& rxa, int taps);
    static void SetANFDelay (RXA& rxa, int delay);
    static void SetANFGain (RXA& rxa, double gain);
    static void SetANFLeakage (RXA& rxa, double leakage);
    static void SetANFPosition (RXA& rxa, int position);
};

} // namespace WDSP

#endif
