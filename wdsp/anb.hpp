/*  anb.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014 Warren Pratt, NR0V
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

#ifndef wdsp_anb_h
#define wdsp_anb_h

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API ANB
{
public:
    int run;
    int buffsize;                   // size of input/output buffer
    float* in;                      // input buffer
    float* out;                     // output buffer
    int dline_size;                 // length of delay line which is 'double dline[length][2]'
    std::vector<float> dline;       // delay line
    double samplerate;              // samplerate, used to convert times into sample counts
    double tau;                     // transition time, signal<->zero
    double hangtime;                // time to stay at zero after noise is no longer detected
    double advtime;                 // deadtime (zero output) in advance of detected noise
    double backtau;                 // time constant used in averaging the magnitude of the input signal
    double threshold;               // triggers if (noise > threshold * average_signal_magnitude)
    std::vector<double> wave;       // array holding transition waveform
    int state;                      // state of the state machine
    double avg;                     // average value of the signal magnitude
    int dtime;                      // count when decreasing the signal magnitude
    int htime;                      // count when hanging
    int itime;                      // count when increasing the signal magnitude
    int atime;                      // count at zero before the noise burst (advance count)
    double coef;                    // parameter in calculating transition waveform
    int trans_count;                // number of samples to equal 'tau' time
    int hang_count;                 // number of samples to equal 'hangtime' time
    int adv_count;                  // number of samples to equal 'advtime' time
    int in_idx;                     // ring buffer position into which new samples are inserted
    int out_idx;                    // ring buffer position from which delayed samples are pulled
    double power;                   // level at which signal was increasing when a new decrease is started
    int count;                      // set each time a noise sample is detected, counts down
    double backmult;                // multiplier for waveform averaging
    double ombackmult;              // multiplier for waveform averaging

    ANB(
        int run,
        int buffsize,
        float* in,
        float* out,
        double samplerate,
        double tau,
        double hangtime,
        double advtime,
        double backtau,
        double threshold
    );
    ANB(const ANB&) = delete;
    ANB& operator=(const ANB& other) = delete;
    ~ANB() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSize(int size);
    // Common interface
    void setRun (int run);
    void setBuffsize (int size);
    void setSamplerate (int rate);
    void setTau (double tau);
    void setHangtime (double time);
    void setAdvtime (double time);
    void setBacktau (double tau);
    void setThreshold (double thresh);

private:
    void initBlanker();
};



} // namespace

#endif
