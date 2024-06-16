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
    double* in;
    double* out;
    double rate;
    double f_low;                       // audio low cutoff
    double f_high;                      // audio high cutoff
    // pll
    double fmin;                        // pll - minimum carrier freq to lock
    double fmax;                        // pll - maximum carrier freq to lock
    double omega_min;                   // pll - minimum lock check parameter
    double omega_max;                   // pll - maximum lock check parameter
    double zeta;                        // pll - damping factor; as coded, must be <=1.0
    double omegaN;                      // pll - natural frequency
    double phs;                         // pll - phase accumulator
    double omega;                       // pll - locked pll frequency
    double fil_out;                     // pll - filter output
    double g1, g2;                      // pll - filter gain parameters
    double pllpole;                     // pll - pole frequency
    // for dc removal
    double tau;
    double mtau;
    double onem_mtau;
    double fmdc;
    // pll audio gain
    double deviation;
    double again;
    // for de-emphasis filter
    double* audio;
    FIRCORE *pde;
    int nc_de;
    int mp_de;
    // for audio filter
    FIRCORE *paud;
    int nc_aud;
    int mp_aud;
    double afgain;
    // CTCSS removal
    SNOTCH *sntch;
    int sntch_run;
    double ctcss_freq;
    // detector limiter
    WCPAGC *plim;
    int lim_run;
    double lim_gain;
    double lim_pre_gain;

    static FMD* create_fmd (
        int run,
        int size,
        double* in,
        double* out,
        int rate,
        double deviation,
        double f_low,
        double f_high,
        double fmin,
        double fmax,
        double zeta,
        double omegaN,
        double tau,
        double afgain,
        int sntch_run,
        double ctcss_freq,
        int nc_de,
        int mp_de,
        int nc_aud,
        int mp_aud
    );
    static void destroy_fmd (FMD *a);
    static void flush_fmd (FMD *a);
    static void xfmd (FMD *a);
    static void setBuffers_fmd (FMD *a, double* in, double* out);
    static void setSamplerate_fmd (FMD *a, int rate);
    static void setSize_fmd (FMD *a, int size);
    // RXA Properties
    static void SetFMDeviation (RXA& rxa, double deviation);
    static void SetCTCSSFreq (RXA& rxa, double freq);
    static void SetCTCSSRun (RXA& rxa, int run);
    static void SetFMNCde (RXA& rxa, int nc);
    static void SetFMMPde (RXA& rxa, int mp);
    static void SetFMNCaud (RXA& rxa, int nc);
    static void SetFMMPaud (RXA& rxa, int mp);
    static void SetFMLimRun (RXA& rxa, int run);
    static void SetFMLimGain (RXA& rxa, double gaindB);
    static void SetFMAFFilter(RXA& rxa, double low, double high);

private:
    static void calc_fmd (FMD *a);
    static void decalc_fmd (FMD *a);
};

} // namespace WDSP

#endif
