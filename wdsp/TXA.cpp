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
#include "emphp.hpp"
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

TXA::TXA(
    int _in_rate,                // input samplerate
    int _out_rate,               // output samplerate
    int _dsp_rate,               // sample rate for mainstream dsp processing
    int _dsp_size                // number complex samples processed per buffer in mainstream dsp processing
) : Unit(
    _in_rate,
    _out_rate,
    _dsp_rate,
    _dsp_size
)
{
    mode   = TXA_LSB;
    f_low  = -5000.0;
    f_high = - 100.0;

    rsmpin = new RESAMPLE(
        0,                                          // run - will be turned on below if needed
        dsp_insize,                     // input buffer size
        inbuff,                        // pointer to input buffer
        midbuff,                       // pointer to output buffer
        in_rate,                        // input sample rate
        dsp_rate,                       // output sample rate
        0.0,                                        // select cutoff automatically
        0,                                          // select ncoef automatically
        1.0);                                       // gain

    gen0 = new GEN(
        0,                                          // run
        dsp_size,                       // buffer size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        dsp_rate,                       // sample rate
        2);                                         // mode

    panel = new PANEL(
        1,                                          // run
        dsp_size,                       // size
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        1.0,                                        // gain1
        1.0,                                        // gain2I
        1.0,                                        // gain2Q
        2,                                          // 1 to use Q, 2 to use I for input
        0);                                         // 0, no copy

    phrot = new PHROT(
        0,                                          // run
        dsp_size,                       // size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        dsp_rate,                       // samplerate
        338.0,                                      // 1/2 of phase frequency
        8);                                         // number of stages

    micmeter = new METER(
        1,                                          // run
        nullptr,                                          // optional pointer to another 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_MIC_AV,                                 // index for average value
        TXA_MIC_PK,                                 // index for peak value
        -1,                                         // index for gain value
        nullptr);                                         // pointer for gain computation

    amsq = new AMSQ(
        0,                                          // run
        dsp_size,                       // size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        midbuff,                       // trigger buffer
        dsp_rate,                       // sample rate
        0.010,                                      // time constant for averaging signal
        0.004,                                      // up-slew time
        0.004,                                      // down-slew time
        0.180,                                      // signal level to initiate tail
        0.200,                                      // signal level to initiate unmute
        0.000,                                      // minimum tail length
        0.025,                                      // maximum tail length
        0.200);                                     // muted gain

    {
        std::array<float, 11> default_F = {0.0,  32.0,  63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
        std::array<float, 11> default_G = {0.0, -12.0, -12.0, -12.0,  -1.0,  +1.0,   +4.0,   +9.0,  +12.0,  -10.0,   -10.0};
        //float default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
        eqp = new EQP (
            0,                                          // run - OFF by default
            dsp_size,                       // size
            std::max(2048, dsp_size),            // number of filter coefficients
            0,                                          // minimum phase flag
            midbuff,                       // pointer to input buffer
            midbuff,                       // pointer to output buffer
            10,                                         // nfreqs
            default_F.data(),                                  // vector of frequencies
            default_G.data(),                                  // vector of gain values
            0,                                          // cutoff mode
            0,                                          // wintype
            dsp_rate);                      // samplerate
    }

    eqmeter = new METER(
        1,                                          // run
        &(eqp->run),                 // pointer to eqp 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_EQ_AV,                                  // index for average value
        TXA_EQ_PK,                                  // index for peak value
        -1,                                         // index for gain value
        nullptr);                                         // pointer for gain computation

    preemph = new EMPHP(
        0,                                          // run
        1,                                          // position
        dsp_size,                       // size
        std::max(2048, dsp_size),            // number of filter coefficients
        0,                                          // minimum phase flag
        midbuff,                       // input buffer
        midbuff,                       // output buffer,
        dsp_rate,                       // sample rate
        0,                                          // pre-emphasis type
        300.0,                                      // f_low
        3000.0);                                    // f_high

    leveler = new WCPAGC(
        0,                                          // run - OFF by default
        5,                                          // mode
        0,                                          // 0 for max(I,Q), 1 for envelope
        midbuff,                       // input buff pointer
        midbuff,                       // output buff pointer
        dsp_size,                       // io_buffsize
        dsp_rate,                       // sample rate
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

    lvlrmeter = new METER(
        1,                                          // run
        &(leveler->run),             // pointer to leveler 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_LVLR_AV,                                // index for average value
        TXA_LVLR_PK,                                // index for peak value
        TXA_LVLR_GAIN,                              // index for gain value
        &leveler->gain);             // pointer for gain computation

    {
        std::array<double, 5> default_F = {200.0, 1000.0, 2000.0, 3000.0, 4000.0};
        std::array<double, 5> default_G = {  0.0,    5.0,   10.0,   10.0,    5.0};
        std::array<double, 5> default_E = {  7.0,    7.0,    7.0,    7.0,    7.0};
        cfcomp = new CFCOMP(
            0,                                          // run
            0,                                          // position
            0,                                          // post-equalizer run
            dsp_size,                       // size
            midbuff,                       // input buffer
            midbuff,                       // output buffer
            2048,                                       // fft size
            4,                                          // overlap
            dsp_rate,                       // samplerate
            1,                                          // window type
            0,                                          // compression method
            5,                                          // nfreqs
            0.0,                                        // pre-compression
            0.0,                                        // pre-postequalization
            default_F.data(),                                  // frequency array
            default_G.data(),                                  // compression array
            default_E.data(),                                  // eq array
            0.25,                                       // metering time constant
            0.50);                                      // display time constant
    }

    cfcmeter = new METER(
        1,                                          // run
        &(cfcomp->run),              // pointer to eqp 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_CFC_AV,                                 // index for average value
        TXA_CFC_PK,                                 // index for peak value
        TXA_CFC_GAIN,                               // index for gain value
        (double*) &cfcomp->gain);              // pointer for gain computation

    bp0 = new BANDPASS(
        1,                                          // always runs
        0,                                          // position
        dsp_size,                       // size
        std::max(2048, dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        f_low,                         // low freq cutoff
        f_high,                        // high freq cutoff
        dsp_rate,                       // samplerate
        1,                                          // wintype
        2.0);                                       // gain

    compressor = new COMPRESSOR(
        0,                                          // run - OFF by default
        dsp_size,                       // size
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        3.0);                                       // gain

    bp1 = new BANDPASS(
        0,                                          // ONLY RUNS WHEN COMPRESSOR IS USED
        0,                                          // position
        dsp_size,                       // size
        std::max(2048, dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        f_low,                         // low freq cutoff
        f_high,                        // high freq cutoff
        dsp_rate,                       // samplerate
        1,                                          // wintype
        2.0);                                       // gain

    osctrl = new OSCTRL(
        0,                                          // run
        dsp_size,                       // size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        dsp_rate,                       // sample rate
        1.95f);                                      // gain for clippings

    bp2 = new BANDPASS(
        0,                                          // ONLY RUNS WHEN COMPRESSOR IS USED
        0,                                          // position
        dsp_size,                       // size
        std::max(2048, dsp_size),            // number of coefficients
        0,                                          // flag for minimum phase
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        f_low,                         // low freq cutoff
        f_high,                        // high freq cutoff
        dsp_rate,                       // samplerate
        1,                                          // wintype
        1.0);                                       // gain

    compmeter = new METER(
        1,                                          // run
        &(compressor->run),          // pointer to compressor 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_COMP_AV,                                // index for average value
        TXA_COMP_PK,                                // index for peak value
        -1,                                         // index for gain value
        nullptr);                                         // pointer for gain computation

    alc = new WCPAGC(
        1,                                          // run - always ON
        5,                                          // mode
        1,                                          // 0 for max(I,Q), 1 for envelope
        midbuff,                       // input buff pointer
        midbuff,                       // output buff pointer
        dsp_size,                       // io_buffsize
        dsp_rate,                       // sample rate
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

    ammod = new AMMOD(
        0,                                          // run - OFF by default
        0,                                          // mode:  0=>AM, 1=>DSB
        dsp_size,                       // size
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to output buffer
        0.5);                                       // carrier level


    fmmod = new FMMOD(
        0,                                          // run - OFF by default
        dsp_size,                       // size
        midbuff,                       // pointer to input buffer
        midbuff,                       // pointer to input buffer
        dsp_rate,                       // samplerate
        5000.0,                                     // deviation
        300.0,                                      // low cutoff frequency
        3000.0,                                     // high cutoff frequency
        1,                                          // ctcss run control
        0.10f,                                       // ctcss level
        100.0,                                      // ctcss frequency
        1,                                          // run bandpass filter
        std::max(2048, dsp_size),            // number coefficients for bandpass filter
        0);                                         // minimum phase flag

    gen1 = new GEN(
        0,                                          // run
        dsp_size,                       // buffer size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        dsp_rate,                       // sample rate
        0);                                         // mode

    uslew = new USLEW(
        &upslew,                        // pointer to channel upslew flag
        dsp_size,                       // buffer size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        (double) dsp_rate,                       // sample rate
        0.000,                                      // delay time
        0.005);                                     // upslew time

    alcmeter = new METER(
        1,                                          // run
        nullptr,                                          // optional pointer to a 'run'
        dsp_size,                       // size
        midbuff,                       // pointer to buffer
        dsp_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_ALC_AV,                                 // index for average value
        TXA_ALC_PK,                                 // index for peak value
        TXA_ALC_GAIN,                               // index for gain value
        &alc->gain);                 // pointer for gain computation

    sip1 = new SIPHON(
        1,                                          // run
        0,                                          // position
        0,                                          // mode
        0,                                          // disp
        dsp_size,                       // input buffer size
        midbuff,                       // input buffer
        16384,                                      // number of samples to buffer
        16384,                                      // fft size for spectrum
        1);                                         // specmode

    // txa->calcc = create_calcc (
    //     channel,                                    // channel number
    //     1,                                          // run calibration
    //     1024,                                       // input buffer size
    //     in_rate,                        // samplerate
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

    iqc.p0 = iqc.p1 = new IQC(
        0,                                          // run
        dsp_size,                       // size
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        (double) dsp_rate,               // sample rate
        16,                                         // ints
        0.005,                                      // changeover time
        256);                                       // spi

    cfir = new CFIR(
        0,                                          // run
        dsp_size,                       // size
        std::max(2048, dsp_size),            // number of filter coefficients
        0,                                          // minimum phase flag
        midbuff,                       // input buffer
        midbuff,                       // output buffer
        dsp_rate,                       // input sample rate
        out_rate,                       // CIC input sample rate
        1,                                          // CIC differential delay
        640,                                        // CIC interpolation factor
        5,                                          // CIC integrator-comb pairs
        20000.0,                                    // cutoff frequency
        2,                                          // brick-wall windowed rolloff
        0.0,                                        // raised-cosine transition width
        0);                                         // window type

    rsmpout = new RESAMPLE(
        0,                                          // run - will be turned ON below if needed
        dsp_size,                       // input size
        midbuff,                       // pointer to input buffer
        outbuff,                       // pointer to output buffer
        dsp_rate,                       // input sample rate
        out_rate,                       // output sample rate
        0.0,                                        // select cutoff automatically
        0,                                          // select ncoef automatically
        0.980);                                     // gain

    outmeter = new METER(
        1,                                          // run
        nullptr,                                          // optional pointer to another 'run'
        dsp_outsize,                    // size
        outbuff,                       // pointer to buffer
        out_rate,                       // samplerate
        0.100,                                      // averaging time constant
        0.100,                                      // peak decay time constant
        meter,                         // result vector
        TXA_OUT_AV,                                 // index for average value
        TXA_OUT_PK,                                 // index for peak value
        -1,                                         // index for gain value
        nullptr);                                         // pointer for gain computation

    // turn OFF / ON resamplers as needed
    resCheck();
}

TXA::~TXA()
{
    // in reverse order, free each item we created
    delete outmeter;
    delete rsmpout;
    delete cfir;
    delete iqc.p0;
    delete sip1;
    delete alcmeter;
    delete uslew;
    delete gen1;
    delete fmmod;
    delete ammod;
    delete alc;
    delete compmeter;
    delete bp2;
    delete osctrl;
    delete bp1;
    delete compressor;
    delete bp0;
    delete cfcmeter;
    delete cfcomp;
    delete lvlrmeter;
    delete leveler;
    delete preemph;
    delete eqmeter;
    delete eqp;
    delete amsq;
    delete micmeter;
    delete phrot;
    delete panel;
    delete gen0;
    delete rsmpin;
}

void TXA::flush()
{
    Unit::flushBuffers();
    rsmpin->flush();
    gen0->flush();
    panel->flush ();
    phrot->flush();
    micmeter->flush ();
    amsq->flush ();
    eqp->flush();
    eqmeter->flush ();
    preemph->flush();
    leveler->flush();
    lvlrmeter->flush ();
    cfcomp->flush();
    cfcmeter->flush ();
    bp0->flush ();
    compressor->flush();
    bp1->flush ();
    osctrl->flush();
    bp2->flush ();
    compmeter->flush ();
    alc->flush ();
    ammod->flush();
    fmmod->flush();
    gen1->flush();
    uslew->flush();
    alcmeter->flush ();
    sip1->flush();
    iqc.p0->flush();
    cfir->flush();
    rsmpout->flush();
    outmeter->flush ();
}

void TXA::execute()
{
    rsmpin->execute();              // input resampler
    gen0->execute();                     // input signal generator
    panel->execute();                  // includes MIC gain
    phrot->execute();                  // phase rotator
    micmeter->execute ();               // MIC meter
    amsq->xcap ();                 // downward expander capture
    amsq->execute ();                    // downward expander action
    eqp->execute ();                      // pre-EQ
    eqmeter->execute ();                // EQ meter
    preemph->execute(0);             // FM pre-emphasis (first option)
    leveler->execute ();               // Leveler
    lvlrmeter->execute ();              // Leveler Meter
    cfcomp->execute(0);             // Continuous Frequency Compressor with post-EQ
    cfcmeter->execute ();               // CFC+PostEQ Meter
    bp0->execute (0);              // primary bandpass filter
    compressor->execute();        // COMP compressor
    bp1->execute (0);              // aux bandpass (runs if COMP)
    osctrl->execute();                // CESSB Overshoot Control
    bp2->execute (0);              // aux bandpass (runs if CESSB)
    compmeter->execute ();              // COMP meter
    alc->execute ();                   // ALC
    ammod->execute();                  // AM Modulator
    preemph->execute(1);             // FM pre-emphasis (second option)
    fmmod->execute();                  // FM Modulator
    gen1->execute();                     // output signal generator (TUN and Two-tone)
    uslew->execute(uslewCheck());                  // up-slew for AM, FM, and gens
    alcmeter->execute ();               // ALC Meter
    sip1->execute(0);               // siphon data for display
    iqc.p0->execute();                     // PureSignal correction
    cfir->execute();                     // compensating FIR filter (used Protocol_2 only)
    rsmpout->execute();             // output resampler
    outmeter->execute ();               // output meter
}

void TXA::setInputSamplerate(int in_rate)
{
    Unit::setBuffersInputSamplerate(in_rate);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    rsmpin->setInRate(in_rate);
    resCheck();
}

void TXA::setOutputSamplerate(int out_rate)
{
    Unit::setBuffersOutputSamplerate(out_rate);
    // cfir - needs to know input rate of firmware CIC
    cfir->setOutRate(out_rate);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setOutRate(out_rate);
    resCheck();
    // output meter
    outmeter->setBuffers(outbuff);
    outmeter->setSize(dsp_outsize);
    outmeter->setSamplerate (out_rate);
}

void TXA::setDSPSamplerate(int dsp_rate)
{
    Unit::setBuffersDSPSamplerate(dsp_rate);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    rsmpin->setOutRate(dsp_rate);
    // dsp_rate blocks
    gen0->setSamplerate(dsp_rate);
    panel->setSamplerate(dsp_rate);
    phrot->setSamplerate(dsp_rate);
    micmeter->setSamplerate (dsp_rate);
    amsq->setSamplerate (dsp_rate);
    eqp->setSamplerate (dsp_rate);
    eqmeter->setSamplerate (dsp_rate);
    preemph->setSamplerate(dsp_rate);
    leveler->setSamplerate (dsp_rate);
    lvlrmeter->setSamplerate (dsp_rate);
    cfcomp->setSamplerate(dsp_rate);
    cfcmeter->setSamplerate (dsp_rate);
    bp0->setSamplerate (dsp_rate);
    compressor->setSamplerate(dsp_rate);
    bp1->setSamplerate (dsp_rate);
    osctrl->setSamplerate(dsp_rate);
    bp2->setSamplerate (dsp_rate);
    compmeter->setSamplerate (dsp_rate);
    alc->setSamplerate (dsp_rate);
    ammod->setSamplerate(dsp_rate);
    fmmod->setSamplerate(dsp_rate);
    gen1->setSamplerate(dsp_rate);
    uslew->setSamplerate(dsp_rate);
    alcmeter->setSamplerate (dsp_rate);
    sip1->setSamplerate (dsp_rate);
    iqc.p0->setSamplerate(dsp_rate);
    cfir->setSamplerate(dsp_rate);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setInRate(dsp_rate);
    resCheck();
    // output meter
    outmeter->setBuffers(outbuff);
    outmeter->setSize (dsp_outsize);
}

void TXA::setDSPBuffsize(int dsp_size)
{
    Unit::setBuffersDSPBuffsize(dsp_size);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    // dsp_size blocks
    gen0->setBuffers(midbuff, midbuff);
    gen0->setSize(dsp_size);
    panel->setBuffers(midbuff, midbuff);
    panel->setSize(dsp_size);
    phrot->setBuffers(midbuff, midbuff);
    phrot->setSize(dsp_size);
    micmeter->setBuffers (midbuff);
    micmeter->setSize (dsp_size);
    amsq->setBuffers (midbuff, midbuff, midbuff);
    amsq->setSize (dsp_size);
    eqp->setBuffers (midbuff, midbuff);
    eqp->setSize (dsp_size);
    eqmeter->setBuffers (midbuff);
    eqmeter->setSize (dsp_size);
    preemph->setBuffers(midbuff, midbuff);
    preemph->setSize(dsp_size);
    leveler->setBuffers(midbuff, midbuff);
    leveler->setSize(dsp_size);
    lvlrmeter->setBuffers(midbuff);
    lvlrmeter->setSize(dsp_size);
    cfcomp->setBuffers(midbuff, midbuff);
    cfcomp->setSize(dsp_size);
    cfcmeter->setBuffers(midbuff);
    cfcmeter->setSize(dsp_size);
    bp0->setBuffers (midbuff, midbuff);
    bp0->setSize (dsp_size);
    compressor->setBuffers(midbuff, midbuff);
    compressor->setSize(dsp_size);
    bp1->setBuffers (midbuff, midbuff);
    bp1->setSize (dsp_size);
    osctrl->setBuffers(midbuff, midbuff);
    osctrl->setSize(dsp_size);
    bp2->setBuffers (midbuff, midbuff);
    bp2->setSize (dsp_size);
    compmeter->setBuffers(midbuff);
    compmeter->setSize(dsp_size);
    alc->setBuffers(midbuff, midbuff);
    alc->setSize( dsp_size);
    ammod->setBuffers(midbuff, midbuff);
    ammod->setSize(dsp_size);
    fmmod->setBuffers(midbuff, midbuff);
    fmmod->setSize(dsp_size);
    gen1->setBuffers(midbuff, midbuff);
    gen1->setSize(dsp_size);
    uslew->setBuffers(midbuff, midbuff);
    uslew->setSize(dsp_size);
    alcmeter->setBuffers (midbuff);
    alcmeter->setSize(dsp_size);
    sip1->setBuffers (midbuff);
    sip1->setSize (dsp_size);
    iqc.p0->IQC::setBuffers(midbuff, midbuff);
    iqc.p0->IQC::setSize(dsp_size);
    cfir->setBuffers(midbuff, midbuff);
    cfir->setSize(dsp_size);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setSize(dsp_size);
    // output meter
    outmeter->setBuffers(outbuff);
    outmeter->setSize(dsp_outsize);
}

/********************************************************************************************************
*                                                                                                       *
*                                           TXA Properties                                              *
*                                                                                                       *
********************************************************************************************************/

void TXA::setMode(int _mode)
{
    if (mode != _mode)
    {
        mode = _mode;
        ammod->run   = 0;
        fmmod->run   = 0;
        preemph->run = 0;

        switch (_mode)
        {
        case TXA_AM:
        case TXA_SAM:
            ammod->run   = 1;
            ammod->mode  = 0;
            break;
        case TXA_DSB:
            ammod->run   = 1;
            ammod->mode  = 1;
            break;
        case TXA_AM_LSB:
        case TXA_AM_USB:
            ammod->run   = 1;
            ammod->mode  = 2;
            break;
        case TXA_FM:
            fmmod->run   = 1;
            preemph->run = 1;
            break;
        default:

            break;
        }

        setupBPFilters();
    }
}

void TXA::setBandpassFreqs(float _f_low, float _f_high)
{
    if ((f_low != _f_low) || (f_high != _f_high))
    {
        f_low = _f_low;
        f_high = _f_high;
        setupBPFilters();
    }
}


/********************************************************************************************************
*                                                                                                       *
*                                       TXA Internal Functions                                          *
*                                                                                                       *
********************************************************************************************************/

void TXA::resCheck()
{
    RESAMPLE *a = rsmpin;
    if (in_rate  != dsp_rate)
        a->run = 1;
    else
        a->run = 0;
    a = rsmpout;
    if (dsp_rate != out_rate)
        a->run = 1;
    else
        a->run = 0;
}

int TXA::uslewCheck()
{
    return  (ammod->run == 1) ||
            (fmmod->run == 1) ||
            (gen0->run  == 1) ||
            (gen1->run  == 1);
}

void TXA::setupBPFilters()
{
    bp0->run = 1;
    bp1->run = 0;
    bp2->run = 0;
    switch (mode)
    {
    case TXA_LSB:
    case TXA_USB:
    case TXA_CWL:
    case TXA_CWU:
    case TXA_DIGL:
    case TXA_DIGU:
    case TXA_SPEC:
    case TXA_DRM:
        bp0->calcBandpassFilter (f_low, f_high, 2.0);
        if (compressor->run)
        {
            bp1->calcBandpassFilter (f_low, f_high, 2.0);
            bp1->run = 1;
            if (osctrl->run)
            {
                bp2->calcBandpassFilter (f_low, f_high, 1.0);
                bp2->run = 1;
            }
        }
        break;
    case TXA_DSB:
    case TXA_AM:
    case TXA_SAM:
    case TXA_FM:
        if (compressor->run)
        {
            bp0->calcBandpassFilter (0.0, f_high, 2.0);
            bp1->calcBandpassFilter (0.0, f_high, 2.0);
            bp1->run = 1;
            if (osctrl->run)
            {
                bp2->calcBandpassFilter (0.0, f_high, 1.0);
                bp2->run = 1;
            }
        }
        else
        {
            bp0->calcBandpassFilter (f_low, f_high, 1.0);
        }
        break;
    case TXA_AM_LSB:
        bp0->calcBandpassFilter (-f_high, 0.0, 2.0);
        if (compressor->run)
        {
            bp1->calcBandpassFilter (-f_high, 0.0, 2.0);
            bp1->run = 1;
            if (osctrl->run)
            {
                bp2->calcBandpassFilter (-f_high, 0.0, 1.0);
                bp2->run = 1;
            }
        }
        break;
    case TXA_AM_USB:
        bp0->calcBandpassFilter (0.0, f_high, 2.0);
        if (compressor->run)
        {
            bp1->calcBandpassFilter (0.0, f_high, 2.0);
            bp1->run = 1;
            if (osctrl->run)
            {
                bp2->calcBandpassFilter(0.0, f_high, 1.0);
                bp2->run = 1;
            }
        }
        break;
    default:
        break;
    }
}

void TXA::setBandpassNC(int _nc)
{
    // NOTE:  'nc' must be >= 'size'
    BANDPASS *a;
    a = bp0;

    if (a->nc != _nc)
    {
        a->nc = _nc;
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        a->fircore->setNc(impulse);
    }

    a = bp1;

    if (a->nc != _nc)
    {
        a->nc = _nc;
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        a->fircore->setNc(impulse);
    }

    a = bp2;

    if (a->nc != _nc)
    {
        a->nc = _nc;
        std::vector<float> impulse;
        FIR::fir_bandpass (
            impulse,
            a->nc,
            a->f_low,
            a->f_high,
            a->samplerate,
            a->wintype,
            1,
            a->gain / (double)(2 * a->size)
        );
        a->fircore->setNc(impulse);
    }
}

void TXA::setBandpassMP(int _mp)
{
    BANDPASS *a;
    a = bp0;

    if (_mp != a->mp)
    {
        a->mp = _mp;
        a->fircore->setMp(a->mp);
    }

    a = bp1;

    if (_mp != a->mp)
    {
        a->mp = _mp;
        a->fircore->setMp(a->mp);
    }

    a = bp2;

    if (_mp != a->mp)
    {
        a->mp = _mp;
        a->fircore->setMp(a->mp);
    }
}

/********************************************************************************************************
*                                                                                                       *
*                                               Collectives                                             *
*                                                                                                       *
********************************************************************************************************/

void TXA::setNC(int _nc)
{
    int oldstate = state;

    setBandpassNC                (_nc);
    preemph->setNC               (_nc);
    eqp->setNC                   (_nc);
    fmmod->setNC                 (_nc);
    cfir->setNC                  (_nc);
    state = oldstate;
}

void TXA::setMP(int _mp)
{
    setBandpassMP                 (_mp);
    preemph->setMP                (_mp);
    eqp->setMP                    (_mp);
    fmmod->setMP                  (_mp);
}

void TXA::setFMAFFilter(float _low, float _high)
{
    preemph->setFreqs         (_low, _high);
    fmmod->setAFFreqs         (_low, _high);
}

void TXA::SetBPSRun (TXA& txa, int _run)
{
    txa.bp1->run = _run;
}

void TXA::SetBPSFreqs (TXA& txa, double _f_low, double _f_high)
{
    std::vector<float> impulse;
    BPS *a;
    a = txa.bps0;

    if ((_f_low != a->f_low) || (_f_high != a->f_high))
    {
        a->f_low = _f_low;
        a->f_high = _f_high;
        FIR::fir_bandpass(impulse, a->size + 1, _f_low, _f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }

    a = txa.bps1;

    if ((_f_low != a->f_low) || (_f_high != a->f_high))
    {
        a->f_low = _f_low;
        a->f_high = _f_high;
        FIR::fir_bandpass(impulse, a->size + 1, _f_low, _f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }

    a = txa.bps2;

    if ((_f_low != a->f_low) || (_f_high != a->f_high))
    {
        a->f_low = _f_low;
        a->f_high = _f_high;
        FIR::fir_bandpass(impulse, a->size + 1, _f_low, _f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }
}

void TXA::SetBPSWindow (TXA& txa, int _wintype)
{
    std::vector<float> impulse;
    BPS *a;
    a = txa.bps0;

    if (a->wintype != _wintype)
    {
        a->wintype = _wintype;
        FIR::fir_bandpass(impulse, a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }

    a = txa.bps1;

    if (a->wintype != _wintype)
    {
        a->wintype = _wintype;
        FIR::fir_bandpass(impulse, a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }

    a = txa.bps2;

    if (a->wintype != _wintype)
    {
        a->wintype = _wintype;
        FIR::fir_bandpass (impulse, a->size + 1, a->f_low, a->f_high, a->samplerate, a->wintype, 1, 1.0 / (float)(2 * a->size));
        FIR::fftcv_mults (a->mults, 2 * a->size, impulse.data());
    }
}

void TXA::SetCompressorRun (TXA& txa, int _run)
{
    if (txa.compressor->run != _run)
    {
        txa.compressor->run = _run;
        txa.setupBPFilters();
    }
}

void TXA::SetosctrlRun (TXA& txa, int run)
{
    if (txa.osctrl->run != run)
    {
        txa.osctrl->run = run;
        txa.setupBPFilters();
    }
}

void TXA::GetiqcValues (TXA& txa, std::vector<double>& cm, std::vector<double>& cc, std::vector<double>& cs)
{
    IQC *a;
    a = txa.iqc.p0;
    cm.resize(a->ints * 4);
    cc.resize(a->ints * 4);
    cs.resize(a->ints * 4);
    std::copy(a->cm[a->cset].begin(), a->cm[a->cset].begin() + a->ints * 4, cm.begin());
    std::copy(a->cc[a->cset].begin(), a->cc[a->cset].begin() + a->ints * 4, cc.begin());
    std::copy(a->cs[a->cset].begin(), a->cs[a->cset].begin() + a->ints * 4, cs.begin());
}

void TXA::SetiqcValues (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs)
{
    IQC *a;
    a = txa.iqc.p0;
    a->cset = 1 - a->cset;
    std::copy(cm.begin(), cm.begin() + a->ints * 4, a->cm[a->cset].begin());
    std::copy(cc.begin(), cc.begin() + a->ints * 4, a->cc[a->cset].begin());
    std::copy(cs.begin(), cs.begin() + a->ints * 4, a->cs[a->cset].begin());
    a->state = IQC::IQCSTATE::RUN;
}

void TXA::SetiqcSwap (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs)
{
    IQC *a = txa.iqc.p1;
    a->cset = 1 - a->cset;
    std::copy(cm.begin(), cm.begin() + a->ints * 4, a->cm[a->cset].begin());
    std::copy(cc.begin(), cc.begin() + a->ints * 4, a->cc[a->cset].begin());
    std::copy(cs.begin(), cs.begin() + a->ints * 4, a->cs[a->cset].begin());
    a->busy = 1;
    a->state = IQC::IQCSTATE::SWAP;
    a->count = 0;
}

void TXA::SetiqcStart (TXA& txa, const std::vector<double>& cm, const std::vector<double>& cc, const std::vector<double>& cs)
{
    IQC *a = txa.iqc.p1;
    a->cset = 0;
    std::copy(cm.begin(), cm.begin() + a->ints * 4, a->cm[a->cset].begin());
    std::copy(cc.begin(), cc.begin() + a->ints * 4, a->cc[a->cset].begin());
    std::copy(cs.begin(), cs.begin() + a->ints * 4, a->cs[a->cset].begin());
    a->busy = 1;
    a->state = IQC::IQCSTATE::BEGIN;
    a->count = 0;
    txa.iqc.p1->run = 1;
}

void TXA::SetiqcEnd (TXA& txa)
{
    IQC *a = txa.iqc.p1;
    a->busy = 1;
    a->state = IQC::IQCSTATE::END;
    a->count = 0;
    txa.iqc.p1->run = 0;
}

void TXA::GetiqcDogCount (TXA& txa, int* count)
{
    IQC *a = txa.iqc.p1;
    *count = a->dog.count;
}

void TXA::SetiqcDogCount (TXA& txa, int count)
{
    IQC *a = txa.iqc.p1;
    a->dog.count = count;
}

} // namespace WDSP
