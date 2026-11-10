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

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dsdangel61receiver.h"

DSDAngel61Receiver::DSDAngel61Receiver(DVAngel61::Profile profile) :
    m_layout(&layout(profile)),
    m_sps(m_layout->m_samplesPerSymbol),
    m_frameSymbols(m_layout->m_frameSymbols),
    m_frameSamples(m_layout->frameSamples()),
    m_uwSpan((UWSymbols - 1) * m_sps),
    m_extendedSymbols(m_frameSymbols + UWSymbols),
    // The frame and the next UW, the timing's probes and retake (a fifth of a symbol, and 1.5 of
    // those) and the cubic interpolation's two samples beyond: with 3 alone the newest sample or two
    // read were still the ring's 8192 samples before
    m_decodeLookahead(m_frameSamples + m_uwSpan + 3 + (int) std::ceil(1.5 * m_sps / 5.0)),
    m_fitDopplers(fitGrid(profile, m_nbFitCandidates)),
    m_delay(m_layout->m_rrcTaps),
    m_mixedRe(2 * m_layout->m_rrcTaps),
    m_mixedIm(2 * m_layout->m_rrcTaps),
    m_ring(RingSize),
    m_metric(RingSize),
    m_searchThreshold(0.62f),
    m_lockedThreshold(0.45f),
    m_hypotheses(3),
    m_decisionFeedback(true),
    m_windowOnly(getenv("DSD_ANGEL61_WINDOW") != nullptr),
    m_amplitudeAware(getenv("DSD_VHF_AMPLITUDE") && (atoi(getenv("DSD_VHF_AMPLITUDE")) != 0)),
    m_debug(false),
    m_scopeEnabled(false),
    m_llr(MaxCodedBits)
{
    // VHF's pulse (2.1): the root raised cosine of NB's or WB's symbol rate
    DVAngel61::rrcTaps(m_layout->m_samplesPerSymbol, m_rrc);

    for (int t = 0; t < m_layout->m_rrcTaps; t++) {
        m_reversedRrc[t] = m_rrc[m_layout->m_rrcTaps - 1 - t];
    }

    setFrequencyHypotheses(m_hypotheses);
    reset();
}

void DSDAngel61Receiver::setFrequencyHypotheses(int n)
{
    // Per hypothesis, the de-rotation of the 16 UW symbols
    m_hypotheses = std::max(0, std::min(6, n));
    m_hypothesisRotation.resize((2 * m_hypotheses + 1) * UWSymbols);

    for (int h = -m_hypotheses; h <= m_hypotheses; h++)
    {
        float step = -2.0f * (float) M_PI * h * HypothesisStepHz * m_sps / SampleRate;

        for (int k = 0; k < UWSymbols; k++) {
            m_hypothesisRotation[(h + m_hypotheses) * UWSymbols + k] = std::complex<float>(std::cos(step * k), std::sin(step * k));
        }
    }
}

void DSDAngel61Receiver::centreFilter(float hz)
{
    // The passband follows the signal (the AFC of the front end is not in this path when the
    // receiver runs alone): the RRC rotated to the offset, h(t) exp(j w t), which passes
    // exp(j w n) as the RRC passes DC. It is worked as the input mixed down by w, the real RRC,
    // and the output mixed back up: sum x(n-t) h(t) exp(j w t) = exp(j w n) sum x(n-t)
    // exp(-j w (n-t)) h(t). Half the multiplications, and the RRC's taps are real, the case of
    // a filter centred on zero. The delay line is mixed again for the new centre, so a change
    // mid stream is as the rotated taps had it: the filter's shape changes, the phase does not step
    m_filterCentreHz = hz;
    const double step = 2.0 * M_PI * hz / SampleRate;
    const int taps = m_layout->m_rrcTaps;
    m_mixerStep = std::polar(1.0, -step);
    m_mixer = std::complex<double>(1.0, 0.0);   // the next sample's; the sample `age` back is exp(j w age)
    m_mixerCount = 0;

    for (int c = 0; c < taps; c++)
    {
        // The next sample is written at m_delayIndex, over the one taps samples back
        int age = (m_delayIndex - c + taps) % taps;
        age = age == 0 ? taps : age;
        std::complex<float> mixed = m_delay[c] * std::complex<float>(std::polar(1.0, step * age));
        m_mixedRe[c] = m_mixedRe[c + taps] = mixed.real();
        m_mixedIm[c] = m_mixedIm[c + taps] = mixed.imag();
    }
}

void DSDAngel61Receiver::reset()
{
    std::fill(m_delay.begin(), m_delay.end(), std::complex<float>(0.0f, 0.0f));
    std::fill(m_mixedRe.begin(), m_mixedRe.end(), 0.0f);
    std::fill(m_mixedIm.begin(), m_mixedIm.end(), 0.0f);
    std::fill(m_ring.begin(), m_ring.end(), std::complex<float>(0.0f, 0.0f));
    std::fill(m_metric.begin(), m_metric.end(), 0.0f);
    m_delayIndex = 0;
    m_sampleIndex = 0;
    m_filteredCount = 0;
    centreFilter(0.0f);
    m_state = StateSearch;
    m_pending.clear();
    m_expectedStart = 0.0;
    m_trackPosition = -1.0;
    m_trackMetric = 0.0f;
    m_timingDrift = 0.0;
    m_framesLocked = 0;
    m_timingCorrection = 0.0;
    m_lastFrameSeen = false;
    m_lastFrameClaimed = false;
    m_haveLastHeader = false;
    m_nextUwValid = false;
    m_frequencyHz = 0.0f;
    m_noiseVariance = 0.1f;
    m_gainVariation = 0.0f;
    m_dopplerHz = 0.0f;
    m_fitDopplerHz = 0.0f;
    std::fill(m_fitError, m_fitError + MaxExtendedSymbols, 0.0f);
    m_status = Status();
    m_frames.clear();
    m_scope.clear();
}

std::vector<DSDAngel61Receiver::Frame> DSDAngel61Receiver::takeFrames()
{
    std::vector<Frame> frames;
    frames.swap(m_frames);
    return frames;
}

std::vector<std::complex<float>> DSDAngel61Receiver::takeScopeSymbols()
{
    std::vector<std::complex<float>> symbols;
    symbols.swap(m_scope);
    return symbols;
}

void DSDAngel61Receiver::skip(int n)
{
    // The samples are not seen: the ring gets zeros so that a UW metric over the gap is nothing,
    // the delay line is cleared so that the filter restarts cleanly, and any lock is over
    if (m_state == StateLocked) {
        unlock();
    }

    m_pending.clear();

    for (int i = 0; i < n; i++)
    {
        m_ring[m_filteredCount & RingMask] = std::complex<float>(0.0f, 0.0f);
        m_metric[m_filteredCount & RingMask] = 0.0f;
        m_filteredCount++;
        m_sampleIndex++;
    }

    std::fill(m_delay.begin(), m_delay.end(), std::complex<float>(0.0f, 0.0f));
    std::fill(m_mixedRe.begin(), m_mixedRe.end(), 0.0f);
    std::fill(m_mixedIm.begin(), m_mixedIm.end(), 0.0f);
}

// ---- Sample processing -------------------------------------------------------------------------

void DSDAngel61Receiver::process(const std::complex<float> *iq, int n)
{
    const int taps = m_layout->m_rrcTaps;

    for (int i = 0; i < n; i++)
    {
        // Matched filter (centreFilter): the input mixed down to the filter's centre, twice into
        // a line twice the filter's length so that the last `taps` samples are always in a row
        const bool centred = m_filterCentreHz != 0.0f;
        m_delay[m_delayIndex] = iq[i];
        std::complex<float> mixed = centred ? iq[i] * std::complex<float>(m_mixer) : iq[i];
        m_mixedRe[m_delayIndex] = m_mixedRe[m_delayIndex + taps] = mixed.real();
        m_mixedIm[m_delayIndex] = m_mixedIm[m_delayIndex + taps] = mixed.imag();
        const float *re = m_mixedRe.data() + m_delayIndex + 1;   // oldest to newest
        const float *im = m_mixedIm.data() + m_delayIndex + 1;
        float r0 = 0.0f, r1 = 0.0f, r2 = 0.0f, r3 = 0.0f, i0 = 0.0f, i1 = 0.0f, i2 = 0.0f, i3 = 0.0f;
        int t = 0;

        for (; t + 4 <= taps; t += 4)
        {
            r0 += re[t] * m_reversedRrc[t];
            r1 += re[t + 1] * m_reversedRrc[t + 1];
            r2 += re[t + 2] * m_reversedRrc[t + 2];
            r3 += re[t + 3] * m_reversedRrc[t + 3];
            i0 += im[t] * m_reversedRrc[t];
            i1 += im[t + 1] * m_reversedRrc[t + 1];
            i2 += im[t + 2] * m_reversedRrc[t + 2];
            i3 += im[t + 3] * m_reversedRrc[t + 3];
        }

        for (; t < taps; t++)
        {
            r0 += re[t] * m_reversedRrc[t];
            i0 += im[t] * m_reversedRrc[t];
        }

        std::complex<float> acc((r0 + r1) + (r2 + r3), (i0 + i1) + (i2 + i3));

        if (centred)
        {
            acc *= std::conj(std::complex<float>(m_mixer));
            m_mixer *= m_mixerStep;

            if (++m_mixerCount >= 1024)
            {
                m_mixer /= std::abs(m_mixer);
                m_mixerCount = 0;
            }
        }

        m_delayIndex = m_delayIndex == taps - 1 ? 0 : m_delayIndex + 1;
        m_ring[m_filteredCount & RingMask] = acc;
        m_filteredCount++;
        m_sampleIndex++;

        // The UW metric for the start whose last UW symbol has just arrived
        int64_t start = m_filteredCount - 1 - m_uwSpan;

        // Every start's metric while searching; while locked only those about the UW expected
        // and the one after it (the frame is decoded after the next UW is in, by when the
        // expectation has not moved on yet), or all of them while no payload has confirmed the
        // lock (rescanUnconfirmed). unlock() works out the recent ones the search needs
        if (start >= 0)
        {
            const bool wanted = (m_state == StateSearch) || !m_status.m_confirmed
                || (std::fabs((double) start - m_expectedStart) <= LockedMetricWindow)
                || (std::fabs((double) start - m_expectedStart - m_frameSamples) <= LockedMetricWindow);
            m_metric[start & RingMask] = wanted ? uwMetric(start, nullptr) : 0.0f;
        }

        if (m_state == StateSearch) {
            search();
        } else {
            track();
        }
    }
}

std::complex<float> DSDAngel61Receiver::interpolate(double position) const
{
    // Four point cubic (Lagrange) between the filtered samples
    int64_t base = (long long) std::floor(position);
    float mu = (float) (position - base);
    const std::complex<float>& y0 = filtered(base - 1);
    const std::complex<float>& y1 = filtered(base);
    const std::complex<float>& y2 = filtered(base + 1);
    const std::complex<float>& y3 = filtered(base + 2);
    float c0 = -mu * (mu - 1.0f) * (mu - 2.0f) / 6.0f;
    float c1 = (mu + 1.0f) * (mu - 1.0f) * (mu - 2.0f) / 2.0f;
    float c2 = -(mu + 1.0f) * mu * (mu - 2.0f) / 2.0f;
    float c3 = (mu + 1.0f) * mu * (mu - 1.0f) / 6.0f;
    return y0 * c0 + y1 * c1 + y2 * c2 + y3 * c3;
}

// ---- Unique word -------------------------------------------------------------------------------

float DSDAngel61Receiver::uwMetric(int64_t start, float *frequency) const
{
    // Four 4 symbol segments correlated coherently, their magnitudes summed: a frequency offset
    // that would turn the whole 16 symbol word through a circle only turns a segment by a quarter.
    // Hypotheses at 0 and +-1 kHz extend the range beyond the segments' own +-600 Hz.
    std::complex<float> y[UWSymbols];
    float energy = 0.0f;

    for (int k = 0; k < UWSymbols; k++)
    {
        y[k] = filtered(start + k * m_sps) * std::conj(m_layout->uwSymbol(k));
        energy += std::norm(y[k]);
    }

    if (energy <= 0.0f) {
        return 0.0f;
    }

    float best = 0.0f;
    float bestFrequency = 0.0f;

    for (int h = -m_hypotheses; h <= m_hypotheses; h++)
    {
        // Hypotheses HypothesisStepHz apart: the residual a segment sees is at most half that
        // (a quarter turn over its 4 symbols at 250 Hz, 0.8 dB of correlation loss)
        float hypothesisHz = h * HypothesisStepHz;
        const std::complex<float> *rotation = m_hypothesisRotation.data() + (h + m_hypotheses) * UWSymbols;
        std::complex<float> segment[4];

        for (int s = 0; s < 4; s++)
        {
            std::complex<float> acc(0.0f, 0.0f);

            for (int k = 0; k < 4; k++) {
                acc += y[4 * s + k] * rotation[4 * s + k];
            }

            segment[s] = acc;
        }

        float sum = std::sqrt(std::norm(segment[0])) + std::sqrt(std::norm(segment[1])) + std::sqrt(std::norm(segment[2])) + std::sqrt(std::norm(segment[3]));
        float metric = sum / (4.0f * std::sqrt(energy));

        if (metric > best)
        {
            best = metric;

            if (frequency)
            {
                // Phase progression between segments (40 samples apart): unambiguous to +-600 Hz
                std::complex<float> lag = segment[1] * std::conj(segment[0]) + segment[2] * std::conj(segment[1]) + segment[3] * std::conj(segment[2]);
                float dphi = std::arg(lag);
                bestFrequency = hypothesisHz + dphi * SampleRate / (2.0f * (float) M_PI * 4 * m_sps);
            }
        }
    }

    if (frequency) {
        *frequency = bestFrequency;
    }

    return best;
}

