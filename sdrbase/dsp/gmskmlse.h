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

#ifndef INCLUDE_GMSKMLSE_H
#define INCLUDE_GMSKMLSE_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include "dsp/dsptypes.h"

// Maximum likelihood sequence estimator for GMSK with modulation index 1/2.
//
// GMSK is continuous phase modulation, not frequency shift keying that happens to be
// filtered, and demodulating it with a phase discriminator and a slicer gives away
// several dB. The transmitted phase is
//
//     phi(t) = pi * sum_i a_i q(t - iT),   a_i = +-1
//
// where q is the integral of the Gaussian frequency pulse, rising from 0 to 1/2 over L
// symbol periods. Every symbol smears its phase contribution over L symbols, and only
// after L symbols has it contributed its full +-pi/2.
//
// That makes the signal a finite state machine. Its state is the accumulated phase - four
// values, since it advances in steps of pi/2 - together with the L-1 symbols still part
// way through their pulse, so 4 * 2^(L-1) states, or 16 for L=3. What a slicer suffers as
// inter symbol interference is structure rather than noise, and a sequence detector
// resolves it. Unlike a discriminator it also never takes the argument of a noisy sample,
// so it has no threshold effect to fall off.
//
// Branch metrics are formed by correlating the received complex baseband against the exact
// waveform each state transition would have produced. The accumulated phase is applied as
// a rotation after correlating, so only 2^L correlations are needed per symbol rather than
// one per branch.
//
// Coherent detection needs a phase reference held over the whole burst, which for a signal
// with any carrier offset a one shot estimate cannot provide - a few Hz of residual error
// walks the phase away over a couple of hundred symbols. So each survivor carries its own
// second order decision directed phase tracking loop, updated from its own decisions. A
// survivor that is decoding correctly tracks the carrier; one that is not accumulates
// phase error and dies, which is what is wanted. Without this the detector performs worse
// than a discriminator, so the loop is not optional.
//
// decode() also returns a per symbol reliability - the metric difference between the best
// surviving path asserting +1 and the best asserting -1 - which can drive soft decision
// techniques such as Chase decoding of an outer CRC.

class GmskMlse
{
public:
    // samplesPerSymbol - the signal must be sampled at an integer multiple of the baud rate
    // span             - assumed length of the Gaussian phase pulse in symbols, 1 to 4.
    //                    3 is right for the BT values used in practice; 4 costs twice the
    //                    CPU for no measurable gain
    // bt               - bandwidth symbol time product of the transmit filter
    void create(int samplesPerSymbol, int span, double bt)
    {
        m_sps = samplesPerSymbol;
        m_L = std::max(1, std::min(4, span));
        m_numCombos = 1 << m_L;
        m_numStates = 4 << (m_L - 1);
        m_histMask = (1 << (m_L - 1)) - 1;

        // Gaussian frequency pulse, integrated to give the phase pulse q. Computed on a
        // fine grid and normalised numerically so that q(LT) is exactly 1/2, which is what
        // makes each symbol's total phase contribution pi/2.
        const int fine = 64;
        const int nFine = m_L * m_sps * fine;
        std::vector<double> g(nFine + 1);

        double k = 2.0 * M_PI * bt / std::sqrt(std::log(2.0));
        double centre = m_L / 2.0;
        double sum = 0.0;

        for (int i = 0; i <= nFine; i++)
        {
            double t = (double) i / (m_sps * fine) - centre;
            g[i] = 0.5 * (qfunc(k * (t - 0.5)) - qfunc(k * (t + 0.5)));
            sum += g[i];
        }

        double total = sum - 0.5 * (g[0] + g[nFine]);
        double scale = (total > 0.0) ? (0.5 / total) : 0.0;

        m_q.assign(m_L * m_sps, 0.0);

        double acc = 0.0;

        for (int i = 0, idx = 0; i < nFine; i++)
        {
            if ((i % fine) == 0) {
                m_q[idx++] = acc * scale;
            }
            acc += 0.5 * (g[i] + g[i+1]);
        }

        // Waveform for every combination of the L symbols in flight, over one symbol
        // period, relative to the accumulated phase state
        m_w.assign((size_t) m_numCombos * m_sps, std::complex<double>(0.0, 0.0));

        for (int combo = 0; combo < m_numCombos; combo++)
        {
            for (int m = 0; m < m_sps; m++)
            {
                double phase = 0.0;

                for (int i = 0; i < m_L; i++)
                {
                    double a = ((combo >> i) & 1) ? 1.0 : -1.0;
                    phase += a * m_q[m + i*m_sps];
                }

                phase *= M_PI;
                m_w[(size_t) combo*m_sps + m] = std::complex<double>(std::cos(phase), std::sin(phase));
            }
        }

        for (int p = 0; p < 4; p++)
        {
            double a = p * M_PI / 2.0;
            m_rot[p] = std::complex<double>(std::cos(a), std::sin(a));
        }
    }

    // Gains for the per survivor phase tracking loop. The optimum is broad; 0.3 and 0.05
    // measured best for AIS and anything from 0.2/0.01 to 0.6/0.1 is within a few percent.
    void setLoopGains(double phaseGain, double freqGain)
    {
        m_phaseGain = phaseGain;
        m_freqGain = freqGain;
    }

