/*  RXA.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2015, 2016, 2023 Warren Pratt, NR0V
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

#include "RXA.hpp"
#include "amd.hpp"
#include "meter.hpp"
#include "shift.hpp"
#include "resample.hpp"
#include "bandpass.hpp"
#include "bps.hpp"
#include "nbp.hpp"
#include "snba.hpp"
#include "bpsnba.hpp"
#include "sender.hpp"
#include "amsq.hpp"
#include "fmd.hpp"
#include "fmsq.hpp"
#include "eqp.hpp"
#include "anf.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "patchpanel.hpp"
#include "siphon.hpp"
#include "cblock.hpp"
#include "ssql.hpp"
#include "fircore.hpp"
#include "wcpAGC.hpp"
#include "anb.hpp"
#include "nob.hpp"
#include "speak.hpp"
#include "mpeak.hpp"
#include "fir.hpp"

namespace WDSP {

RXA::RXA(
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
    mode = RXA::RXA_LSB;
    std::fill(meter.begin(), meter.end(), 0);

    // Noise blanker (ANB or "NB")
    anb = new ANB(
        0, // run
        dsp_insize,                        // input buffer size
        inbuff,                            // pointer to input buffer
        inbuff,                            // pointer to output buffer
        in_rate,                           // samplerate
        0.0001,                                 // tau
        0.0001,                                 // hang time
        0.0001,                                 // advance time
        0.05,                                   // back tau
        30                                      // thershold
    );
    // Noise blanker (NOB or "NB2")
    nob = new NOB(
        0, // run
        dsp_insize,                        // input buffer size
        inbuff,                            // pointer to input buffer
        inbuff,                            // pointer to output buffer
        in_rate,                           // samplerate
        0,                                      // mode (zero)
        0.0001,                                 // advance slew time
        0.0001,                                 // advance time
        0.0001,                                 // hang slew time
        0.0001,                                 // hang time
        0.025,                                  // max_imp_seq_time:
        0.05,                                   // back tau
        30
    );

    // Ftequency shifter - shift to select a slice of spectrum
    shift = new SHIFT(
        0,                                      // run
        dsp_insize,                        // input buffer size
        inbuff,                            // pointer to input buffer
        inbuff,                            // pointer to output buffer
        in_rate,                           // samplerate
        0.0);                                   // amount to shift (Hz)

    // Input resampler - resample to dsp rate for main processing
    rsmpin = new RESAMPLE(
        0,                                      // run - will be turned ON below if needed
        dsp_insize,                        // input buffer size
        inbuff,                            // pointer to input buffer
        midbuff,                           // pointer to output buffer
        in_rate,                           // input samplerate
        dsp_rate,                          // output samplerate
        0.0,                                    // select cutoff automatically
        0,                                      // select ncoef automatically
        1.0);                                   // gain

    // Input meter - ADC
    adcmeter = new METER(
        0,                                      // run
        nullptr,                                      // optional pointer to another 'run'
        dsp_size,                          // size
        midbuff,                           // pointer to buffer
        dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        meter.data(),                           // result vector
        RXA_ADC_AV,                             // index for average value
        RXA_ADC_PK,                             // index for peak value
        -1,                                     // index for gain value - disabled
        nullptr);                                     // pointer for gain computation

    // Notched bandpass section

    // notch database
    ndb = new NOTCHDB (
        0,                                      // master run for all nbp's
        1024);                                  // max number of notches

    // notched bandpass
    nbp0 = new NBP (
        1,                                      // run, always runs
        0,                                      // run the notches
        0,                                      // position
        dsp_size,                          // buffer size
        std::max(2048, dsp_size),          // number of coefficients
        0,                                      // minimum phase flag
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        -4150.0,                                // lower filter frequency
        -150.0,                                 // upper filter frequency
        dsp_rate,                          // sample rate
        0,                                      // wintype
        1.0,                                    // gain
        1,                                      // auto-increase notch width
        1025,                                   // max number of passbands
        ndb);                              // addr of database pointer

    // bandpass for snba
    bpsnba = new BPSNBA (
        0,                                      // bpsnba run flag
        0,                                      // run the notches
        0,                                      // position
        dsp_size,                          // size
        std::max(2048, dsp_size),          // number of filter coefficients
        0,                                      // minimum phase flag
        midbuff,                           // input buffer
        midbuff,                           // output buffer
        dsp_rate,                          // samplerate
        + 250.0,                                // abs value of cutoff nearest zero
        + 5700.0,                               // abs value of cutoff farthest zero
        - 5700.0,                               // current low frequency
        - 250.0,                                // current high frequency
        0,                                      // wintype
        1.0,                                    // gain
        1,                                      // auto-increase notch width
        1025,                                   // max number of passbands
        ndb);                              // addr of database pointer

    // Post filter display send - send spectrum display (after S-meter in the block diagram)
    sender = new SENDER (
        0,                                      // run
        0,                                      // flag
        0,                                      // mode
        dsp_size,                          // size
        midbuff                            // pointer to input buffer
    );

    // End notched bandpass section

    // S-meter
    smeter = new METER(
        1,                                      // run
        nullptr,                                      // optional pointer to another 'run'
        dsp_size,                          // size
        midbuff,                           // pointer to buffer
        dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        meter.data(),                           // result vector
        RXA_S_AV,                               // index for average value
        RXA_S_PK,                               // index for peak value
        -1,                                     // index for gain value - disabled
        nullptr);                                     // pointer for gain computation

    // AM squelch capture (for other modes than FM)
    amsq = new AMSQ(
        0,                                      // run
        dsp_size,                          // buffer size
        midbuff,                           // pointer to signal input buffer used by xamsq
        midbuff,                           // pointer to signal output buffer used by xamsq
        midbuff,                           // pointer to trigger buffer that xamsqcap will capture
        dsp_rate,                          // sample rate
        0.010,                                  // time constant for averaging signal level
        0.070,                                  // signal up transition time
        0.070,                                  // signal down transition time
        0.009,                                  // signal level to initiate tail
        0.010,                                  // signal level to initiate unmute
        0.000,                                  // minimum tail length
        1.500,                                  // maximum tail length
        0.0);                                   // muted gain

    // AM/SAM demodulator
    amd = new AMD(
        0,                                      // run - OFF by default
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        0,                                      // mode:  0->AM, 1->SAM
        1,                                      // levelfade:  0->OFF, 1->ON
        0,                                      // sideband mode:  0->OFF
        dsp_rate,                          // sample rate
        -2000.0,                                // minimum lock frequency
        +2000.0,                                // maximum lock frequency
        1.0,                                    // zeta
        250.0,                                  // omegaN
        0.02,                                   // tauR
        1.4);                                   // tauI

    // FM demodulator
    fmd = new FMD(
        0,                                      // run
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        dsp_rate,                          // sample rate
        5000.0,                                 // deviation
        300.0,                                  // f_low
        3000.0,                                 // f_high
        -8000.0,                                // fmin
        +8000.0,                                // fmax
        1.0,                                    // zeta
        20000.0,                                // omegaN
        0.02,                                   // tau - for dc removal
        0.5,                                    // audio gain
        1,                                      // run tone filter
        254.1,                                  // ctcss frequency
        std::max(2048, dsp_size),          // # coefs for de-emphasis filter
        0,                                      // min phase flag for de-emphasis filter
        std::max(2048, dsp_size),          // # coefs for audio cutoff filter
        0);                                     // min phase flag for audio cutoff filter

    // FM squelch apply
    fmsq = new FMSQ(
        0,                                      // run
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input signal buffer
        midbuff,                           // pointer to output signal buffer
        fmd->audio.data(),                 // pointer to trigger buffer
        dsp_rate,                          // sample rate
        5000.0,                                 // cutoff freq for noise filter (Hz)
        &fmd->pllpole,                     // pointer to pole frequency of the fmd pll (Hz)
        0.100,                                  // delay time after channel flush
        0.001,                                  // tau for noise averaging
        0.100,                                  // tau for long noise averaging
        0.050,                                  // signal up transition time
        0.010,                                  // signal down transition time
        0.750,                                  // noise level to initiate tail
        0.562,                                  // noise level to initiate unmute
        0.000,                                  // minimum tail time
        0.100,                                  // maximum tail time
        std::max(2048, dsp_size),          // number of coefficients for noise filter
        0);                                     // minimum phase flag

    // Spectral noise blanker (SNB)
    snba = new SNBA(
        0,                                      // run
        midbuff,                           // input buffer
        midbuff,                           // output buffer
        dsp_rate,                          // input / output sample rate
        12000,                                  // internal processing sample rate
        dsp_size,                          // buffer size
        4,                                      // overlap factor to use
        256,                                    // frame size to use; sized for 12K rate
        64,                                     // asize
        2,                                      // npasses
        8.0,                                    // k1
        20.0,                                   // k2
        10,                                     // b
        2,                                      // pre
        2,                                      // post
        0.5,                                    // pmultmin
        200.0,                                  // output resampler low cutoff
        5400.0);                                // output resampler high cutoff

    // Equalizer
    {
        std::array<float, 11> default_F = {0.0,  32.0,  63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
        std::array<float, 11> default_G = {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
        eqp = new EQP(
            0,                                  // run - OFF by default
            dsp_size,                      // buffer size
            std::max(2048, dsp_size),      // number of filter coefficients
            0,                                  // minimum phase flag
            midbuff,                       // pointer to input buffer
            midbuff,                       // pointer to output buffer
            10,                                 // number of frequencies
            default_F.data(),                   // frequency vector
            default_G.data(),                   // gain vector
            0,                                  // cutoff mode
            0,                                  // wintype
            dsp_rate);                     // sample rate
    }

    // Auto notch filter
    anf = new ANF(
        0,                                      // run - OFF by default
        0,                                      // position
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        ANF::ANF_DLINE_SIZE,                    // dline_size
        64,                                     // taps
        16,                                     // delay
        0.0001,                                 // two_mu
        0.1,                                    // gamma
        1.0,                                    // lidx
        0.0,                                    // lidx_min
        200.0,                                  // lidx_max
        6.25e-12,                               // ngamma
        6.25e-10,                               // den_mult
        1.0,                                    // lincr
        3.0);                                   // ldecr

    // LMS noise reduction (ANR or "NR")
    anr = new ANR(
        0,                                      // run - OFF by default
        0,                                      // position
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        ANR::ANR_DLINE_SIZE,                    // dline_size
        64,                                     // taps
        16,                                     // delay
        0.0001,                                 // two_mu
        0.1,                                    // gamma
        120.0,                                  // lidx
        120.0,                                  // lidx_min
        200.0,                                  // lidx_max
        0.001,                                  // ngamma
        6.25e-10,                               // den_mult
        1.0,                                    // lincr
        3.0);                                   // ldecr

    // Spectral noise reduyction (EMNR or "NR2")
    emnr = new EMNR(
        0,                                      // run
        0,                                      // position
        dsp_size,                          // buffer size
        midbuff,                           // input buffer
        midbuff,                           // output buffer
        4096,                                   // FFT size
        4,                                      // overlap
        dsp_rate,                          // samplerate
        0,                                      // window type
        1.0,                                    // gain
        2,                                      // gain method
        0,                                      // npe_method
        1);                                     // ae_run

    // AGC
    agc = new WCPAGC(
        1,                                      // run
        3,                                      // mode
        1,                                      // peakmode = envelope
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        dsp_size,                          // buffer size
        dsp_rate,                          // sample rate
        0.001,                                  // tau_attack
        0.250,                                  // tau_decay
        4,                                      // n_tau
        10000.0,                                // max_gain
        1.5,                                    // var_gain
        1000.0,                                 // fixed_gain
        1.0,                                    // max_input
        1.0,                                    // out_target
        0.250,                                  // tau_fast_backaverage
        0.005,                                  // tau_fast_decay
        5.0,                                    // pop_ratio
        1,                                      // hang_enable
        0.500,                                  // tau_hang_backmult
        0.250,                                  // hangtime
        0.250,                                  // hang_thresh
        0.100);                                 // tau_hang_decay

    // AGC meter
    agcmeter = new METER(
        0,                                      // run
        nullptr,                                      // optional pointer to another 'run'
        dsp_size,                          // size
        midbuff,                           // pointer to buffer
        dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        meter.data(),                             // result vector
        RXA_AGC_AV,                             // index for average value
        RXA_AGC_PK,                             // index for peak value
        RXA_AGC_GAIN,                           // index for gain value
        &agc->gain);                       // pointer for gain computation

    // Bandpass filter - After spectral noise reduction in the block diagram
    bp1 = new BANDPASS (
        1,                                      // run - used only with ( AM || ANF || ANR || EMNR)
        0,                                      // position
        dsp_size,                          // buffer size
        std::max(2048, dsp_size),          // number of coefficients
        0,                                      // flag for minimum phase
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        -4150.0,                                // lower filter frequency
        -150.0,                                 // upper filter frequency
        dsp_rate,                          // sample rate
        1,                                      // wintype
        1.0);                                   // gain

    // Scope/phase display send - pull phase & scope display data
    sip1 = new SIPHON(
        0,                                      // run - needed only for phase display
        0,                                      // position
        0,                                      // mode
        0,                                      // disp
        dsp_size,                          // size of input buffer
        midbuff,                           // input buffer
        4096,                                   // number of samples to store
        4096,                                   // fft size for spectrum
        0);                                     // specmode

    // AM carrier block
    cbl = new CBL(
        0,                                      // run - needed only if set to ON
        dsp_size,                          // buffer size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        0,                                      // mode
        dsp_rate,                          // sample rate
        0.02);                                  // tau

    // CW peaking filter
    speak = new SPEAK(
        0,                                      // run
        dsp_size,                          // buffer size,
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        dsp_rate,                          // sample rate
        600.0,                                  // center frequency
        100.0,                                  // bandwidth
        2.0,                                    // gain
        4,                                      // number of stages
        1);                                     // design

    // Dolly filter (multiple peak filter) - default is 2 for RTTY
    {
        std::array<int, 2> def_enable  = {   1,      1};
        std::array<double, 2> def_freq = {2125.0, 2295.0};
        std::array<double, 2> def_bw   = {  75.0, 75.0};
        std::array<double, 2> def_gain = {   1.0, 1.0};
        mpeak = new MPEAK(
            0,                                  // run
            dsp_size,                      // size
            midbuff,                       // pointer to input buffer
            midbuff,                       // pointer to output buffer
            dsp_rate,                      // sample rate
            2,                                  // number of peaking filters
            def_enable.data(),                  // enable vector
            def_freq.data(),                    // frequency vector
            def_bw.data(),                      // bandwidth vector
            def_gain.data(),                    // gain vector
            4 );                                // number of stages
    }

    // Syllabic squelch (Voice suelch) - Not in the block diagram
    ssql = new SSQL(
        0,                                      // run
        dsp_size,                          // size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        dsp_rate,                          // sample rate
        0.070,                                  // signal up transition time
        0.070,                                  // signal down transition time
        0.0,                                    // muted gain
        0.1,                                    // mute time-constant
        0.1,                                    // unmute time-constant
        0.08,                                   // window threshold
        0.8197,                                 // trigger threshold
        2400,                                   // ring size for f_to_v converter
        2000.0);                                // max freq for f_to_v converter

    // PatchPanel
    panel = new PANEL(
        1,                                      // run
        dsp_size,                          // size
        midbuff,                           // pointer to input buffer
        midbuff,                           // pointer to output buffer
        4.0,                                    // gain1
        1.0,                                    // gain2I
        1.0,                                    // gain2Q
        3,                                      // 3 for I and Q
        0);                                     // no copy

    // AM squelch apply - absent but in the block diagram

    // Output resampler
    rsmpout = new RESAMPLE(
        0,                                      // run - will be turned ON below if needed
        dsp_size,                          // input buffer size
        midbuff,                           // pointer to input buffer
        outbuff,                           // pointer to output buffer
        dsp_rate,                          // input sample rate
        out_rate,                          // output sample rate
        0.0,                                    // select cutoff automatically
        0,                                      // select ncoef automatically
        1.0);                                   // gain

    // turn OFF / ON resamplers as needed
    resCheck();
}

RXA::~RXA()
{
    delete rsmpout;
    delete panel;
    delete ssql;
    delete mpeak;
    delete speak;
    delete cbl;
    delete sip1;
    delete bp1;
    delete agcmeter;
    delete agc;
    delete emnr;
    delete anr;
    delete anf;
    delete eqp;
    delete snba;
    delete fmsq;
    delete fmd;
    delete amd;
    delete amsq;
    delete smeter;
    delete sender;
    delete bpsnba;
    delete nbp0;
    delete ndb;
    delete adcmeter;
    delete rsmpin;
    delete shift;
    delete nob;
    delete anb;
}

void RXA::flush()
{
    Unit::flushBuffers();
    anb->flush();
    nob->flush();
    shift->flush();
    rsmpin->flush();
    adcmeter->flush();
    nbp0->flush();
    bpsnba->flush();
    sender->flush();
    smeter->flush();
    amsq->flush();
    amd->flush();
    fmd->flush();
    fmsq->flush();
    snba->flush();
    eqp->flush();
    anf->flush();
    anr->flush();
    emnr->flush();
    agc->flush();
    agcmeter->flush();
    bp1->flush();
    sip1->flush();
    cbl->flush();
    speak->flush();
    mpeak->flush();
    ssql->flush();
    panel->flush();
    rsmpout->flush();
}

void RXA::execute()
{
    anb->execute();
    nob->execute();
    shift->execute();
    rsmpin->execute();
    adcmeter->execute();
    bpsnba->exec_in(0);
    nbp0->execute(0);
    smeter->execute();
    sender->execute();
    amsq->xcap();
    bpsnba->exec_out(0);
    amd->execute();
    fmd->execute();
    fmsq->execute();
    bpsnba->exec_in(1);
    bpsnba->exec_out(1);
    snba->execute();
    eqp->execute();
    anf->execute(0);
    anr->ANR::execute(0);
    emnr->execute(0);
    bp1->BANDPASS::execute(0);
    agc->execute();
    anf->execute(1);
    anr->execute(1);
    emnr->execute(1);
    bp1->execute(1);
    agcmeter->execute();
    sip1->execute(0);
    cbl->execute();
    speak->execute();
    mpeak->execute();
    ssql->execute();
    panel->execute();
    amsq->execute();
    rsmpout->execute();
}

void RXA::setInputSamplerate(int _in_rate)
{
    Unit::setBuffersInputSamplerate(_in_rate);
    // anb
    anb->setBuffers(inbuff, inbuff);
    anb->setSize(dsp_insize);
    anb->setSamplerate(in_rate);
    // nob
    nob->setBuffers(inbuff, inbuff);
    nob->setSize(dsp_insize);
    nob->setSamplerate(in_rate);
    // shift
    shift->setBuffers(inbuff, inbuff);
    shift->setSize(dsp_insize);
    shift->setSamplerate(in_rate);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    rsmpin->setInRate(in_rate);
    resCheck();
}

void RXA::setOutputSamplerate(int _out_rate)
{
    Unit::setBuffersOutputSamplerate(_out_rate);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setOutRate(out_rate);
    resCheck();
}

void RXA::setDSPSamplerate(int _dsp_rate)
{
    Unit::setBuffersDSPSamplerate(_dsp_rate);
    // anb
    anb->setBuffers(inbuff, inbuff);
    anb->setSize(dsp_insize);
    // nob
    nob->setBuffers(inbuff, inbuff);
    nob->setSize(dsp_insize);
    // shift
    shift->setBuffers(inbuff, inbuff);
    shift->setSize(dsp_insize);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    rsmpin->setOutRate(dsp_rate);
    // dsp_rate blocks
    adcmeter->setSamplerate(dsp_rate);
    nbp0->setSamplerate(dsp_rate);
    bpsnba->setSamplerate(dsp_rate);
    smeter->setSamplerate(dsp_rate);
    sender->setSamplerate(dsp_rate);
    amsq->setSamplerate(dsp_rate);
    amd->setSamplerate(dsp_rate);
    fmd->setSamplerate(dsp_rate);
    fmsq->setBuffers(midbuff, midbuff, fmd->audio.data());
    fmsq->setSamplerate(dsp_rate);
    // snba->setSamplerate(dsp_rate); SMBA removed
    eqp->setSamplerate(dsp_rate);
    anf->setSamplerate(dsp_rate);
    anr->setSamplerate(dsp_rate);
    emnr->setSamplerate(dsp_rate);
    bp1->setSamplerate(dsp_rate);
    agc->setSamplerate(dsp_rate);
    agcmeter->setSamplerate(dsp_rate);
    sip1->setSamplerate(dsp_rate);
    cbl->setSamplerate(dsp_rate);
    speak->setSamplerate(dsp_rate);
    mpeak->setSamplerate(dsp_rate);
    ssql->setSamplerate(dsp_rate);
    panel->setSamplerate(dsp_rate);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setInRate(dsp_rate);
    resCheck();
}

void RXA::setDSPBuffsize(int _dsp_size)
{
    Unit::setBuffersDSPBuffsize(_dsp_size);
    // anb
    anb->setBuffers(inbuff, inbuff);
    anb->setSize(dsp_insize);
    // nob
    nob->setBuffers(inbuff, inbuff);
    nob->setSize(dsp_insize);
    // shift
    shift->setBuffers(inbuff, inbuff);
    shift->setSize(dsp_insize);
    // input resampler
    rsmpin->setBuffers(inbuff, midbuff);
    rsmpin->setSize(dsp_insize);
    // dsp_size blocks
    adcmeter->setBuffers(midbuff);
    adcmeter->setSize(dsp_size);
    nbp0->setBuffers(midbuff, midbuff);
    nbp0->setSize(dsp_size);
    bpsnba->setBuffers(midbuff, midbuff);
    bpsnba->setSize(dsp_size);
    smeter->setBuffers(midbuff);
    smeter->setSize(dsp_size);
    sender->setBuffers(midbuff);
    sender->setSize(dsp_size);
    amsq->setBuffers(midbuff, midbuff, midbuff);
    amsq->setSize(dsp_size);
    amd->setBuffers(midbuff, midbuff);
    amd->setSize(dsp_size);
    fmd->setBuffers(midbuff, midbuff);
    fmd->setSize(dsp_size);
    fmsq->setBuffers(midbuff, midbuff, fmd->audio.data());
    fmsq->setSize(dsp_size);
    snba->setBuffers(midbuff, midbuff);
    snba->setSize(dsp_size);
    eqp->setBuffers(midbuff, midbuff);
    eqp->setSize(dsp_size);
    anf->setBuffers(midbuff, midbuff);
    anf->setSize(dsp_size);
    anr->setBuffers(midbuff, midbuff);
    anr->setSize(dsp_size);
    emnr->setBuffers(midbuff, midbuff);
    emnr->setSize(dsp_size);
    bp1->setBuffers(midbuff, midbuff);
    bp1->setSize(dsp_size);
    agc->setBuffers(midbuff, midbuff);
    agc->setSize(dsp_size);
    agcmeter->setBuffers(midbuff);
    agcmeter->setSize(dsp_size);
    sip1->setBuffers(midbuff);
    sip1->setSize(dsp_size);
    cbl->setBuffers(midbuff, midbuff);
    cbl->setSize(dsp_size);
    speak->setBuffers(midbuff, midbuff);
    speak->setSize(dsp_size);
    mpeak->setBuffers(midbuff, midbuff);
    mpeak->setSize(dsp_size);
    ssql->setBuffers(midbuff, midbuff);
    ssql->setSize(dsp_size);
    panel->setBuffers(midbuff, midbuff);
    panel->setSize(dsp_size);
    // output resampler
    rsmpout->setBuffers(midbuff, outbuff);
    rsmpout->setSize(dsp_size);
}

void RXA::setSpectrumProbe(BufferProbe *spectrumProbe)
{
    sender->SetSpectrum(1, spectrumProbe);
    sender->run = 1;
}

/********************************************************************************************************
*                                                                                                       *
*                                       RXA Mode & Filter Controls                                      *
*                                                                                                       *
********************************************************************************************************/

