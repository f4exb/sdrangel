/*  fmd.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2023 Warren Pratt, NR0V
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

#include <array>

#include "comm.hpp"
#include "fircore.hpp"
#include "fcurve.hpp"
#include "fir.hpp"
#include "wcpAGC.hpp"
#include "snotch.hpp"
#include "fmd.hpp"

namespace WDSP {

void FMD::calc()
{
    // pll
    omega_min = TWOPI * fmin / rate;
    omega_max = TWOPI * fmax / rate;
    g1 = 1.0 - exp(-2.0 * omegaN * zeta / rate);
    g2 = -g1 + 2.0 * (1 - exp(-omegaN * zeta / rate) * cos(omegaN / rate * sqrt(1.0 - zeta * zeta)));
    phs = 0.0;
    fil_out = 0.0;
    omega = 0.0;
    pllpole = omegaN * sqrt(2.0 * zeta * zeta + 1.0 + sqrt((2.0 * zeta * zeta + 1.0) * (2.0 * zeta * zeta + 1.0) + 1)) / TWOPI;
    // dc removal
    mtau = exp(-1.0 / (rate * tau));
    onem_mtau = 1.0 - mtau;
    fmdc = 0.0;
    // pll audio gain
    again = rate / (deviation * TWOPI);
    // CTCSS Removal
    sntch = new SNOTCH(
        1,
        size,
        out,
        out,
        (int) rate,
        ctcss_freq,
        0.0002)
    ;
    // detector limiter
    plim = new WCPAGC(
        1,                                          // run - always ON
        5,                                          // mode
        1,                                          // 0 for max(I,Q), 1 for envelope
        out,                                     // input buff pointer
        out,                                     // output buff pointer
        size,                                    // io_buffsize
        (int)rate,                               // sample rate
        0.001,                                      // tau_attack
        0.008,                                      // tau_decay
        4,                                          // n_tau
        lim_gain,                                // max_gain (sets threshold, initial value)
        1.0,                                        // var_gain / slope
        1.0,                                        // fixed_gain
        1.0,                                        // max_input
        0.9,                                        // out_targ
        0.250,                                      // tau_fast_backaverage
        0.004,                                      // tau_fast_decay
        4.0,                                        // pop_ratio
        0,                                          // hang_enable
        0.500,                                      // tau_hang_backmult
        0.500,                                      // hangtime
        2.000,                                      // hang_thresh
        0.100);                                     // tau_hang_decay
}

void FMD::decalc()
{
    delete plim;
    delete sntch;
}

FMD::FMD(
    int _run,
    int _size,
    float* _in,
    float* _out,
    int _rate,
    double _deviation,
    double _f_low,
    double _f_high,
    double _fmin,
    double _fmax,
    double _zeta,
    double _omegaN,
    double _tau,
    double _afgain,
    int _sntch_run,
    double _ctcss_freq,
    int _nc_de,
    int _mp_de,
    int _nc_aud,
    int _mp_aud
) :
    run(_run),
    size(_size),
    in(_in),
    out(_out),
    rate((double) _rate),
    f_low(_f_low),
    f_high(_f_high),
    fmin(_fmin),
    fmax(_fmax),
    zeta(_zeta),
    omegaN(_omegaN),
    tau(_tau),
    deviation(_deviation),
    nc_de(_nc_de),
    mp_de(_mp_de),
    nc_aud(_nc_aud),
    mp_aud(_mp_aud),
    afgain(_afgain),
    sntch_run(_sntch_run),
    ctcss_freq(_ctcss_freq),
    lim_run(0),
    lim_gain(0.0001), // 2.5
    lim_pre_gain(0.01) // 0.4
{
    calc();
    // de-emphasis filter
    audio.resize(size * 2);
    std::vector<float> impulse(2 * nc_de);
    FCurve::fc_impulse (
        impulse,
        nc_de,
        (float) f_low,
        (float) f_high,
        (float) (+20.0 * log10(f_high / f_low)),
        0.0, 1,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0,
        0
    );
    pde = new FIRCORE(size, audio.data(), out, mp_de, impulse);
    // audio filter
    std::vector<float> impulseb;
    FIR::fir_bandpass(impulseb, nc_aud, 0.8 * f_low, 1.1 * f_high, rate, 0, 1, afgain / (2.0 * size));
    paud = new FIRCORE(size, out, out, mp_aud, impulseb);
}

FMD::~FMD()
{
    delete paud;
    delete pde;
    decalc();
}

void FMD::flush()
{
    std::fill(audio.begin(), audio.end(), 0);
    pde->flush();
    paud->flush();
    phs = 0.0;
    fil_out = 0.0;
    omega = 0.0;
    fmdc = 0.0;
    sntch->flush();
    plim->flush();
}

void FMD::execute()
{
    if (run)
    {
        int i;
        double det;
        double del_out;
        std::array<double, 2> vco;
        std::array<double, 2> corr;

        for (i = 0; i < size; i++)
        {
            // pll
            vco[0]  = cos (phs);
            vco[1]  = sin (phs);
            corr[0] = + in[2 * i + 0] * vco[0] + in[2 * i + 1] * vco[1];
            corr[1] = - in[2 * i + 0] * vco[1] + in[2 * i + 1] * vco[0];
            if ((corr[0] == 0.0) && (corr[1] == 0.0)) corr[0] = 1.0;
            det = atan2 (corr[1], corr[0]);
            del_out = fil_out;
            omega += g2 * det;
            if (omega < omega_min) omega = omega_min;
            if (omega > omega_max) omega = omega_max;
            fil_out = g1 * det + omega;
            phs += del_out;
            while (phs >= TWOPI) phs -= TWOPI;
            while (phs < 0.0) phs += TWOPI;
            // dc removal, gain, & demod output
            fmdc = mtau * fmdc + onem_mtau * fil_out;
            audio[2 * i + 0] = (float) (again * (fil_out - fmdc));
            audio[2 * i + 1] = audio[2 * i + 0];
        }
        // de-emphasis
        pde->execute();
        // audio filter
        paud->execute();
        // CTCSS Removal
        sntch->execute();
        if (lim_run)
        {
            for (i = 0; i < 2 * size; i++)
                out[i] *= (float) lim_pre_gain;
            plim->execute();
        }
    }
    else if (in != out)
        std::copy( in,  in + size * 2, out);
}

void FMD::setBuffers(float* _in, float* _out)
{
    decalc();
    in = _in;
    out = _out;
    calc();
    pde->setBuffers(audio.data(), out);
    paud->setBuffers(out, out);
    plim->setBuffers(out, out);
}

void FMD::setSamplerate(int _rate)
{
    decalc();
    rate = _rate;
    calc();
    // de-emphasis filter
    std::vector<float> impulse(2 * nc_de);
    FCurve::fc_impulse (
        impulse,
        nc_de,
        (float) f_low,
        (float) f_high,
        (float) (+20.0 * log10(f_high / f_low)),
        0.0,
        1,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0,
        0
    );
    pde->setImpulse(impulse, 1);
    // audio filter
    std::vector<float> impulseb;
    FIR::fir_bandpass(impulseb, nc_aud, 0.8 * f_low, 1.1 * f_high, rate, 0, 1, afgain / (2.0 * size));
    paud->setImpulse(impulseb, 1);
    plim->setSamplerate((int) rate);
}

void FMD::setSize(int _size)
{
    decalc();
    size = _size;
    calc();
    audio.resize(size * 2);
    // de-emphasis filter
    delete pde;
    std::vector<float> impulse(2 * nc_de);
    FCurve::fc_impulse (
        impulse,
        nc_de,
        (float) f_low,
        (float) f_high,
        (float) (+20.0 * log10(f_high / f_low)),
        0.0,
        1,
        (float) rate,
        (float) (1.0 / (2.0 * size)),
        0,
        0
    );
    pde = new FIRCORE(size, audio.data(), out, mp_de, impulse);
    // audio filter
    delete paud;
    std::vector<float> impulseb;
    FIR::fir_bandpass(impulseb, nc_aud, 0.8 * f_low, 1.1 * f_high, rate, 0, 1, afgain / (2.0 * size));
    paud = new FIRCORE(size, out, out, mp_aud, impulseb);
    plim->setSize(size);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMD::setDeviation(double _deviation)
{
    deviation = _deviation;
    again = rate / (deviation * TWOPI);
}

void FMD::setCTCSSFreq(double freq)
{
    ctcss_freq = freq;
    sntch->setFreq(ctcss_freq);
}

void FMD::setCTCSSRun(int _run)
{
    sntch_run = _run;
    sntch->setRun(sntch_run);
}

void FMD::setNCde(int nc)
{
    if (nc_de != nc)
    {
        nc_de = nc;
        std::vector<float> impulse(2 * nc_de);
        FCurve::fc_impulse (
            impulse,
            nc_de,
            (float) f_low,
            (float) f_high,
            (float) (+20.0 * log10(f_high / f_low)),
            0.0,
            1,
            (float) rate,
            (float) (1.0 / (2.0 * size)),
            0,
            0
        );
        pde->setNc(impulse);
    }
}

void FMD::setMPde(int mp)
{
    if (mp_de != mp)
    {
        mp_de = mp;
        pde->setMp(mp_de);
    }
}

void FMD::setNCaud(int nc)
{
    if (nc_aud != nc)
    {
        nc_aud = nc;
        std::vector<float> impulse;
        FIR::fir_bandpass(impulse, nc_aud, 0.8 * f_low, 1.1 * f_high, rate, 0, 1, afgain / (2.0 * size));
        paud->setNc(impulse);
    }
}

void FMD::setMPaud(int mp)
{
    if (mp_aud != mp)
    {
        mp_aud = mp;
        paud->setMp(mp_aud);
    }
}

void FMD::setLimRun(int _run)
{
    if (lim_run != _run) {
        lim_run = _run;
    }
}

void FMD::setLimGain(double gaindB)
{
    double gain = pow(10.0, gaindB / 20.0);

    if (lim_gain != gain)
    {
        decalc();
        lim_gain = gain;
        calc();
    }
}

void FMD::setAFFilter(double low, double high)
{
    if (f_low != low || f_high != high)
    {
        f_low = low;
        f_high = high;
        // de-emphasis filter
        std::vector<float> impulse(2 * nc_de);
        FCurve::fc_impulse (
            impulse,
            nc_de,
            (float) f_low,
            (float) f_high,
            (float) (+20.0 * log10(f_high / f_low)),
            0.0,
            1,
            (float) rate,
            (float) (1.0 / (2.0 * size)),
            0,
            0
        );
        pde->setImpulse(impulse, 1);
        // audio filter
        std::vector<float> impulseb;
        FIR::fir_bandpass (impulseb, nc_aud, 0.8 * f_low, 1.1 * f_high, rate, 0, 1, afgain / (2.0 * size));
        paud->setImpulse(impulseb, 1);
    }
}

} // namespace WDSP
