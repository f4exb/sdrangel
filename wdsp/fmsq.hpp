/*  fmsq.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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

#ifndef wdsp_fmsq_h
#define wdsp_fmsq_h

#include "export.h"

namespace WDSP {

class FIRCORE;
class RXA;

class WDSP_API FMSQ
{
public:
    int run;                            // 0 if squelch system is OFF; 1 if it's ON
    int size;                           // size of input/output buffers
    double* insig;                      // squelch input signal buffer
    double* outsig;                     // squelch output signal buffer
    double* trigger;                    // buffer used to trigger mute/unmute (may be same as input; matches timing of input buffer)
    double rate;                        // sample rate
    double* noise;
    double fc;                          // corner frequency for sig / noise detection
    double* pllpole;                    // pointer to pole frequency of the fm demodulator pll
    double F[4];
    double G[4];
    double avtau;                       // time constant for averaging noise
    double avm;
    double onem_avm;
    double avnoise;
    double longtau;                     // time constant for long averaging
    double longavm;
    double onem_longavm;
    double longnoise;
    int state;                          // state machine control
    int count;
    double tup;
    double tdown;
    int ntup;
    int ntdown;
    double* cup;
    double* cdown;
    double tail_thresh;
    double unmute_thresh;
    double min_tail;
    double max_tail;
    int ready;
    double ramp;
    double rstep;
    double tdelay;
    int nc;
    int mp;
    FIRCORE *p;

    static FMSQ* create_fmsq (
        int run,
        int size,
        double* insig,
        double* outsig,
        double* trigger,
        int rate,
        double fc,
        double* pllpole,
        double tdelay,
        double avtau,
        double longtau,
        double tup,
        double tdown,
        double tail_thresh,
        double unmute_thresh,
        double min_tail,
        double max_tail,
        int nc,
        int mp
    );
    static void destroy_fmsq (FMSQ *a);
    static void flush_fmsq (FMSQ *a);
    static void xfmsq (FMSQ *a);
    static void setBuffers_fmsq (FMSQ *a, double* in, double* out, double* trig);
    static void setSamplerate_fmsq (FMSQ *a, int rate);
    static void setSize_fmsq (FMSQ *a, int size);
    // RXA Properties
    static void SetFMSQRun (RXA& rxa, int run);
    static void SetFMSQThreshold (RXA& rxa, double threshold);
    static void SetFMSQNC (RXA& rxa, int nc);
    static void SetFMSQMP (RXA& rxa, int mp);

private:
    static void calc_fmsq (FMSQ *a);
    static void decalc_fmsq (FMSQ *a);
};

} // namespace WDSP

#endif
