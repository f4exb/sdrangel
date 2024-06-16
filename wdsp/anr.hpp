/*  anr.h

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

#ifndef wdsp_anr_h
#define wdsp_anr_h

#include "export.h"

#define ANR_DLINE_SIZE 2048

namespace WDSP {

class RXA;

class WDSP_API ANR
{
public:
    int run;
    int position;
    int buff_size;
    double *in_buff;
    double *out_buff;
    int dline_size;
    int mask;
    int n_taps;
    int delay;
    double two_mu;
    double gamma;
    double d [ANR_DLINE_SIZE];
    double w [ANR_DLINE_SIZE];
    int in_idx;

    double lidx;
    double lidx_min;
    double lidx_max;
    double ngamma;
    double den_mult;
    double lincr;
    double ldecr;

    static ANR* create_anr   (
        int run,
        int position,
        int buff_size,
        double *in_buff,
        double *out_buff,
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

    static void destroy_anr (ANR *a);
    static void flush_anr (ANR *a);
    static void xanr (ANR *a, int position);
    static void setBuffers_anr (ANR *a, double* in, double* out);
    static void setSamplerate_anr (ANR *a, int rate);
    static void setSize_anr (ANR *a, int size);
    // RXA Properties
    static void SetANRRun (RXA& rxa, int setit);
    static void SetANRVals (RXA& rxa, int taps, int delay, double gain, double leakage);
    static void SetANRTaps (RXA& rxa, int taps);
    static void SetANRDelay (RXA& rxa, int delay);
    static void SetANRGain (RXA& rxa, double gain);
    static void SetANRLeakage (RXA& rxa, double leakage);
    static void SetANRPosition (RXA& rxa, int position);
};

} // namespace WDSP

#endif
