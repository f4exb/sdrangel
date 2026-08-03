///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_AFSKMLSE_H
#define INCLUDE_AFSKMLSE_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include "dsp/dsptypes.h"

// Maximum likelihood sequence estimator for AFSK over FM, as used by Bell 202 at 1200 baud
// for AX.25 packet radio.
//
// The usual way to demodulate this is to run an FM discriminator and then correlate its
// output against the two audio tones. That works, but the discriminator takes the argument
// of every noisy sample, and below roughly 8 dB carrier to noise in the channel bandwidth
// it starts producing 2 pi phase slips. Its output signal to noise then collapses far
// faster than the input degrades - the FM threshold effect - and the demodulator falls off
// a cliff rather than degrading gracefully. Measured on real signals, the discriminator
// stops decoding at about 3 dB while this detector still works below -2 dB.
//
// AFSK over FM is not two independent tones. The audio is phase continuous, so the
// transmitted RF is
//
//     s(t) = exp(j * 2 * pi * dev * integral of a(t)),   a(t) = cos(2 pi f t + theta)
//
// which is a continuous phase modulation with memory in theta, the audio phase. Over one
// bit the audio phase advances by f/baud cycles: exactly 1 for the 1200 Hz mark tone and
// 11/6 for the 2200 Hz space tone. Both are multiples of 1/6 of a cycle, so the audio phase
// at bit boundaries takes only six values and the signal is a six state machine. A mark
// leaves the state alone, a space steps it back by one sixth.
//
// Each of the twelve state and bit combinations therefore has one exact waveform over a bit
// period, and branch metrics are formed by correlating the received complex baseband
// against it. Nothing takes the argument of a sample, so there is no threshold to fall off.
//
// Real transmitters do not honour those ratios. NO-84 (PSAT) measures mark/baud = 0.99 and
// space/baud = 1.830 against a 1201.9 Hz baud - the tones look like an analog oscillator
// rather than a divided crystal, and the mark tone even wanders a few Hz within a pass.
// Under the exact six state model the assumed audio phase then drifts by up to 10
// millicycles per symbol, which is half a state within a few tens of symbols: the detector
// acquires, decodes 60 symbols, and falls apart, while a discriminator - which never
// integrates phase - does not care. The audio phase therefore has to be tracked, not
// assumed. Each survivor carries a continuous audio phase, branch waveforms are drawn from
// a table quantised much more finely than the states, and every branch may slip its phase
// by one table step per symbol, choosing whichever reference correlates best. One step per
// symbol tracks a tone/baud ratio error of about +/-1%, an order of magnitude beyond what
// real TNCs get wrong, while the states keep their role of separating candidate paths. On
// an exact Bell 202 signal the slips simply never win, and nothing changes.
//
// Real transmitters do not switch tone instantly either. NO-84's audio is band limited
// somewhere in the transmit chain, so a tone change sweeps from one frequency to the other
// over about a third of a symbol. Measured on the recording, each symbol adjacent to a
// transition advances the audio phase 37 millicycles away from the instant switch model -
// away in opposite directions for mark and space, the signature of a symmetric frequency
// ramp - and since HDLC data changes tone on about half its symbols, the reference phase is
// wrong nearly everywhere it matters: the branch correlations stay strong but stop telling
// the two bits apart. setTransitionRamp() puts a raised cosine frequency sweep of that
// length at the start of every tone changing branch waveform. The waveform then depends on
// the previous tone as well, but the previous bit of each survivor is already determined by
// its surviving branch, so the tables gain a dimension while the trellis does not.
//
// As in GmskMlse, coherent detection needs a phase reference held across the burst, so each
// survivor carries its own second order decision directed loop. The deterministic part of
// the RF phase - what the modulation itself contributes across a bit, which is zero for a
// mark and depends on the state for a space - is folded into the same rotator.
//
// The loop tracks residual carrier error but will not acquire hundreds of Hz on its own.
// Pass in a coarse carrier estimate removed beforehand; on weak signals estimating it by
// maximising the path metric over a small set of trial frequencies works well, where a
// discriminator based estimate is biased toward zero by the noise.
//
// decode() returns a per bit reliability - the metric difference between the best surviving
// path asserting space and the best asserting mark - to drive soft decision techniques such
// as Chase decoding of the outer CRC.

