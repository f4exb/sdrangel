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

#include "comm.hpp"
#include "iir.hpp"
#include "firmin.hpp"
#include "fcurve.hpp"
#include "fir.hpp"
#include "wcpAGC.hpp"
#include "fmd.hpp"
#include "RXA.hpp"

namespace WDSP {

void FMD::calc_fmd (FMD *a)
{
    // pll
    a->omega_min = TWOPI * a->fmin / a->rate;
    a->omega_max = TWOPI * a->fmax / a->rate;
    a->g1 = 1.0 - exp(-2.0 * a->omegaN * a->zeta / a->rate);
    a->g2 = -a->g1 + 2.0 * (1 - exp(-a->omegaN * a->zeta / a->rate) * cos(a->omegaN / a->rate * sqrt(1.0 - a->zeta * a->zeta)));
    a->phs = 0.0;
    a->fil_out = 0.0;
    a->omega = 0.0;
    a->pllpole = a->omegaN * sqrt(2.0 * a->zeta * a->zeta + 1.0 + sqrt((2.0 * a->zeta * a->zeta + 1.0) * (2.0 * a->zeta * a->zeta + 1.0) + 1)) / TWOPI;
    // dc removal
    a->mtau = exp(-1.0 / (a->rate * a->tau));
    a->onem_mtau = 1.0 - a->mtau;
    a->fmdc = 0.0;
    // pll audio gain
    a->again = a->rate / (a->deviation * TWOPI);
    // CTCSS Removal
    a->sntch = SNOTCH::create_snotch(1, a->size, a->out, a->out, (int)a->rate, a->ctcss_freq, 0.0002);
    // detector limiter
    a->plim = WCPAGC::create_wcpagc (
        1,                                          // run - always ON
        5,                                          // mode
        1,                                          // 0 for max(I,Q), 1 for envelope
        a->out,                                     // input buff pointer
        a->out,                                     // output buff pointer
        a->size,                                    // io_buffsize
        (int)a->rate,                               // sample rate
        0.001,                                      // tau_attack
        0.008,                                      // tau_decay
        4,                                          // n_tau
        a->lim_gain,                                // max_gain (sets threshold, initial value)
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

void FMD::decalc_fmd (FMD *a)
{
    WCPAGC::destroy_wcpagc(a->plim);
    SNOTCH::destroy_snotch(a->sntch);
}

FMD* FMD::create_fmd(
    int run,
    int size,
    float* in,
    float* out,
    int rate,
    float deviation,
    float f_low,
    float f_high,
    float fmin,
    float fmax,
    float zeta,
    float omegaN,
    float tau,
    float afgain,
    int sntch_run,
    float ctcss_freq,
    int nc_de,
    int mp_de,
    int nc_aud,
    int mp_aud
)
{
    FMD *a = new FMD;
    float* impulse;
    a->run = run;
    a->size = size;
    a->in = in;
    a->out = out;
    a->rate = (float)rate;
    a->deviation = deviation;
    a->f_low = f_low;
    a->f_high = f_high;
    a->fmin = fmin;
    a->fmax = fmax;
    a->zeta = zeta;
    a->omegaN = omegaN;
    a->tau = tau;
    a->afgain = afgain;
    a->sntch_run = sntch_run;
    a->ctcss_freq = ctcss_freq;
    a->nc_de = nc_de;
    a->mp_de = mp_de;
    a->nc_aud = nc_aud;
    a->mp_aud = mp_aud;
    a->lim_run = 0;
    a->lim_pre_gain = 0.4;
    a->lim_gain = 2.5;
    calc_fmd (a);
    // de-emphasis filter
    a->audio = new float[a->size * 2]; // (float *) malloc0 (a->size * sizeof (complex));
    impulse = FCurve::fc_impulse (a->nc_de, a->f_low, a->f_high, +20.0 * log10(a->f_high / a->f_low), 0.0, 1, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    a->pde = FIRCORE::create_fircore (a->size, a->audio, a->out, a->nc_de, a->mp_de, impulse);
    delete[] (impulse);
    // audio filter
    impulse = FIR::fir_bandpass(a->nc_aud, 0.8 * a->f_low, 1.1 * a->f_high, a->rate, 0, 1, a->afgain / (2.0 * a->size));
    a->paud = FIRCORE::create_fircore (a->size, a->out, a->out, a->nc_aud, a->mp_aud, impulse);
    delete[] (impulse);
    return a;
}

void FMD::destroy_fmd (FMD *a)
{
    FIRCORE::destroy_fircore (a->paud);
    FIRCORE::destroy_fircore (a->pde);
    delete[] (a->audio);
    decalc_fmd (a);
    delete (a);
}

void FMD::flush_fmd (FMD *a)
{
    memset (a->audio, 0, a->size * sizeof (wcomplex));
    FIRCORE::flush_fircore (a->pde);
    FIRCORE::flush_fircore (a->paud);
    a->phs = 0.0;
    a->fil_out = 0.0;
    a->omega = 0.0;
    a->fmdc = 0.0;
    SNOTCH::flush_snotch (a->sntch);
    WCPAGC::flush_wcpagc (a->plim);
}

void FMD::xfmd (FMD *a)
{
    if (a->run)
    {
        int i;
        float det, del_out;
        float vco[2], corr[2];
        for (i = 0; i < a->size; i++)
        {
            // pll
            vco[0]  = cos (a->phs);
            vco[1]  = sin (a->phs);
            corr[0] = + a->in[2 * i + 0] * vco[0] + a->in[2 * i + 1] * vco[1];
            corr[1] = - a->in[2 * i + 0] * vco[1] + a->in[2 * i + 1] * vco[0];
            if ((corr[0] == 0.0) && (corr[1] == 0.0)) corr[0] = 1.0;
            det = atan2 (corr[1], corr[0]);
            del_out = a->fil_out;
            a->omega += a->g2 * det;
            if (a->omega < a->omega_min) a->omega = a->omega_min;
            if (a->omega > a->omega_max) a->omega = a->omega_max;
            a->fil_out = a->g1 * det + a->omega;
            a->phs += del_out;
            while (a->phs >= TWOPI) a->phs -= TWOPI;
            while (a->phs < 0.0) a->phs += TWOPI;
            // dc removal, gain, & demod output
            a->fmdc = a->mtau * a->fmdc + a->onem_mtau * a->fil_out;
            a->audio[2 * i + 0] = a->again * (a->fil_out - a->fmdc);
            a->audio[2 * i + 1] = a->audio[2 * i + 0];
        }
        // de-emphasis
        FIRCORE::xfircore (a->pde);
        // audio filter
        FIRCORE::xfircore (a->paud);
        // CTCSS Removal
        SNOTCH::xsnotch (a->sntch);
        if (a->lim_run)
        {
            for (i = 0; i < 2 * a->size; i++)
                a->out[i] *= a->lim_pre_gain;
            WCPAGC::xwcpagc (a->plim);
        }
    }
    else if (a->in != a->out)
        memcpy (a->out, a->in, a->size * sizeof (wcomplex));
}

void FMD::setBuffers_fmd (FMD *a, float* in, float* out)
{
    decalc_fmd (a);
    a->in = in;
    a->out = out;
    calc_fmd (a);
    FIRCORE::setBuffers_fircore (a->pde,  a->audio, a->out);
    FIRCORE::setBuffers_fircore (a->paud, a->out, a->out);
    WCPAGC::setBuffers_wcpagc (a->plim, a->out, a->out);
}

void FMD::setSamplerate_fmd (FMD *a, int rate)
{
    float* impulse;
    decalc_fmd (a);
    a->rate = rate;
    calc_fmd (a);
    // de-emphasis filter
    impulse = FCurve::fc_impulse (a->nc_de, a->f_low, a->f_high, +20.0 * log10(a->f_high / a->f_low), 0.0, 1, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    FIRCORE::setImpulse_fircore (a->pde, impulse, 1);
    delete[] (impulse);
    // audio filter
    impulse = FIR::fir_bandpass(a->nc_aud, 0.8 * a->f_low, 1.1 * a->f_high, a->rate, 0, 1, a->afgain / (2.0 * a->size));
    FIRCORE::setImpulse_fircore (a->paud, impulse, 1);
    delete[] (impulse);
    WCPAGC::setSamplerate_wcpagc (a->plim, (int)a->rate);
}

void FMD::setSize_fmd (FMD *a, int size)
{
    float* impulse;
    decalc_fmd (a);
    delete[] (a->audio);
    a->size = size;
    calc_fmd (a);
    a->audio = new float[a->size * 2]; // (float *) malloc0 (a->size * sizeof (complex));
    // de-emphasis filter
    FIRCORE::destroy_fircore (a->pde);
    impulse = FCurve::fc_impulse (a->nc_de, a->f_low, a->f_high, +20.0 * log10(a->f_high / a->f_low), 0.0, 1, a->rate, 1.0 / (2.0 * a->size), 0, 0);
    a->pde = FIRCORE::create_fircore (a->size, a->audio, a->out, a->nc_de, a->mp_de, impulse);
    delete[] (impulse);
    // audio filter
    FIRCORE::destroy_fircore (a->paud);
    impulse = FIR::fir_bandpass(a->nc_aud, 0.8 * a->f_low, 1.1 * a->f_high, a->rate, 0, 1, a->afgain / (2.0 * a->size));
    a->paud = FIRCORE::create_fircore (a->size, a->out, a->out, a->nc_aud, a->mp_aud, impulse);
    delete[] (impulse);
    WCPAGC::setSize_wcpagc (a->plim, a->size);
}

/********************************************************************************************************
*                                                                                                       *
*                                           RXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void FMD::SetFMDeviation (RXA& rxa, float deviation)
{
    FMD *a;
    rxa.csDSP.lock();
    a = rxa.fmd.p;
    a->deviation = deviation;
    a->again = a->rate / (a->deviation * TWOPI);
    rxa.csDSP.unlock();
}

void FMD::SetCTCSSFreq (RXA& rxa, float freq)
{
    FMD *a;
    rxa.csDSP.lock();
    a = rxa.fmd.p;
    a->ctcss_freq = freq;
    SNOTCH::SetSNCTCSSFreq (a->sntch, a->ctcss_freq);
    rxa.csDSP.unlock();
}

void FMD::SetCTCSSRun (RXA& rxa, int run)
{
    FMD *a;
    rxa.csDSP.lock();
    a = rxa.fmd.p;
    a->sntch_run = run;
    SNOTCH::SetSNCTCSSRun (a->sntch, a->sntch_run);
    rxa.csDSP.unlock();
}

void FMD::SetFMNCde (RXA& rxa, int nc)
{
    FMD *a;
    float* impulse;
    rxa.csDSP.lock();
    a = rxa.fmd.p;
    if (a->nc_de != nc)
    {
        a->nc_de = nc;
        impulse = FCurve::fc_impulse (a->nc_de, a->f_low, a->f_high, +20.0 * log10(a->f_high / a->f_low), 0.0, 1, a->rate, 1.0 / (2.0 * a->size), 0, 0);
        FIRCORE::setNc_fircore (a->pde, a->nc_de, impulse);
        delete[] (impulse);
    }
    rxa.csDSP.unlock();
}

void FMD::SetFMMPde (RXA& rxa, int mp)
{
    FMD *a;
    a = rxa.fmd.p;
    if (a->mp_de != mp)
    {
        a->mp_de = mp;
        FIRCORE::setMp_fircore (a->pde, a->mp_de);
    }
}

void FMD::SetFMNCaud (RXA& rxa, int nc)
{
    FMD *a;
    float* impulse;
    rxa.csDSP.lock();
    a = rxa.fmd.p;
    if (a->nc_aud != nc)
    {
        a->nc_aud = nc;
        impulse = FIR::fir_bandpass(a->nc_aud, 0.8 * a->f_low, 1.1 * a->f_high, a->rate, 0, 1, a->afgain / (2.0 * a->size));
        FIRCORE::setNc_fircore (a->paud, a->nc_aud, impulse);
        delete[] (impulse);
    }
    rxa.csDSP.unlock();
}

void FMD::SetFMMPaud (RXA& rxa, int mp)
{
    FMD *a;
    a = rxa.fmd.p;
    if (a->mp_aud != mp)
    {
        a->mp_aud = mp;
        FIRCORE::setMp_fircore (a->paud, a->mp_aud);
    }
}

void FMD::SetFMLimRun (RXA& rxa, int run)
{
    FMD *a;
    a = rxa.fmd.p;
    rxa.csDSP.lock();
    if (a->lim_run != run)
    {
        a->lim_run = run;
    }
    rxa.csDSP.unlock();
}

void FMD::SetFMLimGain (RXA& rxa, float gaindB)
{
    float gain = pow(10.0, gaindB / 20.0);
    FMD *a = rxa.fmd.p;
    rxa.csDSP.lock();
    if (a->lim_gain != gain)
    {
        decalc_fmd(a);
        a->lim_gain = gain;
        calc_fmd(a);
    }
    rxa.csDSP.unlock();
}

void FMD::SetFMAFFilter(RXA& rxa, float low, float high)
{
    FMD *a = rxa.fmd.p;
    float* impulse;
    rxa.csDSP.lock();
    if (a->f_low != low || a->f_high != high)
    {
        a->f_low = low;
        a->f_high = high;
        // de-emphasis filter
        impulse = FCurve::fc_impulse (a->nc_de, a->f_low, a->f_high, +20.0 * log10(a->f_high / a->f_low), 0.0, 1, a->rate, 1.0 / (2.0 * a->size), 0, 0);
        FIRCORE::setImpulse_fircore (a->pde, impulse, 1);
        delete[] (impulse);
        // audio filter
        impulse = FIR::fir_bandpass (a->nc_aud, 0.8 * a->f_low, 1.1 * a->f_high, a->rate, 0, 1, a->afgain / (2.0 * a->size));
        FIRCORE::setImpulse_fircore (a->paud, impulse, 1);
        delete[] (impulse);
    }
    rxa.csDSP.unlock();
}

} // namespace WDSP
