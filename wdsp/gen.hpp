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

#include "export.h"

namespace WDSP {

class RXA;
class TXA;

class WDSP_API GEN
{
public:
    int run;                    // run
    int size;                   // number of samples per buffer
    float* in;                 // input buffer (retained in case I want to mix in a generated signal)
    float* out;                // output buffer
    float rate;                // sample rate
    int mode;
    struct _tone
    {
        float mag;
        float freq;
        float phs;
        float delta;
        float cosdelta;
        float sindelta;
    } tone;
    struct _tt
    {
        float mag1;
        float mag2;
        float f1;
        float f2;
        float phs1;
        float phs2;
        float delta1;
        float delta2;
        float cosdelta1;
        float cosdelta2;
        float sindelta1;
        float sindelta2;
    } tt;
    struct _noise
    {
        float mag;
    } noise;
    struct _sweep
    {
        float mag;
        float f1;
        float f2;
        float sweeprate;
        float phs;
        float dphs;
        float d2phs;
        float dphsmax;
    } sweep;
    struct _saw
    {
        float mag;
        float f;
        float period;
        float delta;
        float t;
    } saw;
    struct _tri
    {
        float mag;
        float f;
        float period;
        float half;
        float delta;
        float t;
        float t1;
    } tri;
    struct _pulse
    {
        float mag;
        float pf;
        float pdutycycle;
        float ptranstime;
        float* ctrans;
        int pcount;
        int pnon;
        int pntrans;
        int pnoff;
        float pperiod;
        float tf;
        float tphs;
        float tdelta;
        float tcosdelta;
        float tsindelta;
        int state;
    } pulse;

    static GEN* create_gen (int run, int size, float* in, float* out, int rate, int mode);
    static void destroy_gen (GEN *a);
    static void flush_gen (GEN *a);
    static void xgen (GEN *a);
    static void setBuffers_gen (GEN *a, float* in, float* out);
    static void setSamplerate_gen (GEN *a, int rate);
    static void setSize_gen (GEN *a, int size);
    // RXA Properties
    static void SetPreGenRun (RXA& rxa, int run);
    static void SetPreGenMode (RXA& rxa, int mode);
    static void SetPreGenToneMag (RXA& rxa, float mag);
    static void SetPreGenToneFreq (RXA& rxa, float freq);
    static void SetPreGenNoiseMag (RXA& rxa, float mag);
    static void SetPreGenSweepMag (RXA& rxa, float mag);
    static void SetPreGenSweepFreq (RXA& rxa, float freq1, float freq2);
    static void SetPreGenSweepRate (RXA& rxa, float rate);
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
    static void calc_tone (GEN *a);
    static void calc_tt (GEN *a);
    static void calc_sweep (GEN *a);
    static void calc_sawtooth (GEN *a);
    static void calc_triangle (GEN *a);
    static void calc_pulse (GEN *a);
    static void calc_gen (GEN *a);
    static void decalc_gen (GEN *a);
};

} // namespace WDSP

#endif
