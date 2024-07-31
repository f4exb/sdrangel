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

namespace WDSP {

RXA* RXA::create_rxa (
    int in_rate,                // input samplerate
    int out_rate,               // output samplerate
    int dsp_rate,               // sample rate for mainstream dsp processing
    int dsp_size                // number complex samples processed per buffer in mainstream dsp processing
)
{
    RXA* rxa = new RXA;

    rxa->in_rate = in_rate;
    rxa->out_rate = out_rate;
    rxa->dsp_rate = dsp_rate;
    rxa->dsp_size = dsp_size;

    if (in_rate  >= dsp_rate)
        rxa->dsp_insize  = dsp_size * (in_rate  / dsp_rate);
    else
        rxa->dsp_insize  = dsp_size / (dsp_rate /  in_rate);

    if (out_rate >= dsp_rate)
        rxa->dsp_outsize = dsp_size * (out_rate / dsp_rate);
    else
        rxa->dsp_outsize = dsp_size / (dsp_rate / out_rate);

    rxa->mode = RXA_LSB;
    rxa->inbuff  = new float[1 * rxa->dsp_insize  * 2]; // (float *) malloc0 (1 * ch.dsp_insize  * sizeof (complex));
    rxa->outbuff = new float[1 * rxa->dsp_outsize  * 2]; // (float *) malloc0 (1 * ch.dsp_outsize * sizeof (complex));
    rxa->midbuff = new float[2 * rxa->dsp_size  * 2]; // (float *) malloc0 (2 * ch.dsp_size    * sizeof (complex));
    std::fill(rxa->meter, rxa->meter + RXA_METERTYPE_LAST, 0);

    // Noise blanker (ANB or "NB")
    rxa->anb = new ANB(
        0, // run
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->inbuff,                            // pointer to output buffer
        rxa->in_rate,                           // samplerate
        0.0001,                                 // tau
        0.0001,                                 // hang time
        0.0001,                                 // advance time
        0.05,                                   // back tau
        30                                      // thershold
    );
    // Noise blanker (NOB or "NB2")
    rxa->nob = new NOB(
        0, // run
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->inbuff,                            // pointer to output buffer
        rxa->in_rate,                           // samplerate
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
    rxa->shift = new SHIFT(
        0,                                      // run
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->inbuff,                            // pointer to output buffer
        rxa->in_rate,                           // samplerate
        0.0);                                   // amount to shift (Hz)

    // Input resampler - resample to dsp rate for main processing
    rxa->rsmpin = new RESAMPLE(
        0,                                      // run - will be turned ON below if needed
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->in_rate,                           // input samplerate
        rxa->dsp_rate,                          // output samplerate
        0.0,                                    // select cutoff automatically
        0,                                      // select ncoef automatically
        1.0);                                   // gain

    // Input meter - ADC
    rxa->adcmeter = new METER(
        0,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        RXA_ADC_AV,                             // index for average value
        RXA_ADC_PK,                             // index for peak value
        -1,                                     // index for gain value - disabled
        0);                                     // pointer for gain computation

    // Notched bandpass section

    // notch database
    rxa->ndb = new NOTCHDB (
        0,                                      // master run for all nbp's
        1024);                                  // max number of notches

    // notched bandpass
    rxa->nbp0 = new NBP (
        1,                                      // run, always runs
        0,                                      // run the notches
        0,                                      // position
        rxa->dsp_size,                          // buffer size
        std::max(2048, rxa->dsp_size),          // number of coefficients
        0,                                      // minimum phase flag
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        -4150.0,                                // lower filter frequency
        -150.0,                                 // upper filter frequency
        rxa->dsp_rate,                          // sample rate
        0,                                      // wintype
        1.0,                                    // gain
        1,                                      // auto-increase notch width
        1025,                                   // max number of passbands
        rxa->ndb);                              // addr of database pointer

    // bandpass for snba
    rxa->bpsnba = new BPSNBA (
        0,                                      // bpsnba run flag
        0,                                      // run the notches
        0,                                      // position
        rxa->dsp_size,                          // size
        std::max(2048, rxa->dsp_size),          // number of filter coefficients
        0,                                      // minimum phase flag
        rxa->midbuff,                           // input buffer
        rxa->midbuff,                           // output buffer
        rxa->dsp_rate,                          // samplerate
        + 250.0,                                // abs value of cutoff nearest zero
        + 5700.0,                               // abs value of cutoff farthest zero
        - 5700.0,                               // current low frequency
        - 250.0,                                // current high frequency
        0,                                      // wintype
        1.0,                                    // gain
        1,                                      // auto-increase notch width
        1025,                                   // max number of passbands
        rxa->ndb);                              // addr of database pointer

    // Post filter display send - send spectrum display (after S-meter in the block diagram)
    rxa->sender = new SENDER (
        0,                                      // run
        0,                                      // flag
        0,                                      // mode
        rxa->dsp_size,                          // size
        rxa->midbuff                            // pointer to input buffer
    );

    // End notched bandpass section

    // S-meter
    rxa->smeter = new METER(
        1,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        RXA_S_AV,                               // index for average value
        RXA_S_PK,                               // index for peak value
        -1,                                     // index for gain value - disabled
        0);                                     // pointer for gain computation

    // AM squelch capture (for other modes than FM)
    rxa->amsq = new AMSQ(
        0,                                      // run
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to signal input buffer used by xamsq
        rxa->midbuff,                           // pointer to signal output buffer used by xamsq
        rxa->midbuff,                           // pointer to trigger buffer that xamsqcap will capture
        rxa->dsp_rate,                          // sample rate
        0.010,                                  // time constant for averaging signal level
        0.070,                                  // signal up transition time
        0.070,                                  // signal down transition time
        0.009,                                  // signal level to initiate tail
        0.010,                                  // signal level to initiate unmute
        0.000,                                  // minimum tail length
        1.500,                                  // maximum tail length
        0.0);                                   // muted gain

    // AM/SAM demodulator
    rxa->amd = new AMD(
        0,                                      // run - OFF by default
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        0,                                      // mode:  0->AM, 1->SAM
        1,                                      // levelfade:  0->OFF, 1->ON
        0,                                      // sideband mode:  0->OFF
        rxa->dsp_rate,                          // sample rate
        -2000.0,                                // minimum lock frequency
        +2000.0,                                // maximum lock frequency
        1.0,                                    // zeta
        250.0,                                  // omegaN
        0.02,                                   // tauR
        1.4);                                   // tauI

    // FM demodulator
    rxa->fmd = new FMD(
        0,                                      // run
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->dsp_rate,                          // sample rate
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
        std::max(2048, rxa->dsp_size),          // # coefs for de-emphasis filter
        0,                                      // min phase flag for de-emphasis filter
        std::max(2048, rxa->dsp_size),          // # coefs for audio cutoff filter
        0);                                     // min phase flag for audio cutoff filter

    // FM squelch apply
    rxa->fmsq = new FMSQ(
        0,                                      // run
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input signal buffer
        rxa->midbuff,                           // pointer to output signal buffer
        rxa->fmd->audio.data(),                 // pointer to trigger buffer
        rxa->dsp_rate,                          // sample rate
        5000.0,                                 // cutoff freq for noise filter (Hz)
        &rxa->fmd->pllpole,                     // pointer to pole frequency of the fmd pll (Hz)
        0.100,                                  // delay time after channel flush
        0.001,                                  // tau for noise averaging
        0.100,                                  // tau for long noise averaging
        0.050,                                  // signal up transition time
        0.010,                                  // signal down transition time
        0.750,                                  // noise level to initiate tail
        0.562,                                  // noise level to initiate unmute
        0.000,                                  // minimum tail time
        0.100,                                  // maximum tail time
        std::max(2048, rxa->dsp_size),          // number of coefficients for noise filter
        0);                                     // minimum phase flag

    // Spectral noise blanker (SNB)
    rxa->snba = new SNBA(
        0,                                      // run
        rxa->midbuff,                           // input buffer
        rxa->midbuff,                           // output buffer
        rxa->dsp_rate,                          // input / output sample rate
        12000,                                  // internal processing sample rate
        rxa->dsp_size,                          // buffer size
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
        float default_F[11] = {0.0,  32.0,  63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
        //float default_G[11] = {0.0, -12.0, -12.0, -12.0,  -1.0,  +1.0,   +4.0,   +9.0,  +12.0,  -10.0,   -10.0};
        float default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
        rxa->eqp = new EQP(
            0,                                  // run - OFF by default
            rxa->dsp_size,                      // buffer size
            std::max(2048, rxa->dsp_size),      // number of filter coefficients
            0,                                  // minimum phase flag
            rxa->midbuff,                       // pointer to input buffer
            rxa->midbuff,                       // pointer to output buffer
            10,                                 // number of frequencies
            default_F,                          // frequency vector
            default_G,                          // gain vector
            0,                                  // cutoff mode
            0,                                  // wintype
            rxa->dsp_rate);                     // sample rate
    }

    // Auto notch filter
    rxa->anf = new ANF(
        0,                                      // run - OFF by default
        0,                                      // position
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        ANF_DLINE_SIZE,                         // dline_size
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
    rxa->anr = new ANR(
        0,                                      // run - OFF by default
        0,                                      // position
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        ANR_DLINE_SIZE,                         // dline_size
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
    rxa->emnr = new EMNR(
        0,                                      // run
        0,                                      // position
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // input buffer
        rxa->midbuff,                           // output buffer
        4096,                                   // FFT size
        4,                                      // overlap
        rxa->dsp_rate,                          // samplerate
        0,                                      // window type
        1.0,                                    // gain
        2,                                      // gain method
        0,                                      // npe_method
        1);                                     // ae_run

    // AGC
    rxa->agc = new WCPAGC(
        1,                                      // run
        3,                                      // mode
        1,                                      // peakmode = envelope
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->dsp_size,                          // buffer size
        rxa->dsp_rate,                          // sample rate
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
    rxa->agcmeter = new METER(
        0,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        RXA_AGC_AV,                             // index for average value
        RXA_AGC_PK,                             // index for peak value
        RXA_AGC_GAIN,                           // index for gain value
        &rxa->agc->gain);                       // pointer for gain computation

    // Bandpass filter - After spectral noise reduction in the block diagram
    rxa->bp1 = new BANDPASS (
        1,                                      // run - used only with ( AM || ANF || ANR || EMNR)
        0,                                      // position
        rxa->dsp_size,                          // buffer size
        std::max(2048, rxa->dsp_size),          // number of coefficients
        0,                                      // flag for minimum phase
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        -4150.0,                                // lower filter frequency
        -150.0,                                 // upper filter frequency
        rxa->dsp_rate,                          // sample rate
        1,                                      // wintype
        1.0);                                   // gain

    // Scope/phase display send - pull phase & scope display data
    rxa->sip1 = new SIPHON(
        0,                                      // run - needed only for phase display
        0,                                      // position
        0,                                      // mode
        0,                                      // disp
        rxa->dsp_size,                          // size of input buffer
        rxa->midbuff,                           // input buffer
        4096,                                   // number of samples to store
        4096,                                   // fft size for spectrum
        0);                                     // specmode

    // AM carrier block
    rxa->cbl = new CBL(
        0,                                      // run - needed only if set to ON
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        0,                                      // mode
        rxa->dsp_rate,                          // sample rate
        0.02);                                  // tau

    // CW peaking filter
    rxa->speak = new SPEAK(
        0,                                      // run
        rxa->dsp_size,                          // buffer size,
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->dsp_rate,                          // sample rate
        600.0,                                  // center frequency
        100.0,                                  // bandwidth
        2.0,                                    // gain
        4,                                      // number of stages
        1);                                     // design

    // Dolly filter (multiple peak filter) - default is 2 for RTTY
    {
        int def_enable[2] = {1, 1};
        double def_freq[2] = {2125.0, 2295.0};
        double def_bw[2] = {75.0, 75.0};
        double def_gain[2] = {1.0, 1.0};
        rxa->mpeak = new MPEAK(
            0,                                  // run
            rxa->dsp_size,                      // size
            rxa->midbuff,                       // pointer to input buffer
            rxa->midbuff,                       // pointer to output buffer
            rxa->dsp_rate,                      // sample rate
            2,                                  // number of peaking filters
            def_enable,                         // enable vector
            def_freq,                           // frequency vector
            def_bw,                             // bandwidth vector
            def_gain,                           // gain vector
            4 );                                // number of stages
    }

    // Syllabic squelch (Voice suelch) - Not in the block diagram
    rxa->ssql = SSQL::create_ssql(
        0,                                      // run
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->dsp_rate,                          // sample rate
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
    rxa->panel = PANEL::create_panel (
        1,                                      // run
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        4.0,                                    // gain1
        1.0,                                    // gain2I
        1.0,                                    // gain2Q
        3,                                      // 3 for I and Q
        0);                                     // no copy

    // AM squelch apply - absent but in the block diagram

    // Output resampler
    rxa->rsmpout = new RESAMPLE(
        0,                                      // run - will be turned ON below if needed
        rxa->dsp_size,                          // input buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->outbuff,                           // pointer to output buffer
        rxa->dsp_rate,                          // input sample rate
        rxa->out_rate,                          // output sample rate
        0.0,                                    // select cutoff automatically
        0,                                      // select ncoef automatically
        1.0);                                   // gain

    // turn OFF / ON resamplers as needed
    ResCheck (*rxa);
    return rxa;
}

void RXA::destroy_rxa (RXA *rxa)
{
    delete (rxa->rsmpout);
    PANEL::destroy_panel (rxa->panel);
    SSQL::destroy_ssql (rxa->ssql);
    delete (rxa->mpeak);
    delete (rxa->speak);
    delete (rxa->cbl);
    delete (rxa->sip1);
    delete (rxa->bp1);
    delete (rxa->agcmeter);
    delete (rxa->agc);
    delete (rxa->emnr);
    delete (rxa->anr);
    delete (rxa->anf);
    delete (rxa->eqp);
    delete (rxa->snba);
    delete (rxa->fmsq);
    delete (rxa->fmd);
    delete (rxa->amd);
    delete (rxa->amsq);
    delete (rxa->smeter);
    delete (rxa->sender);
    delete (rxa->bpsnba);
    delete (rxa->nbp0);
    delete (rxa->ndb);
    delete (rxa->adcmeter);
    delete (rxa->rsmpin);
    delete (rxa->shift);
    delete (rxa->nob);
    delete (rxa->anb);
    delete[] (rxa->midbuff);
    delete[] (rxa->outbuff);
    delete[] (rxa->inbuff);
    delete rxa;
}

void RXA::flush_rxa (RXA *rxa)
{
    std::fill(rxa->inbuff,  rxa->inbuff  + 1 * rxa->dsp_insize  * 2, 0);
    std::fill(rxa->outbuff, rxa->outbuff + 1 * rxa->dsp_outsize * 2, 0);
    std::fill(rxa->midbuff, rxa->midbuff + 2 * rxa->dsp_size    * 2, 0);
    rxa->anb->flush();
    rxa->nob->flush();
    rxa->shift->flush();
    rxa->rsmpin->flush();
    rxa->adcmeter->flush();
    rxa->nbp0->flush();
    rxa->bpsnba->flush();
    rxa->sender->flush();
    rxa->smeter->flush();
    rxa->amsq->flush();
    rxa->amd->flush();
    rxa->fmd->flush();
    rxa->fmsq->flush();
    rxa->snba->flush();
    rxa->eqp->flush();
    rxa->anf->flush();
    rxa->anr->flush();
    rxa->emnr->flush();
    rxa->agc->flush();
    rxa->agcmeter->flush();
    rxa->bp1->flush();
    rxa->sip1->flush();
    rxa->cbl->flush();
    rxa->speak->flush();
    rxa->mpeak->flush();
    SSQL::flush_ssql (rxa->ssql);
    PANEL::flush_panel (rxa->panel);
    rxa->rsmpout->flush();
}

void RXA::xrxa (RXA *rxa)
{
    rxa->anb->execute();
    rxa->nob->execute();
    rxa->shift->execute();
    rxa->rsmpin->execute();
    rxa->adcmeter->execute();
    rxa->bpsnba->exec_in(0);
    rxa->nbp0->execute(0);
    rxa->smeter->execute();
    rxa->sender->execute();
    rxa->amsq->xcap();
    rxa->bpsnba->exec_out(0);
    rxa->amd->execute();
    rxa->fmd->execute();
    rxa->fmsq->execute();
    rxa->bpsnba->exec_in(1);
    rxa->bpsnba->exec_out(1);
    rxa->snba->execute();
    rxa->eqp->execute();
    rxa->anf->execute(0);
    rxa->anr->ANR::execute(0);
    rxa->emnr->execute(0);
    rxa->bp1->BANDPASS::execute(0);
    rxa->agc->execute();
    rxa->anf->execute(1);
    rxa->anr->execute(1);
    rxa->emnr->execute(1);
    rxa->bp1->execute(1);
    rxa->agcmeter->execute();
    rxa->sip1->execute(0);
    rxa->cbl->execute();
    rxa->speak->execute();
    rxa->mpeak->execute();
    SSQL::xssql (rxa->ssql);
    PANEL::xpanel (rxa->panel);
    rxa->amsq->execute();
    rxa->rsmpout->execute();
}

void RXA::setInputSamplerate (RXA *rxa, int in_rate)
{
    if (in_rate  >= rxa->dsp_rate)
        rxa->dsp_insize  = rxa->dsp_size * (in_rate  / rxa->dsp_rate);
    else
        rxa->dsp_insize  = rxa->dsp_size / (rxa->dsp_rate /  in_rate);

    rxa->in_rate = in_rate;
    // buffers
    delete[] (rxa->inbuff);
    rxa->inbuff = new float[1 * rxa->dsp_insize  * 2]; // (float *)malloc0(1 * ch.dsp_insize  * sizeof(complex));
    // anb
    rxa->anb->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->anb->setSize(rxa->dsp_insize);
    rxa->anb->setSamplerate(rxa->in_rate);
    // nob
    rxa->nob->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->nob->setSize(rxa->dsp_insize);
    rxa->nob->setSamplerate(rxa->in_rate);
    // shift
    rxa->shift->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->shift->setSize(rxa->dsp_insize);
    rxa->shift->setSamplerate(rxa->in_rate);
    // input resampler
    rxa->rsmpin->setBuffers(rxa->inbuff, rxa->midbuff);
    rxa->rsmpin->setSize(rxa->dsp_insize);
    rxa->rsmpin->setInRate(rxa->in_rate);
    ResCheck (*rxa);
}

void RXA::setOutputSamplerate (RXA *rxa, int out_rate)
{
    if (out_rate >= rxa->dsp_rate)
        rxa->dsp_outsize = rxa->dsp_size * (out_rate / rxa->dsp_rate);
    else
        rxa->dsp_outsize = rxa->dsp_size / (rxa->dsp_rate / out_rate);

    rxa->out_rate = out_rate;
    // buffers
    delete[] (rxa->outbuff);
    rxa->outbuff = new float[1 * rxa->dsp_outsize * 2]; // (float *)malloc0(1 * ch.dsp_outsize * sizeof(complex));
    // output resampler
    rxa->rsmpout->setBuffers(rxa->midbuff, rxa->outbuff);
    rxa->rsmpout->setOutRate(rxa->out_rate);
    ResCheck (*rxa);
}

void RXA::setDSPSamplerate (RXA *rxa, int dsp_rate)
{
    if (rxa->in_rate  >= dsp_rate)
        rxa->dsp_insize  = rxa->dsp_size * (rxa->in_rate  / dsp_rate);
    else
        rxa->dsp_insize  = rxa->dsp_size / (dsp_rate /  rxa->in_rate);

    if (rxa->out_rate >= dsp_rate)
        rxa->dsp_outsize = rxa->dsp_size * (rxa->out_rate / dsp_rate);
    else
        rxa->dsp_outsize = rxa->dsp_size / (dsp_rate / rxa->out_rate);

    rxa->dsp_rate = dsp_rate;
    // buffers
    delete[] (rxa->inbuff);
    rxa->inbuff = new float[1 * rxa->dsp_insize  * 2]; // (float *)malloc0(1 * rxa->dsp_insize  * sizeof(complex));
    delete[] (rxa->outbuff);
    rxa->outbuff = new float[1 * rxa->dsp_outsize * 2]; // (float *)malloc0(1 * rxa->dsp_outsize * sizeof(complex));
    // anb
    rxa->anb->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->anb->setSize(rxa->dsp_insize);
    // nob
    rxa->nob->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->nob->setSize(rxa->dsp_insize);
    // shift
    rxa->shift->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->shift->setSize(rxa->dsp_insize);
    // input resampler
    rxa->rsmpin->setBuffers(rxa->inbuff, rxa->midbuff);
    rxa->rsmpin->setSize(rxa->dsp_insize);
    rxa->rsmpin->setOutRate(rxa->dsp_rate);
    // dsp_rate blocks
    rxa->adcmeter->setSamplerate(rxa->dsp_rate);
    rxa->nbp0->setSamplerate(rxa->dsp_rate);
    rxa->bpsnba->setSamplerate(rxa->dsp_rate);
    rxa->smeter->setSamplerate(rxa->dsp_rate);
    rxa->sender->setSamplerate(rxa->dsp_rate);
    rxa->amsq->setSamplerate(rxa->dsp_rate);
    rxa->amd->setSamplerate(rxa->dsp_rate);
    rxa->fmd->setSamplerate(rxa->dsp_rate);
    rxa->fmsq->setBuffers(rxa->midbuff, rxa->midbuff, rxa->fmd->audio.data());
    rxa->fmsq->setSamplerate(rxa->dsp_rate);
    // rxa->snba->setSamplerate(rxa->dsp_rate); SMBA removed
    rxa->eqp->setSamplerate(rxa->dsp_rate);
    rxa->anf->setSamplerate(rxa->dsp_rate);
    rxa->anr->setSamplerate(rxa->dsp_rate);
    rxa->emnr->setSamplerate(rxa->dsp_rate);
    rxa->bp1->setSamplerate(rxa->dsp_rate);
    rxa->agc->setSamplerate(rxa->dsp_rate);
    rxa->agcmeter->setSamplerate(rxa->dsp_rate);
    rxa->sip1->setSamplerate(rxa->dsp_rate);
    rxa->cbl->setSamplerate(rxa->dsp_rate);
    rxa->speak->setSamplerate(rxa->dsp_rate);
    rxa->mpeak->setSamplerate(rxa->dsp_rate);
    SSQL::setSamplerate_ssql (rxa->ssql, rxa->dsp_rate);
    PANEL::setSamplerate_panel (rxa->panel, rxa->dsp_rate);
    // output resampler
    rxa->rsmpout->setBuffers(rxa->midbuff, rxa->outbuff);
    rxa->rsmpout->setInRate(rxa->dsp_rate);
    ResCheck (*rxa);
}

void RXA::setDSPBuffsize (RXA *rxa, int dsp_size)
{
    if (rxa->in_rate  >= rxa->dsp_rate)
        rxa->dsp_insize  = dsp_size * (rxa->in_rate  / rxa->dsp_rate);
    else
        rxa->dsp_insize  = dsp_size / (rxa->dsp_rate /  rxa->in_rate);

    if (rxa->out_rate >= rxa->dsp_rate)
        rxa->dsp_outsize = dsp_size * (rxa->out_rate / rxa->dsp_rate);
    else
        rxa->dsp_outsize = dsp_size / (rxa->dsp_rate / rxa->out_rate);

    rxa->dsp_size = dsp_size;
    // buffers
    delete[](rxa->inbuff);
    rxa->inbuff = new float[1 * rxa->dsp_insize  * 2]; // (float *)malloc0(1 * rxa->dsp_insize  * sizeof(complex));
    delete[] (rxa->midbuff);
    rxa->midbuff = new float[2 * rxa->dsp_size * 2]; // (float *)malloc0(2 * rxa->dsp_size * sizeof(complex));
    delete[] (rxa->outbuff);
    rxa->outbuff = new float[1 * rxa->dsp_outsize * 2]; // (float *)malloc0(1 * rxa->dsp_outsize * sizeof(complex));
    // anb
    rxa->anb->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->anb->setSize(rxa->dsp_insize);
    // nob
    rxa->nob->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->nob->setSize(rxa->dsp_insize);
    // shift
    rxa->shift->setBuffers(rxa->inbuff, rxa->inbuff);
    rxa->shift->setSize(rxa->dsp_insize);
    // input resampler
    rxa->rsmpin->setBuffers(rxa->inbuff, rxa->midbuff);
    rxa->rsmpin->setSize(rxa->dsp_insize);
    // dsp_size blocks
    rxa->adcmeter->setBuffers(rxa->midbuff);
    rxa->adcmeter->setSize(rxa->dsp_size);
    rxa->nbp0->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->nbp0->setSize(rxa->dsp_size);
    rxa->bpsnba->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->bpsnba->setSize(rxa->dsp_size);
    rxa->smeter->setBuffers(rxa->midbuff);
    rxa->smeter->setSize(rxa->dsp_size);
    rxa->sender->setBuffers(rxa->midbuff);
    rxa->sender->setSize(rxa->dsp_size);
    rxa->amsq->setBuffers(rxa->midbuff, rxa->midbuff, rxa->midbuff);
    rxa->amsq->setSize(rxa->dsp_size);
    rxa->amd->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->amd->setSize(rxa->dsp_size);
    rxa->fmd->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->fmd->setSize(rxa->dsp_size);
    rxa->fmsq->setBuffers(rxa->midbuff, rxa->midbuff, rxa->fmd->audio.data());
    rxa->fmsq->setSize(rxa->dsp_size);
    rxa->snba->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->snba->setSize(rxa->dsp_size);
    rxa->eqp->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->eqp->setSize(rxa->dsp_size);
    rxa->anf->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->anf->setSize(rxa->dsp_size);
    rxa->anr->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->anr->setSize(rxa->dsp_size);
    rxa->emnr->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->emnr->setSize(rxa->dsp_size);
    rxa->bp1->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->bp1->setSize(rxa->dsp_size);
    rxa->agc->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->agc->setSize(rxa->dsp_size);
    rxa->agcmeter->setBuffers(rxa->midbuff);
    rxa->agcmeter->setSize(rxa->dsp_size);
    rxa->sip1->setBuffers(rxa->midbuff);
    rxa->sip1->setSize(rxa->dsp_size);
    rxa->cbl->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->cbl->setSize(rxa->dsp_size);
    rxa->speak->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->speak->setSize(rxa->dsp_size);
    rxa->mpeak->setBuffers(rxa->midbuff, rxa->midbuff);
    rxa->mpeak->setSize(rxa->dsp_size);
    SSQL::setBuffers_ssql (rxa->ssql, rxa->midbuff, rxa->midbuff);
    SSQL::setSize_ssql (rxa->ssql, rxa->dsp_size);
    PANEL::setBuffers_panel (rxa->panel, rxa->midbuff, rxa->midbuff);
    PANEL::setSize_panel (rxa->panel, rxa->dsp_size);
    // output resampler
    rxa->rsmpout->setBuffers(rxa->midbuff, rxa->outbuff);
    rxa->rsmpout->setSize(rxa->dsp_size);
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

void RXA::SetMode (RXA& rxa, int mode)
{
    if (rxa.mode != mode)
    {
        int amd_run = (mode == RXA_AM) || (mode == RXA_SAM);
        bpsnbaCheck (rxa, mode, rxa.ndb->master_run);
        bp1Check (
            rxa,
            amd_run,
            rxa.snba->run,
            rxa.emnr->run,
            rxa.anf->run,
            rxa.anr->run
        );
        rxa.mode = mode;
        rxa.amd->run  = 0;
        rxa.fmd->run  = 0;

        switch (mode)
        {
        case RXA_AM:
            rxa.amd->run  = 1;
            rxa.amd->mode = 0;
            break;
        case RXA_SAM:
            rxa.amd->run  = 1;
            rxa.amd->mode = 1;
            break;
        case RXA_DSB:
            break;
        case RXA_FM:
            rxa.fmd->run  = 1;
            break;
        default:

            break;
        }

        bp1Set (rxa);
        bpsnbaSet (rxa);                         // update variables
    }
}

void RXA::ResCheck (RXA& rxa)
{
    // turn OFF/ON resamplers depending upon whether they're needed
    RESAMPLE *a = rxa.rsmpin;
    if (rxa.in_rate  != rxa.dsp_rate)
        a->run = 1;
    else
        a->run = 0;
    a = rxa.rsmpout;
    if (rxa.dsp_rate != rxa.out_rate)
        a->run = 1;
    else
        a->run = 0;
}

void RXA::bp1Check (
    RXA& rxa,
    int amd_run,
    int snba_run,
    int emnr_run,
    int anf_run,
    int anr_run
)
{
    BANDPASS *a = rxa.bp1;
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

void RXA::bp1Set (RXA& rxa)
{
    BANDPASS *a = rxa.bp1;
    int old = a->run;
    if ((rxa.amd->run  == 1) ||
        (rxa.snba->run == 1) ||
        (rxa.emnr->run == 1) ||
        (rxa.anf->run  == 1) ||
        (rxa.anr->run  == 1)
    )
        a->run = 1;
    else
        a->run = 0;
    if (!old && a->run)
        a->flush();
    FIRCORE::setUpdate_fircore (a->fircore);
}

void RXA::bpsnbaCheck (RXA& rxa, int mode, int notch_run)
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = rxa.bpsnba;
    float f_low = 0.0, f_high = 0.0;
    int run_notches = 0;
    switch (mode)
    {
        case RXA_LSB:
        case RXA_CWL:
        case RXA_DIGL:
            f_low  = -a->abs_high_freq;
            f_high = -a->abs_low_freq;
            run_notches = notch_run;
            break;
        case RXA_USB:
        case RXA_CWU:
        case RXA_DIGU:
            f_low  = +a->abs_low_freq;
            f_high = +a->abs_high_freq;
            run_notches = notch_run;
            break;
        case RXA_AM:
        case RXA_SAM:
        case RXA_DSB:
            f_low  = +a->abs_low_freq;
            f_high = +a->abs_high_freq;
            run_notches = 0;
            break;
        case RXA_FM:
            f_low  = +a->abs_low_freq;
            f_high = +a->abs_high_freq;
            run_notches = 0;
            break;
        case RXA_DRM:
        case RXA_SPEC:

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
        rxa.bpsnba->recalc_bpsnba_filter(0);
    }
}

void RXA::bpsnbaSet (RXA& rxa)
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = rxa.bpsnba;
    switch (rxa.mode)
    {
        case RXA_LSB:
        case RXA_CWL:
        case RXA_DIGL:
            a->run = rxa.snba->run;
            a->position = 0;
            break;
        case RXA_USB:
        case RXA_CWU:
        case RXA_DIGU:
            a->run = rxa.snba->run;
            a->position = 0;
            break;
        case RXA_AM:
        case RXA_SAM:
        case RXA_DSB:
            a->run = rxa.snba->run;
            a->position = 1;
            break;
        case RXA_FM:
            a->run = rxa.snba->run;
            a->position = 1;
            break;
        case RXA_DRM:
        case RXA_SPEC:
            a->run = 0;
            break;
    }
    FIRCORE::setUpdate_fircore (a->bpsnba->fircore);
}

void RXA::UpdateNBPFiltersLightWeight (RXA& rxa)
{   // called when setting tune freq or shift freq
    rxa.nbp0->calc_lightweight();
    rxa.bpsnba->bpsnba->calc_lightweight();
}

void RXA::UpdateNBPFilters(RXA& rxa)
{
    NBP *a = rxa.nbp0;
    BPSNBA *b = rxa.bpsnba;
    if (a->fnfrun)
    {
        a->calc_impulse();
        FIRCORE::setImpulse_fircore (a->fircore, a->impulse, 1);
        delete[] (a->impulse);
    }
    if (b->bpsnba->fnfrun)
    {
        b->recalc_bpsnba_filter(1);
    }
}

int RXA::NBPAddNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active)
{
    NOTCHDB *b = rxa.ndb;
    int rval = b->addNotch(notch, fcenter, fwidth, active);

    if (rval == 0) {
        RXA::UpdateNBPFilters (rxa);
    }

    return rval;
}

int RXA::NBPGetNotch (RXA& rxa, int notch, double* fcenter, double* fwidth, int* active)
{
    NOTCHDB *a = rxa.ndb;
    int rval = a->getNotch(notch, fcenter, fwidth, active);
    return rval;
}

int RXA::NBPDeleteNotch (RXA& rxa, int notch)
{
    NOTCHDB *a = rxa.ndb;
    int rval = a->deleteNotch(notch);

    if (rval == 0) {
        RXA::UpdateNBPFilters (rxa);
    }

    return rval;
}

int RXA::NBPEditNotch (RXA& rxa, int notch, double fcenter, double fwidth, int active)
{
    NOTCHDB *a = rxa.ndb;
    int rval = a->editNotch(notch, fcenter, fwidth, active);

    if (rval == 0) {
        RXA::UpdateNBPFilters (rxa);
    }

    return rval;
}

void RXA::NBPGetNumNotches (RXA& rxa, int* nnotches)
{
    NOTCHDB *a = rxa.ndb;
    a->getNumNotches(nnotches);
}

void RXA::NBPSetTuneFrequency (RXA& rxa, double tunefreq)
{
    NOTCHDB *a;
    a = rxa.ndb;

    if (tunefreq != a->tunefreq)
    {
        a->tunefreq = tunefreq;
        RXA::UpdateNBPFiltersLightWeight (rxa);
    }
}

void RXA::NBPSetShiftFrequency (RXA& rxa, double shift)
{
    NOTCHDB *a;
    a = rxa.ndb;
    if (shift != a->shift)
    {
        a->shift = shift;
        RXA::UpdateNBPFiltersLightWeight (rxa);
    }
}

void RXA::NBPSetNotchesRun (RXA& rxa, int run)
{
    NOTCHDB *a = rxa.ndb;
    NBP *b = rxa.nbp0;

    if ( run != a->master_run)
    {
        a->master_run = run;                            // update variables
        b->fnfrun = a->master_run;
        RXA::bpsnbaCheck (rxa, rxa.mode, run);
        b->calc_impulse();                           // recalc nbp impulse response
        FIRCORE::setImpulse_fircore (b->fircore, b->impulse, 0);       // calculate new filter masks
        delete[] (b->impulse);
        RXA::bpsnbaSet (rxa);
        FIRCORE::setUpdate_fircore (b->fircore);                       // apply new filter masks
    }
}

void RXA::NBPSetWindow (RXA& rxa, int wintype)
{
    NBP *a;
    BPSNBA *b;
    a = rxa.nbp0;
    b = rxa.bpsnba;

    if ((a->wintype != wintype))
    {
        a->wintype = wintype;
        a->calc_impulse();
        FIRCORE::setImpulse_fircore (a->fircore, a->impulse, 1);
        delete[] (a->impulse);
    }

    if ((b->wintype != wintype))
    {
        b->wintype = wintype;
        b->recalc_bpsnba_filter(1);
    }
}

void RXA::NBPSetAutoIncrease (RXA& rxa, int autoincr)
{
    NBP *a;
    BPSNBA *b;
    a = rxa.nbp0;
    b = rxa.bpsnba;

    if ((a->autoincr != autoincr))
    {
        a->autoincr = autoincr;
        a->calc_impulse();
        FIRCORE::setImpulse_fircore (a->fircore, a->impulse, 1);
        delete[] (a->impulse);
    }

    if ((b->autoincr != autoincr))
    {
        b->autoincr = autoincr;
        b->recalc_bpsnba_filter(1);
    }
}

void RXA::SetAMDRun(RXA& rxa, int run)
{
    if (rxa.amd->run != run)
    {
        RXA::bp1Check (
            rxa,
            run,
            rxa.snba->run,
            rxa.emnr->run,
            rxa.anf->run,
            rxa.anr->run
        );

        rxa.amd->run = run;
        RXA::bp1Set (rxa);
    }
}

void RXA::SetSNBARun (RXA& rxa, int run)
{
    SNBA *a = rxa.snba;

    if (a->run != run)
    {
        RXA::bpsnbaCheck (rxa, rxa.mode, rxa.ndb->master_run);
        RXA::bp1Check (
            rxa,
            rxa.amd->run,
            run,
            rxa.emnr->run,
            rxa.anf->run,
            rxa.anr->run
        );
        a->run = run;
        RXA::bp1Set (rxa);
        RXA::bpsnbaSet (rxa);
    }
}

void RXA::SetANFRun (RXA& rxa, int run)
{
    ANF *a = rxa.anf;

    if (a->run != run)
    {
        RXA::bp1Check (
            rxa,
            rxa.amd->run,
            rxa.snba->run,
            rxa.emnr->run,
            run,
            rxa.anr->run
        );
        a->run = run;
        RXA::bp1Set (rxa);
        a->flush();
    }
}

void RXA::SetANFPosition (RXA& rxa, int position)
{
    rxa.anf->position = position;
    rxa.bp1->position = position;
    rxa.anf->flush();
}

void RXA::SetANRRun (RXA& rxa, int run)
{
    ANR *a = rxa.anr;

    if (a->run != run)
    {
        RXA::bp1Check (
            rxa,
            rxa.amd->run,
            rxa.snba->run,
            rxa.emnr->run,
            rxa.anf->run,
            run
        );
        a->run = run;
        RXA::bp1Set (rxa);
        a->flush();
    }
}

void RXA::SetANRPosition (RXA& rxa, int position)
{
    rxa.anr->position = position;
    rxa.bp1->position = position;
    rxa.anr->flush();
}

void RXA::SetEMNRRun (RXA& rxa, int run)
{
    EMNR *a = rxa.emnr;

    if (a->run != run)
    {
        RXA::bp1Check (
            rxa,
            rxa.amd->run,
            rxa.snba->run,
            run,
            rxa.anf->run,
            rxa.anr->run
        );
        a->run = run;
        RXA::bp1Set (rxa);
    }
}

void RXA::SetEMNRPosition (RXA& rxa, int position)
{
    rxa.emnr->position = position;
    rxa.bp1->position  = position;
}

void RXA::GetAGCThresh(RXA& rxa, double *thresh, double size, double rate)
//for line on bandscope.
{
    double noise_offset;
    noise_offset = 10.0 * log10((rxa.nbp0->fhigh - rxa.nbp0->flow) * size / rate);
    *thresh = 20.0 * log10( rxa.agc->min_volts ) - noise_offset;
}

void RXA::SetAGCThresh(RXA& rxa, double thresh, double size, double rate)
//for line on bandscope
{
    double noise_offset;
    noise_offset = 10.0 * log10((rxa.nbp0->fhigh - rxa.nbp0->flow) * size / rate);
    rxa.agc->max_gain = rxa.agc->out_target / (rxa.agc->var_gain * pow (10.0, (thresh + noise_offset) / 20.0));
    rxa.agc->loadWcpAGC();
}

/********************************************************************************************************
*                                                                                                       *
*                                               Collectives                                             *
*                                                                                                       *
********************************************************************************************************/

void RXA::SetPassband (RXA& rxa, float f_low, float f_high)
{
    rxa.bp1->setBandpassFreqs         (f_low, f_high); // After spectral noise reduction ( AM || ANF || ANR || EMNR)
    rxa.snba->setOutputBandwidth      (f_low, f_high); // Spectral noise blanker (SNB)
    rxa.nbp0->SetFreqs                (f_low, f_high); // Notched bandpass
}

void RXA::SetNC (RXA& rxa, int nc)
{
    int oldstate = rxa.state;
    rxa.nbp0->SetNC              (nc);
    rxa.bpsnba->SetNC            (nc);
    rxa.bp1->SetBandpassNC       (nc);
    rxa.eqp->setNC               (nc);
    rxa.fmsq->setNC              (nc);
    rxa.fmd->setNCde             (nc);
    rxa.fmd->setNCaud            (nc);
    rxa.state = oldstate;
}

void RXA::SetMP (RXA& rxa, int mp)
{
    rxa.nbp0->SetMP              (mp);
    rxa.bpsnba->SetMP            (mp);
    rxa.bp1->SetBandpassMP       (mp);
    rxa.eqp->setMP               (mp);
    rxa.fmsq->setMP              (mp);
    rxa.fmd->setMPde             (mp);
    rxa.fmd->setMPaud            (mp);
}

} // namespace WDSP
