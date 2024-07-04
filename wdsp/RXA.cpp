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
#include "gen.hpp"
#include "bandpass.hpp"
#include "bps.hpp"
#include "nbp.hpp"
#include "snba.hpp"
#include "bpsnba.hpp"
#include "sender.hpp"
#include "amsq.hpp"
#include "fmd.hpp"
#include "fmsq.hpp"
#include "eq.hpp"
#include "anf.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "patchpanel.hpp"
#include "siphon.hpp"
#include "cblock.hpp"
#include "ssql.hpp"
#include "iir.hpp"
#include "firmin.hpp"
#include "wcpAGC.hpp"
#include "anb.hpp"
#include "nob.hpp"

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
    memset(rxa->meter, 0, sizeof(float)*RXA_METERTYPE_LAST);

    // Noise blanker (ANB or "NB")
    rxa->anb.p = ANB::create_anb(
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
    rxa->nob.p = NOB::create_nob(
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
    rxa->shift.p = SHIFT::create_shift (
        1,                                      // run
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->inbuff,                            // pointer to output buffer
        rxa->in_rate,                           // samplerate
        0.0);                                   // amount to shift (Hz)

    // Input resampler - resample to dsp rate for main processing
    rxa->rsmpin.p = RESAMPLE::create_resample (
        0,                                      // run - will be turned ON below if needed
        rxa->dsp_insize,                        // input buffer size
        rxa->inbuff,                            // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        rxa->in_rate,                           // input samplerate
        rxa->dsp_rate,                          // output samplerate
        0.0,                                    // select cutoff automatically
        0,                                      // select ncoef automatically
        1.0);                                   // gain

    // Signal generator
    rxa->gen0.p = GEN::create_gen (
        0,                                      // run
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // input buffer
        rxa->midbuff,                           // output buffer
        rxa->dsp_rate,                          // sample rate
        2);                                     // mode

    // Input meter - ADC
    rxa->adcmeter.p = METER::create_meter (
        0,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        rxa->pmtupdate,                         // locks for meter access
        RXA_ADC_AV,                             // index for average value
        RXA_ADC_PK,                             // index for peak value
        -1,                                     // index for gain value
        0);                                     // pointer for gain computation

    // Notched bandpass section

    // notch database
    rxa->ndb.p = NOTCHDB::create_notchdb (
        0,                                      // master run for all nbp's
        1024);                                  // max number of notches

    // notched bandpass
    rxa->nbp0.p = NBP::create_nbp (
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
        rxa->ndb.p);                            // addr of database pointer

    // bandpass for snba
    rxa->bpsnba.p = BPSNBA::create_bpsnba (
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
        rxa->ndb.p);                            // addr of database pointer

    // Post filter display send - send spectrum display (after S-meter in the block diagram)
    rxa->sender.p = SENDER::create_sender (
        0,                                      // run
        0,                                      // flag
        0,                                      // mode
        rxa->dsp_size,                          // size
        rxa->midbuff                            // pointer to input buffer
    );

    // End notched bandpass section

    // S-meter
    rxa->smeter.p = METER::create_meter (
        1,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        rxa->pmtupdate,                         // locks for meter access
        RXA_S_AV,                               // index for average value
        RXA_S_PK,                               // index for peak value
        -1,                                     // index for gain value
        0);                                     // pointer for gain computation

    // AM squelch capture (for other modes than FM)
    rxa->amsq.p = AMSQ::create_amsq (
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
    rxa->amd.p = AMD::create_amd (
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
    rxa->fmd.p = FMD::create_fmd (
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
    rxa->fmsq.p = FMSQ::create_fmsq (
        0,                                      // run
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input signal buffer
        rxa->midbuff,                           // pointer to output signal buffer
        rxa->fmd.p->audio,                      // pointer to trigger buffer
        rxa->dsp_rate,                          // sample rate
        5000.0,                                 // cutoff freq for noise filter (Hz)
        &rxa->fmd.p->pllpole,                   // pointer to pole frequency of the fmd pll (Hz)
        0.100,                                  // delay time after channel flush
        0.001,                                  // tau for noise averaging
        0.100,                                  // tau for long noise averaging
        0.050,                                  // signal up transition time
        0.010,                                  // signal down transition time
        0.750,                                  // noise level to initiate tail
        0.562,                                  // noise level to initiate unmute
        0.000,                                  // minimum tail time
        1.200,                                  // maximum tail time
        std::max(2048, rxa->dsp_size),          // number of coefficients for noise filter
        0);                                     // minimum phase flag

    // Spectral noise blanker (SNB)
    rxa->snba.p = SNBA::create_snba (
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
        rxa->eqp.p = EQP::create_eqp (
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
    rxa->anf.p = ANF::create_anf (
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
    rxa->anr.p = ANR::create_anr (
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
    rxa->emnr.p = EMNR::create_emnr (
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
    rxa->agc.p = WCPAGC::create_wcpagc (
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
    rxa->agcmeter.p = METER::create_meter (
        0,                                      // run
        0,                                      // optional pointer to another 'run'
        rxa->dsp_size,                          // size
        rxa->midbuff,                           // pointer to buffer
        rxa->dsp_rate,                          // samplerate
        0.100,                                  // averaging time constant
        0.100,                                  // peak decay time constant
        rxa->meter,                             // result vector
        rxa->pmtupdate,                         // locks for meter access
        RXA_AGC_AV,                             // index for average value
        RXA_AGC_PK,                             // index for peak value
        RXA_AGC_GAIN,                           // index for gain value
        &rxa->agc.p->gain);                     // pointer for gain computation

    // Bandpass filter - After spectral noise reduction in the block diagram
    rxa->bp1.p = BANDPASS::create_bandpass (
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
    rxa->sip1.p = SIPHON::create_siphon (
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
    rxa->cbl.p = CBL::create_cbl (
        0,                                      // run - needed only if set to ON
        rxa->dsp_size,                          // buffer size
        rxa->midbuff,                           // pointer to input buffer
        rxa->midbuff,                           // pointer to output buffer
        0,                                      // mode
        rxa->dsp_rate,                          // sample rate
        0.02);                                  // tau

    // CW peaking filter
    rxa->speak.p = SPEAK::create_speak (
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
        float def_freq[2] = {2125.0, 2295.0};
        float def_bw[2] = {75.0, 75.0};
        float def_gain[2] = {1.0, 1.0};
        rxa->mpeak.p = MPEAK::create_mpeak (
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
    rxa->ssql.p = SSQL::create_ssql(
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
    rxa->panel.p = PANEL::create_panel (
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
    rxa->rsmpout.p = RESAMPLE::create_resample (
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
    RESAMPLE::destroy_resample (rxa->rsmpout.p);
    PANEL::destroy_panel (rxa->panel.p);
    SSQL::destroy_ssql (rxa->ssql.p);
    MPEAK::destroy_mpeak (rxa->mpeak.p);
    SPEAK::destroy_speak (rxa->speak.p);
    CBL::destroy_cbl (rxa->cbl.p);
    SIPHON::destroy_siphon (rxa->sip1.p);
    BANDPASS::destroy_bandpass (rxa->bp1.p);
    METER::destroy_meter (rxa->agcmeter.p);
    WCPAGC::destroy_wcpagc (rxa->agc.p);
    EMNR::destroy_emnr (rxa->emnr.p);
    ANR::destroy_anr (rxa->anr.p);
    ANF::destroy_anf (rxa->anf.p);
    EQP::destroy_eqp (rxa->eqp.p);
    SNBA::destroy_snba (rxa->snba.p);
    FMSQ::destroy_fmsq (rxa->fmsq.p);
    FMD::destroy_fmd (rxa->fmd.p);
    AMD::destroy_amd (rxa->amd.p);
    AMSQ::destroy_amsq (rxa->amsq.p);
    METER::destroy_meter (rxa->smeter.p);
    SENDER::destroy_sender (rxa->sender.p);
    BPSNBA::destroy_bpsnba (rxa->bpsnba.p);
    NBP::destroy_nbp (rxa->nbp0.p);
    NOTCHDB::destroy_notchdb (rxa->ndb.p);
    METER::destroy_meter (rxa->adcmeter.p);
    GEN::destroy_gen (rxa->gen0.p);
    RESAMPLE::destroy_resample (rxa->rsmpin.p);
    SHIFT::destroy_shift (rxa->shift.p);
    ANB::destroy_anb(rxa->anb.p);
    NOB::destroy_nob(rxa->nob.p);
    delete[] (rxa->midbuff);
    delete[] (rxa->outbuff);
    delete[] (rxa->inbuff);
    delete rxa;
}

void RXA::flush_rxa (RXA *rxa)
{
    memset (rxa->inbuff,  0, 1 * rxa->dsp_insize  * sizeof (wcomplex));
    memset (rxa->outbuff, 0, 1 * rxa->dsp_outsize * sizeof (wcomplex));
    memset (rxa->midbuff, 0, 2 * rxa->dsp_size    * sizeof (wcomplex));
    SHIFT::flush_shift (rxa->shift.p);
    RESAMPLE::flush_resample (rxa->rsmpin.p);
    GEN::flush_gen (rxa->gen0.p);
    METER::flush_meter (rxa->adcmeter.p);
    NBP::flush_nbp (rxa->nbp0.p);
    BPSNBA::flush_bpsnba (rxa->bpsnba.p);
    SENDER::flush_sender (rxa->sender.p);
    METER::flush_meter (rxa->smeter.p);
    AMSQ::flush_amsq (rxa->amsq.p);
    AMD::flush_amd (rxa->amd.p);
    FMD::flush_fmd (rxa->fmd.p);
    FMSQ::flush_fmsq (rxa->fmsq.p);
    SNBA::flush_snba (rxa->snba.p);
    EQP::flush_eqp (rxa->eqp.p);
    ANF::flush_anf (rxa->anf.p);
    ANR::flush_anr (rxa->anr.p);
    EMNR::flush_emnr (rxa->emnr.p);
    WCPAGC::flush_wcpagc (rxa->agc.p);
    METER::flush_meter (rxa->agcmeter.p);
    BANDPASS::flush_bandpass (rxa->bp1.p);
    SIPHON::flush_siphon (rxa->sip1.p);
    CBL::flush_cbl (rxa->cbl.p);
    SPEAK::flush_speak (rxa->speak.p);
    MPEAK::flush_mpeak (rxa->mpeak.p);
    SSQL::flush_ssql (rxa->ssql.p);
    PANEL::flush_panel (rxa->panel.p);
    RESAMPLE::flush_resample (rxa->rsmpout.p);
    ANB::flush_anb (rxa->anb.p);
    NOB::flush_nob(rxa->nob.p);
}

void RXA::xrxa (RXA *rxa)
{
    ANB::xanb (rxa->anb.p);
    NOB::xnob (rxa->nob.p);
    SHIFT::xshift (rxa->shift.p);
    RESAMPLE::xresample (rxa->rsmpin.p);
    GEN::xgen (rxa->gen0.p);
    METER::xmeter (rxa->adcmeter.p);
    BPSNBA::xbpsnbain (rxa->bpsnba.p, 0);
    NBP::xnbp (rxa->nbp0.p, 0);
    METER::xmeter (rxa->smeter.p);
    SENDER::xsender (rxa->sender.p);
    AMSQ::xamsqcap (rxa->amsq.p);
    BPSNBA::xbpsnbaout (rxa->bpsnba.p, 0);
    AMD::xamd (rxa->amd.p);
    FMD::xfmd (rxa->fmd.p);
    FMSQ::xfmsq (rxa->fmsq.p);
    BPSNBA::xbpsnbain (rxa->bpsnba.p, 1);
    BPSNBA::xbpsnbaout (rxa->bpsnba.p, 1);
    SNBA::xsnba (rxa->snba.p);
    EQP::xeqp (rxa->eqp.p);
    ANF::xanf (rxa->anf.p, 0);
    ANR::xanr (rxa->anr.p, 0);
    EMNR::xemnr (rxa->emnr.p, 0);
    BANDPASS::xbandpass (rxa->bp1.p, 0);
    WCPAGC::xwcpagc (rxa->agc.p);
    ANF::xanf (rxa->anf.p, 1);
    ANR::xanr (rxa->anr.p, 1);
    EMNR::xemnr (rxa->emnr.p, 1);
    BANDPASS::xbandpass (rxa->bp1.p, 1);
    METER::xmeter (rxa->agcmeter.p);
    SIPHON::xsiphon (rxa->sip1.p, 0);
    CBL::xcbl (rxa->cbl.p);
    SPEAK::xspeak (rxa->speak.p);
    MPEAK::xmpeak (rxa->mpeak.p);
    SSQL::xssql (rxa->ssql.p);
    PANEL::xpanel (rxa->panel.p);
    AMSQ::xamsq (rxa->amsq.p);
    RESAMPLE::xresample (rxa->rsmpout.p);
}

void RXA::setInputSamplerate (RXA *rxa, int in_rate)
{
    rxa->csDSP.lock();
    if (in_rate  >= rxa->dsp_rate)
        rxa->dsp_insize  = rxa->dsp_size * (in_rate  / rxa->dsp_rate);
    else
        rxa->dsp_insize  = rxa->dsp_size / (rxa->dsp_rate /  in_rate);
    rxa->in_rate = in_rate;
    // buffers
    delete[] (rxa->inbuff);
    rxa->inbuff = new float[1 * rxa->dsp_insize  * 2]; // (float *)malloc0(1 * ch.dsp_insize  * sizeof(complex));
    // anb
    ANB::setBuffers_anb(rxa->anb.p, rxa->inbuff, rxa->inbuff);
    ANB::setSize_anb(rxa->anb.p, rxa->dsp_insize);
    ANB::setSamplerate_anb(rxa->anb.p, rxa->in_rate);
    // nob
    NOB::setBuffers_nob(rxa->nob.p, rxa->inbuff, rxa->inbuff);
    NOB::setSize_nob(rxa->nob.p, rxa->dsp_insize);
    NOB::setSamplerate_nob(rxa->nob.p, rxa->in_rate);
    // shift
    SHIFT::setBuffers_shift (rxa->shift.p, rxa->inbuff, rxa->inbuff);
    SHIFT::setSize_shift (rxa->shift.p, rxa->dsp_insize);
    SHIFT::setSamplerate_shift (rxa->shift.p, rxa->in_rate);
    // input resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpin.p, rxa->inbuff, rxa->midbuff);
    RESAMPLE::setSize_resample (rxa->rsmpin.p, rxa->dsp_insize);
    RESAMPLE::setInRate_resample (rxa->rsmpin.p, rxa->in_rate);
    ResCheck (*rxa);
    rxa->csDSP.unlock();
}

void RXA::setOutputSamplerate (RXA *rxa, int out_rate)
{
    rxa->csDSP.lock();
    if (out_rate >= rxa->dsp_rate)
        rxa->dsp_outsize = rxa->dsp_size * (out_rate / rxa->dsp_rate);
    else
        rxa->dsp_outsize = rxa->dsp_size / (rxa->dsp_rate / out_rate);
    rxa->out_rate = out_rate;
    // buffers
    delete[] (rxa->outbuff);
    rxa->outbuff = new float[1 * rxa->dsp_outsize * 2]; // (float *)malloc0(1 * ch.dsp_outsize * sizeof(complex));
    // output resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpout.p, rxa->midbuff, rxa->outbuff);
    RESAMPLE::setOutRate_resample (rxa->rsmpout.p, rxa->out_rate);
    ResCheck (*rxa);
    rxa->csDSP.unlock();
}

void RXA::setDSPSamplerate (RXA *rxa, int dsp_rate)
{
    rxa->csDSP.lock();
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
    ANB::setBuffers_anb (rxa->anb.p, rxa->inbuff, rxa->inbuff);
    ANB::setSize_anb(rxa->anb.p, rxa->dsp_insize);
    // nob
    NOB::setBuffers_nob(rxa->nob.p, rxa->inbuff, rxa->inbuff);
    NOB::setSize_nob(rxa->nob.p, rxa->dsp_insize);
    // shift
    SHIFT::setBuffers_shift (rxa->shift.p, rxa->inbuff, rxa->inbuff);
    SHIFT::setSize_shift (rxa->shift.p, rxa->dsp_insize);
    // input resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpin.p, rxa->inbuff, rxa->midbuff);
    RESAMPLE::setSize_resample (rxa->rsmpin.p, rxa->dsp_insize);
    RESAMPLE::setOutRate_resample (rxa->rsmpin.p, rxa->dsp_rate);
    // dsp_rate blocks
    GEN::setSamplerate_gen (rxa->gen0.p, rxa->dsp_rate);
    METER::setSamplerate_meter (rxa->adcmeter.p, rxa->dsp_rate);
    NBP::setSamplerate_nbp (rxa->nbp0.p, rxa->dsp_rate);
    BPSNBA::setSamplerate_bpsnba (rxa->bpsnba.p, rxa->dsp_rate);
    METER::setSamplerate_meter (rxa->smeter.p, rxa->dsp_rate);
    SENDER::setSamplerate_sender (rxa->sender.p, rxa->dsp_rate);
    AMSQ::setSamplerate_amsq (rxa->amsq.p, rxa->dsp_rate);
    AMD::setSamplerate_amd (rxa->amd.p, rxa->dsp_rate);
    FMD::setSamplerate_fmd (rxa->fmd.p, rxa->dsp_rate);
    FMSQ::setBuffers_fmsq (rxa->fmsq.p, rxa->midbuff, rxa->midbuff, rxa->fmd.p->audio);
    FMSQ::setSamplerate_fmsq (rxa->fmsq.p, rxa->dsp_rate);
    SNBA::setSamplerate_snba (rxa->snba.p, rxa->dsp_rate);
    EQP::setSamplerate_eqp (rxa->eqp.p, rxa->dsp_rate);
    ANF::setSamplerate_anf (rxa->anf.p, rxa->dsp_rate);
    ANR::setSamplerate_anr (rxa->anr.p, rxa->dsp_rate);
    EMNR::setSamplerate_emnr (rxa->emnr.p, rxa->dsp_rate);
    BANDPASS::setSamplerate_bandpass (rxa->bp1.p, rxa->dsp_rate);
    WCPAGC::setSamplerate_wcpagc (rxa->agc.p, rxa->dsp_rate);
    METER::setSamplerate_meter (rxa->agcmeter.p, rxa->dsp_rate);
    SIPHON::setSamplerate_siphon (rxa->sip1.p, rxa->dsp_rate);
    CBL::setSamplerate_cbl (rxa->cbl.p, rxa->dsp_rate);
    SPEAK::setSamplerate_speak (rxa->speak.p, rxa->dsp_rate);
    MPEAK::setSamplerate_mpeak (rxa->mpeak.p, rxa->dsp_rate);
    SSQL::setSamplerate_ssql (rxa->ssql.p, rxa->dsp_rate);
    PANEL::setSamplerate_panel (rxa->panel.p, rxa->dsp_rate);
    // output resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpout.p, rxa->midbuff, rxa->outbuff);
    RESAMPLE::setInRate_resample (rxa->rsmpout.p, rxa->dsp_rate);
    ResCheck (*rxa);
    rxa->csDSP.unlock();
}

void RXA::setDSPBuffsize (RXA *rxa, int dsp_size)
{
    rxa->csDSP.lock();
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
    ANB::setBuffers_anb (rxa->anb.p, rxa->inbuff, rxa->inbuff);
    ANB::setSize_anb (rxa->anb.p, rxa->dsp_insize);
    // nob
    NOB::setBuffers_nob(rxa->nob.p, rxa->inbuff, rxa->inbuff);
    NOB::setSize_nob(rxa->nob.p, rxa->dsp_insize);
    // shift
    SHIFT::setBuffers_shift (rxa->shift.p, rxa->inbuff, rxa->inbuff);
    SHIFT::setSize_shift (rxa->shift.p, rxa->dsp_insize);
    // input resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpin.p, rxa->inbuff, rxa->midbuff);
    RESAMPLE::setSize_resample (rxa->rsmpin.p, rxa->dsp_insize);
    // dsp_size blocks
    GEN::setBuffers_gen (rxa->gen0.p, rxa->midbuff, rxa->midbuff);
    GEN::setSize_gen (rxa->gen0.p, rxa->dsp_size);
    METER::setBuffers_meter (rxa->adcmeter.p, rxa->midbuff);
    METER::setSize_meter (rxa->adcmeter.p, rxa->dsp_size);
    NBP::setBuffers_nbp (rxa->nbp0.p, rxa->midbuff, rxa->midbuff);
    NBP::setSize_nbp (rxa->nbp0.p, rxa->dsp_size);
    BPSNBA::setBuffers_bpsnba (rxa->bpsnba.p, rxa->midbuff, rxa->midbuff);
    BPSNBA::setSize_bpsnba (rxa->bpsnba.p, rxa->dsp_size);
    METER::setBuffers_meter (rxa->smeter.p, rxa->midbuff);
    METER::setSize_meter (rxa->smeter.p, rxa->dsp_size);
    SENDER::setBuffers_sender (rxa->sender.p, rxa->midbuff);
    SENDER::setSize_sender (rxa->sender.p, rxa->dsp_size);
    AMSQ::setBuffers_amsq (rxa->amsq.p, rxa->midbuff, rxa->midbuff, rxa->midbuff);
    AMSQ::setSize_amsq (rxa->amsq.p, rxa->dsp_size);
    AMD::setBuffers_amd (rxa->amd.p, rxa->midbuff, rxa->midbuff);
    AMD::setSize_amd (rxa->amd.p, rxa->dsp_size);
    FMD::setBuffers_fmd (rxa->fmd.p, rxa->midbuff, rxa->midbuff);
    FMD::setSize_fmd (rxa->fmd.p, rxa->dsp_size);
    FMSQ::setBuffers_fmsq (rxa->fmsq.p, rxa->midbuff, rxa->midbuff, rxa->fmd.p->audio);
    FMSQ::setSize_fmsq (rxa->fmsq.p, rxa->dsp_size);
    SNBA::setBuffers_snba (rxa->snba.p, rxa->midbuff, rxa->midbuff);
    SNBA::setSize_snba (rxa->snba.p, rxa->dsp_size);
    EQP::setBuffers_eqp (rxa->eqp.p, rxa->midbuff, rxa->midbuff);
    EQP::setSize_eqp (rxa->eqp.p, rxa->dsp_size);
    ANF::setBuffers_anf (rxa->anf.p, rxa->midbuff, rxa->midbuff);
    ANF::setSize_anf (rxa->anf.p, rxa->dsp_size);
    ANR::setBuffers_anr (rxa->anr.p, rxa->midbuff, rxa->midbuff);
    ANR::setSize_anr (rxa->anr.p, rxa->dsp_size);
    EMNR::setBuffers_emnr (rxa->emnr.p, rxa->midbuff, rxa->midbuff);
    EMNR::setSize_emnr (rxa->emnr.p, rxa->dsp_size);
    BANDPASS::setBuffers_bandpass (rxa->bp1.p, rxa->midbuff, rxa->midbuff);
    BANDPASS::setSize_bandpass (rxa->bp1.p, rxa->dsp_size);
    WCPAGC::setBuffers_wcpagc (rxa->agc.p, rxa->midbuff, rxa->midbuff);
    WCPAGC::setSize_wcpagc (rxa->agc.p, rxa->dsp_size);
    METER::setBuffers_meter (rxa->agcmeter.p, rxa->midbuff);
    METER::setSize_meter (rxa->agcmeter.p, rxa->dsp_size);
    SIPHON::setBuffers_siphon (rxa->sip1.p, rxa->midbuff);
    SIPHON::setSize_siphon (rxa->sip1.p, rxa->dsp_size);
    CBL::setBuffers_cbl (rxa->cbl.p, rxa->midbuff, rxa->midbuff);
    CBL::setSize_cbl (rxa->cbl.p, rxa->dsp_size);
    SPEAK::setBuffers_speak (rxa->speak.p, rxa->midbuff, rxa->midbuff);
    SPEAK::setSize_speak (rxa->speak.p, rxa->dsp_size);
    MPEAK::setBuffers_mpeak (rxa->mpeak.p, rxa->midbuff, rxa->midbuff);
    MPEAK::setSize_mpeak (rxa->mpeak.p, rxa->dsp_size);
    SSQL::setBuffers_ssql (rxa->ssql.p, rxa->midbuff, rxa->midbuff);
    SSQL::setSize_ssql (rxa->ssql.p, rxa->dsp_size);
    PANEL::setBuffers_panel (rxa->panel.p, rxa->midbuff, rxa->midbuff);
    PANEL::setSize_panel (rxa->panel.p, rxa->dsp_size);
    // output resampler
    RESAMPLE::setBuffers_resample (rxa->rsmpout.p, rxa->midbuff, rxa->outbuff);
    RESAMPLE::setSize_resample (rxa->rsmpout.p, rxa->dsp_size);
    rxa->csDSP.unlock();
}

void RXA::setSpectrumProbe(BufferProbe *spectrumProbe)
{
    SENDER::SetSpectrum(*this, 1, spectrumProbe);
    sender.p->run = 1;
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
        bpsnbaCheck (rxa, mode, rxa.ndb.p->master_run);
        bp1Check (
            rxa,
            amd_run,
            rxa.snba.p->run,
            rxa.emnr.p->run,
            rxa.anf.p->run,
            rxa.anr.p->run
        );
        rxa.csDSP.lock();
        rxa.mode = mode;
        rxa.amd.p->run  = 0;
        rxa.fmd.p->run  = 0;

        switch (mode)
        {
        case RXA_AM:
            rxa.amd.p->run  = 1;
            rxa.amd.p->mode = 0;
            break;
        case RXA_SAM:
            rxa.amd.p->run  = 1;
            rxa.amd.p->mode = 1;
            break;
        case RXA_DSB:
            break;
        case RXA_FM:
            rxa.fmd.p->run  = 1;
            break;
        default:

            break;
        }

        bp1Set (rxa);
        bpsnbaSet (rxa);                         // update variables
        rxa.csDSP.unlock();
    }
}

void RXA::ResCheck (RXA& rxa)
{
    // turn OFF/ON resamplers depending upon whether they're needed
    RESAMPLE *a = rxa.rsmpin.p;
    if (rxa.in_rate  != rxa.dsp_rate)
        a->run = 1;
    else
        a->run = 0;
    a = rxa.rsmpout.p;
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
    BANDPASS *a = rxa.bp1.p;
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
        BANDPASS::setGain_bandpass (a, gain, 0);
}

void RXA::bp1Set (RXA& rxa)
{
    BANDPASS *a = rxa.bp1.p;
    int old = a->run;
    if ((rxa.amd.p->run  == 1) ||
        (rxa.snba.p->run == 1) ||
        (rxa.emnr.p->run == 1) ||
        (rxa.anf.p->run  == 1) ||
        (rxa.anr.p->run  == 1)
    )
        a->run = 1;
    else
        a->run = 0;
    if (!old && a->run)
        BANDPASS::flush_bandpass (a);
    FIRCORE::setUpdate_fircore (a->p);
}

void RXA::bpsnbaCheck (RXA& rxa, int mode, int notch_run)
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = rxa.bpsnba.p;
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
        BPSNBA::recalc_bpsnba_filter (a, 0);
    }
}

void RXA::bpsnbaSet (RXA& rxa)
{
    // for BPSNBA: set run, position, freqs, run_notches
    // call this upon change in RXA_mode, snba_run, notch_master_run
    BPSNBA *a = rxa.bpsnba.p;
    switch (rxa.mode)
    {
        case RXA_LSB:
        case RXA_CWL:
        case RXA_DIGL:
            a->run = rxa.snba.p->run;
            a->position = 0;
            break;
        case RXA_USB:
        case RXA_CWU:
        case RXA_DIGU:
            a->run = rxa.snba.p->run;
            a->position = 0;
            break;
        case RXA_AM:
        case RXA_SAM:
        case RXA_DSB:
            a->run = rxa.snba.p->run;
            a->position = 1;
            break;
        case RXA_FM:
            a->run = rxa.snba.p->run;
            a->position = 1;
            break;
        case RXA_DRM:
        case RXA_SPEC:
            a->run = 0;
            break;
    }
    FIRCORE::setUpdate_fircore (a->bpsnba->p);
}

/********************************************************************************************************
*                                                                                                       *
*                                               Collectives                                             *
*                                                                                                       *
********************************************************************************************************/

void RXA::SetPassband (RXA& rxa, float f_low, float f_high)
{
    BANDPASS::SetBandpassFreqs   (rxa, f_low, f_high); // After spectral noise reduction ( AM || ANF || ANR || EMNR)
    SNBA::SetSNBAOutputBandwidth (rxa, f_low, f_high); // Spectral noise blanker (SNB)
    NBP::NBPSetFreqs             (rxa, f_low, f_high); // Notched bandpass
}

void RXA::SetNC (RXA& rxa, int nc)
{
    int oldstate = rxa.state;
    NBP::NBPSetNC           (rxa, nc);
    BPSNBA::BPSNBASetNC     (rxa, nc);
    BANDPASS::SetBandpassNC (rxa, nc);
    EQP::SetEQNC            (rxa, nc);
    FMSQ::SetFMSQNC         (rxa, nc);
    FMD::SetFMNCde          (rxa, nc);
    FMD::SetFMNCaud         (rxa, nc);
    rxa.state = oldstate;
}

void RXA::SetMP (RXA& rxa, int mp)
{
    NBP::NBPSetMP           (rxa, mp);
    BPSNBA::BPSNBASetMP     (rxa, mp);
    BANDPASS::SetBandpassMP (rxa, mp);
    EQP::SetEQMP            (rxa, mp);
    FMSQ::SetFMSQMP         (rxa, mp);
    FMD::SetFMMPde          (rxa, mp);
    FMD::SetFMMPaud         (rxa, mp);
}

} // namespace WDSP
