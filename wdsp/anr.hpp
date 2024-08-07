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

#include <array>

#include "export.h"

namespace WDSP {

class WDSP_API ANR
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
    static const int ANR_DLINE_SIZE = 2048;
    std::array<double, ANR_DLINE_SIZE> d;
    std::array<double, ANR_DLINE_SIZE> w;
    int in_idx;

    double lidx;
    double lidx_min;
    double lidx_max;
    double ngamma;
    double den_mult;
    double lincr;
    double ldecr;

    ANR(
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
    ANR(const ANR&) = delete;
    ANR& operator=(const ANR& other) = delete;
    ~ANR() = default;

    void flush();
    void execute(int position);
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA Properties
    void setVals(int taps, int delay, double gain, double leakage);
    void setTaps(int taps);
    void setDelay(int delay);
    void setGain(double gain);
    void setLeakage(double leakage);
};

} // namespace WDSP

#endif
