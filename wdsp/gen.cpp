/*  gen.c

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
#include <chrono>

#include "comm.hpp"
#include "gen.hpp"
#include "TXA.hpp"

namespace WDSP {

void GEN::calc_tone()
{
    tone.phs = 0.0;
    tone.delta = TWOPI * tone.freq / rate;
    tone.cosdelta = cos (tone.delta);
    tone.sindelta = sin (tone.delta);
}

void GEN::calc_tt()
{
    tt.phs1 = 0.0;
    tt.phs2 = 0.0;
    tt.delta1 = TWOPI * tt.f1 / rate;
    tt.delta2 = TWOPI * tt.f2 / rate;
    tt.cosdelta1 = cos (tt.delta1);
    tt.cosdelta2 = cos (tt.delta2);
    tt.sindelta1 = sin (tt.delta1);
    tt.sindelta2 = sin (tt.delta2);
}

void GEN::calc_sweep()
{
    sweep.phs = 0.0;
    sweep.dphs = TWOPI * sweep.f1 / rate;
    sweep.d2phs = TWOPI * sweep.sweeprate / (rate * rate);
    sweep.dphsmax = TWOPI * sweep.f2 / rate;
}

void GEN::calc_sawtooth()
{
    saw.period = 1.0 / saw.f;
    saw.delta = 1.0 / rate;
    saw.t = 0.0;
}

void GEN::calc_triangle()
{
    tri.period = 1.0 / tri.f;
    tri.half = 0.5 * tri.period;
    tri.delta = 1.0 / rate;
    tri.t = 0.0;
    tri.t1 = 0.0;
}

void GEN::calc_pulse ()
{
    double delta;
    double theta;
    pulse.pperiod = 1.0 / pulse.pf;
    pulse.tphs = 0.0;
    pulse.tdelta = TWOPI * pulse.tf / rate;
    pulse.tcosdelta = cos (pulse.tdelta);
    pulse.tsindelta = sin (pulse.tdelta);
    pulse.pntrans = (int)(pulse.ptranstime * rate);
    pulse.pnon = (int)(pulse.pdutycycle * pulse.pperiod * rate);
    pulse.pnoff = (int)(pulse.pperiod * rate) - pulse.pnon - 2 * pulse.pntrans;

    if (pulse.pnoff < 0)
        pulse.pnoff = 0;

    pulse.pcount = pulse.pnoff;
    pulse.state = PState::OFF;
    pulse.ctrans.resize(pulse.pntrans + 1);
    delta = PI / (float)pulse.pntrans;
    theta = 0.0;
    for (int i = 0; i <= pulse.pntrans; i++)
    {
        pulse.ctrans[i] = 0.5 * (1.0 - cos (theta));
        theta += delta;
    }
}

void GEN::calc()
{
    calc_tone();
    calc_tt();
    calc_sweep();
    calc_sawtooth();
    calc_triangle();
    calc_pulse();
}

GEN::GEN(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    int _mode
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate((double) _rate),
    mode(_mode)
{
    // tone
    tone.mag = 1.0;
    tone.freq = 1000.0;
    // two-tone
    tt.mag1 = 0.5;
    tt.mag2 = 0.5;
    tt.f1 = +  900.0;
    tt.f2 = + 1700.0;
    // noise
    srand ((unsigned int) time (nullptr));
    noise.mag = 1.0;
    // sweep
    sweep.mag = 1.0;
    sweep.f1 = -20000.0;
    sweep.f2 = +20000.0;
    sweep.sweeprate = +4000.0;
    // sawtooth
    saw.mag = 1.0;
    saw.f = 500.0;
    // triangle
    tri.mag = 1.0;
    tri.f = 500.0;
    // pulse
    pulse.mag = 1.0;
    pulse.pf = 0.25;
    pulse.pdutycycle = 0.25;
    pulse.ptranstime = 0.002;
    pulse.tf = 1000.0;
    calc();
}

void GEN::flush()
{
    pulse.state = PState::OFF;
}

void GEN::execute()
{
    if (run)
    {
        switch (mode)
        {
        case 0: // tone
            {
                double t1;
                double t2;
                double cosphase = cos (tone.phs);
                double sinphase = sin (tone.phs);
                for (int i = 0; i < size; i++)
                {
                    out[2 * i + 0] = (float) (+ tone.mag * cosphase);
                    out[2 * i + 1] = (float) (- tone.mag * sinphase);
                    t1 = cosphase;
                    t2 = sinphase;
                    cosphase = t1 * tone.cosdelta - t2 * tone.sindelta;
                    sinphase = t1 * tone.sindelta + t2 * tone.cosdelta;
                    tone.phs += tone.delta;
                    if (tone.phs >= TWOPI) tone.phs -= TWOPI;
                    if (tone.phs <   0.0 ) tone.phs += TWOPI;
                }
                break;
            }
        case 1: // two-tone
            {
                double tcos;
                double tsin;
                double cosphs1 = cos (tt.phs1);
                double sinphs1 = sin (tt.phs1);
                double cosphs2 = cos (tt.phs2);
                double sinphs2 = sin (tt.phs2);
                for (int i = 0; i < size; i++)
                {
                    out[2 * i + 0] = (float) (+ tt.mag1 * cosphs1 + tt.mag2 * cosphs2);
                    out[2 * i + 1] = (float) (- tt.mag1 * sinphs1 - tt.mag2 * sinphs2);
                    tcos = cosphs1;
                    tsin = sinphs1;
                    cosphs1 = tcos * tt.cosdelta1 - tsin * tt.sindelta1;
                    sinphs1 = tcos * tt.sindelta1 + tsin * tt.cosdelta1;
                    tt.phs1 += tt.delta1;
                    if (tt.phs1 >= TWOPI) tt.phs1 -= TWOPI;
                    if (tt.phs1 <   0.0 ) tt.phs1 += TWOPI;
                    tcos = cosphs2;
                    tsin = sinphs2;
                    cosphs2 = tcos * tt.cosdelta2 - tsin * tt.sindelta2;
                    sinphs2 = tcos * tt.sindelta2 + tsin * tt.cosdelta2;
                    tt.phs2 += tt.delta2;
                    if (tt.phs2 >= TWOPI) tt.phs2 -= TWOPI;
                    if (tt.phs2 <   0.0 ) tt.phs2 += TWOPI;
                }
                break;
            }
        case 2: // noise
            {
                double r1;
                double r2;
                double c;
                double rad;
                for (int i = 0; i < size; i++)
                {
                    do
                    {
                        r1 = 2.0 * (double)rand() / (double)RAND_MAX - 1.0;
                        r2 = 2.0 * (double)rand() / (double)RAND_MAX - 1.0;
                        c = r1 * r1 + r2 * r2;
                    } while (c >= 1.0);
                    rad = sqrt (-2.0 * log (c) / c);
                    out[2 * i + 0] = (float) (noise.mag * rad * r1);
                    out[2 * i + 1] = (float) (noise.mag * rad * r2);
                }
                break;
            }
        case 3:  // sweep
            {
                for (int i = 0; i < size; i++)
                {
                    out[2 * i + 0] = (float) (+ sweep.mag * cos(sweep.phs));
                    out[2 * i + 1] = (float) (- sweep.mag * sin(sweep.phs));
                    sweep.phs += sweep.dphs;
                    sweep.dphs += sweep.d2phs;
                    if (sweep.phs >= TWOPI) sweep.phs -= TWOPI;
                    if (sweep.phs <   0.0 ) sweep.phs += TWOPI;
                    if (sweep.dphs > sweep.dphsmax)
                        sweep.dphs = TWOPI * sweep.f1 / rate;
                }
                break;
            }
        case 4:  // sawtooth (audio only)
            {
                for (int i = 0; i < size; i++)
                {
                    if (saw.t > saw.period) saw.t -= saw.period;
                    out[2 * i + 0] = (float) (saw.mag * (saw.t * saw.f - 1.0));
                    out[2 * i + 1] = 0.0;
                    saw.t += saw.delta;
                }
            }
            break;
        case 5:  // triangle (audio only)
            {
                for (int i = 0; i < size; i++)
                {
                    if (tri.t > tri.period) tri.t1 = tri.t -= tri.period;
                    if (tri.t > tri.half) tri.t1 -= tri.delta;
                    else                        tri.t1 += tri.delta;
                    out[2 * i + 0] = (float) (tri.mag * (4.0 * tri.t1 * tri.f - 1.0));
                    out[2 * i + 1] = 0.0;
                    tri.t += tri.delta;
                }
            }
            break;
        case 6:  // pulse (audio only)
            {
                double t1;
                double t2;
                double cosphase = cos (pulse.tphs);
                double sinphase = sin (pulse.tphs);
                for (int i = 0; i < size; i++)
                {
                    if (pulse.pnoff != 0)
                        switch (pulse.state)
                        {
                        case PState::OFF:
                            out[2 * i + 0] = 0.0;
                            if (--pulse.pcount == 0)
                            {
                                pulse.state = PState::UP;
                                pulse.pcount = pulse.pntrans;
                            }
                            break;
                        case PState::UP:
                            out[2 * i + 0] = (float) (pulse.mag * cosphase * pulse.ctrans[pulse.pntrans - pulse.pcount]);
                            if (--pulse.pcount == 0)
                            {
                                pulse.state = PState::ON;
                                pulse.pcount = pulse.pnon;
                            }
                            break;
                        case PState::ON:
                            out[2 * i + 0] = (float) (pulse.mag * cosphase);
                            if (--pulse.pcount == 0)
                            {
                                pulse.state = PState::DOWN;
                                pulse.pcount = pulse.pntrans;
                            }
                            break;
                        case PState::DOWN:
                            out[2 * i + 0] = (float) (pulse.mag * cosphase * pulse.ctrans[pulse.pcount]);
                            if (--pulse.pcount == 0)
                            {
                                pulse.state = PState::OFF;
                                pulse.pcount = pulse.pnoff;
                            }
                            break;
                        }
                    else
                        out[2 * i + 0] = 0.0;
                    out[2 * i + 1] = 0.0;
                    t1 = cosphase;
                    t2 = sinphase;
                    cosphase = t1 * pulse.tcosdelta - t2 * pulse.tsindelta;
                    sinphase = t1 * pulse.tsindelta + t2 * pulse.tcosdelta;
                    pulse.tphs += pulse.tdelta;
                    if (pulse.tphs >= TWOPI) pulse.tphs -= TWOPI;
                    if (pulse.tphs <   0.0 ) pulse.tphs += TWOPI;
                }
            }
            break;
        default:    // silence
            {
                std::fill(out, out + size * 2, 0);
                break;
            }
        }
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void GEN::setBuffers(float* _in, float* _out)
{
    in = _in;
    out = _out;
}

void GEN::setSamplerate(int _rate)
{
    rate = _rate;
    calc();
}

void GEN::setSize(int _size)
{
    size = _size;
    flush();
}


/********************************************************************************************************
*                                                                                                       *
*                                           Public Properties                                           *
*                                                                                                       *
********************************************************************************************************/