class AfskMlse
{
public:
    // The branch waveform tables are read only once create() has built them, and identical
    // for every detector running the same model - so they are shared rather than copied.
    // They are about 250 kB, and a detector runs a hundred or more chains over one signal,
    // rebuilding a set per chain whenever the model is retuned. Sharing turns that into a
    // reference count: it removes tens of megabytes, and removes a burst-end stall long
    // enough to overflow the baseband FIFO and drop samples.
    struct Tables
    {
        std::vector<double> m_psiDot;             // d(psi)/d(tau), [phase][prev][bit][sample]
        std::vector<std::complex<double>> m_w;    // branch waveforms, same indexing
        std::vector<std::complex<double>> m_dw;   // ... and their differential form
        std::vector<std::complex<double>> m_dpsi; // phase carried into the next bit
        std::vector<int> m_next;                  // next state, [state][prev][bit]
        double m_ratio[4] = { 1.0, 11.0 / 6.0,    // Audio phase advance per bit, cycles,
                              1.0, 11.0 / 6.0 };  // [previous tone][current tone]
    };

    // samplesPerSymbol - the signal must be sampled at an integer multiple of the baud rate
    // baud             - symbol rate, 1200 for Bell 202
    // markFreq         - lower audio tone, 1200 Hz for Bell 202
    // spaceFreq        - upper audio tone, 2200 Hz for Bell 202
    // markDeviation    - peak FM deviation of the mark tone
    // spaceDeviation   - and of the space tone. Transmitters normally apply pre-emphasis, so
    //                    the space tone deviates further; the ratio matters much less than
    //                    being in the right region, and 1.0 costs little
    // states           - audio phase states. 6 is exact for 1200/2200 at 1200 baud
    // rampSamples      - tone transitions sweep frequency over this many samples rather
    //                    than switching instantly, matching a transmitter whose audio is
    //                    band limited. 0 models an instant switch
    void create(int samplesPerSymbol, double baud, double markFreq, double spaceFreq,
                double markDeviation, double spaceDeviation, int states = 6,
                int rampSamples = 0)
    {
        m_sps = samplesPerSymbol;

        // KNOWN NON-OPTIMALITY, measured and accepted rather than overlooked.
        //
        // The state is the audio phase alone. With rampSamples > 0 the branch waveform
        // depends on the PREVIOUS tone as well as the current one, and so does the phase
        // advance, because the advance is integrated over the ramped profile - see
        // m_ratio, which is indexed by both. Two paths reaching the same phase having
        // sent different tones are therefore not equivalent for the next branch, but ACS
        // keeps one survivor per phase and m_prevBit is simply inherited from whichever
        // path won. That is not a valid Viterbi merge: in principle the globally best
        // sequence can be discarded. Concretely at 6 states, mark advances 6 and space
        // 11, so state 0 is reached from state 0 having sent mark and from state 1 having
        // sent space, and one of the two prior tones is thrown away.
        //
        // The exact state is phase x previous tone - 12 states for Bell 202. That was
        // implemented and measured against this:
        //
        //   AWGN 8/4/0/-2 dB   20/20/19/13 both      no difference
        //   NO-84 pass         14 payloads both      no difference
        //   weak_signal 08-01  5 payloads both       no difference
        //   V.23 recording     5 -> 4                slightly worse, within that
        //                                            recording's noise
        //   CPU                2.5x                  1.69 s -> 4.32 s on one AWGN run,
        //                                            and streaming fell from 28x to 15x
        //                                            realtime on the NO-84 pass
        //
        // Doubling the survivors bought nothing measurable, so 6 states stands. The error
        // appears to be small in practice because the ramp is a few samples of a symbol
        // and paths merging with different prior tones rarely have metrics close enough
        // for the discarded one to have won. Two ways to make it exact if that ever stops
        // being true: carry the tone in the state, or set rampSamples to 0, which removes
        // the dependence altogether at the cost of the ramp model.
        m_numStates = std::max(1, states);

        // The waveform tables are built on a grid several times finer than the states, so a
        // survivor's audio phase can sit between states and be tracked there. 8 sub phases
        // leaves a worst case quantisation of 1/96 cycle, whose waveform mismatch costs
        // under 1% of correlation.
        m_numPhases = m_numStates * SUB_PHASES;

        const double f[2] = { markFreq, spaceFreq };
        const double dev[2] = { markDeviation, spaceDeviation };
        const int ramp = std::max(0, std::min(rampSamples, m_sps));

        // Waveforms are indexed by starting audio phase, the previous tone and the current
        // one - the branch that changes tone begins with the sweep away from the old tone
        auto tables = std::make_shared<Tables>();
        Tables& t = *tables;

        t.m_w.assign((size_t) m_numPhases * 4 * m_sps, std::complex<double>(0.0, 0.0));
        t.m_psiDot.assign((size_t) m_numPhases * 4 * m_sps, 0.0);
        t.m_dw.assign((size_t) m_numPhases * 4 * m_sps, std::complex<double>(0.0, 0.0));
        t.m_dpsi.assign((size_t) m_numPhases * 4, std::complex<double>(1.0, 0.0));
        t.m_next.assign((size_t) m_numStates * 4, 0);

        // Audio frequency and deviation profile over one bit period, per previous/current
        // tone pair: a raised cosine sweep from the old values over the first ramp samples,
        // then steady at the new ones
        auto profile = [&](int p, int b, double m, double& fr, double& dv)
        {
            double blend = 1.0;

            if ((p != b) && (ramp > 0) && (m < (double) ramp)) {
                blend = 0.5 * (1.0 - std::cos(M_PI * (m + 0.5) / ramp));
            }

            fr = f[p] + (f[b] - f[p]) * blend;
            dv = dev[p] + (dev[b] - dev[p]) * blend;
        };

        // The waveforms no longer have a closed form, so integrate the audio phase and the
        // RF phase numerically, with substeps for accuracy. Only done at create time.
        const int sub = 8;
        const double dt = 1.0 / (m_sps * baud * sub);

        for (int p = 0; p < 2; p++)
        {
            for (int b = 0; b < 2; b++)
            {
                // Audio phase advance for this tone pair, in cycles - exact, not quantised
                double cycles = 0.0;

                for (int m = 0; m < m_sps * sub; m++)
                {
                    double fr, dv;
                    profile(p, b, (double) m / sub, fr, dv);
                    cycles += fr * dt;
                }

                t.m_ratio[(size_t) p*2 + b] = cycles;

                for (int s = 0; s < m_numStates; s++)
                {
                    // Advance in units of one state, exact for Bell 202. The states only
                    // decide which paths merge; the phase itself is carried per survivor.
                    int adv = (int) std::lround(cycles * m_numStates);
                    t.m_next[((size_t) s*2 + p)*2 + b] =
                        ((s + adv) % m_numStates + m_numStates) % m_numStates;
                }

                for (int q = 0; q < m_numPhases; q++)
                {
                    double theta = 2.0 * M_PI * q / m_numPhases;
                    size_t base = ((size_t) q*2 + p)*2 + b;

                    // Waveform this branch produces over one bit period, taken relative to
                    // the phase already accumulated, so it starts at zero phase
                    double audioPhase = theta;
                    double psi = 0.0;

                    for (int m = 0; m < m_sps; m++)
                    {
                        double fr, dv;
                        profile(p, b, (double) m, fr, dv);

                        t.m_w[base*m_sps + m] = std::complex<double>(std::cos(psi), std::sin(psi));

                        // d(psi)/d(tau), for the timing error discriminant
                        t.m_psiDot[base*m_sps + m] = 2.0*M_PI*dv * std::cos(audioPhase);

                        for (int u = 0; u < sub; u++)
                        {
                            profile(p, b, m + (double) u / sub, fr, dv);
                            psi += 2.0*M_PI*dv * std::cos(audioPhase + M_PI*fr*dt) * dt;
                            audioPhase += 2.0*M_PI*fr * dt;
                        }
                    }

                    // Differential reference: what one sample times the conjugate of the
                    // last looks like. An unknown carrier phase cancels in that product,
                    // which is the whole point - fading destroys the phase reference a
                    // coherent metric needs, and a tumbling satellite fades hard.
                    for (int m = 1; m < m_sps; m++)
                    {
                        t.m_dw[base*m_sps + m] =
                            t.m_w[base*m_sps + m] * std::conj(t.m_w[base*m_sps + m - 1]);
                    }

                    // Phase the branch carries into the next bit. Zero for a mark, since a
                    // full cycle of audio returns the RF phase to where it started.
                    t.m_dpsi[base] = std::complex<double>(std::cos(psi), std::sin(psi));
                }
            }
        }

        m_t = tables;

        // Converts the timing discriminant to samples. For a sampling offset d, the branch
        // correlation picks up a phase psiDot*d, so Im(conj(c).cw) comes out proportional to
        // d times the variance of psiDot over the symbol - which for a tone of peak deviation
        // dev is (2.pi.dev)^2/2. Dividing that out leaves the loop gains dimensionless.
        double devAvg = 0.5 * (markDeviation + spaceDeviation);
        double var = 0.5 * (2.0*M_PI*devAvg) * (2.0*M_PI*devAvg);

        m_timingScale = (var > 0.0) ? ((m_sps * baud) / var) : 0.0;

        m_timingLo = m_sps / 4;
        m_timingHi = m_sps - m_sps / 4;

        m_buf.assign(m_sps, std::complex<double>(0.0, 0.0));
        m_dbuf.assign(m_sps, std::complex<double>(0.0, 0.0));
    }