// ---- Search ------------------------------------------------------------------------------------

void DSDAngel61Receiver::search()
{
    // A candidate is a local maximum of the metric above the threshold, called when
    // PeakLookahead further metrics are known. Candidates queue up until their frames are in:
    // a chance peak just before a real UW must not hide it
    int64_t latest = m_filteredCount - 1 - m_uwSpan;   // start whose metric was just computed
    int64_t candidate = latest - PeakLookahead;

    if (candidate >= PeakLookahead)
    {
        float m = m_metric[candidate & RingMask];

        if (m > m_searchThreshold)
        {
            bool peak = true;

            for (int k = 1; k <= PeakLookahead && peak; k++) {
                peak = (m >= m_metric[(candidate - k) & RingMask]) && (m >= m_metric[(candidate + k) & RingMask]);
            }

            if (peak)
            {
                if ((int) m_pending.size() >= MaxPending)
                {
                    // Drop the weakest
                    size_t weakest = 0;
                    for (size_t i = 1; i < m_pending.size(); i++) {
                        if (m_pending[i].m_metric < m_pending[weakest].m_metric) {
                            weakest = i;
                        }
                    }
                    if (m_pending[weakest].m_metric < m) {
                        m_pending.erase(m_pending.begin() + weakest);
                    }
                }

                if ((int) m_pending.size() < MaxPending)
                {
                    m_pending.push_back({ candidate, m });
                    m_status.m_uwDetections++;
                }
            }
        }
    }

    // Decode the oldest pending candidate's frame once all of it is in
    if (!m_pending.empty() && (m_filteredCount - 1 >= m_pending.front().m_start + m_decodeLookahead))
    {
        int64_t start = m_pending.front().m_start;
        float m = m_pending.front().m_metric;
        m_pending.erase(m_pending.begin());
        // Fractional timing from the metric's parabola around the peak
        float before = m_metric[(start - 1) & RingMask];
        float after = m_metric[(start + 1) & RingMask];
        float denominator = before - 2.0f * m + after;
        double tau = (denominator < 0.0f) ? std::max(-0.5, std::min(0.5, 0.5 * (before - after) / denominator)) : 0.0;

        if (decodeFrame(start + tau, m))
        {
            m_state = StateLocked;
            m_pending.clear();
            m_lockStart = start;
            m_expectedStart = start + tau + m_timingCorrection + m_frameSamples;
            m_timingDrift = 0.0;
            m_framesLocked = 0;
            m_trackPosition = -1.0;
            m_status.m_locked = true;
            m_status.m_misses = 0;

            if (m_status.m_firstSyncSample < 0) {
                m_status.m_firstSyncSample = start;
            }

            if (m_debug) {
                printf("angel61: lock at sample %lld metric %.2f frequency %.0f Hz\n", start, m, m_frequencyHz);
            }
        }
    }
}

// ---- Tracking ----------------------------------------------------------------------------------

void DSDAngel61Receiver::track()
{
    // First find the UW around its expected position, as soon as the samples of that window are in
    if (m_trackPosition < 0.0)
    {
        int64_t windowEnd = (long long) std::ceil(m_expectedStart) + LockedWindow;

        if (m_filteredCount - 1 < windowEnd + m_uwSpan + 1) {
            return;
        }

        int64_t centre = std::llround(m_expectedStart);
        int64_t best = centre;
        float bestMetric = -1.0f;

        for (int64_t s = centre - LockedWindow; s <= centre + LockedWindow; s++)
        {
            float m = m_metric[s & RingMask];

            if (m > bestMetric)
            {
                bestMetric = m;
                best = s;
            }
        }

        if (bestMetric > m_lockedThreshold)
        {
            // The UW's position corrects the frame clock through a second order loop: the sample
            // clock is stable, a single UW's timing at low SNR is not
            float before = m_metric[(best - 1) & RingMask];
            float after = m_metric[(best + 1) & RingMask];
            float denominator = before - 2.0f * bestMetric + after;
            double tau = (denominator < 0.0f) ? std::max(-0.5, std::min(0.5, 0.5 * (before - after) / denominator)) : 0.0;
            double error = std::max(-3.0, std::min(3.0, best + tau - m_expectedStart));
            m_expectedStart += TimingGain * error;
            m_timingDrift += TimingDriftGain * error;
            m_trackMetric = bestMetric;
        }
        else
        {
            // No UW where expected: demodulate at the expected position, the header decides
            m_trackMetric = bestMetric;
        }

        m_trackPosition = m_expectedStart;
        m_status.m_uwDetections++;
    }

    if (m_filteredCount - 1 < (long long) std::ceil(m_trackPosition) + m_decodeLookahead) {
        return;
    }

    double position = m_trackPosition;
    float metric = m_trackMetric;
    m_trackPosition = -1.0;
    bool headerOk = decodeFrame(position, metric);
    // The frame's own timing from its known symbols (decodeFrame) corrects the frame clock too:
    // the UW's position alone is noisy near threshold, hence its loop's low gains
    // Faster for the first frames of a lock, while the drift is learnt (a sample clock 100 ppm
    // off gains 0.4 samples a frame, and at the settled gains that took twenty frames)
    const double proportionalScale = m_framesLocked < 20 ? 2.0 : 1.0;
    const double driftScale = m_framesLocked < 8 ? 4.0 : m_framesLocked < 20 ? 2.0 : 1.0;
    m_framesLocked++;
    m_expectedStart = position + m_frameSamples + m_timingDrift + proportionalScale * TimingCorrectionGain * m_timingCorrection;
    m_timingDrift += driftScale * TimingCorrectionDriftGain * m_timingCorrection;

    if (headerOk)
    {
        m_status.m_misses = 0;

        if (m_lastFrameClaimed)
        {
            // A header said last frame but its payload did not decode: two more frames without
            // a header end the lock rather than MaxMisses
            m_status.m_misses = MaxMisses - 2;
        }

        if (m_lastFrameSeen)
        {
            if (m_debug) {
                printf("angel61: last frame at sample %lld\n", (long long) position);
            }
            unlock();
        }
    }
    else
    {
        m_status.m_misses++;

        if (m_debug) {
            printf("angel61: miss %d at sample %lld metric %.2f\n", m_status.m_misses, (long long) position, metric);
        }

        // A lock no payload has confirmed may be a header from noise (its CRC-8 lets one chance UW
        // in 256 past): locked on it, the receiver would not see a transmission starting beside it
        // until MaxMisses. A UW since, away from the lock's frames, ends it at once
        if (!m_status.m_confirmed && rescanUnconfirmed()) {
            return;
        }

        if (m_status.m_misses >= MaxMisses) {
            unlock();
        }
    }
}

bool DSDAngel61Receiver::rescanUnconfirmed()
{
    // The metrics since the lock are all known (process), as far back as the ring holds a
    // candidate's frame. A candidate is a local maximum above the search's threshold, as search()
    // calls it, more than LockedMetricWindow from every frame start the lock expected
    const int64_t latest = m_filteredCount - 1 - m_uwSpan - PeakLookahead;
    const int64_t first = std::max(m_lockStart + PeakLookahead + 1, m_filteredCount - RingSize + PeakLookahead + 1);
    std::vector<Candidate> found;

    for (int64_t s = first; s <= latest; s++)
    {
        const float m = m_metric[s & RingMask];

        if (m <= m_searchThreshold) {
            continue;
        }

        bool peak = true;

        for (int k = 1; (k <= PeakLookahead) && peak; k++) {
            peak = (m >= m_metric[(s - k) & RingMask]) && (m >= m_metric[(s + k) & RingMask]);
        }

        const double frames = (double) (s - m_lockStart) / m_frameSamples;
        const double off = std::fabs((frames - std::round(frames)) * m_frameSamples);

        if (!peak || (off <= LockedMetricWindow)) {
            continue;
        }

        found.push_back({ s, m });
    }

    if (found.empty()) {
        return false;
    }

    // The strongest MaxPending, in time order
    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.m_metric > b.m_metric; });
    found.resize(std::min((int) found.size(), MaxPending));
    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.m_start < b.m_start; });

    if (m_debug) {
        printf("angel61: unconfirmed lock at sample %lld dropped for %d candidates from sample %lld\n",
            (long long) m_lockStart, (int) found.size(), (long long) found.front().m_start);
    }

    unlock();
    m_pending = found;
    return true;
}

void DSDAngel61Receiver::unlock()
{
    // The search looks back PeakLookahead starts either side of a candidate: the metrics a
    // locked receiver did not work out
    const int64_t latest = m_filteredCount - 1 - m_uwSpan;

    for (int64_t s = std::max((int64_t) 0, latest - 4 * PeakLookahead); s <= latest; s++) {
        m_metric[s & RingMask] = uwMetric(s, nullptr);
    }

    m_state = StateSearch;
    m_pending.clear();
    m_trackPosition = -1.0;
    m_status.m_locked = false;
    m_status.m_confirmed = false;
    m_status.m_misses = 0;
    m_status.m_unlocks++;
    m_lastFrameSeen = false;
    m_lastFrameClaimed = false;
    m_haveLastHeader = false;
}

// ---- Frame demodulation ------------------------------------------------------------------------

//! Weighted least squares slope of unwrapped phases against position
float DSDAngel61Receiver::phaseSlope(const std::vector<KnownGroup>& groups, float reference)
{
    float sw = 0.0f, sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f;
    float previous = 0.0f, unwrapped = 0.0f;

    for (size_t g = 0; g < groups.size(); g++)
    {
        // Against the reference slope, so that the unwrapping sees a nearly flat progression
        float phase = std::arg(groups[g].m_gain) - reference * groups[g].m_centre;

        if (g == 0) {
            unwrapped = phase;
        } else {
            float d = phase - previous;
            while (d > (float) M_PI) d -= 2.0f * (float) M_PI;
            while (d < -(float) M_PI) d += 2.0f * (float) M_PI;
            unwrapped += d;
        }

        previous = phase;
        float w = groups[g].m_weight, x = groups[g].m_centre;
        sw += w;
        sx += w * x;
        sy += w * unwrapped;
        sxx += w * x * x;
        sxy += w * x * unwrapped;
    }

    float denominator = sw * sxx - sx * sx;
    return reference + (denominator > 0.0f ? (sw * sxy - sx * sy) / denominator : 0.0f);
}

//! Piecewise linear interpolation of the gain between group centres, held beyond the ends
void DSDAngel61Receiver::interpolateGain(const std::vector<KnownGroup>& groups, std::complex<float> *gain, int n,
    float noiseVariance)
{
    size_t g = 0;
    size_t segment = (size_t) -1;       // the segment m0 to limit belong to
    float m0 = 0.0f, m1 = 0.0f, p0 = 0.0f, d = 0.0f, limit = 0.0f;

    for (int k = 0; k < n; k++)
    {
        while ((g + 1 < groups.size()) && (groups[g + 1].m_centre <= k)) {
            g++;
        }

        if (k <= groups.front().m_centre) {
            gain[k] = groups.front().m_gain;
        } else if (g + 1 >= groups.size()) {
            gain[k] = groups.back().m_gain;
        } else {
            float t = (k - groups[g].m_centre) / (groups[g + 1].m_centre - groups[g].m_centre);

            if (segment != g)
            {
                // once a segment (its two anchors), not once a symbol
                segment = g;
                m0 = std::abs(groups[g].m_gain);
                m1 = std::abs(groups[g + 1].m_gain);
                p0 = std::arg(groups[g].m_gain);
                float p1 = std::arg(groups[g + 1].m_gain);
                d = p1 - p0;

                while (d > (float) M_PI) { d -= 2.0f * (float) M_PI; }
                while (d < -(float) M_PI) { d += 2.0f * (float) M_PI; }

                // The phase of an anchor of w symbols measured in this noise is good to about
                // sqrt(noise / 2w) / |gain| radians; the difference of two of them to the root of
                // the sum of their variances. Three of those is the line between noise and fading
                auto phaseSigma = [noiseVariance](const KnownGroup& group) {
                    float magnitude = std::max(1e-6f, std::norm(group.m_gain));
                    return noiseVariance / (2.0f * std::max(1.0f, group.m_weight) * magnitude);
                };
                limit = NoiseAngleSigmas
                    * std::sqrt(phaseSigma(groups[g]) + phaseSigma(groups[g + 1]));
            }

            if (std::fabs(d) < limit)
            {
                // The anchors disagree by no more than their own noise explains: the channel
                // did not move, so the estimate between them keeps its magnitude and turns
                // smoothly. Interpolating the complex value here would let it collapse towards
                // zero, and dividing a symbol by almost nothing manufactures confidence
                gain[k] = std::polar(m0 + t * (m1 - m0), p0 + t * d);
            }
            else
            {
                // They disagree by more than that: the channel moved, and what it did between
                // them is genuinely unknown. The complex interpolation fades, which is this
                // estimator saying so - and the decoder's confidence falls with the magnitude
                gain[k] = groups[g].m_gain * (1.0f - t) + groups[g + 1].m_gain * t;
            }
        }
    }
}