// 'PreGen', gen0

void GEN::SetPreRun(int _run)
{
    run = _run;
}

void GEN::SetPreMode(int _mode)
{
    mode = _mode;
}

void GEN::SetPreToneMag(float mag)
{
    tone.mag = mag;
}

void GEN::SetPreToneFreq(float freq)
{
    tone.freq = freq;
    calc_tone();
}

void GEN::SetPreNoiseMag (float mag)
{
    noise.mag = mag;
}

void GEN::SetPreSweepMag(float mag)
{
    sweep.mag = mag;
}

void GEN::SetPreSweepFreq(float freq1, float freq2)
{
    sweep.f1 = freq1;
    sweep.f2 = freq2;
    calc_sweep();
}

void GEN::SetPreSweepRate(float _rate)
{
    sweep.sweeprate = _rate;
    calc_sweep();
}


/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

// 'PreGen', gen0

void GEN::SetPreGenRun (TXA& txa, int run)
{
    txa.gen0->run = run;
}

void GEN::SetPreGenMode (TXA& txa, int mode)
{
    txa.gen0->mode = mode;
}

void GEN::SetPreGenToneMag (TXA& txa, float mag)
{
    txa.gen0->tone.mag = mag;
}

void GEN::SetPreGenToneFreq (TXA& txa, float freq)
{
    txa.gen0->tone.freq = freq;
    txa.gen0->calc_tone();
}

