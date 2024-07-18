/*  amsq.h

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
#ifndef _amsq_h
#define _amsq_h

#include "export.h"

namespace WDSP {

class RXA;
class TXA;

class WDSP_API AMSQ
{
public:
    int run;                            // 0 if squelch system is OFF; 1 if it's ON
    int size;                           // size of input/output buffers
    float* in;                         // squelch input signal buffer
    float* out;                        // squelch output signal buffer
    float* trigger;                    // pointer to trigger data source
    float* trigsig;                    // buffer containing trigger signal
    double rate;                        // sample rate
    double avtau;                       // time constant for averaging noise
    double avm;
    double onem_avm;
    double avsig;
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
    double muted_gain;

    static AMSQ* create_amsq (
        int run,
        int size,
        float* in,
        float* out,
        float* trigger,
        int rate,
        double avtau,
        double tup,
        double tdown,
        double tail_thresh,
        double unmute_thresh,
        double min_tail,
        double max_tail,
        double muted_gain
    );
    static void destroy_amsq (AMSQ *a);
    static void flush_amsq (AMSQ *a);
    static void xamsq (AMSQ *a);
    static void xamsqcap (AMSQ *a);
    static void setBuffers_amsq (AMSQ *a, float* in, float* out, float* trigger);
    static void setSamplerate_amsq (AMSQ *a, int rate);
    static void setSize_amsq (AMSQ *a, int size);
    // RXA Properties
    static void SetAMSQRun (RXA& rxa, int run);
    static void SetAMSQThreshold (RXA& rxa, double threshold);
    static void SetAMSQMaxTail (RXA& rxa, double tail);
    // TXA Properties
    static void SetAMSQRun (TXA& txa, int run);
    static void SetAMSQMutedGain (TXA& txa, double dBlevel);
    static void SetAMSQThreshold (TXA& txa, double threshold);

private:
    static void compute_slews(AMSQ *a);
    static void calc_amsq(AMSQ *a);
    static void decalc_amsq (AMSQ *a);
};

} // namespace WDSP

#endif
