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

#include "export.h"

#define MAX_SAMPLE_RATE     (384000.0)
#define MAX_N_TAU           (8)
#define MAX_TAU_ATTACK      (0.01)
#define RB_SIZE             (int)(MAX_SAMPLE_RATE * MAX_N_TAU * MAX_TAU_ATTACK + 1)

namespace WDSP {

class RXA;
class TXA;

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

    double* ring;
    double* abs_ring;
    int ring_buffsize;
    double ring_max;

    double attack_mult;
    double decay_mult;
    double volts;
    double save_volts;
    double out_sample[2];
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

    static void xwcpagc (WCPAGC *a);
    static WCPAGC* create_wcpagc (
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
    static void destroy_wcpagc (WCPAGC *a);
    static void flush_wcpagc (WCPAGC *a);
    static void setBuffers_wcpagc (WCPAGC *a, float* in, float* out);
    static void setSamplerate_wcpagc (WCPAGC *a, int rate);
    static void setSize_wcpagc (WCPAGC *a, int size);
    // RXA Properties
    static void SetAGCMode (RXA& rxa, int mode);
    static void SetAGCFixed (RXA& rxa, float fixed_agc);
    static void SetAGCAttack (RXA& rxa, int attack);
    static void SetAGCDecay (RXA& rxa, int decay);
    static void SetAGCHang (RXA& rxa, int hang);
    static void GetAGCHangLevel(RXA& rxa, float *hangLevel);
    static void SetAGCHangLevel(RXA& rxa, float hangLevel);
    static void GetAGCHangThreshold(RXA& rxa, int *hangthreshold);
    static void SetAGCHangThreshold (RXA& rxa, int hangthreshold);
    static void GetAGCTop(RXA& rxa, float *max_agc);
    static void SetAGCTop (RXA& rxa, float max_agc);
    static void SetAGCSlope (RXA& rxa, int slope);
    static void SetAGCThresh(RXA& rxa, float thresh, float size, float rate);
    static void GetAGCThresh(RXA& rxa, float *thresh, float size, float rate);
    static void SetAGCMaxInputLevel (RXA& rxa, float level);
    // TXA Properties
    static void SetALCSt (TXA& txa, int state);
    static void SetALCAttack (TXA& txa, int attack);
    static void SetALCDecay (TXA& txa, int decay);
    static void SetALCHang (TXA& txa, int hang);
    static void SetLevelerSt (TXA& txa, int state);
    static void SetLevelerAttack (TXA& txa, int attack);
    static void SetLevelerDecay (TXA& txa, int decay);
    static void SetLevelerHang (TXA& txa, int hang);
    static void SetLevelerTop (TXA& txa, float maxgain);
    static void SetALCMaxGain (TXA& txa, float maxgain);

private:
    static void loadWcpAGC (WCPAGC *a);
    static void calc_wcpagc (WCPAGC *a);
    static void decalc_wcpagc (WCPAGC *a);
};

} // namespace WDSP

#endif