void GEN::SetPreGenNoiseMag (TXA& txa, float mag)
{
    txa.gen0->noise.mag = mag;
}

void GEN::SetPreGenSweepMag (TXA& txa, float mag)
{
    txa.gen0->sweep.mag = mag;
}

void GEN::SetPreGenSweepFreq (TXA& txa, float freq1, float freq2)
{
    txa.gen0->sweep.f1 = freq1;
    txa.gen0->sweep.f2 = freq2;
    txa.gen0->calc_sweep();
}

void GEN::SetPreGenSweepRate (TXA& txa, float rate)
{
    txa.gen0->sweep.sweeprate = rate;
    txa.gen0->calc_sweep();
}

void GEN::SetPreGenSawtoothMag (TXA& txa, float mag)
{
    txa.gen0->saw.mag = mag;
}

void GEN::SetPreGenSawtoothFreq (TXA& txa, float freq)
{
    txa.gen0->saw.f = freq;
    txa.gen0->calc_sawtooth();
}

void GEN::SetPreGenTriangleMag (TXA& txa, float mag)
{
    txa.gen0->tri.mag = mag;
}

void GEN::SetPreGenTriangleFreq (TXA& txa, float freq)
{
    txa.gen0->tri.f = freq;
    txa.gen0->calc_triangle();
}

