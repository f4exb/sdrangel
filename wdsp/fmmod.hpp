/*  fmmod.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2023 Warren Pratt, NR0V
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

#ifndef wdsp_fmmod_h
#define wdsp_fmmod_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class TXA;

class WDSP_API FMMOD
{
public:
    int run;
    int size;
    double* in;
    double* out;
    double samplerate;
    double deviation;
    double f_low;
    double f_high;
    int ctcss_run;
    double ctcss_level;
    double ctcss_freq;
    // for ctcss gen
    double tscale;
    double tphase;
    double tdelta;
    // mod
    double sphase;
    double sdelta;
    // bandpass
    int bp_run;
    double bp_fc;
    int nc;
    int mp;
    FIRCORE *p;

    static FMMOD* create_fmmod (
        int run,
        int size,
        double* in,
        double* out,
        int rate,
        double dev,
        double f_low,
        double f_high,
        int ctcss_run,
        double ctcss_level,
        double ctcss_freq,
        int bp_run,
        int nc,
        int mp
    );
    static void destroy_fmmod (FMMOD *a);
    static void flush_fmmod (FMMOD *a);
    static void xfmmod (FMMOD *a);
    static void setBuffers_fmmod (FMMOD *a, double* in, double* out);
    static void setSamplerate_fmmod (FMMOD *a, int rate);
    static void setSize_fmmod (FMMOD *a, int size);
    // TXA Properties
    static void SetFMDeviation (TXA& txa, double deviation);
    static void SetCTCSSFreq (TXA& txa, double freq);
    static void SetCTCSSRun (TXA& txa, int run);
    static void SetFMMP (TXA& txa, int mp);
    static void SetFMNC (TXA& txa, int nc);
    static void SetFMAFFreqs (TXA& txa, double low, double high);

private:
    static void calc_fmmod (FMMOD *a);
};

} // namespace WDSP

#endif