void RXA::setMode(int _mode)
{
    if (mode != _mode)
    {
        int amd_run = (_mode == RXA_AM) || (_mode == RXA_SAM);
        bpsnbaCheck (_mode, ndb->master_run);
        bp1Check (
            amd_run,
            snba->run,
            emnr->run,
            anf->run,
            anr->run
        );
        mode = _mode;
        amd->run  = 0;
        fmd->run  = 0;

        switch (_mode)
        {
        case RXA_AM:
            amd->run  = 1;
            amd->mode = 0;
            break;
        case RXA_SAM:
            amd->run  = 1;
            amd->mode = 1;
            break;
        case RXA_DSB:
            break;
        case RXA_FM:
            fmd->run  = 1;
            break;
        default:

            break;
        }

        bp1Set();
        bpsnbaSet();                         // update variables
    }
}

void RXA::resCheck()
{
    // turn OFF/ON resamplers depending upon whether they're needed
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

void RXA::bp1Check (
    int amd_run,
    int snba_run,
    int emnr_run,
    int anf_run,
    int anr_run
)
{
    BANDPASS *a = bp1;
    float gain;
    if (amd_run  ||
        snba_run ||
        emnr_run ||
        anf_run  ||
        anr_run
    )
        gain = 2.0;
    else
        gain = 1.0;
    if (a->gain != gain)
        a->setGain(gain, 0);
}

void RXA::bp1Set ()
{
    BANDPASS *a = bp1;
    int old = a->run;
    if ((amd->run  == 1) ||
        (snba->run == 1) ||
        (emnr->run == 1) ||
        (anf->run  == 1) ||
        (anr->run  == 1)
    )
        a->run = 1;
    else
        a->run = 0;
    if (!old && a->run)
        a->flush();
    a->fircore->setUpdate();
}

void RXA::bpsnbaCheck(int _mode, int _notch_run)
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = bpsnba;
    double f_low = 0.0;
    double f_high = 0.0;
    int run_notches = 0;
    switch (_mode)
    {
        case RXA_LSB:
        case RXA_CWL:
        case RXA_DIGL:
            f_low  = -a->abs_high_freq;
            f_high = -a->abs_low_freq;
            run_notches = _notch_run;
            break;
        case RXA_USB:
        case RXA_CWU:
        case RXA_DIGU:
            f_low  = +a->abs_low_freq;
            f_high = +a->abs_high_freq;
            run_notches = _notch_run;
            break;
        case RXA_AM:
        case RXA_SAM:
        case RXA_DSB:
        case RXA_FM:
            f_low  = +a->abs_low_freq;
            f_high = +a->abs_high_freq;
            run_notches = 0;
            break;
        case RXA_DRM:
        case RXA_SPEC:
            break;
        default:
            break;
    }
    // 'run' and 'position' are examined at run time; no filter changes required.
    // Recalculate filter if frequencies OR 'run_notches' changed.
    if ((a->f_low       != f_low      ) ||
        (a->f_high      != f_high     ) ||
        (a->run_notches != run_notches))
    {
        a->f_low  = f_low;
        a->f_high = f_high;
        a->run_notches = run_notches;
        // f_low, f_high, run_notches are needed for the filter recalculation
        bpsnba->recalc_bpsnba_filter(0);
    }
}

