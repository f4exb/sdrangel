/*  TXA.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2017, 2021, 2023 Warren Pratt, NR0V
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

#include "ammod.hpp"
#include "meter.hpp"
#include "resample.hpp"
#include "patchpanel.hpp"
#include "amsq.hpp"
#include "eq.hpp"
#include "cfcomp.hpp"
#include "compress.hpp"
#include "bandpass.hpp"
#include "bps.hpp"
#include "osctrl.hpp"
#include "wcpAGC.hpp"
#include "emph.hpp"
#include "fmmod.hpp"
#include "siphon.hpp"
#include "gen.hpp"
#include "slew.hpp"
#include "iqc.hpp"
#include "cfir.hpp"
#include "fircore.hpp"
#include "phrot.hpp"
#include "fir.hpp"
#include "TXA.hpp"

namespace WDSP {

TXA* TXA::create_txa (
    int in_rate,                // input samplerate
    int out_rate,               // output samplerate
    int dsp_rate,               // sample rate for mainstream dsp processing
    int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
)
{
    TXA *txa = new TXA;

    txa->in_rate = in_rate;
    txa->out_rate = out_rate;
    txa->dsp_rate = dsp_rate;
    txa->dsp_size = dsp_size;

    if (in_rate  >= dsp_rate)
        txa->dsp_insize  = dsp_size * (in_rate  / dsp_rate);
    else
        txa->dsp_insize  = dsp_size / (dsp_rate /  in_rate);

    if (out_rate >= dsp_rate)
        txa->dsp_outsize = dsp_size * (out_rate / dsp_rate);
    else
        txa->dsp_outsize = dsp_size / (dsp_rate / out_rate);

    txa->mode   = TXA_LSB;
    txa->f_low  = -5000.0;
    txa->f_high = - 100.0;
    txa->inbuff  = new float[1 * txa->dsp_insize  * 2]; // (float *) malloc0 (1 * txa->dsp_insize  * sizeof (complex));
    txa->outbuff = new float[1 * txa->dsp_outsize  * 2]; // (float *) malloc0 (1 * txa->dsp_outsize * sizeof (complex));
    txa->midbuff = new float[3 * txa->dsp_size  * 2]; //(float *) malloc0 (2 * txa->dsp_size    * sizeof (complex));

    txa->rsmpin = new RESAMPLE(
        0,                                          // run - will be turned on below if needed
        txa->dsp_insize,                     // input buffer size
        txa->inbuff,                        // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        txa->in_rate,                        // input sample rate
        txa->dsp_rate,                       // output sample rate
        0.0,                                        // select cutoff automatically
        0,                                          // select ncoef automatically
        1.0);                                       // gain

    txa->gen0 = new GEN(
        0,                                          // run
        txa->dsp_size,                       // buffer size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // sample rate
        2);                                         // mode

    txa->panel = PANEL::create_panel (
        1,                                          // run
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        1.0,                                        // gain1
        1.0,                                        // gain2I
        1.0,                                        // gain2Q
        2,                                          // 1 to use Q, 2 to use I for input
        0);                                         // 0, no copy

    txa->phrot = new PHROT(
        0,                                          // run
        txa->dsp_size,                       // size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // samplerate
        338.0,                                      // 1/2 of phase frequency
        8);                                         // number of stages

    txa->micmeter = new METER(
        1,                                          // run
        0,                                          // optional pointer to another 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_MIC_AV,                                 // index for average value
        TXA_MIC_PK,                                 // index for peak value
        -1,                                         // index for gain value
        0);                                         // pointer for gain computation

    txa->amsq = new AMSQ(
        0,                                          // run
        txa->dsp_size,                       // size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->midbuff,                       // trigger buffer
        txa->dsp_rate,                       // sample rate
        0.010,                                      // time constant for averaging signal
        0.004,                                      // up-slew time
        0.004,                                      // down-slew time
        0.180,                                      // signal level to initiate tail
        0.200,                                      // signal level to initiate unmute
        0.000,                                      // minimum tail length
        0.025,                                      // maximum tail length
        0.200);                                     // muted gain

    {
        float default_F[11] = {0.0,  32.0,  63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
        float default_G[11] = {0.0, -12.0, -12.0, -12.0,  -1.0,  +1.0,   +4.0,   +9.0,  +12.0,  -10.0,   -10.0};
        //float default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
        txa->eqp = new EQP (
            0,                                          // run - OFF by default
            txa->dsp_size,                       // size
            std::max(2048, txa->dsp_size),            // number of filter coefficients
            0,                                          // minimum phase flag
            txa->midbuff,                       // pointer to input buffer
            txa->midbuff,                       // pointer to output buffer
            10,                                         // nfreqs
            default_F,                                  // vector of frequencies
            default_G,                                  // vector of gain values
            0,                                          // cutoff mode
            0,                                          // wintype
            txa->dsp_rate);                      // samplerate
    }

    txa->eqmeter = new METER(
        1,                                          // run
        &(txa->eqp->run),                 // pointer to eqp 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_EQ_AV,                                  // index for average value
        TXA_EQ_PK,                                  // index for peak value
        -1,                                         // index for gain value
        0);                                         // pointer for gain computation

    txa->preemph = EMPHP::create_emphp (
        0,                                          // run
        1,                                          // position
        txa->dsp_size,                       // size
        std::max(2048, txa->dsp_size),            // number of filter coefficients
        0,                                          // minimum phase flag
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer,
        txa->dsp_rate,                       // sample rate
        0,                                          // pre-emphasis type
        300.0,                                      // f_low
        3000.0);                                    // f_high

    txa->leveler = new WCPAGC(
        0,                                          // run - OFF by default
        5,                                          // mode
        0,                                          // 0 for max(I,Q), 1 for envelope
        txa->midbuff,                       // input buff pointer
        txa->midbuff,                       // output buff pointer
        txa->dsp_size,                       // io_buffsize
        txa->dsp_rate,                       // sample rate
        0.001,                                      // tau_attack
        0.500,                                      // tau_decay
        6,                                          // n_tau
        1.778,                                      // max_gain
        1.0,                                        // var_gain
        1.0,                                        // fixed_gain
        1.0,                                        // max_input
        1.05,                                       // out_targ
        0.250,                                      // tau_fast_backaverage
        0.005,                                      // tau_fast_decay
        5.0,                                        // pop_ratio
        0,                                          // hang_enable
        0.500,                                      // tau_hang_backmult
        0.500,                                      // hangtime
        2.000,                                      // hang_thresh
        0.100);                                     // tau_hang_decay

    txa->lvlrmeter = new METER(
        1,                                          // run
        &(txa->leveler->run),             // pointer to leveler 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_LVLR_AV,                                // index for average value
        TXA_LVLR_PK,                                // index for peak value
        TXA_LVLR_GAIN,                              // index for gain value
        &txa->leveler->gain);             // pointer for gain computation

    {
        float default_F[5] = {200.0, 1000.0, 2000.0, 3000.0, 4000.0};
        float default_G[5] = {0.0, 5.0, 10.0, 10.0, 5.0};
        float default_E[5] = {7.0, 7.0, 7.0, 7.0, 7.0};
        txa->cfcomp = CFCOMP::create_cfcomp(
            0,                                          // run
            0,                                          // position
            0,                                          // post-equalizer run
            txa->dsp_size,                       // size
            txa->midbuff,                       // input buffer
            txa->midbuff,                       // output buffer
            2048,                                       // fft size
            4,                                          // overlap
            txa->dsp_rate,                       // samplerate
            1,                                          // window type
            0,                                          // compression method
            5,                                          // nfreqs
            0.0,                                        // pre-compression
            0.0,                                        // pre-postequalization
            default_F,                                  // frequency array
            default_G,                                  // compression array
            default_E,                                  // eq array
            0.25,                                       // metering time constant
            0.50);                                      // display time constant
    }

    txa->cfcmeter = new METER(
        1,                                          // run
        &(txa->cfcomp->run),              // pointer to eqp 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_CFC_AV,                                 // index for average value
        TXA_CFC_PK,                                 // index for peak value
        TXA_CFC_GAIN,                               // index for gain value
        (double*) &txa->cfcomp->gain);              // pointer for gain computation

    txa->bp0 = new BANDPASS(
        1,                                          // always runs
        0,                                          // position
        txa->dsp_size,                       // size
        std::max(2048, txa->dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        txa->f_low,                         // low freq cutoff
        txa->f_high,                        // high freq cutoff
        txa->dsp_rate,                       // samplerate
        1,                                          // wintype
        2.0);                                       // gain

    txa->compressor = COMPRESSOR::create_compressor (
        0,                                          // run - OFF by default
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        3.0);                                       // gain

    txa->bp1 = new BANDPASS(
        0,                                          // ONLY RUNS WHEN COMPRESSOR IS USED
        0,                                          // position
        txa->dsp_size,                       // size
        std::max(2048, txa->dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        txa->f_low,                         // low freq cutoff
        txa->f_high,                        // high freq cutoff
        txa->dsp_rate,                       // samplerate
        1,                                          // wintype
        2.0);                                       // gain

    txa->osctrl = OSCTRL::create_osctrl (
        0,                                          // run
        txa->dsp_size,                       // size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // sample rate
        1.95);                                      // gain for clippings

    txa->bp2 = new BANDPASS(
        0,                                          // ONLY RUNS WHEN COMPRESSOR IS USED
        0,                                          // position
        txa->dsp_size,                       // size
        std::max(2048, txa->dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        txa->f_low,                         // low freq cutoff
        txa->f_high,                        // high freq cutoff
        txa->dsp_rate,                       // samplerate
        1,                                          // wintype
        1.0);                                       // gain

    txa->compmeter = new METER(
        1,                                          // run
        &(txa->compressor->run),          // pointer to compressor 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_COMP_AV,                                // index for average value
        TXA_COMP_PK,                                // index for peak value
        -1,                                         // index for gain value
        0);                                         // pointer for gain computation

    txa->alc = new WCPAGC(
        1,                                          // run - always ON
        5,                                          // mode
        1,                                          // 0 for max(I,Q), 1 for envelope
        txa->midbuff,                       // input buff pointer
        txa->midbuff,                       // output buff pointer
        txa->dsp_size,                       // io_buffsize
        txa->dsp_rate,                       // sample rate
        0.001,                                      // tau_attack
        0.010,                                      // tau_decay
        6,                                          // n_tau
        1.0,                                        // max_gain
        1.0,                                        // var_gain
        1.0,                                        // fixed_gain
        1.0,                                        // max_input
        1.0,                                        // out_targ
        0.250,                                      // tau_fast_backaverage
        0.005,                                      // tau_fast_decay
        5.0,                                        // pop_ratio
        0,                                          // hang_enable
        0.500,                                      // tau_hang_backmult
        0.500,                                      // hangtime
        2.000,                                      // hang_thresh
        0.100);                                     // tau_hang_decay

    txa->ammod = AMMOD::create_ammod (
        0,                                          // run - OFF by default
        0,                                          // mode:  0=>AM, 1=>DSB
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to output buffer
        0.5);                                       // carrier level


    txa->fmmod = FMMOD::create_fmmod (
        0,                                          // run - OFF by default
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to input buffer
        txa->midbuff,                       // pointer to input buffer
        txa->dsp_rate,                       // samplerate
        5000.0,                                     // deviation
        300.0,                                      // low cutoff frequency
        3000.0,                                     // high cutoff frequency
        1,                                          // ctcss run control
        0.10,                                       // ctcss level
        100.0,                                      // ctcss frequency
        1,                                          // run bandpass filter
        std::max(2048, txa->dsp_size),            // number coefficients for bandpass filter
        0);                                         // minimum phase flag

    txa->gen1 = new GEN(
        0,                                          // run
        txa->dsp_size,                       // buffer size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // sample rate
        0);                                         // mode

    txa->uslew = USLEW::create_uslew (
        txa,
        &(txa->upslew),                        // pointer to channel upslew flag
        txa->dsp_size,                       // buffer size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // sample rate
        0.000,                                      // delay time
        0.005);                                     // upslew time

    txa->alcmeter = new METER(
        1,                                          // run
        0,                                          // optional pointer to a 'run'
        txa->dsp_size,                       // size
        txa->midbuff,                       // pointer to buffer
        txa->dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_ALC_AV,                                 // index for average value
        TXA_ALC_PK,                                 // index for peak value
        TXA_ALC_GAIN,                               // index for gain value
        &txa->alc->gain);                 // pointer for gain computation

    txa->sip1 = new SIPHON(
        1,                                          // run
        0,                                          // position
        0,                                          // mode
        0,                                          // disp
        txa->dsp_size,                       // input buffer size
        txa->midbuff,                       // input buffer
        16384,                                      // number of samples to buffer
        16384,                                      // fft size for spectrum
        1);                                         // specmode

    // txa->calcc = create_calcc (
    //     channel,                                    // channel number
    //     1,                                          // run calibration
    //     1024,                                       // input buffer size
    //     txa->in_rate,                        // samplerate
    //     16,                                         // ints
    //     256,                                        // spi
    //     (1.0 / 0.4072),                             // hw_scale
    //     0.1,                                        // mox delay
    //     0.0,                                        // loop delay
    //     0.8,                                        // ptol
    //     0,                                          // mox
    //     0,                                          // solidmox
    //     1,                                          // pin mode
    //     1,                                          // map mode
    //     0,                                          // stbl mode
    //     256,                                        // pin samples
    //     0.9);                                       // alpha

    txa->iqc.p0 = txa->iqc.p1 = IQC::create_iqc (
        0,                                          // run
        txa->dsp_size,                       // size
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        (float)txa->dsp_rate,               // sample rate
        16,                                         // ints
        0.005,                                      // changeover time
        256);                                       // spi

    txa->cfir = CFIR::create_cfir(
        0,                                          // run
        txa->dsp_size,                       // size
        std::max(2048, txa->dsp_size),            // number of filter coefficients
        0,                                          // minimum phase flag
        txa->midbuff,                       // input buffer
        txa->midbuff,                       // output buffer
        txa->dsp_rate,                       // input sample rate
        txa->out_rate,                       // CIC input sample rate
        1,                                          // CIC differential delay
        640,                                        // CIC interpolation factor
        5,                                          // CIC integrator-comb pairs
        20000.0,                                    // cutoff frequency
        2,                                          // brick-wall windowed rolloff
        0.0,                                        // raised-cosine transition width
        0);                                         // window type

    txa->rsmpout = new RESAMPLE(
        0,                                          // run - will be turned ON below if needed
        txa->dsp_size,                       // input size
        txa->midbuff,                       // pointer to input buffer
        txa->outbuff,                       // pointer to output buffer
        txa->dsp_rate,                       // input sample rate
        txa->out_rate,                       // output sample rate
        0.0,                                        // select cutoff automatically
        0,                                          // select ncoef automatically
        0.980);                                     // gain

    txa->outmeter = new METER(
        1,                                          // run
        0,                                          // optional pointer to another 'run'
        txa->dsp_outsize,                    // size
        txa->outbuff,                       // pointer to buffer
        txa->out_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        txa->meter,                         // result vector
        TXA_OUT_AV,                                 // index for average value
        TXA_OUT_PK,                                 // index for peak value
        -1,                                         // index for gain value
        0);                                         // pointer for gain computation

    // turn OFF / ON resamplers as needed
    ResCheck (*txa);
    return txa;
}

void TXA::destroy_txa (TXA *txa)
{
    // in reverse order, free each item we created
    delete (txa->outmeter);
    delete (txa->rsmpout);
    CFIR::destroy_cfir(txa->cfir);
    // destroy_calcc (txa->calcc);
    IQC::destroy_iqc (txa->iqc.p0);
    delete (txa->sip1);
    delete (txa->alcmeter);
    USLEW::destroy_uslew (txa->uslew);
    delete (txa->gen1);
    FMMOD::destroy_fmmod (txa->fmmod);
    AMMOD::destroy_ammod (txa->ammod);
    delete (txa->alc);
    delete (txa->compmeter);
    delete (txa->bp2);
    OSCTRL::destroy_osctrl (txa->osctrl);
    delete (txa->bp1);
    COMPRESSOR::destroy_compressor (txa->compressor);
    delete (txa->bp0);
    delete (txa->cfcmeter);
    CFCOMP::destroy_cfcomp (txa->cfcomp);
    delete (txa->lvlrmeter);
    delete (txa->leveler);
    EMPHP::destroy_emphp (txa->preemph);
    delete (txa->eqmeter);
    delete (txa->eqp);
    delete (txa->amsq);
    delete (txa->micmeter);
    delete (txa->phrot);
    PANEL::destroy_panel (txa->panel);
    delete (txa->gen0);
    delete (txa->rsmpin);
    delete[] (txa->midbuff);
    delete[] (txa->outbuff);
    delete[] (txa->inbuff);
    delete txa;
}

void TXA::flush_txa (TXA* txa)
{
    std::fill(txa->inbuff,  txa->inbuff  + 1 * txa->dsp_insize  * 2, 0);
    std::fill(txa->outbuff, txa->outbuff + 1 * txa->dsp_outsize * 2, 0);
    std::fill(txa->midbuff, txa->midbuff + 2 * txa->dsp_size    * 2, 0);
    txa->rsmpin->flush();
    txa->gen0->flush();
    PANEL::flush_panel (txa->panel);
    txa->phrot->flush();
    txa->micmeter->flush ();
    txa->amsq->flush ();
    txa->eqp->flush();
    txa->eqmeter->flush ();
    EMPHP::flush_emphp (txa->preemph);
    txa->leveler->flush();
    txa->lvlrmeter->flush ();
    CFCOMP::flush_cfcomp (txa->cfcomp);
    txa->cfcmeter->flush ();
    txa->bp0->flush ();
    COMPRESSOR::flush_compressor (txa->compressor);
    txa->bp1->flush ();
    OSCTRL::flush_osctrl (txa->osctrl);
    txa->bp2->flush ();
    txa->compmeter->flush ();
    txa->alc->flush ();
    AMMOD::flush_ammod (txa->ammod);
    FMMOD::flush_fmmod (txa->fmmod);
    txa->gen1->flush();
    USLEW::flush_uslew (txa->uslew);
    txa->alcmeter->flush ();
    txa->sip1->flush();
    IQC::flush_iqc (txa->iqc.p0);
    CFIR::flush_cfir(txa->cfir);
    txa->rsmpout->flush();
    txa->outmeter->flush ();
}

void xtxa (TXA* txa)
{
    txa->rsmpin->execute();              // input resampler
    txa->gen0->execute();                     // input signal generator
    PANEL::xpanel (txa->panel);                  // includes MIC gain
    txa->phrot->execute();                  // phase rotator
    txa->micmeter->execute ();               // MIC meter
    txa->amsq->xcap ();                 // downward expander capture
    txa->amsq->execute ();                    // downward expander action
    txa->eqp->execute ();                      // pre-EQ
    txa->eqmeter->execute ();                // EQ meter
    EMPHP::xemphp (txa->preemph, 0);             // FM pre-emphasis (first option)
    txa->leveler->execute ();               // Leveler
    txa->lvlrmeter->execute ();              // Leveler Meter
    CFCOMP::xcfcomp (txa->cfcomp, 0);             // Continuous Frequency Compressor with post-EQ
    txa->cfcmeter->execute ();               // CFC+PostEQ Meter
    txa->bp0->execute (0);              // primary bandpass filter
    COMPRESSOR::xcompressor (txa->compressor);        // COMP compressor
    txa->bp1->execute (0);              // aux bandpass (runs if COMP)
    OSCTRL::xosctrl (txa->osctrl);                // CESSB Overshoot Control
    txa->bp2->execute (0);              // aux bandpass (runs if CESSB)
    txa->compmeter->execute ();              // COMP meter
    txa->alc->execute ();                   // ALC
    AMMOD::xammod (txa->ammod);                  // AM Modulator
    EMPHP::xemphp (txa->preemph, 1);             // FM pre-emphasis (second option)
    FMMOD::xfmmod (txa->fmmod);                  // FM Modulator
    txa->gen1->execute();                     // output signal generator (TUN and Two-tone)
    USLEW::xuslew (txa->uslew);                  // up-slew for AM, FM, and gens
    txa->alcmeter->execute ();               // ALC Meter
    txa->sip1->execute(0);               // siphon data for display
    IQC::xiqc (txa->iqc.p0);                     // PureSignal correction
    CFIR::xcfir(txa->cfir);                     // compensating FIR filter (used Protocol_2 only)
    txa->rsmpout->execute();             // output resampler
    txa->outmeter->execute ();               // output meter
    // print_peak_env ("env_exception.txt", txa->dsp_outsize, txa->outbuff, 0.7);
}

void TXA::setInputSamplerate (TXA *txa, int in_rate)
{
    if (in_rate  >= txa->dsp_rate)
        txa->dsp_insize  = txa->dsp_size * (in_rate  / txa->dsp_rate);
    else
        txa->dsp_insize  = txa->dsp_size / (txa->dsp_rate /  in_rate);

    txa->in_rate = in_rate;
    // buffers
    delete[] (txa->inbuff);
    txa->inbuff = new float[1 * txa->dsp_insize  * 2]; //(float *)malloc0(1 * txa->dsp_insize  * sizeof(complex));
    // input resampler
    txa->rsmpin->setBuffers(txa->inbuff, txa->midbuff);
    txa->rsmpin->setSize(txa->dsp_insize);
    txa->rsmpin->setInRate(txa->in_rate);
    ResCheck (*txa);
}

void TXA::setOutputSamplerate (TXA* txa, int out_rate)
{
    if (out_rate >= txa->dsp_rate)
        txa->dsp_outsize = txa->dsp_size * (out_rate / txa->dsp_rate);
    else
        txa->dsp_outsize = txa->dsp_size / (txa->dsp_rate / out_rate);

    txa->out_rate = out_rate;
    // buffers
    delete[] (txa->outbuff);
    txa->outbuff = new float[1 * txa->dsp_outsize * 2]; // (float *)malloc0(1 * txa->dsp_outsize * sizeof(complex));
    // cfir - needs to know input rate of firmware CIC
    CFIR::setOutRate_cfir (txa->cfir, txa->out_rate);
    // output resampler
    txa->rsmpout->setBuffers(txa->midbuff, txa->outbuff);
    txa->rsmpout->setOutRate(txa->out_rate);
    ResCheck (*txa);
    // output meter
    txa->outmeter->setBuffers(txa->outbuff);
    txa->outmeter->setSize(txa->dsp_outsize);
    txa->outmeter->setSamplerate (txa->out_rate);
}

void TXA::setDSPSamplerate (TXA *txa, int dsp_rate)
{
    if (txa->in_rate  >= dsp_rate)
        txa->dsp_insize  = txa->dsp_size * (txa->in_rate  / dsp_rate);
    else
        txa->dsp_insize  = txa->dsp_size / (dsp_rate /  txa->in_rate);

    if (txa->out_rate >= dsp_rate)
        txa->dsp_outsize = txa->dsp_size * (txa->out_rate / dsp_rate);
    else
        txa->dsp_outsize = txa->dsp_size / (dsp_rate / txa->out_rate);

    txa->dsp_rate = dsp_rate;
    // buffers
    delete[] (txa->inbuff);
    txa->inbuff = new float[1 * txa->dsp_insize  * 2]; // (float *)malloc0(1 * txa->dsp_insize  * sizeof(complex));
    delete[] (txa->outbuff);
    txa->outbuff = new float[1 * txa->dsp_outsize  * 2]; // (float *)malloc0(1 * txa->dsp_outsize * sizeof(complex));
    // input resampler
    txa->rsmpin->setBuffers(txa->inbuff, txa->midbuff);
    txa->rsmpin->setSize(txa->dsp_insize);
    txa->rsmpin->setOutRate(txa->dsp_rate);
    // dsp_rate blocks
    txa->gen0->setSamplerate(txa->dsp_rate);
    PANEL::setSamplerate_panel (txa->panel, txa->dsp_rate);
    txa->phrot->setSamplerate(txa->dsp_rate);
    txa->micmeter->setSamplerate (txa->dsp_rate);
    txa->amsq->setSamplerate (txa->dsp_rate);
    txa->eqp->setSamplerate (txa->dsp_rate);
    txa->eqmeter->setSamplerate (txa->dsp_rate);
    EMPHP::setSamplerate_emphp (txa->preemph, txa->dsp_rate);
    txa->leveler->setSamplerate (txa->dsp_rate);
    txa->lvlrmeter->setSamplerate (txa->dsp_rate);
    CFCOMP::setSamplerate_cfcomp (txa->cfcomp, txa->dsp_rate);
    txa->cfcmeter->setSamplerate (txa->dsp_rate);
    txa->bp0->setSamplerate (txa->dsp_rate);
    COMPRESSOR::setSamplerate_compressor (txa->compressor, txa->dsp_rate);
    txa->bp1->setSamplerate (txa->dsp_rate);
    OSCTRL::setSamplerate_osctrl (txa->osctrl, txa->dsp_rate);
    txa->bp2->setSamplerate (txa->dsp_rate);
    txa->compmeter->setSamplerate (txa->dsp_rate);
    txa->alc->setSamplerate (txa->dsp_rate);
    AMMOD::setSamplerate_ammod (txa->ammod, txa->dsp_rate);
    FMMOD::setSamplerate_fmmod (txa->fmmod, txa->dsp_rate);
    txa->gen1->setSamplerate(txa->dsp_rate);
    USLEW::setSamplerate_uslew (txa->uslew, txa->dsp_rate);
    txa->alcmeter->setSamplerate (txa->dsp_rate);
    txa->sip1->setSamplerate (txa->dsp_rate);
    IQC::setSamplerate_iqc (txa->iqc.p0, txa->dsp_rate);
    CFIR::setSamplerate_cfir (txa->cfir, txa->dsp_rate);
    // output resampler
    txa->rsmpout->setBuffers(txa->midbuff, txa->outbuff);
    txa->rsmpout->setInRate(txa->dsp_rate);
    ResCheck (*txa);
    // output meter
    txa->outmeter->setBuffers(txa->outbuff);
    txa->outmeter->setSize (txa->dsp_outsize);
}

void TXA::setDSPBuffsize (TXA *txa, int dsp_size)
{
    if (txa->in_rate  >= txa->dsp_rate)
        txa->dsp_insize  = dsp_size * (txa->in_rate  / txa->dsp_rate);
    else
        txa->dsp_insize  = dsp_size / (txa->dsp_rate /  txa->in_rate);

    if (txa->out_rate >= txa->dsp_rate)
        txa->dsp_outsize = dsp_size * (txa->out_rate / txa->dsp_rate);
    else
        txa->dsp_outsize = dsp_size / (txa->dsp_rate / txa->out_rate);

    txa->dsp_size = dsp_size;
    // buffers
    delete[] (txa->inbuff);
    txa->inbuff = new float[1 * txa->dsp_insize  * 2]; // (float *)malloc0(1 * txa->dsp_insize  * sizeof(complex));
    delete[] (txa->midbuff);
    txa->midbuff = new float[2 * txa->dsp_size  * 2]; // (float *)malloc0(2 * txa->dsp_size * sizeof(complex));
    delete[] (txa->outbuff);
    txa->outbuff = new float[1 * txa->dsp_outsize  * 2]; // (float *)malloc0(1 * txa->dsp_outsize * sizeof(complex));
    // input resampler
    txa->rsmpin->setBuffers(txa->inbuff, txa->midbuff);
    txa->rsmpin->setSize(txa->dsp_insize);
    // dsp_size blocks
    txa->gen0->setBuffers(txa->midbuff, txa->midbuff);
    txa->gen0->setSize(txa->dsp_size);
    PANEL::setBuffers_panel (txa->panel, txa->midbuff, txa->midbuff);
    PANEL::setSize_panel (txa->panel, txa->dsp_size);
    txa->phrot->setBuffers(txa->midbuff, txa->midbuff);
    txa->phrot->setSize(txa->dsp_size);
    txa->micmeter->setBuffers (txa->midbuff);
    txa->micmeter->setSize (txa->dsp_size);
    txa->amsq->setBuffers (txa->midbuff, txa->midbuff, txa->midbuff);
    txa->amsq->setSize (txa->dsp_size);
    txa->eqp->setBuffers (txa->midbuff, txa->midbuff);
    txa->eqp->setSize (txa->dsp_size);
    txa->eqmeter->setBuffers (txa->midbuff);
    txa->eqmeter->setSize (txa->dsp_size);
    EMPHP::setBuffers_emphp (txa->preemph, txa->midbuff, txa->midbuff);
    EMPHP::setSize_emphp (txa->preemph, txa->dsp_size);
    txa->leveler->setBuffers(txa->midbuff, txa->midbuff);
    txa->leveler->setSize(txa->dsp_size);
    txa->lvlrmeter->setBuffers(txa->midbuff);
    txa->lvlrmeter->setSize(txa->dsp_size);
    CFCOMP::setBuffers_cfcomp (txa->cfcomp, txa->midbuff, txa->midbuff);
    CFCOMP::setSize_cfcomp (txa->cfcomp, txa->dsp_size);
    txa->cfcmeter->setBuffers(txa->midbuff);
    txa->cfcmeter->setSize(txa->dsp_size);
    txa->bp0->setBuffers (txa->midbuff, txa->midbuff);
    txa->bp0->setSize (txa->dsp_size);
    COMPRESSOR::setBuffers_compressor (txa->compressor, txa->midbuff, txa->midbuff);
    COMPRESSOR::setSize_compressor (txa->compressor, txa->dsp_size);
    txa->bp1->setBuffers (txa->midbuff, txa->midbuff);
    txa->bp1->setSize (txa->dsp_size);
    OSCTRL::setBuffers_osctrl (txa->osctrl, txa->midbuff, txa->midbuff);
    OSCTRL::setSize_osctrl (txa->osctrl, txa->dsp_size);
    txa->bp2->setBuffers (txa->midbuff, txa->midbuff);
    txa->bp2->setSize (txa->dsp_size);
    txa->compmeter->setBuffers(txa->midbuff);
    txa->compmeter->setSize(txa->dsp_size);
    txa->alc->setBuffers(txa->midbuff, txa->midbuff);
    txa->alc->setSize( txa->dsp_size);
    AMMOD::setBuffers_ammod (txa->ammod, txa->midbuff, txa->midbuff);
    AMMOD::setSize_ammod (txa->ammod, txa->dsp_size);
    FMMOD::setBuffers_fmmod (txa->fmmod, txa->midbuff, txa->midbuff);
    FMMOD::setSize_fmmod (txa->fmmod, txa->dsp_size);
    txa->gen1->setBuffers(txa->midbuff, txa->midbuff);
    txa->gen1->setSize(txa->dsp_size);
    USLEW::setBuffers_uslew (txa->uslew, txa->midbuff, txa->midbuff);
    USLEW::setSize_uslew (txa->uslew, txa->dsp_size);
    txa->alcmeter->setBuffers (txa->midbuff);
    txa->alcmeter->setSize(txa->dsp_size);
    txa->sip1->setBuffers (txa->midbuff);
    txa->sip1->setSize (txa->dsp_size);
    IQC::setBuffers_iqc (txa->iqc.p0, txa->midbuff, txa->midbuff);
    IQC::setSize_iqc (txa->iqc.p0, txa->dsp_size);
    CFIR::setBuffers_cfir (txa->cfir, txa->midbuff, txa->midbuff);
    CFIR::setSize_cfir (txa->cfir, txa->dsp_size);
    // output resampler
    txa->rsmpout->setBuffers(txa->midbuff, txa->outbuff);
    txa->rsmpout->setSize(txa->dsp_size);
    // output meter
    txa->outmeter->setBuffers(txa->outbuff);
    txa->outmeter->setSize(txa->dsp_outsize);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void TXA::SetMode (TXA& txa, int mode)
{
    if (txa.mode != mode)
    {
        txa.mode = mode;
        txa.ammod->run   = 0;
        txa.fmmod->run   = 0;
        txa.preemph->run = 0;

        switch (mode)
        {
        case TXA_AM:
        case TXA_SAM:
            txa.ammod->run   = 1;
            txa.ammod->mode  = 0;
            break;
        case TXA_DSB:
            txa.ammod->run   = 1;
            txa.ammod->mode  = 1;
            break;
        case TXA_AM_LSB:
        case TXA_AM_USB:
            txa.ammod->run   = 1;
            txa.ammod->mode  = 2;
            break;
        case TXA_FM:
            txa.fmmod->run   = 1;
            txa.preemph->run = 1;
            break;
        default:

            break;
        }

        SetupBPFilters (txa);
    }
}

void TXA::SetBandpassFreqs (TXA& txa, float f_low, float f_high)
{
    if ((txa.f_low != f_low) || (txa.f_high != f_high))
    {
        txa.f_low = f_low;
        txa.f_high = f_high;
        SetupBPFilters (txa);
    }
}


/********************************************************************************************************
*                                                                                                       *
*                                       TXA Internal Functions                                          *
*                                                                                                       *
********************************************************************************************************/