bool DSDAngel61Receiver::decodeFrame(double position, float uwMetric)
{
    using namespace std;
    m_timingCorrection = 0.0;
    // The symbols' spacing: the frame clock's drift spread over the frame. A sample clock 100 ppm
    // off moves the frame's last symbols 0.4 samples from where a frame taken at the nominal
    // spacing puts them, a twelfth of a WB symbol
    const double spacing = m_sps + (m_state == StateLocked ? m_timingDrift / m_frameSymbols : 0.0);
    const int N = m_frameSymbols;
    const int NH = m_layout->m_headerSymbols;   // 48, scattered: m_layout->m_headerPositions[k]

    // Coarse frequency: the UW's segment estimate while searching, the tracked value when locked
    float uwFrequency = 0.0f;
    this->uwMetric(std::llround(position), &uwFrequency);
    bool tracking = (m_state == StateLocked) && m_status.m_confirmed;
    // Near the edge of a hypothesis' range the estimate may belong to the neighbouring one:
    // its alias a hypothesis step away is the second coarse candidate
    float residual = uwFrequency - HypothesisStepHz * std::round(uwFrequency / HypothesisStepHz);
    float coarseCandidates[2] = { tracking ? m_frequencyHz : uwFrequency, uwFrequency - (residual > 0.0f ? HypothesisStepHz : -HypothesisStepHz) };
    int nbCoarse = (!tracking && (std::fabs(residual) > 0.35f * HypothesisStepHz)) ? 2 : 1;
    float coarse = coarseCandidates[0];
    // The frame's symbols and the next frame's UW: within a transmission frames are contiguous,
    // so the following UW is a known anchor for the tail of this frame's channel estimate
    const int NE = m_extendedSymbols;
    complex<float> raw[MaxExtendedSymbols];
    complex<float> samples[MaxExtendedSymbols];

    for (int k = 0; k < NE; k++) {
        samples[k] = interpolate(position + k * spacing);
    }

    // The correlation is normalised, so the flush after a transmission's last frame (the pulse
    // tails of its last symbols, at a fraction of the level) can pass it by chance: the next UW
    // must also carry a comparable power (a fade of 20 dB allowed)
    float framePower = 0.0f, nextPower = 0.0f;

    for (int k = 0; k < NE; k++)
    {
        if (k < N) {
            framePower += std::norm(samples[k]);
        } else {
            nextPower += std::norm(samples[k]);
        }
    }

    m_nextUwValid = (this->uwMetric(std::llround(position) + m_frameSamples, nullptr) > m_lockedThreshold)
        && (nextPower * N > 0.01f * framePower * UWSymbols);

    // Known symbol groups: the UW, then the 24 anchor groups of 2.2. A group holds its pilot
    // and, once the header has decoded and `headerSymbols` is given, its header symbol as well -
    // which is what doubles every anchor for the payload's estimate
    auto measureGroups = [&](const complex<float> *y, vector<KnownGroup>& groups,
        const complex<float> *headerSymbols = nullptr) {
        groups.clear();
        KnownGroup uw;
        uw.m_centre = (UWSymbols - 1) / 2.0f;
        uw.m_gain = complex<float>(0.0f, 0.0f);
        uw.m_weight = UWSymbols;
        for (int k = 0; k < UWSymbols; k++) {
            uw.m_gain += y[k] * conj(m_layout->uwSymbol(k));
        }
        uw.m_gain /= (float) UWSymbols;
        groups.push_back(uw);

        complex<float> acc[MaxGroups];
        float weight[MaxGroups], centre[MaxGroups];
        std::fill(acc, acc + MaxGroups, complex<float>(0.0f, 0.0f));
        std::fill(weight, weight + MaxGroups, 0.0f);
        std::fill(centre, centre + MaxGroups, 0.0f);

        for (int k = UWSymbols; k < N; k++)
        {
            int g = m_layout->groupOfSymbol(k);

            if (g < 0) {
                continue;
            }

            int pilot = m_layout->m_symbolKind[k] == 1 ? m_layout->m_symbolIndex[k] : -1;

            if (pilot >= 0) {
                acc[g] += y[k] * conj(m_layout->pilotSymbol(pilot));
            } else if (headerSymbols) {
                acc[g] += y[k] * conj(headerSymbols[m_layout->m_symbolIndex[k]]);
            } else {
                continue;
            }

            weight[g] += 1.0f;
            centre[g] += (float) k;
        }

        for (int g = 0; g < m_layout->m_nbGroups; g++)
        {
            if (weight[g] <= 0.0f) {
                continue;
            }

            KnownGroup grp;
            grp.m_centre = centre[g] / weight[g];
            grp.m_gain = acc[g] / weight[g];
            grp.m_weight = weight[g];
            groups.push_back(grp);
        }
    };

    // The channel over the frame fitted to the UW and the pilots, for the header: the anchors
    // of the fit that decodes the payload, less the header itself
    // What the fit itself does not know at each symbol, in units of the channel's power: it is
    // largest midway between two pilots, which is where half the header symbols are (2.2)
    float headerFitError[MaxFrameSymbols];
    auto fitGain = [&](complex<float> *gain) -> const complex<float> * {
        int nbAnchors = 0;
        int anchorIndex[MaxFrameSymbols];
        complex<float> anchorValue[MaxFrameSymbols];

        for (int k = 0; k < UWSymbols; k++)
        {
            anchorIndex[nbAnchors] = k;
            anchorValue[nbAnchors] = m_symbols[k] * conj(m_layout->uwSymbol(k));
            nbAnchors++;
        }

        for (int i = 0; i < m_layout->m_pilotSymbols; i++)
        {
            int p = m_layout->m_pilotPositions[i];
            anchorIndex[nbAnchors] = p;
            anchorValue[nbAnchors] = m_symbols[p] * conj(m_layout->pilotSymbol(i));
            nbAnchors++;
        }

        ChannelFit& fit = channelFit(anchorIndex, nbAnchors, N, max(1e-4f, m_noiseVariance));
        float bestResidual = 1e30f;
        int bestCandidate = 0;
        complex<double> alpha[MaxFrameSymbols], bestAlpha[MaxFrameSymbols];

        for (int c = 0; c < m_nbFitCandidates; c++)
        {
            float residual = fitResidual(fit, c, anchorValue, alpha);

            if (residual < bestResidual)
            {
                bestResidual = residual;
                bestCandidate = c;
                std::copy(alpha, alpha + nbAnchors, bestAlpha);
            }
        }

        const float *errorVariance = fitErrorVariance(fit, bestCandidate);
        std::copy(errorVariance, errorVariance + N, headerFitError);
        fitGains(fit, bestCandidate, bestAlpha, gain);
        return gain;
    };

    // The header from a gain estimate: LLRs and the Golay/CRC decode. `fitError`, when the
    // estimate comes with one, is what the estimate itself does not know at each symbol.
    //
    // Header prediction (section 5): once a lock is confirmed - a payload has decoded - the next
    // header is all but known: the same tier, the counter moved on by the frames since the last
    // header, the last frame flag either way, a type of this profile. When the Golay words fail,
    // the candidate that best matches the LLRs is taken if few enough of its bits disagree:
    // a far stronger decision than decoding any of the 4096 x 4096, and on noise alone one that
    // passes about 4 frames in 10 000, each of which the payload's CRC-12 still has to pass
    // Revision 3 (2.7): the 24 coded header symbols are sent twice, half a frame apart, and the
    // copies' LLRs are added before the Golay decode; that is what revision 2's prediction was
    // for, and it is not needed (section 5)
    float headerLLR[DVAngel61::HeaderCodedBits];
    int headerErrors = 0;
    DVAngel61::Header header;
    const bool headerPredicted = false;
    auto tryHeader = [&](const complex<float> *gain, const float *fitError = nullptr) {
        complex<float> equalised[MaxHeaderSymbols];
        float headerNoise[MaxHeaderSymbols];
        float power = 0.0f;

        for (int k = 0; k < N; k++) {
            power += norm(gain[k]);
        }

        power /= N;

        for (int k = 0; k < NH; k++)
        {
            int hs = m_layout->m_headerPositions[k];
            equalised[k] = m_symbols[hs] / gain[hs];
            // Per symbol rather than one number for the frame: a header symbol in a fade is
            // worth less than the others, and saying so is what lets the soft Golay decoder
            // spend its three corrections on the symbols that are merely noisy. The scale is
            // the 0.1 this passed before, so nothing changes on a channel that is not fading
            headerNoise[k] = 0.1f * power / max(1e-3f * power, norm(gain[hs]));

            if (fitError)
            {
                // The channel's noise **and** the estimate's own error, as the payload's LLRs
                // have it: equalising by an estimate that is off by e leaves the symbol off by
                // e as well. With the anchors of 2.2 half the header symbols sit midway between
                // two pilots, and at 100 Hz of Doppler what the fit does not know there is
                // several times the noise - telling the decoder so is worth half the lost
                // headers (92 to 46 in 1500 frames at C/N 20 dB)
                headerNoise[k] = (max(1e-4f, m_noiseVariance) + fitError[hs]) * power / max(1e-3f * power, norm(gain[hs]));
            }
        }

        // Header symbol h carries coded symbol h mod 24: each symbol's LLRs with its own noise,
        // the copies added
        std::fill(headerLLR, headerLLR + DVAngel61::HeaderCodedBits, 0.0f);
        float copyLLR[2][DVAngel61::HeaderCodedBits];

        for (int k = 0; k < NH; k++)
        {
            float two[2];
            DVAngel61::demapCells(DVAngel61::QPSK, &equalised[k], 1, headerNoise[k], two);
            headerLLR[2 * (k % DVAngel61::HeaderCells)] += two[0];
            headerLLR[2 * (k % DVAngel61::HeaderCells) + 1] += two[1];
            copyLLR[k / DVAngel61::HeaderCells][2 * (k % DVAngel61::HeaderCells)] = two[0];
            copyLLR[k / DVAngel61::HeaderCells][2 * (k % DVAngel61::HeaderCells) + 1] = two[1];
        }

        // While searching, the copies must agree before the Golay words are decoded
        // (HeaderCopyGate): the correlation of their LLRs, in which a symbol in a fade weighs
        // what its LLRs do. A header's copies are the same symbols, noise's are not; the decode
        // is most of what a UW candidate on noise costs, fifteen attempts of two exhaustive Golay
        // decodes, and the gate takes nine tenths of it away. Not once a lock is confirmed: a
        // rescue method's gain can be wrong on one copy only, and the gate then cost NB tier 1
        // at 20 Hz of Doppler twice its frames. While searching it cost no frame and no
        // acquisition in the harness (NB and WB, AWGN at threshold, 20 to 200 Hz)
        {
            float cross = 0.0f, first = 0.0f, second = 0.0f;

            for (int b = 0; b < DVAngel61::HeaderCodedBits; b++)
            {
                cross += copyLLR[0][b] * copyLLR[1][b];
                first += copyLLR[0][b] * copyLLR[0][b];
                second += copyLLR[1][b] * copyLLR[1][b];
            }

            if (!tracking && (cross < HeaderCopyGate * std::sqrt(first * second)))
            {
                m_status.m_headerGated++;
                return false;
            }
        }

        bool ok = DVAngel61::decodeHeader(headerLLR, header, &headerErrors) && DVAngel61::isKnownType(header.m_type)
            && (DVAngel61::profileOf(header.m_type) == m_layout->m_profile);
        if (m_debug && getenv("DSD_ANGEL61_DEBUG_HEADER"))
        {
            printf("angel61:   header attempt %s errors %d type %02X counter %d |z|:", ok ? "ok" : "failed", headerErrors, header.m_type, header.m_counter);
            for (int k = 0; k < NH; k++) {
                printf(" %.2f/%.0f", abs(equalised[k]), arg(equalised[k]) * 180.0f / (float) M_PI);
            }
            int h0 = m_layout->m_headerPositions[0], hN = m_layout->m_headerPositions[NH - 1];
            printf(" gain %.2f/%.0f .. %.2f/%.0f", abs(gain[h0]), arg(gain[h0]) * 180.0f / (float) M_PI, abs(gain[hN]), arg(gain[hN]) * 180.0f / (float) M_PI);
            printf(" position %.1f coarse %.1f\n", position, coarse);
        }
        return ok;
    };

    // Residual frequency from the known groups' phase progression, unwrapped against the unique
    // word's own slope so that the ambiguity is in the difference rather than in the whole
    // residual; the header is then tried for that fit and for one turn more or less across the
    // largest gap between groups, and the header CRC picks.
    vector<KnownGroup> groups;
    bool headerOk = false;
    float slope = 0.0f;
    float candidates[3] = { 0.0f, 0.0f, 0.0f };
    const int nbCandidates = 3;
    float aliasTurn = 0.0f;   // searching: the slope candidates' spacing (a turn across the widest gap)

    for (int cc = 0; cc < nbCoarse && !headerOk; cc++)
    {
        // Symbols at the symbol instants, de-rotated by the coarse estimate
        coarse = coarseCandidates[cc];
        double step = -2.0 * M_PI * coarse * m_sps / SampleRate;

        for (int k = 0; k < NE; k++)
        {
            double angle = step * k;
            raw[k] = samples[k] * complex<float>((float) cos(angle), (float) sin(angle));
        }

        // The UW's own phase slope (16 symbols: coarse, but unambiguous, which is what the
        // groups' unwrapping needs, and in fading what the symbols beside it actually see)
        float uwSlope;
        {
            float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f, unwrapped = 0.0f, previous = 0.0f;
            for (int k = 0; k < UWSymbols; k++)
            {
                float phase = arg(raw[k] * conj(m_layout->uwSymbol(k)));
                if (k == 0) {
                    unwrapped = phase;
                } else {
                    float d = phase - previous;
                    while (d > (float) M_PI) d -= 2.0f * (float) M_PI;
                    while (d < -(float) M_PI) d += 2.0f * (float) M_PI;
                    unwrapped += d;
                }
                previous = phase;
                sx += k; sy += unwrapped; sxx += (float) k * k; sxy += k * unwrapped;
            }
            const float nUw = (float) UWSymbols;
            uwSlope = (nUw * sxy - sx * sy) / (nUw * sxx - sx * sx);
        }

        measureGroups(raw, groups);
        float slope0 = phaseSlope(groups, uwSlope);

        if (tracking)
        {
            // The tracked frequency first: the carrier does not move by more than a few hertz
            // between frames, and the tracking loop already has it. A slope fitted to the
            // pilots' phases is the frequency only on a steady channel; in fast fading those
            // phases wander through several fades a frame and the fit reads the wander as
            // frequency - at 100 Hz of Doppler it was pinned at the limit below on 84% of
            // frames. De-rotating by it moves the channel's spectrum off centre, which the
            // Jakes fit the header leans on cannot represent, and that alone lost more than
            // half the headers the receiver lost there. The pilots' slope, limited, and the
            // UW's own come after, for a carrier that did move
            const float limit = 2.0f * (float) M_PI * TrackedSlopeLimitHz * m_sps / SampleRate;
            slope0 = max(-limit, min(limit, slope0));
            candidates[0] = 0.0f;
            candidates[1] = slope0;
            candidates[2] = uwSlope;
        }
        else
        {
            // A turn across the **largest** gap between consecutive groups: that is the gap the
            // unwrapping's ambiguity belongs to, and with the anchors of 2.2 it is a pilot to
            // pilot gap rather than the one from the unique word to the first of them
            float maxGap = 1.0f;

            for (size_t g = 1; g < groups.size(); g++) {
                maxGap = max(maxGap, groups[g].m_centre - groups[g - 1].m_centre);
            }

            const float turn = 2.0f * (float) M_PI / maxGap;
            aliasTurn = turn;
            candidates[0] = slope0;
            candidates[1] = slope0 + turn;
            candidates[2] = slope0 - turn;
        }

    for (int c = 0; c < nbCandidates && !headerOk; c++)
    {
        slope = candidates[c];

        for (int k = 0; k < NE; k++) {
            m_symbols[k] = raw[k] * complex<float>(cos(-slope * k), sin(-slope * k));
        }

        measureGroups(m_symbols, groups);

        // The header is 24 symbols spread over the whole frame (2.2), so a single gain for all
        // of it is no longer a sensible first guess: a residual of thirty hertz turns the last
        // header symbol through sixty degrees where, when the header was 24 contiguous symbols
        // behind the unique word, it turned through five. The interpolation between the unique
        // word and the pilots goes first, then the fit; the constant gain is the last resort it
        // has become rather than the first guess it was
        interpolateGain(groups, m_gain, N, m_noiseVariance);
        headerOk = tryHeader(m_gain);

        if (!headerOk)
        {
            // The MMSE fit of the channel to the UW and the pilots - the header is what is
            // being decoded, so it cannot be an anchor yet. The interpolation above is
            // piecewise linear between anchors; this one knows what a Jakes channel does
            // between them, which at 50 Hz of Doppler is the difference between a header that
            // decodes and one that does not
            headerOk = tryHeader(fitGain(m_gain), headerFitError);
        }

        if (!headerOk)
        {
            // One gain for the frame: what a clean static channel wants, and the cheapest
            // thing that can rescue a frame whose pilots were unlucky
            complex<float> uwGain = groups[0].m_gain;

            for (int k = 0; k < N; k++) {
                m_gain[k] = uwGain;
            }

            headerOk = tryHeader(m_gain);
        }

        if (!headerOk)
        {
            // Fast fading: a decision directed tracker from the UW through the header symbols,
            // forward and backward, at the gain of a good SNR (the only one fast fading is decodable at)
            complex<float> forward[MaxHeaderSymbols];
            complex<float> g(0.0f, 0.0f);

            // From the gain over the last four UW symbols (the whole UW is 3 ms: too long a mean in fast fading)
            const int uwEnd = UWSymbols;

            for (int k = uwEnd - 4; k < uwEnd; k++) {
                g += m_symbols[k] * conj(m_layout->uwSymbol(k)) * 0.25f;
            }

            for (int k = 0; k < NH; k++)
            {
                int hs = m_layout->m_headerPositions[k];
                g += TrackerGainMax * (m_symbols[hs] * conj(hardDecision(DVAngel61::QPSK, m_symbols[hs] / g)) - g);
                forward[k] = g;
            }

            int p = m_layout->m_pilotPositions[m_layout->m_pilotSymbols - 1];
            g = m_symbols[p] * conj(m_layout->pilotSymbol(m_layout->m_pilotSymbols - 1));

            for (int k = NH - 1; k >= 0; k--)
            {
                int hs = m_layout->m_headerPositions[k];
                g += TrackerGainMax * (m_symbols[hs] * conj(hardDecision(DVAngel61::QPSK, m_symbols[hs] / g)) - g);
                m_gain[hs] = (forward[k] + g) * 0.5f;
            }

            headerOk = tryHeader(m_gain);
        }
    }
    }

    float frequency = coarse + slope * SampleRate / (2.0f * (float) M_PI * m_sps);

    if (!headerOk)
    {
        m_status.m_headerFailed++;

        if (m_debug) {
            printf("angel61: header failed at sample %lld metric %.2f frequency %.0f Hz errors %d\n", (long long) position, uwMetric, frequency, headerErrors);
        }

        return false;
    }

    m_status.m_headerOk++;
    // The last frame flag is acted on once the payload's CRC-12 has passed (2.4): until then a
    // header is only its CRC-8, and near threshold one in a few hundred wrong headers passes it
    m_lastFrameSeen = false;
    m_lastFrameClaimed = header.m_lastFrame;
    m_lastHeader = header;
    m_haveLastHeader = true;

    if (headerPredicted) {
        m_status.m_headerPredicted++;
    }

    // The decoded header is 24 more known symbols: the gain groups become UW, header, pilots
    uint8_t headerBits[DVAngel61::HeaderCodedBits];
    complex<float> coded[DVAngel61::HeaderCells];
    DVAngel61::encodeHeader(header, headerBits);
    DVAngel61::mapCells(DVAngel61::QPSK, headerBits, DVAngel61::HeaderCells, coded);
    complex<float> *headerSymbols = m_headerSymbols;

    for (int k = 0; k < NH; k++) {
        headerSymbols[k] = coded[k % DVAngel61::HeaderCells];
    }

    // The residual frequency from the known symbols alone: the phase of the channel's lag one
    // correlation over the adjacent pairs of them (the UWs, the pilots and the header beside
    // them), which no decision can bias, and which has no ambiguity a symbol apart
    auto knownLagPhase = [&]() {
        const int NK = m_nextUwValid ? NE : N;
        complex<float> previous(0.0f, 0.0f), correlation(0.0f, 0.0f);
        bool previousKnown = false;

        for (int k = 0; k < NK; k++)
        {
            complex<float> d;
            bool isKnown = k >= N ? true : m_layout->knownSymbol(k, d);

            if (k >= N) {
                d = m_layout->uwSymbol(k - N);
            } else if (!isKnown && (m_layout->headerIndexOfSymbol(k) >= 0)) {
                d = headerSymbols[m_layout->headerIndexOfSymbol(k)];
                isKnown = true;
            }

            complex<float> g = isKnown ? m_symbols[k] * conj(d) : complex<float>(0.0f, 0.0f);

            if (isKnown && previousKnown) {
                correlation += g * conj(previous);
            }

            previous = g;
            previousKnown = isKnown;
        }

        return arg(correlation);
    };

    // A searching receiver's slope can be an alias of its candidates', whole turns across the
    // widest gap between the known groups away: the groups agree with it, and the header's code
    // corrects what it costs the header symbols between them, but the payload's estimate cannot
    // (it failed the first frame of a cold acquisition about once in 600 at Es/N0 6 dB, 380 to 640
    // Hz off, one or two turns). With the header the known symbols are three times as many and
    // spread through the frame: their coherent sum after the de-rotation peaks at the true slope,
    // a frame's reciprocal wide (12 Hz NB, 25 Hz WB), its side lobes about three quarters of it.
    // Scanned over two and a half turns either side in steps of a 64th, a peak beyond half a turn
    // that beats the nearest by AliasMargin takes over (peaks, not the slope's own value, which
    // sits off its narrow peak and loses to noise). Measured on 4000 cold transmissions each: at
    // 6 dB NB 6 first frames missed to 1, WB 8 to 1; the thresholds and fading unchanged
    if (!tracking && (aliasTurn > 0.0f))
    {
        complex<float> known[MaxFrameSymbols];
        int positions[MaxFrameSymbols];
        int nbKnown = 0;
        float magnitudes = 0.0f;   // the coherent sum's most: all the known symbols in phase

        for (int k = 0; k < N; k++)
        {
            complex<float> d;
            bool isKnown = m_layout->knownSymbol(k, d);

            if (!isKnown && (m_layout->headerIndexOfSymbol(k) >= 0)) {
                d = headerSymbols[m_layout->headerIndexOfSymbol(k)];
                isKnown = true;
            }

            if (isKnown)
            {
                known[nbKnown] = raw[k] * conj(d);
                magnitudes += abs(known[nbKnown]);
                positions[nbKnown++] = k;
            }
        }

        auto coherence = [&](float sl) {
            complex<float> sum(0.0f, 0.0f);

            for (int i = 0; i < nbKnown; i++) {
                sum += known[i] * complex<float>(cos(-sl * positions[i]), sin(-sl * positions[i]));
            }

            return abs(sum);
        };

        const int steps = AliasScanSteps;   // a turn's
        float nearBest = 0.0f, farBest = 0.0f, farOffset = 0.0f;

        for (int i = -5 * steps / 2; i <= 5 * steps / 2; i++)
        {
            const float offset = aliasTurn * i / steps;
            const float c = coherence(slope + offset);

            if (2 * std::abs(i) <= steps) {
                nearBest = std::max(nearBest, c);
            } else if (c > farBest) {
                farBest = c;
                farOffset = offset;
            }
        }

        // (and only a steady channel's: in fast fading the phases wander through the frame, no
        // slope makes them coherent, and a chance peak is no alias)
        if ((farBest > AliasMargin * nearBest) && (farBest > AliasCoherence * magnitudes))
        {
            if (m_debug) {
                printf("angel61: slope alias at sample %lld: %.0f Hz more\n", (long long) position,
                    farOffset * SampleRate / (2.0f * (float) M_PI * m_sps));
            }

            slope += farOffset;
            frequency = coarse + slope * SampleRate / (2.0f * (float) M_PI * m_sps);

            for (int k = 0; k < NE; k++) {
                m_symbols[k] = raw[k] * complex<float>(cos(-slope * k), sin(-slope * k));
            }
        }
    }

    measureGroups(m_symbols, groups, headerSymbols);
    interpolateGain(groups, m_gain, N, m_noiseVariance);

    // The frame's timing from all its known symbols - the UWs, the pilots and now the header,
    // ten times the UW's - before the payload: each projected on the channel estimate at the
    // timing and a fifth of a symbol either side, a parabola through the three. The correction
    // goes to the frame clock (track), and a frame far off is taken again where it should be. The UW alone at Es/N0 1 dB in WB left frames half a sample off and more
    // (a tenth of a symbol), and most of the frames WB tier 0 lost beyond the bound. Not in fast
    // fading (as the last frame judged it), where a projection on the pilots' interpolated gain
    // is a poor measure of timing: NB tier 2 at 150 Hz lost a point more with it

    const bool fadingFrame = (m_gainVariation > 3.0f) && (m_fitDopplerHz >= FadingSlopeHz);   // the last frame's judgement

    if (!fadingFrame)
    {
        const int NK = m_nextUwValid ? NE : N;
        const double derotation = -2.0 * M_PI * coarse * m_sps / SampleRate - slope;
        complex<float> rotation[MaxExtendedSymbols], reference[MaxExtendedSymbols];
        bool isKnown[MaxExtendedSymbols];

        for (int k = 0; k < NK; k++)
        {
            rotation[k] = complex<float>((float) cos(derotation * k), (float) sin(derotation * k));
            complex<float> d;
            isKnown[k] = k >= N ? true : m_layout->knownSymbol(k, d);

            if (k >= N) {
                d = m_layout->uwSymbol(k - N);
            } else if (!isKnown[k] && (m_layout->headerIndexOfSymbol(k) >= 0)) {
                d = headerSymbols[m_layout->headerIndexOfSymbol(k)];
                isKnown[k] = true;
            }

            reference[k] = isKnown[k] ? conj(d) * conj(m_gain[std::min(k, N - 1)]) : complex<float>(0.0f, 0.0f);
        }

        auto projection = [&](double delta) {
            double sum = 0.0;
            for (int k = 0; k < NK; k++) {
                if (isKnown[k]) sum += (interpolate(position + delta + k * spacing) * rotation[k] * reference[k]).real();
            }
            return sum;
        };

        const double delta = m_sps / 5.0;
        const double before = projection(-delta), at = projection(0.0), after = projection(delta);
        const double curvature = before - 2.0 * at + after;

        if ((curvature < 0.0) && (at > 0.0))
        {
            const double tau = std::max(-1.5 * delta, std::min(1.5 * delta, 0.5 * delta * (before - after) / curvature));

            // The frame clock always hears of it; the frame is taken again only when it is off by
            // more than a tenth of a symbol, which is mostly the first frames of a lock. Taken
            // again whenever it was a tenth of a sample off, NB tier 3 (16-QAM) lost two points
            // more at Es/N0 9.9 dB: at 10 samples a symbol the estimate's own noise is then
            // larger than what it corrects
            m_timingCorrection = tau;

            if (std::fabs(tau) > TimingResampleSymbols * m_sps)
            {
                position += tau;

                for (int k = 0; k < NE; k++) {
                    m_symbols[k] = interpolate(position + k * spacing) * complex<float>((float) cos(derotation * k), (float) sin(derotation * k));
                }

                measureGroups(m_symbols, groups, headerSymbols);
                interpolateGain(groups, m_gain, N, m_noiseVariance);
            }
        }
    }

    // Noise from the known symbols' residuals after equalisation
    {
        float residual = 0.0f;
        int count = 0;

        for (int k = 0; k < N; k++)
        {
            complex<float> known;
            bool isKnown = m_layout->knownSymbol(k, known);

            int h = m_layout->headerIndexOfSymbol(k);

            if (!isKnown && (h >= 0))
            {
                known = headerSymbols[h];
                isKnown = true;
            }

            if (isKnown)
            {
                residual += norm(m_symbols[k] / m_gain[k] - known);
                count++;
            }
        }

        m_noiseVariance = max(1e-4f, residual / max(1, count));
        float level = 0.0f;

        for (const KnownGroup& g : groups) {
            level += abs(g.m_gain) * g.m_weight;
        }

        m_status.m_level = level / (UWSymbols + NH + m_layout->m_pilotSymbols);
    }

    // The residual frequency from the known symbols alone (knownLagPhase)
    const float knownSlope = knownLagPhase();

    // Payload: the channel gain per symbol from the known symbols and the payload decisions
    DVAngel61::Constellation constellation = DVAngel61::constellationOf(header.m_type);
    complex<float> equalised[MaxFrameSymbols];
    bool genieGains = false;

    if (m_genie && m_genieMode)
    {
        // Studies: the true channel c at each symbol, mapped onto the receiver's symbols by a gain
        // and a phase ramp fitted over the known symbols (the matched filter's scale and phase, and
        // what frequency the receiver has left)
        complex<float> c[MaxExtendedSymbols], z[MaxExtendedSymbols];
        bool isKnown[MaxExtendedSymbols];

        for (int k = 0; k < N; k++)
        {
            complex<float> d;
            isKnown[k] = m_layout->knownSymbol(k, d);
            int h = m_layout->headerIndexOfSymbol(k);
            if (!isKnown[k] && (h >= 0)) { d = headerSymbols[h]; isKnown[k] = true; }
            c[k] = m_genie(position + k * spacing);
            z[k] = isKnown[k] ? m_symbols[k] * conj(d) * conj(c[k]) : complex<float>(0.0f, 0.0f);
        }

        // The ramp: the one that makes the known symbols most coherent (a lag one estimate was
        // led astray by the pilot and header pairs)
        float b = 0.0f;
        {
            float best = -1.0f, bestB = 0.0f;
            for (int i = -400; i <= 400; i++)
            {
                const float t = 0.0001f * i;
                complex<float> s(0.0f, 0.0f);
                for (int k = 0; k < N; k++) if (isKnown[k]) s += z[k] * complex<float>(cos(-t * k), sin(-t * k));
                if (abs(s) > best) { best = abs(s); bestB = t; }
            }
            b = bestB;
        }
        complex<float> num(0.0f, 0.0f);
        float den = 0.0f;
        for (int k = 0; k < N; k++) {
            if (isKnown[k]) { num += z[k] * complex<float>(cos(-b * k), sin(-b * k)); den += norm(c[k]); }
        }
        const complex<float> a = num / max(1e-12f, den);

        if (m_genieMode == 1)
        {
            float residual = 0.0f, power = 0.0f;
            int count = 0;
            for (int k = 0; k < N; k++)
            {
                m_gain[k] = a * c[k] * complex<float>(cos(b * k), sin(b * k));
                power += norm(m_gain[k]);
                if (isKnown[k])
                {
                    complex<float> d;
                    if (!m_layout->knownSymbol(k, d)) d = headerSymbols[m_layout->headerIndexOfSymbol(k)];
                    residual += norm(m_symbols[k] - m_gain[k] * d);
                    count++;
                }
            }
            const float absoluteNoise = residual / max(1, count);
            power /= N;
            m_noiseVariance = max(1e-4f, absoluteNoise / max(1e-12f, power));
            for (int k = 0; k < N; k++) {
                m_noisePerSymbol[k] = absoluteNoise / max(1e-3f * power, norm(m_gain[k]));
                m_thermalPerSymbol[k] = m_noisePerSymbol[k];
                m_estimatePerSymbol[k] = 0.0f;
            }
            genieGains = true;
        }
        else
        {
            for (int k = 0; k < m_extendedSymbols; k++) {
                m_symbols[k] *= complex<float>(cos(-b * k), sin(-b * k));
            }
        }
    }

    for (int k = 0; k < N; k++) {
        equalised[k] = m_symbols[k] / m_gain[k];
    }

    float slope2 = 0.0f;

    if (m_decisionFeedback && !genieGains) {
        slope2 = trackGains(constellation, equalised);
    }

    // Fading, for the frequency, as for the phase slope (trackGains): a short window and a fit
    // that found 100 Hz or more. Near threshold decision errors shorten the window on a steady
    // channel, and the known symbols' estimate the loop would follow is good to a few hundred
    // hertz at Es/N0 1 dB in WB
    const bool steadyFrequency = !m_decisionFeedback || (m_gainVariation <= 3.0f) || (m_fitDopplerHz < FadingSlopeHz);

    // The frame's residual phase slope refines the tracked frequency only on a channel steady
    // enough for a slope to mean frequency (in fading the phase wanders on its own)
    if (steadyFrequency) {
        frequency += slope2 * SampleRate / (2.0f * (float) M_PI * m_sps);
    }

    // The tracked frequency: this frame's estimate on a steady channel. In fading the phases of
    // the pilots and decisions follow the channel rather than the carrier, so a slope fitted to
    // them is not a frequency; the phase of the channel's own lag one autocorrelation over the
    // frame is, since a Doppler spectrum symmetric about the carrier contributes nothing to it.
    //
    // It is taken over the known symbols (knownSlope), not every symbol's decision as revision
    // 2's receiver takes it: a frame the estimate has lost decides a rotating channel
    // into a slower one, which pulls the estimate towards the error rather than away from it. At
    // 200 Hz of Doppler (WB tier 1, Es/N0 12 dB) the decisions' estimate walked the loop 200 Hz
    // off in one stream in sixteen, which then lost 80% of its frames. A frame's estimate still
    // varies by tens of hertz with the fading's own asymmetry over 80 ms, hence the loop gain
    if (!steadyFrequency)
    {
        float estimate = frequency + knownSlope * SampleRate / (2.0f * (float) M_PI * m_sps);
        m_frequencyHz += FadingFrequencyGain * (estimate - m_frequencyHz);
    }

    Frame frame;
    frame.m_sample = std::llround(position);
    frame.m_position = position;
    frame.m_header = header;
    frame.m_frequencyHz = frequency;
    frame.m_snrDB = 10.0f * log10(1.0f / m_noiseVariance);
    frame.m_uwCorrelation = uwMetric;
    frame.m_gainVariation = m_gainVariation;
    frame.m_block.resize(DVAngel61::infoBits(header.m_type));
    std::vector<uint8_t> codeword(DVAngel61::codedBits(header.m_type));

    payloadLLR(equalised, header.m_type, m_decisionFeedback ? m_noisePerSymbol : nullptr, m_llr.data());

    std::vector<float> posterior(DVAngel61::codedBits(header.m_type));
    frame.m_payloadOk = decodePayload(header, frame, codeword, posterior);

    // Decoding aided estimation: a failed decode is still mostly right, and its re-encoded
    // symbols, where the decoder is confident of them, are references for the channel estimate
    // (the decisions through a fade settle a quarter turn off; the code's output does not), then
    // decode again

    for (int round = 0; m_decisionFeedback && !genieGains && !frame.m_payloadOk && (round < DecodingRounds); round++)
    {
        complex<float> reference[MaxPayloadSymbols];
        float confidence[MaxPayloadSymbols];
        DVAngel61::payloadCells(codeword.data(), header.m_counter, header.m_type, reference, posterior.data(), confidence);
        trackGains(constellation, equalised, reference, confidence);
        payloadLLR(equalised, header.m_type, m_noisePerSymbol, m_llr.data());
        frame.m_payloadOk = decodePayload(header, frame, codeword, posterior);
        m_status.m_decodingRounds++;
        frame.m_snrDB = 10.0f * log10(1.0f / m_noiseVariance);
        frame.m_gainVariation = m_gainVariation;
    }

    // On a steady channel the frame's own frequency is the tracked one, but only from a frame
    // whose payload decoded. Near threshold a header decodes now and then with a slope candidate
    // that is wrong - the header's code is the stronger - and its payload fails. Taking its
    // frequency sent the next frames off by 40 to 70 Hz, and WB tier 0 at Es/N0 1.2 dB lost
    // them in bursts
    if (steadyFrequency && frame.m_payloadOk) {
        m_frequencyHz = frequency;
    }

    m_status.m_frequencyHz = m_frequencyHz;
    frame.m_coarseOk = frame.m_payloadOk;
    m_lastFrameSeen = m_lastFrameClaimed && frame.m_payloadOk;
    m_lastFrameClaimed = m_lastFrameClaimed && !frame.m_payloadOk;

    if (frame.m_payloadOk)
    {
        m_status.m_payloadOk++;

        m_status.m_confirmed = true;

        // The matched filter follows a frame that decoded **all the way through**, never a
        // header alone: a header CRC-8 lets one chance unique word in 256 past, and a false one
        // carries a nonsense frequency. Moving the filter onto that makes the receiver deaf to
        // the real signal, and deaf it stays, because nothing can then decode to move it back
        if (std::fabs(frequency - m_filterCentreHz) > RecentreHz)
        {
            if (m_debug) {
                printf("angel61: matched filter re-centred from %.0f to %.0f Hz\n", m_filterCentreHz, frequency);
            }

            centreFilter(frequency);
        }
    }
    else
    {
        m_status.m_payloadFailed++;
        frame.m_block.clear();
    }

    if (m_scopeEnabled && m_status.m_confirmed && (m_scope.size() < 4096))
    {
        for (int k = 0; k < m_layout->m_payloadSymbols; k++) {
            m_scope.push_back(equalised[m_layout->payloadSymbolIndex(k)]);
        }
    }

    m_status.m_snrDB = frame.m_snrDB;

    if (getenv("DSD_ANGEL61_DUMP"))
    {
        // Diagnostics: the frame position, the decode result, the gain estimate and the equalised symbols
        FILE *dump = fopen(getenv("DSD_ANGEL61_DUMP"), "ab");
        if (dump)
        {
            float head[4] = { (float) position, frame.m_payloadOk ? 1.0f : 0.0f, (float) header.m_type, coarse + slope * SampleRate / (2.0f * (float) M_PI * m_sps) };
            fwrite(head, sizeof(float), 4, dump);
            for (int k = 0; k < N; k++)
            {
                float v[4] = { m_gain[k].real(), m_gain[k].imag(), equalised[k].real(), equalised[k].imag() };
                fwrite(v, sizeof(float), 4, dump);
            }
            fclose(dump);
        }
    }

    if (m_debug) {
        printf("angel61: frame at %.2f type %02X counter %d %s%s snr %.1f dB f %.0f Hz metric %.2f it %d var %.0f bit errors %d/%d\n", position, header.m_type, header.m_counter,
            frame.m_payloadOk ? "ok" : frame.m_coarseOk ? "COARSE ONLY" : "PAYLOAD FAILED", header.m_lastFrame ? " last" : "", frame.m_snrDB, frequency, uwMetric, frame.m_iterations, m_gainVariation,
            frame.m_channelErrors, frame.m_codedBits);
    }

    m_frames.push_back(frame);
    return true;
}