void RXA::bpsnbaSet()
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = bpsnba;
    switch (mode)
    {
        case RXA_LSB:
        case RXA_CWL:
        case RXA_DIGL:
        case RXA_USB:
        case RXA_CWU:
        case RXA_DIGU:
            a->run = snba->run;
            a->position = 0;
            break;
        case RXA_DSB:
        case RXA_AM:
        case RXA_FM:
            a->run = snba->run;
            a->position = 1;
            break;
        case RXA_SAM:
        case RXA_DRM:
        case RXA_SPEC:
            a->run = 0;
            break;
        default:
            break;
    }
    a->bpsnba->fircore->setUpdate();
}

void RXA::updateNBPFiltersLightWeight()
{   // called when setting tune freq or shift freq
    nbp0->calc_lightweight();
    bpsnba->bpsnba->calc_lightweight();
}

void RXA::updateNBPFilters()
{
    NBP *a = nbp0;
    BPSNBA *b = bpsnba;
    if (a->fnfrun)
    {
        a->calc_impulse();
        a->fircore->setImpulse(a->impulse, 1);
    }
    if (b->bpsnba->fnfrun)
    {
        b->recalc_bpsnba_filter(1);
    }
}

int RXA::nbpAddNotch(int _notch, double _fcenter, double _fwidth, int _active)
{
    NOTCHDB *b = ndb;
    int rval = b->addNotch(_notch, _fcenter, _fwidth, _active);

    if (rval == 0) {
        updateNBPFilters();
    }

    return rval;
}

