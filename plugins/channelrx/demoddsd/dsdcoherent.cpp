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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "dsp/rootraisedcosine.h"

#include "dsdcoherent.h"

namespace
{
    // Level of a dibit and the dibit of a level index (0..3 = +1, +3, -1, -3 as the slicer maps them)
    const int8_t levelOf[4] = { 1, 3, -1, -3 };

    // Unit phasor table for the search's trajectories, cosine and sine interleaved so that a
    // lookup touches one cache line and the table (16 kB) stays in the first level cache; a step
    // of 2 pi / 2048 is 0.18 degrees, -56 dB against the prediction
    const int s_phaseTableSize = 2048;
    const float s_phaseScale = (float) (s_phaseTableSize / (2.0 * M_PI));

    struct PhaseTable
    {
        std::vector<float> m_cosSin;    //!< cos, sin pairs

        PhaseTable() : m_cosSin(2 * s_phaseTableSize)
        {
            for (int i = 0; i < s_phaseTableSize; i++)
            {
                m_cosSin[2 * i] = (float) std::cos(2.0 * M_PI * i / s_phaseTableSize);
                m_cosSin[2 * i + 1] = (float) std::sin(2.0 * M_PI * i / s_phaseTableSize);
            }
        }
    };

    const PhaseTable& phaseTable()
    {
        static const PhaseTable table;
        return table;
    }

    // Table index of a phase: rounded, the phase made positive by a multiple of the table size
    // (2^20 entries, so the index is unchanged) so that the truncation is a floor; the search's
    // phases stay within a few hundred radians (they are wrapped at every symbol)
    inline int phaseIndex(float phase)
    {
        return ((int) (phase * s_phaseScale + (1048576.0f + 0.5f))) & (s_phaseTableSize - 1);
    }
}

DSDCoherentDetector::DSDCoherentDetector(int samplesPerSymbol, float sampleRate, double alpha) :
    m_sps(samplesPerSymbol),
    m_sampleRate(sampleRate),
    m_loopGain(0.4f),
    m_passes(2),
    m_dispersionThreshold(0.15f),
    m_dispersive(false),
    m_lastNoise(0.0f),
    m_lastEnergy(0.0f),
    m_lastFrequencyHz(0.0f)
{
    phaseTable();

    // The transmit pulse: the root raised cosine impulse response scaled as the DSE modulator
    // scales it (a constant stream of one level gives that level at the symbol instants)
    RootRaisedCosine<float> filter;
    const int span = 8;
    filter.create(alpha, span, m_sps, RootRaisedCosine<float>::Gain);
    const int length = span * m_sps + 1;
    m_pulse.assign(length, 0.0f);

    for (int i = 0; i < length; i++) {
        m_pulse[i] = filter.filter(i == 0 ? 1.0f : 0.0f);
    }

    m_pulseCentre = (int) (std::max_element(m_pulse.begin(), m_pulse.end()) - m_pulse.begin());
    float sum = 0.0f;

    for (int i = m_pulseCentre % m_sps; i < length; i += m_sps) {
        sum += m_pulse[i];
    }

    for (float& v : m_pulse) {
        v /= sum;
    }
}

float DSDCoherentDetector::pulseAt(double tSamples) const
{
    double x = tSamples + m_pulseCentre;
    int i = (int) std::floor(x);

    if ((i < 0) || (i + 1 >= (int) m_pulse.size())) {
        return 0.0f;
    }

    float f = (float) (x - i);
    return m_pulse[i] + f * (m_pulse[i + 1] - m_pulse[i]);
}

bool DSDCoherentDetector::detect(const std::vector<std::complex<float>>& samples, const std::vector<double>& centres,
    const std::vector<int8_t>& firstPass, const int8_t *before, const int8_t *after,
    float levelHz, float offsetHz, std::vector<Result>& results)
{
    // Two passes: the second takes the pulse tails from the first's decisions instead of the
    // discriminator path's
    static const int8_t levelOf[4] = { 1, 3, -1, -3 };
    std::vector<int8_t> tails = firstPass;

    for (int pass = 0; pass < m_passes; pass++)
    {
        if (!detectOnce(samples, centres, tails, before, after, levelHz, offsetHz, results)) {
            return false;
        }

        for (size_t k = 0; k < results.size(); k++) {
            tails[k] = levelOf[results[k].m_dibit];
        }
    }

    return true;
}

