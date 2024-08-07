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

#include <array>
#include <vector>

#include "export.h"

namespace WDSP {

class FIRCORE;

class WDSP_API FMSQ
{
public:
    enum class FMSQState
    {
        MUTED,
        INCREASE,
        UNMUTED,
        TAIL,
        DECREASE
    };

    int run;                            // 0 if squelch system is OFF; 1 if it's ON
    int size;                           // size of input/output buffers
    float* insig;                      // squelch input signal buffer
    float* outsig;                     // squelch output signal buffer
    float* trigger;                    // buffer used to trigger mute/unmute (may be same as input; matches timing of input buffer)
    double rate;                        // sample rate
    std::vector<float> noise;
    double fc;                          // corner frequency for sig / noise detection
    double* pllpole;                    // pointer to pole frequency of the fm demodulator pll
    std::array<float, 4> F;
    std::array<float, 4> G;
    double avtau;                       // time constant for averaging noise
    double avm;
    double onem_avm;
    double avnoise;
    double longtau;                     // time constant for long averaging
    double longavm;
    double onem_longavm;
    double longnoise;
    FMSQState state;                          // state machine control
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
    int ready;
    double ramp;
    double rstep;
    double tdelay;
    int nc;
    int mp;
    FIRCORE *p;

    FMSQ(
        int _run,
        int _size,
        float* _insig,
        float* _outsig,
        float* _trigger,
        int _rate,
        double _fc,
        double* _pllpole,
        double _tdelay,
        double _avtau,
        double _longtau,
        double _tup,
        double _tdown,
        double _tail_thresh,
        double _unmute_thresh,
        double _min_tail,
        double _max_tail,
        int _nc,
        int _mp
    );
    FMSQ(const FMSQ&) = delete;
    FMSQ& operator=(const FMSQ& other) = delete;
    ~FMSQ();

    void flush();
    void execute();
    void setBuffers(float* in, float* out, float* trig);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void setRun(int run);
    void setThreshold(double threshold);
    void setNC(int nc);
    void setMP(int mp);

private:
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
