/*  gen.h

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

#ifndef wdsp_gen_h
#define wdsp_gen_h

#include <vector>

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API GEN
{
public:
    enum class PState
    {
        OFF,
        UP,
        ON,
        DOWN
    };

    int run;                    // run
    int size;                   // number of samples per buffer
    float* in;                 // input buffer (retained in case I want to mix in a generated signal)
    float* out;                // output buffer
    double rate;                // sample rate
    int mode;
    struct _tone
    {
        double mag;
        double freq;
        double phs;
        double delta;
        double cosdelta;
        double sindelta;
    };
    _tone tone;
    struct _tt
    {
        double mag1;
        double mag2;
        double f1;
        double f2;
        double phs1;
        double phs2;
        double delta1;
        double delta2;
        double cosdelta1;
        double cosdelta2;
        double sindelta1;
        double sindelta2;
    };
    _tt tt;
    struct _noise
    {
        double mag;
    };
    _noise noise;
    struct _sweep
    {
        double mag;
        double f1;
        double f2;
        double sweeprate;
        double phs;
        double dphs;
        double d2phs;
        double dphsmax;
    };
    _sweep sweep;
    struct _saw
    {
        double mag;
        double f;
        double period;
        double delta;
        double t;
    };
    _saw saw;
    struct _tri
    {
        double mag;
        double f;
        double period;
        double half;
        double delta;
        double t;
        double t1;
    };
    _tri tri;
    struct _pulse
    {
        double mag;
        double pf;
        double pdutycycle;
        double ptranstime;
        std::vector<double> ctrans;
        int pcount;
        int pnon;
        int pntrans;
        int pnoff;
        double pperiod;
        double tf;
        double tphs;
        double tdelta;
        double tcosdelta;
        double tsindelta;
        PState state;
    };
    _pulse pulse;

    GEN(
        int run,
        int size,
        float* in,
        float* out,
        int rate,
        int mode
    );
    GEN(const GEN&) = delete;
    GEN& operator=(const GEN& other) = delete;
    ~GEN() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // Public Properties
    void SetPreRun(int run);
    void SetPreMode(int mode);
    void SetPreToneMag(float mag);
    void SetPreToneFreq(float freq);
    void SetPreNoiseMag(float mag);
    void SetPreSweepMag(float mag);
    void SetPreSweepFreq(float freq1, float freq2);
    void SetPreSweepRate(float rate);
    // TXA Properties
    static void SetPreGenRun (TXA& txa, int run);
    static void SetPreGenMode (TXA& txa, int mode);
    static void SetPreGenToneMag (TXA& txa, float mag);
    static void SetPreGenToneFreq (TXA& txa, float freq);
    static void SetPreGenNoiseMag (TXA& txa, float mag);
    static void SetPreGenSweepMag (TXA& txa, float mag);
    static void SetPreGenSweepFreq (TXA& txal, float freq1, float freq2);
    static void SetPreGenSweepRate (TXA& txa, float rate);
    static void SetPreGenSawtoothMag (TXA& txa, float mag);
    static void SetPreGenSawtoothFreq (TXA& txa, float freq);
    static void SetPreGenTriangleMag (TXA& txa, float mag);
    static void SetPreGenTriangleFreq (TXA& txa, float freq);
    static void SetPreGenPulseMag (TXA& txa, float mag);
    static void SetPreGenPulseFreq (TXA& txa, float freq);
    static void SetPreGenPulseDutyCycle (TXA& txa, float dc);
    static void SetPreGenPulseToneFreq (TXA& txa, float freq);
    static void SetPreGenPulseTransition (TXA& txa, float transtime);
    // 'PostGen', gen1
    static void SetPostGenRun (TXA& txa, int run);
    static void SetPostGenMode (TXA& txa, int mode);
    static void SetPostGenToneMag (TXA& txa, float mag);
    static void SetPostGenToneFreq (TXA& txa, float freq);
    static void SetPostGenTTMag (TXA& txa, float mag1, float mag2);
    static void SetgenRun (TXA& txa, int run);
    static void SetPostGenTTFreq (TXA& txa, float freq1, float freq2);
    static void SetPostGenSweepMag (TXA& txa, float mag);
    static void SetPostGenSweepFreq (TXA& txa, float freq1, float freq2);
    static void SetPostGenSweepRate (TXA& txa, float rate);

private:
    void calc_tone();
    void calc_tt();
    void calc_sweep();
    void calc_sawtooth();
    void calc_triangle();
    void calc_pulse();
    void calc();
};

} // namespace WDSP

#endif