    // Gains for the per survivor phase tracking loop. Higher gains widen the frequency the
    // loop will pull in but are noisier, and the loop will not acquire hundreds of Hz at any
    // gain worth using. 0.1/0.01 measures best on a static carrier but loses whole frames to
    // a satellite pass drifting tens of Hz per second; 0.3/0.03 tracks NO-84's Doppler to one
    // symbol error per frame and measures no worse on AWGN. 0.2/0.05 is bad everywhere.
    void setLoopGains(double phaseGain, double freqGain)
    {
        m_phaseGain = phaseGain;
        m_freqGain = freqGain;
    }

    int getStates() const { return m_numStates; }

    // How far back to trace before committing a bit. The trellis only carries one bit of
    // memory so the survivors merge quickly; 48 is already far more than needed.
    void setTracebackDepth(int depth) { m_depth = std::max(2, depth); }

    // Enables the experimental timing gradient. Off by default: it costs a multiply per
    // sample and has not been made to beat a fixed grid of timing hypotheses.
    void setTimingEnabled(bool enabled) { m_timingEnabled = enabled; }

    // Audio phase slip tracking, on by default. Off restores the exact Bell 202 model,
    // which is only right for a transmitter whose tones and baud share one clock.
    void setToneTracking(bool enabled) { m_toneTracking = enabled; }