bool DSDCoherentDetector::detectOnce(const std::vector<std::complex<float>>& samples, const std::vector<double>& centres,
    const std::vector<int8_t>& firstPass, const int8_t *before, const int8_t *after,
    float levelHz, float offsetHz, std::vector<Result>& results)
{
    const int n = (int) centres.size();
    results.clear();
    m_dispersive = false;
    m_lastFrequencyHz = 0.0f;

    if ((n < 2) || ((int) firstPass.size() != n) || samples.empty()) {
        return false;
    }

    // Channel taps of the second stage: two symbols of memory when the burst is long enough to
    // estimate them, capped so that the fit and the equalising search cost the same on the 20
    // samples per symbol chain (an echo beyond 500 us is out of its reach there)
    const int taps = std::min(24, (n >= 60) ? 2 * m_sps : m_sps);
    // Channel filtered samples: the filter (2 * fd + 1 taps, delay fd) is part of the channel
    // model, so the prediction of a sample reaches fd samples ahead of it (the lead, into the
    // modelled trajectory, not the samples) and the echo taps combined with the filter give the
    // kernel of the second stage
    const bool filtered = !m_filter.empty();
    const int fd = filtered ? (int) m_filter.size() / 2 : 0;
    const int lead = fd;
    const int pre = taps + 2 * fd;      //!< Reference samples modelled before the burst

    // Symbol intervals: from midway to the previous centre to midway to the next
    std::vector<int> start(n + 1);

    for (int k = 0; k <= n; k++)
    {
        double boundary = (k == 0) ? centres[0] - 0.5 * m_sps : (k == n) ? centres[n - 1] + 0.5 * m_sps : 0.5 * (centres[k - 1] + centres[k]);
        start[k] = (int) std::lround(boundary);
    }

    if ((start[0] < pre + 1) || (start[n] > (int) samples.size())) {
        return false;
    }

    // Level of symbol j, first pass, with the tails outside the burst
    auto levelAt = [&](int j) -> float {
        if ((j >= 0) && (j < n)) {
            return firstPass[j];
        }

        if ((j < 0) && (j >= -4)) {
            return before[-j - 1];
        }

        if ((j >= n) && (j < n + 4)) {
            return after[j - n];
        }

        return 0.0f;
    };

    // Centre of symbol j, extrapolated outside the burst
    auto centreAt = [&](int j) -> double {
        int c = std::max(0, std::min(n - 1, j));
        return centres[c] + (j - c) * (double) m_sps;
    };

    // Per sample: the phase step contributed by the far symbols and the offset, the pulse values
    // of the three near symbols, and the step with every symbol from the first pass. Sample m
    // carries the phase step from sample m - 1 to m, the frequency at m - 0.5. The samples of
    // the channel memory before the burst are modelled too.
    const double radPerHz = 2.0 * M_PI / m_sampleRate;
    const int base = start[0] - pre;
    const int nbSamples = start[n] + lead - base;
    std::vector<float> farStep(nbSamples);
    std::vector<float> near(3 * nbSamples);
    std::vector<float> refStep(nbSamples);
    // The levels the far and reference steps are built from (the first pass, then the stage 1
    // decisions)
    std::vector<int8_t> baseLevels(firstPass);
    auto baseAt = [&](int j) -> float {
        return ((j >= 0) && (j < n)) ? baseLevels[j] : levelAt(j);
    };
    int k = 0;

    for (int m = base; m < start[n] + lead; m++)
    {
        if (m < start[0]) {
            k = -1 - (start[0] - 1 - m) / m_sps;
        }
        else
        {
            k = std::max(k, 0);

            while ((k + 1 < n) && (m >= start[k + 1])) {
                k++;
            }
        }

        double t = m - 0.5;
        float far = offsetHz;
        float all = offsetHz;
        int i = m - base;

        for (int j = k - 4; j <= k + 4; j++)
        {
            float v = levelHz * levelAt(j) * pulseAt(t - centreAt(j));
            all += v;

            if (std::abs(j - k) >= 2) {
                far += v;
            }
        }

        farStep[i] = (float) (far * radPerHz);
        refStep[i] = (float) (all * radPerHz);

        for (int d = -1; d <= 1; d++) {
            near[3 * i + d + 1] = (float) (levelHz * pulseAt(t - centreAt(k + d)) * radPerHz);
        }
    }

    // With the filter in the model, a branch predicts its samples from the phase trajectory up
    // to fd samples past its interval: the pulse values there of the three symbols the branch
    // pins (k - 1, k, k + 1), per symbol k and sample of the extension, the rest of the
    // trajectory coming from the base levels
    std::vector<float> ext(filtered ? (size_t) n * fd * 3 : 0);

    for (int kk = 0; filtered && (kk < n); kk++)
    {
        for (int q = 0; q < fd; q++)
        {
            double t = start[kk + 1] + q - 0.5;

            for (int d = 0; d < 3; d++) {
                ext[((size_t) kk * fd + q) * 3 + d] = (float) (levelHz * pulseAt(t - centreAt(kk + d - 1)) * radPerHz);
            }
        }
    }

    // Phase steps of the extension of symbol kk's trajectory (samples start[kk + 1] .. + fd)
    // for the levels lPrev, lCur, lNext of symbols kk - 1, kk, kk + 1
    auto extensionStep = [&](int kk, int q, float lPrev, float lCur, float lNext) -> float {
        const float *e = &ext[((size_t) kk * fd + q) * 3];
        return refStep[start[kk + 1] + q - base] + (lPrev - baseAt(kk - 1)) * e[0] + (lCur - baseAt(kk)) * e[1] + (lNext - baseAt(kk + 1)) * e[2];
    };

    // Reference trajectory (first pass decisions, phase 0 at the first modelled sample) and the
    // residual frequency error of the model (the discriminator path's offset is not exact): the
    // rotation of the received samples against the reference over one symbol from the lag sps
    // autocorrelation of their product, then over eight symbols for precision
    std::vector<std::complex<float>> ref(nbSamples);
    auto buildReference = [&]() {
        float psi = 0.0f;

        for (int i = 0; i < nbSamples; i++)
        {
            psi += refStep[i];
            ref[i] = std::polar(1.0f, psi);
        }
    };

    buildReference();

    for (int lag = m_sps; lag <= 8 * m_sps; lag *= 8)
    {
        if (start[n] - start[0] < 4 * lag) {
            break;
        }

        std::complex<double> acc(0.0, 0.0);

        for (int m = start[0] + lag; m < start[n]; m++)
        {
            int i = m - base;
            std::complex<float> u = samples[m] * std::conj(ref[i]);
            std::complex<float> v = samples[m - lag] * std::conj(ref[i - lag]);
            acc += std::complex<double>(u * std::conj(v));
        }

        float stepCorrection = (float) (std::arg(acc) / lag);

        for (int i = 0; i < nbSamples; i++)
        {
            refStep[i] += stepCorrection;
            farStep[i] += stepCorrection;
        }

        buildReference();
        m_lastFrequencyHz += stepCorrection * m_sampleRate / (2.0f * (float) M_PI);
    }

    // Initial phase from the first symbols with the first pass decisions
    std::complex<float> acc(0.0f, 0.0f);
    const int training = std::min(n, 8);

    for (int m = start[0]; m < start[training]; m++) {
        acc += samples[m] * std::conj(ref[m - base]);
    }

    float phase0 = std::arg(acc);           // offset of the received phasors from the reference
    float psiAtStart = 0.0f;                // phase of the reference just before the burst

    for (int i = 0; i < pre; i++) {
        psiAtStart += refStep[i];
    }

    const int nbStates = 16;
    std::vector<uint8_t> decided(n);
    std::vector<float> stateMetrics((size_t) n * nbStates);
    std::vector<float> pathPhase(n);     // phase of the best path at the start of each symbol
    float noise = 0.0f;
    std::vector<std::complex<float>> hf;

    // The Viterbi search, shared by the two stages: with a single unit tap it is the correlation
    // of the samples with the predicted unit phasor; with a channel it is the distance to the
    // channel filtered prediction, each survivor keeping the phasors of its own recent past.
    // The kernel h applies to the trajectory led by kernelLead samples: the prediction of sample
    // m is sum_l h[l] s[m - l + kernelLead], so a kernel that includes the channel filter (whose
    // response is symmetric about its delay) reaches into the trajectory past the interval, which
    // the branch extends from the symbols it pins. With filtered samples the metric is taken on
    // every stride-th sample: the filtered signal and noise are oversampled and the kernel is long.
    auto search = [&](const std::vector<std::complex<float>>& h, int kernelLead, int stride, float initialPhase) {
        const int nbTaps = (int) h.size();
        const int reach = std::max(0, kernelLead);          // samples past the interval the prediction needs
        const int histLen = std::max(0, nbTaps - 1 - kernelLead);   // phasors before the interval it needs
        std::vector<float> metric(nbStates, 0.0f), phase(nbStates, initialPhase);
        std::vector<float> newMetric(nbStates), newPhase(nbStates);
        // Each survivor also carries the amplitude of the channel relative to the fit (1 at the
        // start): the distance metric weighs the predicted envelope by it, so a fade within the
        // burst does not turn the metric towards the sequences with the smallest envelope
        std::vector<float> amp(nbStates, 1.0f), newAmp(nbStates);
        std::vector<float> winCorrRe(nbStates), winCorrIm(nbStates), winPredEnergy(nbStates);   // the winning branch's sums, for the phase and amplitude updates
        // Survivor history: the histLen phasors before the interval, oldest first
        std::vector<float> histRe((size_t) nbStates * histLen), histIm((size_t) nbStates * histLen);
        std::vector<float> newHistRe((size_t) nbStates * histLen), newHistIm((size_t) nbStates * histLen);
        std::vector<uint8_t> predecessor((size_t) n * nbStates);
        std::vector<float> startPhase((size_t) n * nbStates);
        // The kernel reversed, as real and imaginary parts, so that a prediction is the dot
        // product of the kernel with a contiguous window of the trajectory
        std::vector<float> hRe(nbTaps), hIm(nbTaps);
        const float *cosSin = phaseTable().m_cosSin.data();

        for (int l = 0; l < nbTaps; l++)
        {
            hRe[l] = h[nbTaps - 1 - l].real();
            hIm[l] = h[nbTaps - 1 - l].imag();
        }

        // Trajectory of a branch: the survivor's history then the interval and its extension
        std::vector<float> trajRe(histLen + m_sps + 4 + reach), trajIm(trajRe.size());

        for (int s = 0; s < nbStates; s++)
        {
            for (int l = 0; l < histLen; l++)
            {
                std::complex<float> v = ref[pre - 1 - l] * std::polar(1.0f, phase0);
                histRe[(size_t) s * histLen + histLen - 1 - l] = v.real();
                histIm[(size_t) s * histLen + histLen - 1 - l] = v.imag();
            }
        }

        for (int k = 0; k < n; k++)
        {
            std::fill(newMetric.begin(), newMetric.end(), -1e30f);
            const int length = start[k + 1] - start[k];

            if ((int) trajRe.size() < histLen + length + reach)
            {
                trajRe.resize(histLen + length + reach);
                trajIm.resize(histLen + length + reach);
            }

            for (int s = 0; s < nbStates; s++)
            {
                float lPrev = levelOf[s >> 2];
                float lCur = levelOf[s & 3];
                std::copy(histRe.begin() + (size_t) s * histLen, histRe.begin() + (size_t) (s + 1) * histLen, trajRe.begin());
                std::copy(histIm.begin() + (size_t) s * histLen, histIm.begin() + (size_t) (s + 1) * histLen, trajIm.begin());

                for (int next = 0; next < 4; next++)
                {
                    float lNext = levelOf[next];
                    float ps = phase[s];

                    for (int q = 0; q < length; q++)
                    {
                        int i = start[k] + q - base;
                        ps += farStep[i] + lPrev * near[3 * i] + lCur * near[3 * i + 1] + lNext * near[3 * i + 2];
                        const float *cs = cosSin + 2 * phaseIndex(ps);
                        trajRe[histLen + q] = cs[0];
                        trajIm[histLen + q] = cs[1];
                    }

                    const float psEnd = ps;

                    for (int q = 0; q < reach; q++)
                    {
                        ps += extensionStep(k, q, lPrev, lCur, lNext);
                        const float *cs = cosSin + 2 * phaseIndex(ps);
                        trajRe[histLen + length + q] = cs[0];
                        trajIm[histLen + length + q] = cs[1];
                    }

                    float gain = 0.0f;
                    float corrRe = 0.0f, corrIm = 0.0f, predEnergy = 0.0f;

                    if (nbTaps == 1)
                    {
                        for (int q = 0; q < length; q++)
                        {
                            const std::complex<float>& z = samples[start[k] + q];
                            float pr = trajRe[histLen + q], pi = trajIm[histLen + q];
                            // z * conj(pred)
                            float cr = z.real() * pr + z.imag() * pi;
                            float ci = z.imag() * pr - z.real() * pi;
                            gain += cr;
                            corrRe += cr;
                            corrIm += ci;
                        }
                    }
                    else
                    {
                        for (int q = stride / 2; q < length; q += stride)
                        {
                            // Prediction: the reversed kernel over trajectory samples q .. q + nbTaps - 1
                            // (trajectory index histLen + q - l + kernelLead for kernel index l)
                            const float *tr = &trajRe[q];
                            const float *ti = &trajIm[q];
                            float pr = 0.0f, pi = 0.0f;

                            for (int l = 0; l < nbTaps; l++)
                            {
                                pr += hRe[l] * tr[l] - hIm[l] * ti[l];
                                pi += hRe[l] * ti[l] + hIm[l] * tr[l];
                            }

                            const std::complex<float>& z = samples[start[k] + q];
                            corrRe += z.real() * pr + z.imag() * pi;
                            corrIm += z.imag() * pr - z.real() * pi;
                            predEnergy += pr * pr + pi * pi;
                        }

                        // -|z - a pred|^2 summed, less the |z|^2 common to every branch
                        const float a = amp[s];
                        gain = 2.0f * a * corrRe - a * a * predEnergy;
                    }

                    float candidate = metric[s] + gain;
                    int t = ((s & 3) << 2) | next;

                    if (candidate > newMetric[t])
                    {
                        newMetric[t] = candidate;
                        newPhase[t] = psEnd;
                        newAmp[t] = amp[s];
                        winCorrRe[t] = corrRe;
                        winCorrIm[t] = corrIm;
                        winPredEnergy[t] = predEnergy;
                        predecessor[(size_t) k * nbStates + t] = (uint8_t) s;
                        startPhase[(size_t) k * nbStates + t] = phase[s];
                        // The new survivor's history: the last histLen phasors before the next interval
                        std::copy(trajRe.begin() + length, trajRe.begin() + length + histLen, newHistRe.begin() + (size_t) t * histLen);
                        std::copy(trajIm.begin() + length, trajIm.begin() + length + histLen, newHistIm.begin() + (size_t) t * histLen);
                    }
                }
            }

            float best = *std::max_element(newMetric.begin(), newMetric.end());

            for (int t = 0; t < nbStates; t++)
            {
                newMetric[t] -= best;
                stateMetrics[(size_t) k * nbStates + t] = newMetric[t];
                // The survivor's phase and amplitude, corrected by a fraction of the errors it measured
                newPhase[t] = std::remainder(newPhase[t] + m_loopGain * std::atan2(winCorrIm[t], winCorrRe[t]), 2.0f * (float) M_PI);

                if (winPredEnergy[t] > 0.0f)
                {
                    float measured = std::max(0.2f, std::min(3.0f, winCorrRe[t] / winPredEnergy[t]));
                    newAmp[t] += m_loopGain * (measured - newAmp[t]);
                }
            }

            metric.swap(newMetric);
            phase.swap(newPhase);
            amp.swap(newAmp);
            histRe.swap(newHistRe);
            histIm.swap(newHistIm);
        }

        int state = (int) (std::max_element(metric.begin(), metric.end()) - metric.begin());

        for (int k = n - 1; k >= 0; k--)
        {
            decided[k] = (uint8_t) (state >> 2); // state after symbol k is (L[k], L[k+1])
            pathPhase[k] = startPhase[(size_t) k * nbStates + state];
            state = predecessor[(size_t) k * nbStates + state];
        }
    };

    // Stage 1: single tap, the carrier followed by the survivors
    search(std::vector<std::complex<float>>(1, std::complex<float>(1.0f, 0.0f)), 0, 1, phase0 + psiAtStart);

    // The best path's phasors, carrier included, are the reference of a least squares channel
    // estimate z[m] = sum_l h[l] s[m - l]: an echo within the channel memory shows as energy
    // outside the main tap, in which case the burst is searched again through that channel
    // (stage 2), with the stage 1 decisions as the pulse tails
    std::vector<std::complex<float>> best(nbSamples);

    for (int i = 0; i < pre; i++) {
        best[i] = ref[i] * std::polar(1.0f, phase0);
    }

    for (int k = 0; k < n; k++)
    {
        float ps = pathPhase[k];
        float lPrev = (k > 0) ? levelOf[decided[k - 1]] : levelAt(-1);
        float lCur = levelOf[decided[k]];
        float lNext = (k + 1 < n) ? levelOf[decided[k + 1]] : levelAt(n);

        for (int m = start[k]; m < start[k + 1]; m++)
        {
            int i = m - base;
            ps += farStep[i] + lPrev * near[3 * i] + lCur * near[3 * i + 1] + lNext * near[3 * i + 2];
            best[i] = std::polar(1.0f, ps);
        }

        if ((k == n - 1) && (lead > 0))
        {
            for (int q = 0; q < lead; q++)
            {
                ps += extensionStep(k, q, lPrev, lCur, lNext);
                best[start[n] + q - base] = std::polar(1.0f, ps);
            }
        }
    }

    // With the filter in the model the echo channel is fitted on the filtered reference
    std::vector<std::complex<float>> regress;

    if (filtered)
    {
        regress.assign(nbSamples, std::complex<float>(0.0f, 0.0f));

        for (int i = pre - taps - fd; i < start[n] - base; i++)
        {
            std::complex<float> acc(0.0f, 0.0f);

            for (int j = 0; j <= 2 * fd; j++) {
                acc += m_filter[j] * best[i - j + fd];
            }

            regress[i] = acc;
        }
    }

    const std::vector<std::complex<float>>& reference = filtered ? regress : best;

    // Normal equations. R[a][b] = sum_m conj(s[m - a]) s[m - b] over the burst is the
    // autocorrelation of the reference at lag b - a but for the few samples at the ends of the
    // burst, so it is taken as such: taps lags per sample instead of taps squared products
    std::vector<std::complex<double>> R((size_t) taps * taps, 0.0), r(taps, 0.0);
    {
        std::vector<float> accRe(taps, 0.0f), accIm(taps, 0.0f);
        std::vector<double> rRe(taps, 0.0), rIm(taps, 0.0);
        const float *refRe = reinterpret_cast<const float *>(reference.data()); // interleaved re, im

        for (int m = start[0]; m < start[n]; m++)
        {
            int i = m - base;
            const float sr = reference[i].real(), si = reference[i].imag();
            const float *w = refRe + 2 * (i - taps + 1);   // reference[i - taps + 1 ..]

            // conj(s[m]) s[m - d] for d = 0 .. taps - 1, reference[i - d] = w[2 (taps - 1 - d)]
            for (int d = 0; d < taps; d++)
            {
                const float xr = w[2 * (taps - 1 - d)], xi = w[2 * (taps - 1 - d) + 1];
                accRe[d] += sr * xr + si * xi;
                accIm[d] += sr * xi - si * xr;
            }

            // r[a] += conj(s[i - a]) z, the complex arithmetic written out (std::complex<double>'s
            // operators were not inlined: the same products and sums, so the same result)
            const double zr = samples[m].real(), zi = samples[m].imag();

            for (int a = 0; a < taps; a++)
            {
                const double xr = reference[i - a].real(), xi = reference[i - a].imag();
                rRe[a] += xr * zr + xi * zi;
                rIm[a] += xr * zi - xi * zr;
            }
        }

        for (int a = 0; a < taps; a++) {
            r[a] = std::complex<double>(rRe[a], rIm[a]);
        }

        for (int a = 0; a < taps; a++)
        {
            for (int b = 0; b < taps; b++)
            {
                // sum_m conj(s[m - a]) s[m - b] = sum conj(s[m']) s[m' + a - b]: the conjugate of
                // acc[a - b] (acc[d] = sum conj(s[m]) s[m - d]), acc[b - a] when b > a
                int d = a - b;
                std::complex<double> c = (d >= 0) ? std::conj(std::complex<double>(accRe[d], accIm[d])) : std::complex<double>(accRe[-d], accIm[-d]);
                R[(size_t) a * taps + b] = c;
            }
        }
    }

    double trace = 0.0;

    for (int a = 0; a < taps; a++) {
        trace += R[(size_t) a * taps + a].real();
    }

    for (int a = 0; a < taps; a++) {
        R[(size_t) a * taps + a] += 1e-3 * trace / taps;
    }

    std::vector<std::complex<double>> h(taps, 0.0);
    bool solved = true;
    {
        std::vector<std::complex<double>> A = R, y = r;

        for (int c = 0; c < taps && solved; c++)
        {
            int pivot = c;

            for (int a = c + 1; a < taps; a++)
            {
                if (std::abs(A[(size_t) a * taps + c]) > std::abs(A[(size_t) pivot * taps + c])) {
                    pivot = a;
                }
            }

            if (std::abs(A[(size_t) pivot * taps + c]) < 1e-12)
            {
                solved = false;
                break;
            }

            if (pivot != c)
            {
                for (int b = 0; b < taps; b++) {
                    std::swap(A[(size_t) c * taps + b], A[(size_t) pivot * taps + b]);
                }

                std::swap(y[c], y[pivot]);
            }

            for (int a = c + 1; a < taps; a++)
            {
                std::complex<double> f = A[(size_t) a * taps + c] / A[(size_t) c * taps + c];

                for (int b = c; b < taps; b++) {
                    A[(size_t) a * taps + b] -= f * A[(size_t) c * taps + b];
                }

                y[a] -= f * y[c];
            }
        }

        for (int c = taps - 1; solved && c >= 0; c--)
        {
            std::complex<double> acc2 = y[c];

            for (int b = c + 1; b < taps; b++) {
                acc2 -= A[(size_t) c * taps + b] * h[b];
            }

            h[c] = acc2 / A[(size_t) c * taps + c];
        }
    }

    int mainTap = 0;
    double energy = 0.0;

    for (int l = 0; l < taps; l++)
    {
        energy += std::norm(h[l]);

        if (std::norm(h[l]) > std::norm(h[mainTap])) {
            mainTap = l;
        }
    }

    double residual = 0.0;

    for (int m = start[0]; m < start[n]; m++)
    {
        int i = m - base;
        double pr = 0.0, pi = 0.0;   // sum h[l] s[i - l], written out as above

        for (int l = 0; l < taps; l++)
        {
            const double hr = h[l].real(), hi = h[l].imag(), xr = reference[i - l].real(), xi = reference[i - l].imag();
            pr += hr * xr - hi * xi;
            pi += hr * xi + hi * xr;
        }

        const double er = samples[m].real() - pr, ei = samples[m].imag() - pi;
        residual += er * er + ei * ei;
    }

    noise = (float) std::max(1e-9 * std::max(energy, 1e-30), residual / (start[n] - start[0]));
    m_lastNoise = noise;
    m_lastEnergy = (float) energy;
    // Dispersive when the full channel explains the samples markedly better than the main tap
    // and its neighbours alone (the lags of a constant envelope signal are correlated, so the
    // least squares spreads a little energy over every tap whatever the channel; an echo lowers
    // the residual). The short fit reuses the normal equations.
    double residualShort = residual;
    const int lo = std::max(0, mainTap - 1), hi = std::min(taps - 1, mainTap + 1);
    const int p = hi - lo + 1;
    std::complex<double> hs[3] = { 0.0, 0.0, 0.0 };
    bool shortOk = false;
    {
        std::complex<double> A[3][3], y[3];

        for (int a = 0; a < p; a++)
        {
            y[a] = r[lo + a];

            for (int b = 0; b < p; b++) {
                A[a][b] = R[(size_t) (lo + a) * taps + lo + b];
            }
        }

        bool ok = true;

        for (int c = 0; c < p && ok; c++)
        {
            if (std::abs(A[c][c]) < 1e-12) { ok = false; break; }

            for (int a = c + 1; a < p; a++)
            {
                std::complex<double> f = A[a][c] / A[c][c];

                for (int b = c; b < p; b++) {
                    A[a][b] -= f * A[c][b];
                }

                y[a] -= f * y[c];
            }
        }

        for (int c = p - 1; ok && c >= 0; c--)
        {
            std::complex<double> acc2 = y[c];

            for (int b = c + 1; b < p; b++) {
                acc2 -= A[c][b] * hs[b];
            }

            hs[c] = acc2 / A[c][c];
        }

        shortOk = ok;

        if (ok)
        {
            residualShort = 0.0;

            for (int m = start[0]; m < start[n]; m++)
            {
                int i = m - base;
                double pr = 0.0, pi = 0.0;

                for (int a = 0; a < p; a++)
                {
                    const double hr = hs[a].real(), hi = hs[a].imag(), xr = reference[i - lo - a].real(), xi = reference[i - lo - a].imag();
                    pr += hr * xr - hi * xi;
                    pi += hr * xi + hi * xr;
                }

                const double er = samples[m].real() - pr, ei = samples[m].imag() - pi;
                residualShort += er * er + ei * ei;
            }
        }
    }

    m_dispersive = solved && (energy > 0.0) && (residualShort > (1.0 + m_dispersionThreshold) * residual);

    if (getenv("DSD_COH_DEBUG"))
    {
        fprintf(stderr, "coherent fit: energy %.4g noise %.4g short/full residual %.2f main tap %d taps", energy, noise, residualShort / std::max(residual, 1e-30), mainTap);

        for (int l = 0; l < taps; l++) {
            fprintf(stderr, " %.3f", std::abs(h[l]) / std::sqrt(std::max(energy, 1e-30)));
        }

        fprintf(stderr, "%c", 10);
    }

    float scale;

    if (m_dispersive || filtered)
    {
        // Stage 2 through the channel: the phase of the main tap is the survivors' start, the
        // stage 1 decisions the pulse tails. With the filter in the model the second stage
        // always runs (the first stage's correlation ignores the filter): its kernel is the
        // echo channel, or the main taps alone when not dispersive, through the filter
        std::vector<std::complex<float>> echo(taps, std::complex<float>(0.0f, 0.0f));

        if (m_dispersive || !shortOk)
        {
            for (int l = 0; l < taps; l++) {
                echo[l] = std::complex<float>(h[l]);
            }
        }
        else
        {
            for (int a = 0; a < p; a++) {
                echo[lo + a] = std::complex<float>(hs[a]);
            }
        }

        int kernelLead = 0;
        int stride = 1;

        if (filtered)
        {
            // The echo channel through the filter, trimmed to where it is above 3% of its peak
            // (the filter's far tails), the trimming at the front reducing the lead
            std::vector<std::complex<float>> full(taps + 2 * fd, std::complex<float>(0.0f, 0.0f));
            float peak = 0.0f;

            for (int l = 0; l < taps; l++)
            {
                for (int j = 0; j <= 2 * fd; j++) {
                    full[l + j] += echo[l] * m_filter[j];
                }
            }

            for (const std::complex<float>& v : full) {
                peak = std::max(peak, std::abs(v));
            }

            int first = 0, last = (int) full.size() - 1;

            while ((first < last) && (std::abs(full[first]) < 0.03f * peak)) {
                first++;
            }

            while ((last > first) && (std::abs(full[last]) < 0.03f * peak)) {
                last--;
            }

            hf.assign(full.begin() + first, full.begin() + last + 1);
            kernelLead = fd - first;
            // The filtered signal is oversampled: a quarter of the symbol is enough of a step
            stride = std::max(1, m_sps / 4);
        }
        else
        {
            hf = echo;
        }

        std::vector<int8_t> tails(n);

        for (int k = 0; k < n; k++) {
            tails[k] = levelOf[decided[k]];
        }

        // The far and reference steps were built from the first pass: move them to the stage 1
        // decisions
        int kk = 0;

        for (int m = base; m < start[n] + lead; m++)
        {
            if (m < start[0]) {
                kk = -1 - (start[0] - 1 - m) / m_sps;
            }
            else
            {
                kk = std::max(kk, 0);

                while ((kk + 1 < n) && (m >= start[kk + 1])) {
                    kk++;
                }
            }

            double t = m - 0.5;

            for (int j = kk - 4; j <= kk + 4; j++)
            {
                if ((j >= 0) && (j < n) && (tails[j] != firstPass[j]))
                {
                    float delta = (float) (levelHz * (tails[j] - firstPass[j]) * pulseAt(t - centreAt(j)) * radPerHz);
                    refStep[m - base] += delta;

                    if (std::abs(j - kk) >= 2) {
                        farStep[m - base] += delta;
                    }
                }
            }
        }

        baseLevels = tails;

        if (getenv("DSD_COH_DEBUG")) {
            fprintf(stderr, "coherent stage 2: kernel %d taps lead %d stride %d%c", (int) hf.size(), kernelLead, stride, 10);
        }

        search(hf, kernelLead, stride, phase0 + psiAtStart);
        scale = (float) stride / noise;     // the metric sums every stride-th sample
    }
    else
    {
        // Single tap: the correlation metric in units of amplitude; N0 the noise power
        scale = 2.0f * (float) std::sqrt(std::max(energy, 1e-30)) / noise;
    }

    results.resize(n);

    for (int k = 0; k < n; k++)
    {
        float bestBit[2][2] = { { -1e30f, -1e30f }, { -1e30f, -1e30f } }; // [bit][value]

        for (int t = 0; t < nbStates; t++)
        {
            int dibit = t >> 2; // L[k] of the state after symbol k
            float mt = stateMetrics[(size_t) k * nbStates + t];
            bestBit[0][(dibit >> 1) & 1] = std::max(bestBit[0][(dibit >> 1) & 1], mt);
            bestBit[1][dibit & 1] = std::max(bestBit[1][dibit & 1], mt);
        }

        Result& r2 = results[k];
        r2.m_dibit = decided[k];
        r2.m_llrMsb = std::max(-255.0f, std::min(255.0f, scale * (bestBit[0][0] - bestBit[0][1])));
        r2.m_llrLsb = std::max(-255.0f, std::min(255.0f, scale * (bestBit[1][0] - bestBit[1][1])));

        // The forward metrics may disagree with the traceback on paths not yet merged: the
        // traceback decision wins
        if (((r2.m_llrMsb < 0.0f) != (((decided[k] >> 1) & 1) != 0))) {
            r2.m_llrMsb = -r2.m_llrMsb;
        }

        if (((r2.m_llrLsb < 0.0f) != ((decided[k] & 1) != 0))) {
            r2.m_llrLsb = -r2.m_llrLsb;
        }

        float minLlr = std::min(std::fabs(r2.m_llrMsb), std::fabs(r2.m_llrLsb));
        r2.m_reliability = std::min(1.0f, minLlr / 20.0f);
    }

    return true;
}
