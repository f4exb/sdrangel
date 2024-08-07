/*  ssql.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2023 Warren Pratt, NR0V
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

warren@pratt.one

*/

#ifndef wdsp_ssql_h
#define wdsp_ssql_h

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API FTOV                    // Frequency to Voltage Converter
{
public:
    int run;                            // 0 => don't run; 1 => run
    int size;                           // buffer size
    int rate;                           // sample-rate
    int rsize;                          // rate * time_to_fill_ring, e.g., 48K/s * 50ms = 2400
    double fmax;                        // frequency (Hz) for full output, e.g., 2000 (Hz)
    float* in;                          // pointer to the intput buffer for ftov
    float* out;                         // pointer to the output buffer for ftov
    std::vector<int> ring;              // the ring
    int rptr;                           // index into the ring
    double inlast;                      // holds last sample from previous buffer
    int rcount;                         // count of zero-crossings currently in the ring
    double div;                         // divisor for 'rcount' to produce output of 1.0 at 'fmax'
    double eps;                         // minimum input change to count as a signal edge transition

    FTOV(
        int run,
        int size,
        int rate,
        int rsize,
        double fmax,
        float* in,
        float* out
    );
    FTOV(const FTOV&) = delete;
    FTOV& operator=(FTOV& other) = delete;
    ~FTOV() = default;

    void flush();
    void execute();
};

class CBL;
class FTDV;
class DBQLP;

class WDSP_API SSQL                    // Syllabic Squelch
{
public:
    enum class SSQLState
    {
        MUTED,
        INCREASE,
        UNMUTED,
        DECREASE
    };
    int run;                            // 0 if squelch system is OFF; 1 if it's ON
    int size;                           // size of input/output buffers
    float* in;                         // squelch input signal buffer
    float* out;                        // squelch output signal buffer
    int rate;                           // sample rate
    SSQLState state;                          // state machine control
    int count;                          // count variable for raised cosine transitions
    double tup;                         // time for turn-on transition
    double tdown;                       // time for turn-off transition
    int ntup;                           // number of samples for turn-on transition
    int ntdown;                         // number of samples for turn-off transition
    std::vector<double> cup;                        // coefficients for up-slew
    std::vector<double> cdown;                      // coefficients for down-slew
    double muted_gain;                  // audio gain while muted; 0.0 for complete silence

    std::vector<float> b1;                         // buffer to hold output of dc-block function
    std::vector<float> ibuff;                      // buffer containing only 'I' component
    std::vector<float> ftovbuff;                   // buffer containing output of f to v converter
    std::vector<float> lpbuff;                     // buffer containing output of low-pass filter
    std::vector<int> wdbuff;                        // buffer containing output of window detector
    CBL *dcbl;                          // pointer to DC Blocker data structure
    FTOV *cvtr;                         // pointer to F to V Converter data structure
    DBQLP *filt;                         // pointer to Bi-Quad Low-Pass Filter data structure
    int ftov_rsize;                     // ring size for f_to_v converter
    double ftov_fmax;                   // fmax for f_to_v converter
    // window detector
    double wdtau;                       // window detector time constant
    double wdmult;                      // window detector time constant multiplier
    double wdaverage;                   // average signal value
    double wthresh;                     // window threshold above/below average
    // trigger
    double tr_thresh;                   // trigger threshold:  100K/(100K+22K)=0.8197
    double tr_tau_unmute;               // trigger unmute time-constant:  (100K||220K)*10uF = 0.6875
    double tr_ss_unmute;                // trigger steady-state level for unmute:  100K/(100K+220K)=0.3125
    double tr_tau_mute;                 // trigger mute time-constant:  220K*10uF = 2.2
    double tr_ss_mute;                  // trigger steady-state level for mute:  1.0
    double tr_voltage;                  // trigger voltage
    double mute_mult;                   // multiplier for successive voltage calcs when muted
    double unmute_mult;                 // multiplier for successive voltage calcs when unmuted
    std::vector<int> tr_signal;                     // trigger signal, 0 or 1

    SSQL(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        double tup,
        double tdown,
        double muted_gain,
        double tau_mute,
        double tau_unmute,
        double wthresh,
        double tr_thresh,
        int rsize,
        double fmax
    );
    SSQL(const SSQL&) = delete;
    SSQL& operator=(const SSQL& other) = delete;
    ~SSQL();

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA Properties
    void setRun(int run);
    void setThreshold(double threshold);
    void setTauMute(double tau_mute);
    void setTauUnMute(double tau_unmute);

private:
    void compute_slews();
    void calc();
    void decalc();
};

} // namespace WDSP

#endif