    // Choose slips on correlation magnitude rather than the coherent metric. Magnitude is
    // invariant to carrier phase so the tracker cannot be recruited to fight the carrier
    // loop, but picking the largest of three magnitudes is noise biased and costs packets
    // near the sensitivity floor.
    void setSlipByMagnitude(bool enabled) { m_slipByMagnitude = enabled; }

    // Differential branch metric instead of coherent. It gives up some of the ideal AWGN
    // gain - a coherent detector is optimum when the phase reference holds - but it does not
    // need one at all, so it keeps working through the fading that makes the coherent metric
    // collapse. Nothing here takes the argument of a sample either way, so the FM threshold
    // is avoided in both modes.
    void setDifferential(bool differential) { m_differential = differential; }

    // Reset the trellis and the tracking loops
    void start()
    {
        m_metric.assign(m_numStates, 0.0);
        m_metricNext.assign(m_numStates, -1e30);
        m_ph.assign(m_numStates, std::complex<double>(1.0, 0.0));
        m_phNext.assign(m_numStates, std::complex<double>(1.0, 0.0));
        m_freq.assign(m_numStates, 0.0);
        m_freqNext.assign(m_numStates, 0.0);
        m_theta.assign(m_numStates, 0.0);
        m_thetaNext.assign(m_numStates, 0.0);
        m_prevBit.assign(m_numStates, 0);
        m_prevNext.assign(m_numStates, 0);

        for (int s = 0; s < m_numStates; s++) {
            m_theta[s] = (double) s / m_numStates;
        }

        m_ringFrom.assign((size_t) m_depth * m_numStates, 0);
        m_ringErr.assign((size_t) m_depth * m_numStates, 0.0);
        m_ringPow.assign((size_t) m_depth * m_numStates, 0.0);
        m_avgPow = 0.0;
        m_ringDelta.assign(m_depth, 0.0);
        m_pos = 0;
        m_total = 0.0;
    }