// The Doppler hypotheses of the channel fit, set by measurement (ANGEL61.md 11). The grid may
// run to 150 Hz because the anchors of ANGEL61.md 2.2 are 2.3 ms apart, which resolves a Jakes
// spectrum to about 215 Hz, and 150 Hz measured well. Revision 1's pilots were 7.5 ms apart,
// resolving it to 67 Hz, and there a 200 Hz candidate cost 14 points of frame loss at 50 Hz:
// the leave one out test picked it though it was far wider than the channel, because its
// residual was dominated by the 40 contiguous anchors at the front of the frame rather than by
// the sparse pilots where the candidates differ. (The Jakes correlation going negative past its
// first zero is not the reason: a fit with the right Doppler is correct across its zeros.)

// WB's anchors are a pilot and a header symbol in every group, 2.3 ms apart, with a pilot in
// each: the pilots alone resolve a Jakes spectrum to about 215 Hz where NB's resolve 108, so the
// grid runs on past NB's 150 Hz
static const float FitDopplersWB[] = {
    0.0f, 12.0f, 25.0f, 50.0f, 75.0f, 100.0f, 150.0f, 200.0f, 300.0f
};

const float *DSDAngel61Receiver::fitGrid(DVAngel61::Profile profile, int& count)
{
    // Revision 3 (2.2): NB's anchors are as dense as WB's once the header has decoded (a pilot or a
    // header symbol every 1.56 ms, WB's every 1.91 ms), so NB takes WB's grid to 300 Hz. With
    // revision 2's NB grid (to 150 Hz) NB tier 0 lost 12% of frames at 150 Hz and Es/N0 12 dB,
    // with WB's 1%; tier 2 at 20 dB 18% and 2.3%. Pinning the fit to the channel's true Doppler
    // did worse than the grid's choice (the residual frequency and timing widen what it sees)
    (void) profile;
    count = (int) (sizeof(FitDopplersWB) / sizeof(FitDopplersWB[0]));
    return FitDopplersWB;
}