void TXA::ResCheck (TXA& txa)
{
    RESAMPLE *a = txa.rsmpin;
    if (txa.in_rate  != txa.dsp_rate)
        a->run = 1;
    else
        a->run = 0;
    a = txa.rsmpout;
    if (txa.dsp_rate != txa.out_rate)
        a->run = 1;
    else
        a->run = 0;
}

int TXA::UslewCheck (TXA& txa)
{
    return  (txa.ammod->run == 1) ||
            (txa.fmmod->run == 1) ||
            (txa.gen0->run  == 1) ||
            (txa.gen1->run  == 1);
}

void TXA::SetupBPFilters (TXA& txa)
{
    txa.bp0->run = 1;
    txa.bp1->run = 0;
    txa.bp2->run = 0;
    switch (txa.mode)
    {
    case TXA_LSB:
    case TXA_USB:
    case TXA_CWL:
    case TXA_CWU:
    case TXA_DIGL:
    case TXA_DIGU:
    case TXA_SPEC:
    case TXA_DRM:
        txa.bp0->calcBandpassFilter (txa.f_low, txa.f_high, 2.0);
        if (txa.compressor->run)
        {
            txa.bp1->calcBandpassFilter (txa.f_low, txa.f_high, 2.0);
            txa.bp1->run = 1;
            if (txa.osctrl->run)
            {
                txa.bp2->calcBandpassFilter (txa.f_low, txa.f_high, 1.0);
                txa.bp2->run = 1;
            }
        }
        break;
    case TXA_DSB:
    case TXA_AM:
    case TXA_SAM:
    case TXA_FM:
        if (txa.compressor->run)
        {
            txa.bp0->calcBandpassFilter (0.0, txa.f_high, 2.0);
            txa.bp1->calcBandpassFilter (0.0, txa.f_high, 2.0);
            txa.bp1->run = 1;
            if (txa.osctrl->run)
            {
                txa.bp2->calcBandpassFilter (0.0, txa.f_high, 1.0);
                txa.bp2->run = 1;
            }
        }
        else
        {
            txa.bp0->calcBandpassFilter (txa.f_low, txa.f_high, 1.0);
        }
        break;
    case TXA_AM_LSB:
        txa.bp0->calcBandpassFilter (-txa.f_high, 0.0, 2.0);
        if (txa.compressor->run)
        {
            txa.bp1->calcBandpassFilter (-txa.f_high, 0.0, 2.0);
            txa.bp1->run = 1;
            if (txa.osctrl->run)
            {
                txa.bp2->calcBandpassFilter (-txa.f_high, 0.0, 1.0);
                txa.bp2->run = 1;
            }
        }
        break;
    case TXA_AM_USB:
        txa.bp0->calcBandpassFilter (0.0, txa.f_high, 2.0);
        if (txa.compressor->run)
        {
            txa.bp1->calcBandpassFilter (0.0, txa.f_high, 2.0);
            txa.bp1->run = 1;
            if (txa.osctrl->run)
            {
                txa.bp2->calcBandpassFilter(0.0, txa.f_high, 1.0);
                txa.bp2->run = 1;
            }
        }
        break;
    }
}