    // Feed one bit period. samples(m) must return sample m of it as a std::complex<double>.
    // Returns true once a decision is available, which is delayed by the traceback depth.
    // soft is a signed reliability whose sign is the decided tone: positive for space,
    // negative for mark.
    //
    // The loop starts with no knowledge of the carrier phase, so the first several bits are
    // unreliable - there is always a flag preamble ahead of the data to absorb them.
    template <class SampleFn>
    bool step(SampleFn samples, Real& soft)
    {
        advance(samples);

        if (m_pos < (uint64_t) m_depth) {
            return false;
        }

        soft = traceback(m_pos - m_depth);

        return true;
    }

    // Decode n bits in one go, for offline use. samples(k, m) returns sample m of bit k.
    // Equivalent to start() then step() for every bit, with the tail flushed, so it measures
    // exactly what a streaming caller gets.
    template <class SampleFn>
    void decode(int n, SampleFn samples, std::vector<Real>& soft)
    {
        soft.assign(n, 0.0f);
        start();

        for (int k = 0; k < n; k++)
        {
            Real s;

            if (step([&samples, k](int m) { return samples(k, m); }, s)) {
                soft[k - m_depth + 1] = s;
            }
        }

        // The last depth-1 bits have not been committed yet
        uint64_t first = (m_pos >= (uint64_t) m_depth) ? (m_pos - m_depth + 1) : 0;

        for (uint64_t k = first; k < m_pos; k++) {
            soft[(size_t) k] = traceback(k);
        }
    }

    // Total path metric of the last decode(). Comparable between runs over the same samples,
    // so trying a few carrier frequencies and keeping the largest gives a maximum likelihood
    // frequency estimate that keeps working where a discriminator based one does not.
    double getMetric() const { return m_total; }

    void resetMetric() { m_total = 0.0; }

    // Timing error in samples, positive when the reference should be taken later. Drive a
    // second order loop with it and shift where the caller reads the next symbol period.
    // Without this the symbol clock is a fixed grid, and a transmitter a few hundred ppm out
    // walks off it part way through a frame.
    //
    // Taken from the branch on the surviving path rather than from whichever branch looked
    // best on the symbol just processed. A tentative decision is wrong often enough near the
    // noise floor to bias the loop. That costs a delay, but the trellis carries only one bit
    // of memory so the survivors merge within a few symbols and the timing traceback can be
    // far shorter than the one used to commit bits.
    double getTimingError()
    {
        if (m_pos < (uint64_t) m_timingDepth) {
            return 0.0;
        }

        int s = bestState();
        uint64_t k = m_pos - m_timingDepth;

        for (uint64_t j = m_pos - 1; j > k; j--) {
            s = m_ringFrom[(size_t) (j % m_depth) * m_numStates + s] >> 1;
        }

        size_t idx = (size_t) (k % m_depth) * m_numStates + s;

        // Weight by how far this symbol's correlation stands above the running average, which
        // most symbols spend on noise. A quiet symbol then contributes almost nothing without
        // the detector needing to be told where the frames are.
        double pow = m_ringPow[idx];
        double weight = pow / (pow + m_timingGate * m_avgPow + 1e-30);

        return m_ringErr[idx] * weight;
    }

    // How far above the running average a symbol must sit to drive the timing loop at full
    // weight. Larger ignores more of the signal; smaller lets noise in.
    void setTimingGate(double gate) { m_timingGate = gate; }