double DSDAngel61Receiver::besselJ0(double x)
{
    // Abramowitz and Stegun 9.4.1 and 9.4.3
    double ax = std::fabs(x);

    if (ax < 8.0)
    {
        double y = x * x;
        double num = 57568490574.0 + y * (-13362590354.0 + y * (651619640.7
            + y * (-11214424.18 + y * (77392.33017 + y * (-184.9052456)))));
        double den = 57568490411.0 + y * (1029532985.0 + y * (9494680.718
            + y * (59272.64853 + y * (267.8532712 + y))));
        return num / den;
    }

    double z = 8.0 / ax;
    double y = z * z;
    double xx = ax - 0.785398164;
    double p0 = 1.0 + y * (-0.1098628627e-2 + y * (0.2734510407e-4 + y * (-0.2073370639e-5 + y * 0.2093887211e-6)));
    double q0 = -0.1562499995e-1 + y * (0.1430488765e-3 + y * (-0.6911147651e-5
        + y * (0.7621095161e-6 + y * (-0.934935152e-7))));
    return std::sqrt(0.636619772 / ax) * (std::cos(xx) * p0 - z * std::sin(xx) * q0);
}

void DSDAngel61Receiver::solveFit(ChannelFit& fit, int candidate, float dopplerHz) const
{
    using namespace std;
    const int n = fit.m_nbAnchors;
    const int *anchors = fit.m_anchors.data();

    // The Jakes correlation at every whole number of symbols: anchors and symbols are all at
    // whole symbols
    const double scale = 2.0 * M_PI * dopplerHz / m_layout->m_symbolRate;
    double *correlation = fit.m_correlation.data() + (size_t) candidate * fit.m_nbSymbols;

    for (int d = 0; d < fit.m_nbSymbols; d++) {
        correlation[d] = besselJ0(scale * d);
    }

    // Cholesky of A = R + noise I: the noise on the diagonal is what makes this the Wiener
    // solution rather than an interpolation
    double *L = fit.m_factor.data() + (size_t) candidate * n * n;

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j <= i; j++)
        {
            double sum = correlation[abs(anchors[i] - anchors[j])] + (i == j ? fit.m_noise : 0.0);

            for (int k = 0; k < j; k++) {
                sum -= L[(size_t) i * n + k] * L[(size_t) j * n + k];
            }

            L[(size_t) i * n + j] = (i == j) ? sqrt(max(1e-12, sum)) : sum / L[(size_t) j * n + j];
        }
    }

    double *LT = fit.m_factorTransposed.data() + (size_t) candidate * n * n;

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) {
            LT[(size_t) j * n + i] = L[(size_t) i * n + j];
        }
    }

    // The diagonal of A^-1: the squared columns of L^-1, a column at a time
    double *inverseDiagonal = fit.m_inverseDiagonal.data() + (size_t) candidate * n;
    vector<double> column(n);

    for (int j = 0; j < n; j++)
    {
        double sum = 0.0;

        for (int i = j; i < n; i++)
        {
            double v = (i == j) ? 1.0 : 0.0;

            for (int k = j; k < i; k++) {
                v -= L[(size_t) i * n + k] * column[k];
            }

            column[i] = v / L[(size_t) i * n + i];
            sum += column[i] * column[i];
        }

        inverseDiagonal[j] = sum;
    }

    fit.m_errorValid[candidate] = 0;
}

