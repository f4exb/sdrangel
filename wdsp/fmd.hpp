/*  fmd.h

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

#ifndef wdsp_fmd_h
#define wdsp_fmd_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class SNOTCH;
class WCPAGC;
class RXA;

class WDSP_API FMD
{
public:
    int run;
    int size;
    float* in;
    float* out;
    float rate;
    float f_low;                       // audio low cutoff
    float f_high;                      // audio high cutoff
    // pll
    float fmin;                        // pll - minimum carrier freq to lock
    float fmax;                        // pll - maximum carrier freq to lock
    float omega_min;                   // pll - minimum lock check parameter
    float omega_max;                   // pll - maximum lock check parameter
    float zeta;                        // pll - damping factor; as coded, must be <=1.0
    float omegaN;                      // pll - natural frequency
    float phs;                         // pll - phase accumulator
    float omega;                       // pll - locked pll frequency
    float fil_out;                     // pll - filter output
    float g1, g2;                      // pll - filter gain parameters
    float pllpole;                     // pll - pole frequency
    // for dc removal
    float tau;
    float mtau;
    float onem_mtau;
    float fmdc;
    // pll audio gain
    float deviation;
    float again;
    // for de-emphasis filter
    float* audio;
    FIRCORE *pde;
    int nc_de;
    int mp_de;
    // for audio filter
    FIRCORE *paud;
    int nc_aud;
    int mp_aud;
    float afgain;
    // CTCSS removal
    SNOTCH *sntch;
    int sntch_run;
    float ctcss_freq;
    // detector limiter
    WCPAGC *plim;
    int lim_run;
    float lim_gain;
    float lim_pre_gain;

    static FMD* create_fmd (
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        float deviation,
        float f_low,
        float f_high,
        float fmin,
        float fmax,
        float zeta,
        float omegaN,
        float tau,
        float afgain,
        int sntch_run,
        float ctcss_freq,
        int nc_de,
        int mp_de,
        int nc_aud,
        int mp_aud
    );
    static void destroy_fmd (FMD *a);
    static void flush_fmd (FMD *a);
    static void xfmd (FMD *a);
    static void setBuffers_fmd (FMD *a, float* in, float* out);
    static void setSamplerate_fmd (FMD *a, int rate);
    static void setSize_fmd (FMD *a, int size);
    // RXA Properties
    static void SetFMDeviation (RXA& rxa, float deviation);
    static void SetCTCSSFreq (RXA& rxa, float freq);
    static void SetCTCSSRun (RXA& rxa, int run);
    static void SetFMNCde (RXA& rxa, int nc);
    static void SetFMMPde (RXA& rxa, int mp);
    static void SetFMNCaud (RXA& rxa, int nc);
    static void SetFMMPaud (RXA& rxa, int mp);
    static void SetFMLimRun (RXA& rxa, int run);
    static void SetFMLimGain (RXA& rxa, float gaindB);
    static void SetFMAFFilter(RXA& rxa, float low, float high);

private:
    static void calc_fmd (FMD *a);
    static void decalc_fmd (FMD *a);
};

} // namespace WDSP

#endif