int RXA::nbpGetNotch(int _notch, double* _fcenter, double* _fwidth, int* _active) const
{
    NOTCHDB *a = ndb;
    int rval = a->getNotch(_notch, _fcenter, _fwidth, _active);
    return rval;
}

int RXA::nbpDeleteNotch(int _notch)
{
    NOTCHDB *a = ndb;
    int rval = a->deleteNotch(_notch);

    if (rval == 0) {
        updateNBPFilters();
    }

    return rval;
}

int RXA::nbpEditNotch(int _notch, double _fcenter, double _fwidth, int _active)
{
    NOTCHDB *a = ndb;
    int rval = a->editNotch(_notch, _fcenter, _fwidth, _active);

    if (rval == 0) {
        updateNBPFilters();
    }

    return rval;
}

void RXA::nbpGetNumNotches(int* _nnotches) const
{
    const NOTCHDB *a = ndb;
    a->getNumNotches(_nnotches);
}

void RXA::nbpSetTuneFrequency(double _tunefreq)
{
    NOTCHDB *a;
    a = ndb;

    if (_tunefreq != a->tunefreq)
    {
        a->tunefreq = _tunefreq;
        updateNBPFiltersLightWeight();
    }
}

void RXA::nbpSetShiftFrequency(double _shift)
{
    NOTCHDB *a;
    a = ndb;
    if (_shift != a->shift)
    {
        a->shift = _shift;
        updateNBPFiltersLightWeight();
    }
}