    int getStates() const { return m_numStates; }

    // Decode n symbols. samples(k, m) must return sample m of symbol period k as a
    // std::complex<double>. soft is filled with a signed reliability whose sign is the
    // decided symbol: positive for +1, negative for -1.
    //
    // The loop starts with no knowledge of the carrier phase, so the first several symbols
    // are unreliable - decode a preamble before the data and discard those symbols.
    template <class SampleFn>
    void decode(int n, SampleFn samples, std::vector<Real>& soft)
    {
        m_metric.assign(m_numStates, 0.0);
        m_next.assign(m_numStates, -1e30);
        m_from.assign((size_t) n * m_numStates, 0);
        m_delta.assign(n, 0.0);

        m_ph.assign(m_numStates, std::complex<double>(1.0, 0.0));
        m_phNext.assign(m_numStates, std::complex<double>(1.0, 0.0));
        m_freq.assign(m_numStates, 0.0);
        m_freqNext.assign(m_numStates, 0.0);

        soft.assign(n, 0.0f);

        m_corr.resize(m_numCombos);

        for (int kSym = 0; kSym < n; kSym++)
        {
            // Correlate this symbol period against every combination of the symbols in
            // flight. Done once per combination rather than once per branch, since the
            // accumulated phase state is only a rotation.
            for (int combo = 0; combo < m_numCombos; combo++)
            {
                std::complex<double> c(0.0, 0.0);

                for (int m = 0; m < m_sps; m++) {
                    c += samples(kSym, m) * std::conj(m_w[(size_t) combo*m_sps + m]);
                }

                m_corr[combo] = c;
            }

            for (int s = 0; s < m_numStates; s++) {
                m_next[s] = -1e30;
            }

            double best1 = -1e30;
            double best0 = -1e30;

            for (int s = 0; s < m_numStates; s++)
            {
                if (m_metric[s] <= -1e29) {
                    continue;
                }

                int p = s & 3;
                int hist = s >> 2;
                std::complex<double> ref = m_rot[p] * m_ph[s];

                for (int u = 0; u < 2; u++)
                {
                    int combo = u | (hist << 1);
                    std::complex<double> c = m_corr[combo] * std::conj(ref);
                    double metric = m_metric[s] + c.real();

                    // The oldest symbol has finished its pulse and joins the accumulated
                    // phase
                    int oldBit = (combo >> (m_L - 1)) & 1;
                    int p2 = (p + (oldBit ? 1 : 3)) & 3;
                    int s2 = p2 | ((combo & m_histMask) << 2);

                    if (metric > m_next[s2])
                    {
                        m_next[s2] = metric;
                        m_from[(size_t) kSym * m_numStates + s2] = (s << 1) | u;

                        // Second order decision directed phase tracking, carried per
                        // survivor. The error is the sine of the residual branch phase, so
                        // it is bounded and needs no atan, and the rotation update uses a
                        // small angle approximation to avoid a sincos per state.
                        double mag = std::abs(c);
                        double e = (mag > 1e-12) ? (c.imag() / mag) : 0.0;
                        double freq = m_freq[s] + m_freqGain * e;
                        double step = freq + m_phaseGain * e;

                        std::complex<double> rot(1.0, step);
                        rot *= 1.0 / std::sqrt(1.0 + step*step);

                        m_phNext[s2] = m_ph[s] * rot;
                        m_freqNext[s2] = freq;
                    }

                    if (u) {
                        best1 = std::max(best1, metric);
                    } else {
                        best0 = std::max(best0, metric);
                    }
                }
            }

            m_delta[kSym] = best1 - best0;

            // Renormalise so the metrics cannot run away over a long burst
            double best = -1e30;

            for (int s = 0; s < m_numStates; s++) {
                best = std::max(best, m_next[s]);
            }
            for (int s = 0; s < m_numStates; s++) {
                m_next[s] -= best;
            }

            m_metric.swap(m_next);
            m_ph.swap(m_phNext);
            m_freq.swap(m_freqNext);
        }

        // Traceback from the best final state
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

        for (int kSym = n - 1; kSym >= 0; kSym--)
        {
            int f = m_from[(size_t) kSym * m_numStates + s];
            int u = f & 1;

            soft[kSym] = (Real) ((u ? 1.0 : -1.0) * std::fabs(m_delta[kSym]));
            s = f >> 1;
        }
    }

private:
    static double qfunc(double x) { return 0.5 * std::erfc(x / std::sqrt(2.0)); }

    int m_sps = 6;
    int m_L = 3;
    int m_numCombos = 8;
    int m_numStates = 16;
    int m_histMask = 3;
    double m_phaseGain = 0.3;
    double m_freqGain = 0.05;

    std::vector<double> m_q;
    std::vector<std::complex<double>> m_w;
    std::complex<double> m_rot[4];

    std::vector<std::complex<double>> m_corr;
    std::vector<double> m_metric;
    std::vector<double> m_next;
    std::vector<int> m_from;
    std::vector<double> m_delta;
    std::vector<std::complex<double>> m_ph;
    std::vector<std::complex<double>> m_phNext;
    std::vector<double> m_freq;
    std::vector<double> m_freqNext;
};

#endif // INCLUDE_GMSKMLSE_H
