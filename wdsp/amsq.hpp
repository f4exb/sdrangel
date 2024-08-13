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

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API AMSQ
{
public:
    enum class AMSQState
    {
        MUTED,
        INCREASE,
        UNMUTED,
        TAIL,
        DECREASE
    };

    int run;                           // 0 if squelch system is OFF; 1 if it's ON
    int size;                          // size of input/output buffers
    float* in;                         // squelch input signal buffer
    float* out;                        // squelch output signal buffer
    float* trigger;                    // pointer to trigger data source
    std::vector<float> trigsig;        // buffer containing trigger signal
    double rate;                       // sample rate
    double avtau;                      // time constant for averaging noise
    double avm;
    double onem_avm;
    double avsig;
    AMSQState state;                   // state machine control
    int count;
    double tup;
    double tdown;
    int ntup;
    int ntdown;
    std::vector<double> cup;
    std::vector<double> cdown;
    double tail_thresh;
    double unmute_thresh;
    double min_tail;
    double max_tail;
    double muted_gain;

    AMSQ (
        int _run,
        int _size,
        float* _in,
        float* _out,
        float* _trigger,
        int _rate,
        double _avtau,
        double _tup,
        double _tdown,
        double _tail_thresh,
        double _unmute_thresh,
        double _min_tail,
        double _max_tail,
        double _muted_gain
    );
    AMSQ(const AMSQ&) = delete;
    AMSQ& operator=(const AMSQ& other) = delete;
    ~AMSQ() = default;

    void flush();
    void execute();
    void xcap();
    void setBuffers(float* in, float* out, float* trigger);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA Properties
    void setRun(int run);
    void setThreshold(double threshold);
    void setMaxTail(double tail);
    void setMutedGain(double dBlevel);

private:
    void compute_slews();
    void calc();
};

} // namespace WDSP

#endif