DSDAngel61Receiver::ChannelFit& DSDAngel61Receiver::channelFit(const int *anchors, int nbAnchors,
    int nbSymbols, float noise)
{
    // Two sets of anchors are asked for, each for a frame or a frame and the next UW: the
    // format's known symbols without the header, to decode the header with, and with it, to
    // decode the payload with. Whichever slot holds the set asked for answers; a set that is
    // not there takes the next slot in turn.
    int slot = -1;

    for (int i = 0; i < (int) (sizeof(m_channelFit) / sizeof(m_channelFit[0])); i++)
    {
        const ChannelFit& candidate = m_channelFit[i];

        if ((candidate.m_nbAnchors == nbAnchors) && (candidate.m_nbSymbols == nbSymbols)
            && std::equal(anchors, anchors + nbAnchors, candidate.m_anchors.begin()))
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        slot = m_channelFitNext;
        m_channelFitNext = (m_channelFitNext + 1) % (sizeof(m_channelFit) / sizeof(m_channelFit[0]));
        m_channelFit[slot].m_noise = -1.0f;
    }

    ChannelFit& fit = m_channelFit[slot];

    // The noise only has to be close: it is the diagonal of a correlation matrix, and solving
    // the candidates again because the estimate moved by a tenth of a dB would undo the point of
    // keeping them. A factor of FitNoiseHysteresis either way, 4.8 dB: in fading the frame's
    // noise to power moves by more than revision 2's 1.8 dB on most frames, and a fit solved for
    // a noise up to 4.8 dB away lost no more frames than one solved for the frame's own
    if ((fit.m_noise > 0.0f) && (noise < fit.m_noise * FitNoiseHysteresis)
        && (noise > fit.m_noise / FitNoiseHysteresis)) {
        return fit;
    }

    fit.m_nbAnchors = nbAnchors;
    fit.m_nbSymbols = nbSymbols;
    fit.m_noise = noise;
    fit.m_anchors.assign(anchors, anchors + nbAnchors);
    fit.m_correlation.resize((size_t) m_nbFitCandidates * nbSymbols);
    fit.m_factor.resize((size_t) m_nbFitCandidates * nbAnchors * nbAnchors);
    fit.m_factorTransposed.resize((size_t) m_nbFitCandidates * nbAnchors * nbAnchors);
    fit.m_inverseDiagonal.resize((size_t) m_nbFitCandidates * nbAnchors);
    fit.m_errorVariance.resize((size_t) m_nbFitCandidates * nbSymbols);
    fit.m_errorValid.assign(m_nbFitCandidates, 0);

    for (int c = 0; c < m_nbFitCandidates; c++) {
        solveFit(fit, c, m_fitDopplers[c]);
    }

    return fit;
}

float DSDAngel61Receiver::fitResidual(const ChannelFit& fit, int candidate, const std::complex<float> *y, std::complex<double> *alpha)
{
    // alpha = A^-1 y: forward then back substitution through the factor
    const int n = fit.m_nbAnchors;
    const double *L = fit.m_factor.data() + (size_t) candidate * n * n;
    const double *LT = fit.m_factorTransposed.data() + (size_t) candidate * n * n;
    const double *inverseDiagonal = fit.m_inverseDiagonal.data() + (size_t) candidate * n;

    for (int i = 0; i < n; i++)
    {
        std::complex<double> v(y[i].real(), y[i].imag());
        const double *row = L + (size_t) i * n;

        for (int k = 0; k < i; k++) {
            v -= row[k] * alpha[k];
        }

        alpha[i] = v / row[i];
    }

    // (L^T's row i is L's column i: contiguous rather than n apart)
    for (int i = n - 1; i >= 0; i--)
    {
        std::complex<double> v = alpha[i];
        const double *column = LT + (size_t) i * n;

        for (int k = i + 1; k < n; k++) {
            v -= column[k] * alpha[k];
        }

        alpha[i] = v / L[(size_t) i * n + i];
    }

    // An anchor's residual is noise (A^-1 y)_a, and 1 - S_aa is noise (A^-1)_aa, held off zero
    // as revision 2's receiver held it
    double residual = 0.0;

    for (int a = 0; a < n; a++)
    {
        const double noise = fit.m_noise;
        const double denominator = std::max(0.05, noise * inverseDiagonal[a]);
        residual += std::norm(noise * alpha[a]) / (denominator * denominator);
    }

    return (float) residual;
}

void DSDAngel61Receiver::fitGains(const ChannelFit& fit, int candidate, const std::complex<double> *alpha, std::complex<float> *gain)
{
    const int n = fit.m_nbAnchors;
    const int N = fit.m_nbSymbols;
    const int *anchors = fit.m_anchors.data();
    const double *correlation = fit.m_correlation.data() + (size_t) candidate * N;
    // Anchors outer, symbols inner: each symbol's sum still runs over the anchors in order (the same
    // result), and the correlation at |s - a| is two contiguous runs about the anchor, so it vectorises
    double re[MaxExtendedSymbols], im[MaxExtendedSymbols];
    std::fill(re, re + N, 0.0);
    std::fill(im, im + N, 0.0);

    for (int i = 0; i < n; i++)
    {
        const int a = anchors[i];
        const double ar = alpha[i].real(), ai = alpha[i].imag();

        for (int s = 0; s < a; s++)
        {
            const double r = correlation[a - s];
            re[s] += r * ar;
            im[s] += r * ai;
        }

        for (int s = a; s < N; s++)
        {
            const double r = correlation[s - a];
            re[s] += r * ar;
            im[s] += r * ai;
        }
    }

    for (int s = 0; s < N; s++) {
        gain[s] = std::complex<float>((float) re[s], (float) im[s]);
    }
}

const float *DSDAngel61Receiver::fitErrorVariance(ChannelFit& fit, int candidate)
{
    // 1 - r_s' A^-1 r_s = 1 - |L^-1 r_s|^2: the channel's power less what the anchors explain of
    // it. It is smallest at an anchor and largest between two, which is exactly what the decoder
    // has never been told before
    const int n = fit.m_nbAnchors;
    float *errorVariance = fit.m_errorVariance.data() + (size_t) candidate * fit.m_nbSymbols;

    if (fit.m_errorValid[candidate]) {
        return errorVariance;
    }

    const int *anchors = fit.m_anchors.data();
    const double *correlation = fit.m_correlation.data() + (size_t) candidate * fit.m_nbSymbols;
    const double *L = fit.m_factor.data() + (size_t) candidate * n * n;
    std::vector<double> v(n);

    for (int s = 0; s < fit.m_nbSymbols; s++)
    {
        double explained = 0.0;

        for (int i = 0; i < n; i++)
        {
            double x = correlation[std::abs(s - anchors[i])];
            const double *row = L + (size_t) i * n;

            for (int k = 0; k < i; k++) {
                x -= row[k] * v[k];
            }

            v[i] = x / row[i];
            explained += v[i] * v[i];
        }

        errorVariance[s] = (float) std::max(0.0, std::min(1.0, 1.0 - explained));
    }

    fit.m_errorValid[candidate] = 1;
    return errorVariance;
}

