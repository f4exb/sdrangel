/*  rmatch.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2022 Warren Pratt, NR0V
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

#ifndef wdsp_rmatch_h
#define wdsp_rmatch_h

#include "export.h"

namespace WDSP {

class VARSAMP;

class WDSP_API MAV
{
public:
    int ringmin;
    int ringmax;        // must be a power of two
    int* ring;
    int mask;
    int i;
    int load;
    int sum;
    float nom_value;

    static MAV* create_mav (int ringmin, int ringmax, float nom_value);
    static void destroy_mav (MAV *a);
    static void flush_mav (MAV *a);
    static void xmav (MAV *a, int input, float* output);
};

class WDSP_API AAMAV
{
public:
    int ringmin;
    int ringmax;        // must be a power of two
    int* ring;
    int mask;
    int i;
    int load;
    int pos;
    int neg;
    float nom_ratio;

    static AAMAV* create_aamav (int ringmin, int ringmax, float nom_ratio);
    static void destroy_aamav (AAMAV *a);
    static void flush_aamav (AAMAV *a);
    static void xaamav (AAMAV *a, int input, float* output);
};

class WDSP_API RMATCH
{
public:
    long run;
    float* in;
    float* out;
    int insize;
    int outsize;
    float* resout;
    int nom_inrate;
    int nom_outrate;
    float nom_ratio;
    float inv_nom_ratio;
    float fc_high;
    float fc_low;
    float gain;
    float startup_delay;
    int auto_ringsize;
    int ringsize;
    int rsize;
    float* ring;
    int n_ring;
    int iin;
    int iout;
    float var;
    int R;
    AAMAV *ffmav;
    MAV *propmav;
    int ff_ringmin;
    int ff_ringmax;         // must be a power of two
    float ff_alpha;
    float feed_forward;
    int prop_ringmin;
    int prop_ringmax;       // must be a power of two
    float prop_gain;
    float pr_gain;
    float av_deviation;
    VARSAMP *v;
    int varmode;
    // blend / slew
    float tslew;
    int ntslew;
    float* cslew;
    float* baux;
    float dlast[2];
    int ucnt;
    // variables to check start-up time for control to become active
    unsigned int readsamps;
    unsigned int writesamps;
    unsigned int read_startup;
    unsigned int write_startup;
    int control_flag;
    // diagnostics
    long underflows;
    long overflows;
    int force;
    float fvar;

    static RMATCH* create_rmatch (
        int run,                // 0 - input and output calls do nothing; 1 - operates normally
        float* in,             // pointer to input buffer
        float* out,            // pointer to output buffer
        int insize,             // size of input buffer
        int outsize,            // size of output buffer
        int nom_inrate,         // nominal input samplerate
        int nom_outrate,        // nominal output samplerate
        float fc_high,         // high cutoff frequency if lower than max
        float fc_low,          // low cutoff frequency if higher than zero
        float gain,            // gain to be applied during this process
        float startup_delay,   // time (seconds) to delay before beginning measurements to control variable resampler
        int auto_ringsize,      // 0 specified ringsize is used; 1 ringsize is auto-optimized - FEATURE NOT IMPLEMENTED!!
        int ringsize,           // specified ringsize; max ringsize if 'auto' is enabled
        int R,                  // density factor for varsamp coefficients
        float var,             // initial value of variable resampler ratio (value of ~1.0)
        int ffmav_min,          // minimum feed-forward moving average size to put full weight on data in the ring
        int ffmav_max,          // maximum feed-forward moving average size - MUST BE A POWER OF TWO!
        float ff_alpha,        // feed-forward exponential averaging multiplier
        int prop_ringmin,       // proportional feedback min moving average ringsize
        int prop_ringmax,       // proportional feedback max moving average ringsize - MUST BE A POWER OF TWO!
        float prop_gain,       // proportional feedback gain factor
        int varmode,            // 0 - use same var for all samples of the buffer; 1 - interpolate from old_var to this var
        float tslew            // slew/blend time (seconds)
    );
    static void destroy_rmatch (RMATCH *a);
    static void reset_rmatch (RMATCH *a);

    static void* create_rmatchV(int in_size, int out_size, int nom_inrate, int nom_outrate, int ringsize, float var);
    static void* create_rmatchLegacyV(int in_size, int out_size, int nom_inrate, int nom_outrate, int ringsize);
    static void destroy_rmatchV (void* ptr);
    static void xrmatchOUT (void* b, float* out);
    static void xrmatchIN (void* b, float* in);
    static void setRMatchInsize (void* ptr, int insize);
    static void setRMatchOutsize (void* ptr, int outsize);
    static void setRMatchNomInrate (void* ptr, int nom_inrate);
    static void setRMatchNomOutrate (void* ptr, int nom_outrate);
    static void setRMatchRingsize (void* ptr, int ringsize);
    static void getRMatchDiags (void* b, int* underflows, int* overflows, float* var, int* ringsize, int* nring);
    static void resetRMatchDiags (void* b);
    static void forceRMatchVar (void* b, int force, float fvar);
    static void setRMatchFeedbackGain(void* b, float feedback_gain);
    static void setRMatchSlewTime(void* b, float slew_time);
    static void setRMatchSlewTime1(void* b, float slew_time);
    static void setRMatchPropRingMin(void* ptr, int prop_min);
    static void setRMatchPropRingMax(void* ptr, int prop_max);
    static void setRMatchFFRingMin(void* ptr, int ff_ringmin);
    static void setRMatchFFRingMax(void* ptr, int ff_ringmax);
    static void setRMatchFFAlpha(void* ptr, float ff_alpha);
    static void getControlFlag(void* ptr, int* control_flag);

private:
    static void calc_rmatch (RMATCH *a);
    static void decalc_rmatch (RMATCH *a);
    static void control (RMATCH *a, int change);
    static void blend (RMATCH *a);
    static void upslew (RMATCH *a, int newsamps);
    static void dslew (RMATCH *a);
};

} // namespace WDSP

#endif