    // How far back to look for a settled decision to take the timing error from
    void setTimingDepth(int depth) { m_timingDepth = std::max(1, depth); }

private:
    // One add compare select over the whole trellis, writing the survivors into the ring
    template <class SampleFn>
    void advance(SampleFn samples)
    {
        // Fetch the bit period once - the caller's sample function may be interpolating -
        // and form the differential products once rather than per branch
        for (int m = 0; m < m_sps; m++) {
            m_buf[m] = samples(m);
        }

        if (m_differential)
        {
            for (int m = 1; m < m_sps; m++) {
                m_dbuf[m] = m_buf[m] * std::conj(m_buf[m-1]);
            }
        }

        for (int s = 0; s < m_numStates; s++) {
            m_metricNext[s] = -1e30;
        }

        size_t ring = (size_t) (m_pos % m_depth);
        int *from = &m_ringFrom[ring * m_numStates];

        double bestSpace = -1e30;
        double bestMark = -1e30;
        double maxPow = 0.0;

        // A branch may slip its audio phase by one table step per symbol, whichever of the
        // three references correlates best. This is what tracks a transmitter whose
        // tone/baud ratios are not the exact Bell 202 values - see the header comment.
        const int slips = m_toneTracking ? 1 : 0;

        for (int s = 0; s < m_numStates; s++)
        {
            if (m_metric[s] <= -1e29) {
                continue;
            }

            int q0 = (int) std::lround(m_theta[s] * m_numPhases) % m_numPhases;

            // Which tone this survivor just sent decides how the next waveform begins,
            // because a real transition sweeps rather than switches
            const int p = m_prevBit[s];

            for (int b = 0; b < 2; b++)
            {
                std::complex<double> cSel(0.0, 0.0);
                double errSel = 0.0;
                double powSel = 0.0;
                int qSel = q0;
                bool first = true;

                for (int dq = -slips; dq <= slips; dq++)
                {
                    int q = q0 + dq;

                    if (q < 0) {
                        q += m_numPhases;
                    } else if (q >= m_numPhases) {
                        q -= m_numPhases;
                    }

                    const std::complex<double> *w = &m_t->m_w[(((size_t) q*2 + p)*2 + b)*m_sps];
                    const double *pd = &m_t->m_psiDot[(((size_t) q*2 + p)*2 + b)*m_sps];
                    std::complex<double> c(0.0, 0.0);
                    std::complex<double> cw(0.0, 0.0);

                    double err = 0.0;
                    double pow = 0.0;

                    if (m_differential)
                    {
                        // Sample 0 would need the previous symbol's last sample, which
                        // depends on the branch history, so it is skipped - one part in sps
                        // of the energy
                        const std::complex<double> *dw = &m_t->m_dw[(((size_t) q*2 + p)*2 + b)*m_sps];

                        for (int m = 1; m < m_sps; m++) {
                            c += m_dbuf[m] * std::conj(dw[m]);
                        }
                    }
                    else if (!m_timingEnabled)
                    {
                        for (int m = 0; m < m_sps; m++) {
                            c += m_buf[m] * std::conj(w[m]);
                        }
                    }
                    else
                    {
                        // EXPERIMENTAL, off by default, and measured worse than leaving the
                        // symbol clock on a fixed grid - see the harness README. The maximum
                        // likelihood timing gradient is Im(conj(c).cw), taken over the middle
                        // of the symbol because a timing offset pulls part of the adjacent
                        // symbol into the window and d(psi)/d(tau) jumps at the boundary
                        // whenever the tone changes. The carrier phase cancels, so it can be
                        // formed before the survivor rotation. It has not been made to work;
                        // something is putting a bias on it that walks the loop off good
                        // timing.
                        std::complex<double> cMid(0.0, 0.0);

                        for (int m = 0; m < m_sps; m++)
                        {
                            std::complex<double> v = m_buf[m] * std::conj(w[m]);
                            c += v;

                            if ((m >= m_timingLo) && (m < m_timingHi))
                            {
                                cMid += v;
                                cw += pd[m] * v;
                            }
                        }

                        pow = std::norm(cMid);
                        err = (pow > 1e-30)
                            ? (std::imag(std::conj(cMid) * cw) / pow * m_timingScale) : 0.0;
                    }

                    c *= std::conj(m_ph[s]);

                    double score = m_slipByMagnitude ? std::norm(c) : c.real();
                    double scoreSel = m_slipByMagnitude ? std::norm(cSel) : cSel.real();

                    if (first || (score > scoreSel))
                    {
                        first = false;
                        cSel = c;
                        qSel = q;
                        errSel = err;
                        powSel = pow;
                    }
                }

                if (powSel > maxPow) {
                    maxPow = powSel;
                }

                double metric = m_metric[s] + cSel.real();
                int s2 = m_t->m_next[((size_t) s*2 + p)*2 + b];

                if (metric > m_metricNext[s2])
                {
                    m_metricNext[s2] = metric;
                    from[s2] = (s << 1) | b;
                    m_ringErr[ring * m_numStates + s2] = errSel;
                    m_ringPow[ring * m_numStates + s2] = powSel;

                    // Second order decision directed phase tracking, carried per survivor.
                    // The error is the sine of the residual branch phase, so it is bounded
                    // and needs no atan, and the rotation update uses a small angle
                    // approximation to avoid a sincos per state.
                    double mag = std::abs(cSel);
                    double e = (mag > 1e-12) ? (cSel.imag() / mag) : 0.0;
                    double freq = m_freq[s] + m_freqGain * e;
                    double step = freq + m_phaseGain * e;

                    std::complex<double> rot(1.0, step);
                    rot *= 1.0 / std::sqrt(1.0 + step*step);

                    m_phNext[s2] = m_differential
                        ? (m_ph[s] * rot)
                        : (m_ph[s] * m_t->m_dpsi[((size_t) qSel*2 + p)*2 + b] * rot);
                    m_freqNext[s2] = freq;
                    m_prevNext[s2] = b;

                    // Audio phase carried into the next bit: the reference this branch
                    // actually used, advanced by the exact tone/baud ratio
                    m_thetaNext[s2] = std::fmod(
                        (double) qSel / m_numPhases + m_t->m_ratio[(size_t) p*2 + b], 1.0);
                }

                if (b) {
                    bestSpace = std::max(bestSpace, metric);
                } else {
                    bestMark = std::max(bestMark, metric);
                }
            }
        }

        m_ringDelta[ring] = bestSpace - bestMark;

        // Renormalise so the metrics cannot run away over a long burst
        double best = -1e30;

        for (int s = 0; s < m_numStates; s++) {
            best = std::max(best, m_metricNext[s]);
        }
        for (int s = 0; s < m_numStates; s++) {
            m_metricNext[s] -= best;
        }

        m_metric.swap(m_metricNext);
        m_ph.swap(m_phNext);
        m_freq.swap(m_freqNext);
        m_theta.swap(m_thetaNext);
        m_prevBit.swap(m_prevNext);

        if (m_timingEnabled) {
            m_avgPow += 1e-3 * (maxPow - m_avgPow);
        }

        m_total += best;
        m_pos++;
    }