float DSDAngel61Receiver::trackGains(DVAngel61::Constellation constellation, std::complex<float> *equalised, const std::complex<float> *payloadReference, const float *confidence)
{
    // Decision directed channel estimation over the frame. Each symbol's instantaneous gain is
    // y_k conj(d_k) / |d_k|^2 with d_k the known symbol or the decision; the estimate is that gain
    // smoothed over a window whose length the frame itself chooses: the window that predicts the
    // known symbols best when each is left out of its own estimate (the whole frame on a static
    // channel, a few symbols in fast fading). The first decisions come from a forward and a
    // backward first order tracker started on the UW and header, so that a channel the pilots
    // are too far apart to follow is still followed from the last decision.
    using namespace std;
    const int N = m_frameSymbols;
    const int NT = m_nextUwValid ? m_extendedSymbols : N;   // symbols tracked: the next UW anchors the tail when it is there
    const int NH = m_layout->m_headerSymbols;
    bool known[MaxExtendedSymbols];
    bool formatKnown[MaxExtendedSymbols];
    complex<float> ref[MaxExtendedSymbols];
    complex<float> inst[MaxExtendedSymbols];

    for (int k = 0; k < NT; k++)
    {
        known[k] = k >= N ? true : m_layout->knownSymbol(k, ref[k]);

        if (k >= N) {
            ref[k] = m_layout->uwSymbol(k - N);
        }

        int h = (k < N) ? m_layout->headerIndexOfSymbol(k) : -1;

        if (!known[k] && (h >= 0))
        {
            known[k] = true;
            ref[k] = m_headerSymbols[h];
        }

        // What the format itself makes known, before any decoding round adds to it: these
        // symbols are at the same places in every frame, which is what lets the channel fit
        // reuse its weights (see wienerGains and m_wienerValid)
        formatKnown[k] = known[k];

        if (!known[k] && payloadReference)
        {
            // A decoding pass gave the payload symbols: those the decoder is sure of are references
            int p = m_layout->payloadIndexOfSymbol(k);

            if (!confidence || (confidence[p] >= ReferenceConfidence))
            {
                known[k] = true;
                ref[k] = payloadReference[p];
            }
        }
    }

    auto instantaneous = [&](int k, const complex<float>& d) { return m_symbols[k] * conj(d) / norm(d); };

    // Pass A: forward and backward trackers from the UW and header, their mean decides. The
    // tracker gain follows the SNR: fast fading is only followed where the SNR allows, and at
    // the threshold a slow tracker does not slip a quadrant on a burst of noise
    {
        complex<float> forward[MaxExtendedSymbols];
        complex<float> g(0.0f, 0.0f);
        const float alpha = max(TrackerGainMin, min(TrackerGainMax, 1.0f / (TrackerGainSnr * m_noiseVariance)));

        for (int k = 0; k < UWSymbols; k++) {
            g += instantaneous(k, ref[k]);
        }

        g /= (float) UWSymbols;

        // A known symbol re-anchors the tracker hard: after a deep fade the decisions can have
        // settled a quarter turn off, which only the known symbols can tell
        const float anchor = min(0.5f, 4.0f * alpha);
        // The absolute noise, for each tracker's exposure to fades since its last anchor
        const float absoluteNoise = m_noiseVariance * norm(g);
        float exposureForward[MaxExtendedSymbols];
        float exposure = 0.0f;

        for (int k = 0; k < NT; k++)
        {
            complex<float> d = known[k] ? ref[k] : hardDecision(constellation, m_symbols[k] / g);
            g += (known[k] ? anchor : alpha) * (instantaneous(k, d) - g);
            forward[k] = g;
            exposure = known[k] ? 0.0f : exposure + absoluteNoise / max(1e-12f, norm(g));
            exposureForward[k] = exposure;
        }

        // Backward, and the two combined by their exposure since their last anchor (the sum of
        // the inverse SNR along the way): a tracker that crossed a deep fade may have settled a
        // quarter turn off until its next anchor, so the one that did not cross it is believed
        exposure = 0.0f;

        for (int k = NT - 1; k >= 0; k--)
        {
            complex<float> d = known[k] ? ref[k] : hardDecision(constellation, m_symbols[k] / g);
            g += (known[k] ? anchor : alpha) * (instantaneous(k, d) - g);
            exposure = known[k] ? 0.0f : exposure + absoluteNoise / max(1e-12f, norm(g));
            float wb = (exposureForward[k] + 1e-3f) / (exposureForward[k] + exposure + 2e-3f);
            complex<float> combined = forward[k] * (1.0f - wb) + g * wb;
            inst[k] = known[k] ? instantaneous(k, ref[k]) : instantaneous(k, hardDecision(constellation, m_symbols[k] / combined));
        }
    }

    // The residual frequency as the least squares slope of the unwrapped phase of the
    // instantaneous gains smoothed over 12 symbols: removed before the smoothing of the gains,
    // which a phase ramp would defeat
    auto phaseStep = [&]() {
        const int w = 12;
        complex<float> sum(0.0f, 0.0f);
        for (int k = 0; k < w; k++) {
            sum += inst[k];
        }
        float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f, unwrapped = 0.0f, previous = 0.0f;
        int count = 0;
        for (int k = w / 2; k + w / 2 < NT; k++)
        {
            float phase = arg(sum);
            if (count == 0) {
                unwrapped = phase;
            } else {
                float d = phase - previous;
                while (d > (float) M_PI) d -= 2.0f * (float) M_PI;
                while (d < -(float) M_PI) d += 2.0f * (float) M_PI;
                unwrapped += d;
            }
            previous = phase;
            sx += k;
            sy += unwrapped;
            sxx += (float) k * k;
            sxy += k * unwrapped;
            count++;
            sum += inst[k + w / 2] - inst[k - w / 2];
        }
        float denominator = count * sxx - sx * sx;
        return denominator > 0.0f ? (count * sxy - sx * sy) / denominator : 0.0f;
    };
    auto derotate = [&](float step) {
        for (int k = 0; k < NT; k++)
        {
            complex<float> rotation(cos(-step * k), sin(-step * k));
            m_symbols[k] *= rotation;
            inst[k] *= rotation;
        }
    };
    // In fading the phases of the decisions follow the channel rather than the carrier, and a slope
    // fitted to them is the fading's own wander over the frame: removing it moves the channel's
    // spectrum off its carrier, which the fit cannot represent. It is removed only when the last
    // frame found the channel steady (and below, when this one does)
    float slope = ((m_genieMode == 2) || ((m_gainVariation > 3.0f) && (m_fitDopplerHz >= FadingSlopeHz))) ? 0.0f : phaseStep();
    derotate(slope);

    if (m_debug) {
        printf("angel61:   tracker pass slope %.1f Hz\n", slope * SampleRate / (2.0f * (float) M_PI * m_sps));
    }

    // Pass B: smoothing window by leave-one-out validation, twice (the second with the decisions
    // the first gave)
    const int scale = m_layout->m_symbolRate / 4800;   // the same times in both profiles: 20, 10, 5, 2.5, 1.25 ms
    const int windows[6] = { N, 96 * scale, 48 * scale, 24 * scale, 12 * scale, 6 * scale };
    int bestWindow = N;

    for (int pass = 0; pass < 2; pass++)
    {
        // Prefix sums of the instantaneous gains
        complex<float> prefix[MaxExtendedSymbols + 1];
        prefix[0] = complex<float>(0.0f, 0.0f);

        for (int k = 0; k < NT; k++) {
            prefix[k + 1] = prefix[k] + inst[k];
        }

        auto windowMean = [&](int k, int w, bool leaveOut) {
            int lo = max(0, k - w / 2);
            int hi = min(NT - 1, k + w / 2);
            complex<float> sum = prefix[hi + 1] - prefix[lo];
            int count = hi - lo + 1;
            if (leaveOut) {
                sum -= inst[k];
                count--;
            }
            return count > 0 ? sum / (float) count : inst[k];
        };

        // Every symbol validates (its decision, or the known value): 288 rather than 52 samples;
        // a shorter window has to earn its extra estimation noise by a margin
        float bestResidual = 1e30f;

        for (int w : windows)
        {
            float residual = 0.0f;

            for (int k = 0; k < NT; k++)
            {
                complex<float> g = windowMean(k, w, true);
                complex<float> d = known[k] ? ref[k] : hardDecision(constellation, m_symbols[k] / g);
                residual += norm(m_symbols[k] - g * d);
            }

            residual *= 1.0f + WindowPenalty / w;

            if (residual < bestResidual)
            {
                bestResidual = residual;
                bestWindow = w;
            }
        }

        // The channel interpolated from the known symbols for a Jakes spectrum, the Doppler
        // chosen by the same leave one out test. On a static channel the zero hypothesis is the
        // plain average the windows would have found; in fading it follows what the anchors can
        // support, which is what the windows cannot do between two pilots 7.5 ms apart.
        // The fit takes the symbols the format makes known and not the ones a decoding round
        // added. Two reasons, and they agree: those are the same symbols in every frame, so the
        // weights are solved once and reused, and a re-encoded symbol that the decoder got
        // wrong is worst exactly where the fit matters, in a fade. Admitting them measured
        // worse than leaving them out (18% against 14.5% of frames lost at 50 Hz Doppler).
        int nbAnchors = 0;
        int anchorIndex[MaxExtendedSymbols];
        complex<float> anchorValue[MaxExtendedSymbols];

        for (int k = 0; k < NT; k++)
        {
            if (formatKnown[k])
            {
                anchorIndex[nbAnchors] = k;
                anchorValue[nbAnchors] = inst[k];
                nbAnchors++;
            }
        }

        complex<float> bestWiener[MaxExtendedSymbols];
        float bestWienerResidual = 1e30f;
        float bestDoppler = -1.0f;
        float anchorResidual = 1e30f;
        int bestCandidate = -1;
        const float *bestError = nullptr;


        if (!m_windowOnly && (nbAnchors >= 8))
        {
            // The same measure for the window: its leave one out error over the anchors only,
            // so that the two candidates are judged alike
            anchorResidual = 0.0f;

            for (int a = 0; a < nbAnchors; a++)
            {
                int k = anchorIndex[a];
                anchorResidual += norm(anchorValue[a] - windowMean(k, bestWindow, true));
            }

            anchorResidual /= nbAnchors;
            ChannelFit& fit = channelFit(anchorIndex, nbAnchors, NT, max(1e-4f, m_noiseVariance));
            complex<double> alpha[MaxExtendedSymbols], bestAlpha[MaxExtendedSymbols];

            for (int c = 0; c < m_nbFitCandidates; c++)
            {
                float residual = fitResidual(fit, c, anchorValue, alpha) / nbAnchors;

                if (residual < bestWienerResidual)
                {
                    bestWienerResidual = residual;
                    bestDoppler = m_fitDopplers[c];
                    bestCandidate = c;
                    std::copy(alpha, alpha + nbAnchors, bestAlpha);
                }
            }

            if (bestCandidate >= 0)
            {
                bestError = fitErrorVariance(fit, bestCandidate);
                fitGains(fit, bestCandidate, bestAlpha, bestWiener);
            }
        }

        m_fitDopplerHz = max(0.0f, bestDoppler);
        // The fit is used when it beats the window by the margin, and whenever it found the
        // channel fading fast. The window's leave one out residual flatters it there: its
        // estimate at an anchor comes from the decisions beside it, which follow the channel
        // wherever they are wrong, and at 150 Hz of Doppler it was chosen over the fit on most
        // frames, losing 36% of NB tier 2's frames at Es/N0 12 dB (21% with the fit)
        if ((bestCandidate >= 0) && ((bestDoppler >= FitFadingHz) || (bestWienerResidual < anchorResidual * FitMargin)))
        {
            std::copy(bestWiener, bestWiener + NT, m_gain);
            std::copy(bestError, bestError + NT, m_fitError);

            m_dopplerHz = bestDoppler;

            if (m_debug) {
                printf("angel61:   channel by MMSE interpolation, Doppler %.0f Hz (residual %.4f against the window's %.4f)\n",
                    bestDoppler, bestWienerResidual, anchorResidual);
            }
        }
        else
        {
            for (int k = 0; k < NT; k++)
            {
                m_gain[k] = windowMean(k, bestWindow, false);
                m_fitError[k] = 0.0f;   // the window does not say, and it is chosen where it hardly matters
            }

            m_dopplerHz = 0.0f;
        }

        // A decoding round's references are anchors too, once the fit is what estimates the
        // channel: the fit above takes the format's symbols alone, so without this a round
        // decodes the same estimate again. The fit's Doppler, a local fit (localFit)
        if (payloadReference && (m_dopplerHz > 0.0f)) {
            localFit(known, ref, inst, NT, m_dopplerHz, max(1e-4f, m_noiseVariance));
        }

        for (int k = 0; k < N; k++)
        {
            equalised[k] = m_symbols[k] / m_gain[k];
            if (!known[k]) {
                inst[k] = instantaneous(k, hardDecision(constellation, equalised[k]));
            }
        }

        if (pass == 0)
        {
            // Better decisions, better frequency: once more before the final smoothing
            const bool steady = ((float) N / bestWindow <= 3.0f) || (m_fitDopplerHz < FadingSlopeHz);
            float step = ((m_genieMode == 2) || !steady) ? 0.0f : phaseStep();
            derotate(step);
            slope += step;

            if (m_debug) {
                printf("angel61:   pass 0 window %d slope %.1f Hz\n", bestWindow, step * SampleRate / (2.0f * (float) M_PI * m_sps));
            }
        }
    }

    // The noise: the channel's thermal noise, not the estimate's error as well. The two used to
    // be measured together, as the known symbols' residual against a window mean of the gains;
    // on a steady channel that is the noise, but in fading the window cannot follow the channel
    // and its lag was counted as noise too - 3 dB over the truth at the median at 100 Hz of
    // Doppler and 9 dB at the ninetieth percentile. The decoder is then told of the estimate's
    // error twice, since m_noisePerSymbol below adds the fit's own, and the payload lost three
    // times the frames it needed to.
    //
    // A second difference of the instantaneous gains, g[k-1] - 2 g[k] + g[k+1], cancels the
    // channel to second order in the Doppler over one symbol - a hundredth of the noise at
    // 100 Hz - and leaves the noise of three symbols. Its median, rather than its mean, keeps a
    // wrong decision (a quarter turn, large beside the noise) from counting: |x|^2 of a complex
    // Gaussian is exponential, whose median is ln 2 of its mean. The noise is in absolute terms
    // (before the equalisation) so that a symbol in a fade gets the low confidence it
    // deserves: its noise relative to the unit constellation is the absolute noise over its gain
    // squared
    {
        float level = 0.0f;
        float power = 0.0f;

        for (int k = 0; k < NT; k++)
        {
            level += abs(m_gain[k]);
            power += norm(m_gain[k]);
        }

        power /= NT;
        complex<float> decision[MaxExtendedSymbols];
        float second[MaxExtendedSymbols];
        int nbSecond = 0;

        for (int k = 0; k < NT; k++) {
            decision[k] = known[k] ? ref[k] : hardDecision(constellation, m_symbols[k] / m_gain[k]);
        }

        for (int k = 1; k + 1 < NT; k++)
        {
            complex<float> d = instantaneous(k - 1, decision[k - 1]) - 2.0f * instantaneous(k, decision[k])
                + instantaneous(k + 1, decision[k + 1]);
            // Each instantaneous gain carries the noise over its symbol's power
            float scale = 1.0f / norm(decision[k - 1]) + 4.0f / norm(decision[k]) + 1.0f / norm(decision[k + 1]);
            second[nbSecond++] = norm(d) / scale;
        }

        std::nth_element(second, second + nbSecond / 2, second + nbSecond);
        float absoluteNoise = max(1e-12f, second[nbSecond / 2] / 0.6931472f);
        m_noiseVariance = max(1e-4f, absoluteNoise / max(1e-12f, power));
        m_status.m_level = level / NT;
        // Where the estimate's error dominates the noise, it is larger than the fit's model says: WB
        // at 250 Hz floored at 2 to 3.5% lost at any SNR, and two to four times the error cut that
        // two to four times, but cost NB at its threshold (VHF results section 9). The frame's error
        // over its noise tells them apart: 4 to 6.6 in WB's floor, about 2 at NB tier 3's 150 Hz
        // threshold, 1.2 at 100 Hz, 0.6 at tier 1's, 0 on a steady channel. So the error is taken at
        // that ratio over 1.2, 1 to 4 times: WB's floor 40 to 75% fewer frames lost, NB within a
        // few frames in 3000 (VHF results section 13)
        double estimateSum = 0.0, thermalSum = 0.0;
        for (int k = 0; k < N; k++)
        {
            const float gain2 = max(1e-3f * power, norm(m_gain[k]));
            estimateSum += power * m_fitError[k] / gain2;
            thermalSum += absoluteNoise / gain2;
        }
        const float errorScale = (float) std::min(EstimateScaleMax, std::max(1.0, estimateSum / std::max(1e-12, thermalSum) / EstimateRatioKnee));
        for (int k = 0; k < N; k++) {
            // The noise the decoder is given is the channel's noise **and** what the estimate
            // does not know: equalising by an estimate that is off by e turns a unit symbol into
            // one off by e as well, so the two variances add. The fit's error is largest halfway
            // between two pilots, which is where the decoder used to be told everything was fine
            const float gain2 = max(1e-3f * power, norm(m_gain[k]));
            m_noisePerSymbol[k] = (absoluteNoise + errorScale * power * m_fitError[k]) / gain2;
            m_thermalPerSymbol[k] = absoluteNoise / gain2;
            m_estimatePerSymbol[k] = errorScale * power * m_fitError[k] / gain2;
        }
    }

    m_gainVariation = (float) N / bestWindow;
    return slope;
}