void TXA::SetBandpassNC (TXA& txa, int nc)
{
    // NOTE:  'nc' must be >= 'size'
    BANDPASS *a;
    a = txa.bp0;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->fircore, a->nc, impulse);
        delete[] (impulse);
    }

    a = txa.bp1;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->fircore, a->nc, impulse);
        delete[] (impulse);
    }

    a = txa.bp2;

    if (a->nc != nc)
    {
        a->nc = nc;
        float* impulse = FIR::fir_bandpass (
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        FIRCORE::setNc_fircore (a->fircore, a->nc, impulse);
        delete[] (impulse);
    }
}

void TXA::SetBandpassMP (TXA& txa, int mp)
{
    BANDPASS *a;
    a = txa.bp0;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->fircore, a->mp);
    }

    a = txa.bp1;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->fircore, a->mp);
    }

    a = txa.bp2;

    if (mp != a->mp)
    {
        a->mp = mp;
        FIRCORE::setMp_fircore (a->fircore, a->mp);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                               Collectives                                             *
*                                                                                                       *
********************************************************************************************************/

void TXA::SetNC (TXA& txa, int nc)
{
    int oldstate = txa.state;

    SetBandpassNC           (txa, nc);
    EMPHP::SetFMEmphNC      (txa, nc);
    txa.eqp->setNC               (nc);
    FMMOD::SetFMNC          (txa, nc);
    CFIR::SetCFIRNC         (txa, nc);
    txa.state = oldstate;
}

void TXA::SetMP (TXA& txa, int mp)
{
    SetBandpassMP            (txa, mp);
    EMPHP::SetFMEmphMP       (txa, mp);
    txa.eqp->setMP                (mp);
    FMMOD::SetFMMP           (txa, mp);
}

void TXA::SetFMAFFilter (TXA& txa, float low, float high)
{
    EMPHP::SetFMPreEmphFreqs (txa, low, high);
    FMMOD::SetFMAFFreqs      (txa, low, high);
}

} // namespace WDSP