void RXA::nbpSetNotchesRun(int _run)
{
    NOTCHDB *a = ndb;
    NBP *b = nbp0;

    if ( _run != a->master_run)
    {
        a->master_run = _run;                            // update variables
        b->fnfrun = a->master_run;
        bpsnbaCheck(mode, _run);
        b->calc_impulse();                           // recalc nbp impulse response
        b->fircore->setImpulse(b->impulse, 0);       // calculate new filter masks
        bpsnbaSet();
        b->fircore->setUpdate();                       // apply new filter masks
    }
}

void RXA::nbpSetWindow(int _wintype)
{
    NBP *a;
    BPSNBA *b;
    a = nbp0;
    b = bpsnba;

    if (a->wintype != _wintype)
    {
        a->wintype = _wintype;
        a->calc_impulse();
        a->fircore->setImpulse(a->impulse, 1);
    }

    if (b->wintype != _wintype)
    {
        b->wintype = _wintype;
        b->recalc_bpsnba_filter(1);
    }
}

void RXA::nbpSetAutoIncrease(int _autoincr)
{
    NBP *a;
    BPSNBA *b;
    a = nbp0;
    b = bpsnba;

    if (a->autoincr != _autoincr)
    {
        a->autoincr = _autoincr;
        a->calc_impulse();
        a->fircore->setImpulse(a->impulse, 1);
    }

    if (b->autoincr != _autoincr)
    {
        b->autoincr = _autoincr;
        b->recalc_bpsnba_filter(1);
    }
}