void GEN::SetPreGenPulseMag (TXA& txa, float mag)
{
    txa.gen0->pulse.mag = mag;
}

void GEN::SetPreGenPulseFreq (TXA& txa, float freq)
{
    txa.gen0->pulse.pf = freq;
    txa.gen0->calc_pulse();
}

void GEN::SetPreGenPulseDutyCycle (TXA& txa, float dc)
{
    txa.gen0->pulse.pdutycycle = dc;
    txa.gen0->calc_pulse();
}

void GEN::SetPreGenPulseToneFreq (TXA& txa, float freq)
{
    txa.gen0->pulse.tf = freq;
    txa.gen0->calc_pulse();
}

void GEN::SetPreGenPulseTransition (TXA& txa, float transtime)
{
    txa.gen0->pulse.ptranstime = transtime;
    txa.gen0->calc_pulse();
}

// 'PostGen', gen1

void GEN::SetPostGenRun (TXA& txa, int run)
{
    txa.gen1->run = run;
}

void GEN::SetPostGenMode (TXA& txa, int mode)
{
    txa.gen1->mode = mode;
}

void GEN::SetPostGenToneMag (TXA& txa, float mag)
{
    txa.gen1->tone.mag = mag;
}

void GEN::SetPostGenToneFreq (TXA& txa, float freq)
{
    txa.gen1->tone.freq = freq;
    txa.gen1->calc_tone();
}

void GEN::SetPostGenTTMag (TXA& txa, float mag1, float mag2)
{
    txa.gen1->tt.mag1 = mag1;
    txa.gen1->tt.mag2 = mag2;
}

void GEN::SetPostGenTTFreq (TXA& txa, float freq1, float freq2)
{
    txa.gen1->tt.f1 = freq1;
    txa.gen1->tt.f2 = freq2;
    txa.gen1->calc_tt();
}

void GEN::SetPostGenSweepMag (TXA& txa, float mag)
{
    txa.gen1->sweep.mag = mag;
}

void GEN::SetPostGenSweepFreq (TXA& txa, float freq1, float freq2)
{
    txa.gen1->sweep.f1 = freq1;
    txa.gen1->sweep.f2 = freq2;
    txa.gen1->calc_sweep();
}

void GEN::SetPostGenSweepRate (TXA& txa, float rate)
{
    txa.gen1->sweep.sweeprate = rate;
    txa.gen1->calc_sweep();
}

} // namespace WDSP
