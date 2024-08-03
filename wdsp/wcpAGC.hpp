/*  wcpAGC.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2011, 2012, 2013 Warren Pratt, NR0V
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

#ifndef wdsp_wcpagc_h
#define wdsp_wcpagc_h

#include <array>

#include "export.h"

#define MAX_SAMPLE_RATE     (384000.0)
#define MAX_N_TAU           (8)
#define MAX_TAU_ATTACK      (0.01)
#define RB_SIZE             (int)(MAX_SAMPLE_RATE * MAX_N_TAU * MAX_TAU_ATTACK + 1)

namespace WDSP {

class WDSP_API WCPAGC
{
public:
    int run;
    int mode;
    int pmode;
    float* in;
    float* out;
    int io_buffsize;
    double sample_rate;

    double tau_attack;
    double tau_decay;
    int n_tau;
    double max_gain;
    double var_gain;
    double fixed_gain;
    double min_volts;
    double max_input;
    double out_targ;
    double out_target;
    double inv_max_input;
    double slope_constant;

    double gain;
    double inv_out_target;

    int out_index;
    int in_index;
    int attack_buffsize;

    std::array<double, RB_SIZE*2> ring;
    std::array<double, RB_SIZE> abs_ring;
    static const int ring_buffsize = RB_SIZE;
    double ring_max;

    double attack_mult;
    double decay_mult;
    double volts;
    double save_volts;
    std::array<double, 2> out_sample;
    double abs_out_sample;
    int state;

    double tau_fast_backaverage;
    double fast_backmult;
    double onemfast_backmult;
    double fast_backaverage;
    double tau_fast_decay;
    double fast_decay_mult;
    double pop_ratio;

    int hang_enable;
    double hang_backaverage;
    double tau_hang_backmult;
    double hang_backmult;
    double onemhang_backmult;
    int hang_counter;
    double hangtime;
    double hang_thresh;
    double hang_level;

    double tau_hang_decay;
    double hang_decay_mult;
    int decay_type;

    WCPAGC(
        int run,
        int mode,
        int pmode,
        float* in,
        float* out,
        int io_buffsize,
        int sample_rate,
        double tau_attack,
        double tau_decay,
        int n_tau,
        double max_gain,
        double var_gain,
        double fixed_gain,
        double max_input,
        double out_targ,
        double tau_fast_backaverage,
        double tau_fast_decay,
        double pop_ratio,
        int hang_enable,
        double tau_hang_backmult,
        double hangtime,
        double hang_thresh,
        double tau_hang_decay
    );
    WCPAGC(const WCPAGC&) = delete;
    WCPAGC& operator=(const WCPAGC& other) = delete;
    ~WCPAGC() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void setMode(int mode);
    void setFixed(double fixed_agc);
    void setAttack(int attack);
    void setDecay(int decay);
    void setHang(int hang);
    void getHangLevel(double *hangLevel) const;
    void setHangLevel(double hangLevel);
    void getHangThreshold(int *hangthreshold) const;
    void setHangThreshold(int hangthreshold);
    void getTop(double *max_agc) const;
    void setTop(double max_agc);
    void setSlope(int slope);
    void setMaxInputLevel(double level);
    void setRun(int state);
    void loadWcpAGC();

private:
    void calc();
};

} // namespace WDSP

#endif