void DSDAngel61Receiver::localFit(const bool *known, const std::complex<float> *ref, const std::complex<float> *inst, int nbSymbols, float dopplerHz, float noise)
{
    // The whole frame's Wiener fit with a round's references among its anchors would be solved
    // anew for every frame (the references are wherever the decoder was sure), in the hundreds
    // of anchors. The nearest LocalAnchors of each symbol hold nearly all of what the fit knows
    // there (Jakes' correlation at 150 Hz is gone within a few milliseconds), and their matrix is
    // a small one, factored once for each run of symbols that shares them
    using namespace std;
    const int M = LocalAnchors;
    int anchors[MaxExtendedSymbols];
    float anchorNoise[MaxExtendedSymbols];
    int nbAnchors = 0;

    for (int k = 0; k < nbSymbols; k++)
    {
        if (known[k])
        {
            anchors[nbAnchors] = k;
            // An anchor's measurement y conj(d) / |d|^2 carries the noise over |d|^2
            anchorNoise[nbAnchors] = noise / max(1e-3f, norm(ref[k]));
            nbAnchors++;
        }
    }

    const int m = min(M, nbAnchors);

    if (m < 2) {
        return;
    }

    // The Jakes correlation at every whole number of symbols
    const double scale = 2.0 * M_PI * dopplerHz / m_layout->m_symbolRate;
    double correlation[MaxExtendedSymbols];

    for (int d = 0; d < nbSymbols; d++) {
        correlation[d] = besselJ0(scale * d);
    }

    // The nearest m anchors of a symbol are a run of the sorted anchors: the factor of that run's
    // matrix is kept while the run is the same
    int run = -1;
    double factor[LocalAnchors * LocalAnchors];
    int first = 0;

    for (int k = 0; k < nbSymbols; k++)
    {
        // Slide the run while the anchor after it is nearer than its first
        while ((first + m < nbAnchors) && (abs(anchors[first + m] - k) < abs(k - anchors[first]))) {
            first++;
        }

        if (first != run)
        {
            // Cholesky of R + diag(noise) over the run
            run = first;

            for (int i = 0; i < m; i++)
            {
                for (int j = 0; j <= i; j++)
                {
                    double sum = correlation[abs(anchors[first + i] - anchors[first + j])] + (i == j ? anchorNoise[first + i] : 0.0);

                    for (int t = 0; t < j; t++) {
                        sum -= factor[i * LocalAnchors + t] * factor[j * LocalAnchors + t];
                    }

                    factor[i * LocalAnchors + j] = (i == j) ? sqrt(max(1e-12, sum)) : sum / factor[j * LocalAnchors + j];
                }
            }
        }

        // w = (R + N)^-1 r: forward and back substitution
        double r[LocalAnchors], y[LocalAnchors], w[LocalAnchors];

        for (int i = 0; i < m; i++) {
            r[i] = correlation[abs(k - anchors[first + i])];
        }

        for (int i = 0; i < m; i++)
        {
            double sum = r[i];
            for (int t = 0; t < i; t++) sum -= factor[i * LocalAnchors + t] * y[t];
            y[i] = sum / factor[i * LocalAnchors + i];
        }

        for (int i = m - 1; i >= 0; i--)
        {
            double sum = y[i];
            for (int t = i + 1; t < m; t++) sum -= factor[t * LocalAnchors + i] * w[t];
            w[i] = sum / factor[i * LocalAnchors + i];
        }

        complex<float> g(0.0f, 0.0f);
        double explained = 0.0;

        for (int i = 0; i < m; i++)
        {
            g += (float) w[i] * inst[anchors[first + i]];
            explained += w[i] * r[i];
        }

        m_gain[k] = g;
        m_fitError[k] = (float) max(0.0, min(1.0, 1.0 - explained));
    }
}

std::complex<float> DSDAngel61Receiver::hardDecision(DVAngel61::Constellation constellation, const std::complex<float>& z)
{
    if (constellation == DVAngel61::QPSK)
    {
        const float a = 0.70710678f;
        return std::complex<float>(z.real() < 0.0f ? -a : a, z.imag() < 0.0f ? -a : a);
    }

    const float a = 0.31622777f;
    auto level = [a](float v) {
        float m = std::fabs(v) < 2.0f * a ? a : 3.0f * a;
        return v < 0.0f ? -m : m;
    };
    return std::complex<float>(level(z.real()), level(z.imag()));
}

// ---- Revision 3 -------------------------------------------------------------------------------

const DSDAngel61Receiver::Layout& DSDAngel61Receiver::layout(DVAngel61::Profile profile)
{
    auto build = [](DVAngel61::Profile p) {
        const DVAngel61::Layout& r = DVAngel61::layout(p);
        const bool wb = p == DVAngel61::ProfileWB;
        Layout l;
        l.m_profile = p;
        l.m_symbolRate = wb ? 9600 : 4800;
        l.m_samplesPerSymbol = SampleRate / l.m_symbolRate;
        l.m_frameSymbols = r.m_cells;
        l.m_payloadSymbols = r.payloadCount();
        l.m_pilotSymbols = (int) r.m_pilotCells.size();
        l.m_headerSymbols = (int) r.m_headerCells.size();
        l.m_rrcTaps = DVAngel61::rrcTapCount(l.m_samplesPerSymbol);
        l.m_pilotPositions = r.m_pilotCells;
        l.m_headerPositions = r.m_headerCells;
        l.m_payloadPositions = r.m_payloadCells;
        l.m_symbolKind.assign(r.m_cells, -1);
        l.m_symbolIndex.assign(r.m_cells, -1);
        l.m_symbolGroup.assign(r.m_cells, -1);

        for (size_t i = 0; i < r.m_uwCells.size() && i < 16; i++)
        {
            l.m_symbolKind[r.m_uwCells[i]] = 0;
            l.m_symbolIndex[r.m_uwCells[i]] = (short) i;
            l.m_uw[i] = r.m_known[r.m_uwCells[i]];
        }
        for (size_t i = 0; i < r.m_pilotCells.size(); i++)
        {
            l.m_symbolKind[r.m_pilotCells[i]] = 1;
            l.m_symbolIndex[r.m_pilotCells[i]] = (short) i;
            l.m_pilots.push_back(r.m_known[r.m_pilotCells[i]]);
        }
        for (size_t i = 0; i < r.m_headerCells.size(); i++)
        {
            l.m_symbolKind[r.m_headerCells[i]] = 2;
            l.m_symbolIndex[r.m_headerCells[i]] = (short) i;
        }
        for (size_t i = 0; i < r.m_payloadCells.size(); i++)
        {
            l.m_symbolKind[r.m_payloadCells[i]] = 3;
            l.m_symbolIndex[r.m_payloadCells[i]] = (short) i;
        }

        // The anchor groups (2.2): each run of pilot and header symbols between payload parts
        int groups = 0;

        for (int s = 0; s < r.m_cells; s++)
        {
            const bool anchor = (l.m_symbolKind[s] == 1) || (l.m_symbolKind[s] == 2);

            if (anchor)
            {
                const bool continues = (s > 0) && ((l.m_symbolKind[s - 1] == 1) || (l.m_symbolKind[s - 1] == 2));
                if (!continues) {
                    groups++;
                }
                l.m_symbolGroup[s] = (signed char) (groups - 1);
            }
        }

        l.m_nbGroups = groups;
        return l;
    };
    static const Layout nb = build(DVAngel61::ProfileNB);
    static const Layout wb = build(DVAngel61::ProfileWB);
    return profile == DVAngel61::ProfileWB ? wb : nb;
}

void DSDAngel61Receiver::payloadLLR(const std::complex<float> *equalised, int type, const float *noisePerSymbol, float *llr) const
{
    const DVAngel61::Constellation k = DVAngel61::constellationOf(type);
    const int bps = DVAngel61::bitsPerCell(k);

    // (16-QAM with the symbols' own noise: amplitude-aware, the estimate's error going with the
    // symbol's power, since it is the gain that is wrong - ANGEL61.md 5)
    const bool amplitude = m_amplitudeAware && noisePerSymbol && (noisePerSymbol == m_noisePerSymbol) && (k == DVAngel61::QAM16);

    for (int p = 0; p < m_layout->m_payloadSymbols; p++)
    {
        const int s = m_layout->m_payloadPositions[p];

        if (amplitude) {
            DVAngel61::demapCellsAmplitude(k, equalised + s, 1, m_thermalPerSymbol[s], m_estimatePerSymbol[s], llr + bps * p);
        } else {
            DVAngel61::demapCells(k, equalised + s, 1, noisePerSymbol ? noisePerSymbol[s] : m_noiseVariance, llr + bps * p);
        }
    }
}

bool DSDAngel61Receiver::decodePayload(const DVAngel61::Header& header, Frame& frame, std::vector<uint8_t>& codeword, std::vector<float>& posterior) const
{
    frame.m_block.assign(DVAngel61::infoBits(header.m_type), 0);
    const bool ok = DVAngel61::decodePayload(m_llr.data(), header, frame.m_block.data(), &frame.m_iterations, codeword.data(), posterior.data());

    // The channel's bit errors the code corrected: the received decisions against the codeword
    const int n = DVAngel61::codedBits(header.m_type);
    std::vector<float> coded(n);
    DVAngel61::positionsToCodeword(m_llr.data(), header.m_counter, DVAngel61::codeOf(header.m_type), coded.data());
    int errors = 0;

    for (int i = 0; i < n; i++) {
        errors += (coded[i] < 0.0f) != (codeword[i] != 0);   // an LLR is positive for a 0
    }

    frame.m_channelErrors = ok ? errors : 0;
    frame.m_codedBits = ok ? n : 0;
    return ok;
}