void RXA::setAMDRun(int _run)
{
    if (amd->run != _run)
    {
        bp1Check (
            _run,
            snba->run,
            emnr->run,
            anf->run,
            anr->run
        );

        amd->run = _run;
        bp1Set();
    }
}

void RXA::setSNBARun(int _run)
{
    SNBA *a = snba;

    if (a->run != _run)
    {
        bpsnbaCheck(mode, ndb->master_run);
        bp1Check(
            amd->run,
            _run,
            emnr->run,
            anf->run,
            anr->run
        );
        a->run = _run;
        bp1Set();
        bpsnbaSet();
    }
}

void RXA::setANFRun(int _run)
{
    ANF *a = anf;

    if (a->run != _run)
    {
        bp1Check (
            amd->run,
            snba->run,
            emnr->run,
            _run,
            anr->run
        );
        a->run = _run;
        bp1Set();
        a->flush();
    }
}

void RXA::setANFPosition(int _position)
{
    anf->position = _position;
    bp1->position = _position;
    anf->flush();
}

void RXA::setANRRun(int _run)
{
    ANR *a = anr;

    if (a->run != _run)
    {
        bp1Check (
            amd->run,
            snba->run,
            emnr->run,
            anf->run,
            _run
        );
        a->run = _run;
        bp1Set();
        a->flush();
    }
}

