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
    double* in;                 // input buffer (retained in case I want to mix in a generated signal)
    double* out;                // output buffer
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
    } tone;
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
    } tt;
    struct _noise
    {
        double mag;
    } noise;
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
    } sweep;
    struct _saw
    {
        double mag;
        double f;
        double period;
        double delta;
        double t;
    } saw;
    struct _tri
    {
        double mag;
        double f;
        double period;
        double half;
        double delta;
        double t;
        double t1;
    } tri;
    struct _pulse
    {
        double mag;
        double pf;
        double pdutycycle;
        double ptranstime;
        double* ctrans;
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
        int state;
    } pulse;

    static GEN* create_gen (int run, int size, double* in, double* out, int rate, int mode);
    static void destroy_gen (GEN *a);
    static void flush_gen (GEN *a);
    static void xgen (GEN *a);
    static void setBuffers_gen (GEN *a, double* in, double* out);
    static void setSamplerate_gen (GEN *a, int rate);
    static void setSize_gen (GEN *a, int size);
    // RXA Properties
    static void SetPreGenRun (RXA& rxa, int run);
    static void SetPreGenMode (RXA& rxa, int mode);
    static void SetPreGenToneMag (RXA& rxa, double mag);
    static void SetPreGenToneFreq (RXA& rxa, double freq);
    static void SetPreGenNoiseMag (RXA& rxa, double mag);
    static void SetPreGenSweepMag (RXA& rxa, double mag);
    static void SetPreGenSweepFreq (RXA& rxa, double freq1, double freq2);
    static void SetPreGenSweepRate (RXA& rxa, double rate);
    // TXA Properties
    static void SetPreGenRun (TXA& txa, int run);
    static void SetPreGenMode (TXA& txa, int mode);
    static void SetPreGenToneMag (TXA& txa, double mag);
    static void SetPreGenToneFreq (TXA& txa, double freq);
    static void SetPreGenNoiseMag (TXA& txa, double mag);
    static void SetPreGenSweepMag (TXA& txa, double mag);
    static void SetPreGenSweepFreq (TXA& txal, double freq1, double freq2);
    static void SetPreGenSweepRate (TXA& txa, double rate);
    static void SetPreGenSawtoothMag (TXA& txa, double mag);
    static void SetPreGenSawtoothFreq (TXA& txa, double freq);
    static void SetPreGenTriangleMag (TXA& txa, double mag);
    static void SetPreGenTriangleFreq (TXA& txa, double freq);
    static void SetPreGenPulseMag (TXA& txa, double mag);
    static void SetPreGenPulseFreq (TXA& txa, double freq);
    static void SetPreGenPulseDutyCycle (TXA& txa, double dc);
    static void SetPreGenPulseToneFreq (TXA& txa, double freq);
    static void SetPreGenPulseTransition (TXA& txa, double transtime);
    // 'PostGen', gen1
    static void SetPostGenRun (TXA& txa, int run);
    static void SetPostGenMode (TXA& txa, int mode);
    static void SetPostGenToneMag (TXA& txa, double mag);
    static void SetPostGenToneFreq (TXA& txa, double freq);
    static void SetPostGenTTMag (TXA& txa, double mag1, double mag2);
    static void SetgenRun (TXA& txa, int run);
    static void SetPostGenTTFreq (TXA& txa, double freq1, double freq2);
    static void SetPostGenSweepMag (TXA& txa, double mag);
    static void SetPostGenSweepFreq (TXA& txa, double freq1, double freq2);
    static void SetPostGenSweepRate (TXA& txa, double rate);

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