    int bestState() const
    {
        int s = 0;
        double best = -1e30;

        for (int i = 0; i < m_numStates; i++)
        {
            if (m_metric[i] > best)
            {
                best = m_metric[i];
                s = i;
            }
        }

        return s;
    }

    // Decision for bit k, which must still be within the ring
    Real traceback(uint64_t k)
    {
        int s = bestState();

        for (uint64_t j = m_pos - 1; j > k; j--) {
            s = m_ringFrom[(size_t) (j % m_depth) * m_numStates + s] >> 1;
        }

        size_t ring = (size_t) (k % m_depth);
        int f = m_ringFrom[ring * m_numStates + s];

        return (Real) ((f & 1 ? 1.0 : -1.0) * std::fabs(m_ringDelta[ring]));
    }

    static const int SUB_PHASES = 8;            // Waveform table steps per state

    int m_sps = 10;
    int m_numStates = 6;
    int m_numPhases = 48;
    int m_depth = 48;
    uint64_t m_pos = 0;
    double m_phaseGain = 0.3;
    double m_freqGain = 0.03;
    double m_total = 0.0;
    double m_timingScale = 0.0;
    int m_timingDepth = 12;
    double m_timingGate = 10.0;
    double m_avgPow = 0.0;
    bool m_timingEnabled = false;
    bool m_toneTracking = true;
    bool m_slipByMagnitude = false;
    bool m_differential = false;
    int m_timingLo = 0;
    int m_timingHi = 0;
    std::shared_ptr<const Tables> m_t;          // Shared, see the declaration above

    std::vector<std::complex<double>> m_buf;    // current bit period of samples
    std::vector<std::complex<double>> m_dbuf;   // ... and its differential products

    std::vector<double> m_metric;
    std::vector<double> m_metricNext;
    std::vector<int> m_ringFrom;                // survivors, [depth][state]
    std::vector<double> m_ringErr;              // timing error per survivor branch
    std::vector<double> m_ringPow;              // ... and its correlation power
    std::vector<double> m_ringDelta;
    std::vector<std::complex<double>> m_ph;
    std::vector<std::complex<double>> m_phNext;
    std::vector<double> m_freq;
    std::vector<double> m_freqNext;
    std::vector<double> m_theta;                // audio phase per survivor, cycles
    std::vector<double> m_thetaNext;
    std::vector<int> m_prevBit;                 // tone each survivor just sent
    std::vector<int> m_prevNext;
};

#endif // INCLUDE_AFSKMLSE_H