void RXA::setANRPosition(int _position)
{
    anr->position = _position;
    bp1->position = _position;
    anr->flush();
}

void RXA::setEMNRRun(int _run)
{
    EMNR *a = emnr;

    if (a->run != _run)
    {
        bp1Check (
            amd->run,
            snba->run,
            _run,
            anf->run,
            anr->run
        );
        a->run = _run;
        bp1Set();
    }
}

void RXA::setEMNRPosition(int _position)
{
    emnr->position = _position;
    bp1->position  = _position;
}

void RXA::getAGCThresh(double *_thresh, double _size, double _rate) const
//for line on bandscope.
{
    double noise_offset;
    noise_offset = 10.0 * log10((nbp0->fhigh - nbp0->flow) * _size / _rate);
    *_thresh = 20.0 * log10( agc->min_volts ) - noise_offset;
}

void RXA::setAGCThresh(double _thresh, double _size, double _rate)
//for line on bandscope
{
    double noise_offset;
    noise_offset = 10.0 * log10((nbp0->fhigh - nbp0->flow) * _size / _rate);
    agc->max_gain = agc->out_target / (agc->var_gain * pow (10.0, (_thresh + noise_offset) / 20.0));
    agc->loadWcpAGC();
}

/********************************************************************************************************
*                                                                                                       *
*                                               Collectives                                             *
*                                                                                                       *
********************************************************************************************************/

void RXA::setPassband(float _f_low, float _f_high)
{
    bp1->setBandpassFreqs         (_f_low, _f_high); // After spectral noise reduction ( AM || ANF || ANR || EMNR)
    snba->setOutputBandwidth      (_f_low, _f_high); // Spectral noise blanker (SNB)
    nbp0->SetFreqs                (_f_low, _f_high); // Notched bandpass
}

void RXA::setNC(int _nc)
{
    int oldstate = state;
    nbp0->SetNC              (_nc);
    bpsnba->SetNC            (_nc);
    bp1->SetBandpassNC       (_nc);
    eqp->setNC               (_nc);
    fmsq->setNC              (_nc);
    fmd->setNCde             (_nc);
    fmd->setNCaud            (_nc);
    state = oldstate;
}

void RXA::setMP(int _mp)
{
    nbp0->SetMP              (_mp);
    bpsnba->SetMP            (_mp);
    bp1->SetBandpassMP       (_mp);
    eqp->setMP               (_mp);
    fmsq->setMP              (_mp);
    fmd->setMPde             (_mp);
    fmd->setMPaud            (_mp);
}

} // namespace WDSP
