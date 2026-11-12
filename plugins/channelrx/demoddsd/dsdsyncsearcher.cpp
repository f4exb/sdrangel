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
#include <cstdint>
#include <cmath>

#include "dsdsyncsearcher.h"

DSDSyncSearcher::DSDSyncSearcher() :
    m_sps(10),
    m_threshold(0.85f),
    m_polarityBias(0.02f),
    m_windowLength(DVSync::m_maxLength + 1),
    m_stride(1),
    m_window(nullptr),
    m_maxLength(0),
    m_sampleIndex(0),
    m_pushed(0),
    m_settle(0),
    m_hasCandidate(false),
    m_candidatePattern(-1),
    m_candidateSample(0),
    m_candidateRho(0.0f),
    m_candidateRhoBefore(0.0f)
{
    m_candidate = Detection();
    setPatterns(std::vector<DVSync::Pattern>(), 10);
}

void DSDSyncSearcher::setPatterns(const std::vector<DVSync::Pattern>& patterns, int samplesPerSymbol, const std::vector<float>& kernel)
{
    m_sps = samplesPerSymbol;
    m_patterns.clear();
    int centre = (int) kernel.size() / 2;

    for (DVSync::Pattern pattern : patterns)
    {
        const DVSync::Info& info = DVSync::get(pattern);
        PatternState p;
        p.m_pattern = pattern;
        p.m_length = info.m_length;
        p.m_levels.assign(info.m_levels, info.m_levels + info.m_length);

        if (!kernel.empty())
        {
            for (int i = 0; i < info.m_length; i++)
            {
                float t = 0.0f;

                for (int j = 0; j < (int) kernel.size(); j++)
                {
                    int k = i + j - centre;
                    t += kernel[j] * ((k >= 0) && (k < info.m_length) ? info.m_levels[k] : 0);
                }

                p.m_levels[i] = t;
            }
        }
        p.m_sumT = 0.0f;
        p.m_sumTT = 0.0f;

        for (float t : p.m_levels)
        {
            p.m_sumT += t;
            p.m_sumTT += t * t;
        }

        std::fill(p.m_rhoHistory, p.m_rhoHistory + 3, 0.0f);
        m_patterns.push_back(p);
    }

    m_maxLength = 0;

    for (const PatternState& p : m_patterns) {
        m_maxLength = std::max(m_maxLength, p.m_length);
    }

    // The fits are evaluated five times per symbol: the correlation peak is a symbol wide and
    // its position is interpolated, the burst refinement measures the timing properly afterwards
    m_stride = std::max(1, m_sps / 5);
    m_phaseBuffers.assign((size_t) m_sps * 2 * m_windowLength, 0.0f);
    m_sumR.assign(m_windowLength + 1, 0.0f);
    m_sumRR.assign(m_windowLength + 1, 0.0f);
    reset();
}

void DSDSyncSearcher::reset(int settleSamples)
{
    std::fill(m_phaseBuffers.begin(), m_phaseBuffers.end(), 0.0f);
    m_window = m_phaseBuffers.data() + m_windowLength - 1;    // the sample just pushed is the window's last
    m_pushed = 0;
    m_settle = settleSamples;
    m_hasCandidate = false;

    for (PatternState& p : m_patterns) {
        std::fill(p.m_rhoHistory, p.m_rhoHistory + 3, 0.0f);
    }
}

void DSDSyncSearcher::updateSums() const
{
    // Sums of the last k window samples and their squares, for every k up to the longest pattern
    const float *r = m_window + m_windowLength - 1;
    float sumR = 0.0f, sumRR = 0.0f;
    m_sumR[0] = 0.0f;
    m_sumRR[0] = 0.0f;

    for (int k = 1; k <= m_maxLength; k++)
    {
        float v = r[1 - k];
        sumR += v;
        sumRR += v * v;
        m_sumR[k] = sumR;
        m_sumRR[k] = sumRR;
    }
}

bool DSDSyncSearcher::fitAt(const PatternState& p, Detection& detection, float& rho) const
{
    // The window's last k samples against the pattern's levels; their sums are shared by every
    // pattern of that length
    int k = p.m_length;
    const float *r = m_window + m_windowLength - k;
    const float *t = p.m_levels.data();
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;   // four partial sums (see Chain::filter)
    int i = 0;

    for (; i + 4 <= k; i += 4)
    {
        s0 += r[i] * t[i];
        s1 += r[i + 1] * t[i + 1];
        s2 += r[i + 2] * t[i + 2];
        s3 += r[i + 3] * t[i + 3];
    }

    for (; i < k; i++) {
        s0 += r[i] * t[i];
    }

    float sumRT = (s0 + s1) + (s2 + s3);

    float sumR = m_sumR[k], sumRR = m_sumRR[k];
    float n = (float) k;
    float covariance = n * sumRT - sumR * p.m_sumT;
    float varianceT = n * p.m_sumTT - p.m_sumT * p.m_sumT;
    float varianceR = n * sumRR - sumR * sumR;

    if ((varianceR <= 1e-6f) || (varianceT <= 0.0f))
    {
        rho = 0.0f;
        return false;
    }

    rho = covariance / std::sqrt(varianceT * varianceR);
    float a = covariance / varianceT;
    float b = (sumR - a * p.m_sumT) / n;
    // Residual sum of squares: sumRR - 2 a sumRT - 2 b sumR + a^2 sumTT + 2 a b sumT + n b^2
    float rss = sumRR - 2.0f * a * sumRT - 2.0f * b * sumR + a * a * p.m_sumTT + 2.0f * a * b * p.m_sumT + n * b * b;
    detection.m_pattern = p.m_pattern;
    detection.m_rho = rho;
    detection.m_scale = std::fabs(a);
    detection.m_offset = b;
    detection.m_sigma2 = std::max(0.0f, rss) / (float) std::max(1, k - 2);
    detection.m_inverted = a < 0.0f;
    detection.m_fraction = 0.0f;
    return true;
}

bool DSDSyncSearcher::fit(DVSync::Pattern pattern, Detection& detection) const
{
    for (const PatternState& p : m_patterns)
    {
        if (p.m_pattern == pattern)
        {
            float rho;
            updateSums();
            bool ok = fitAt(p, detection, rho);
            detection.m_sample = m_sampleIndex;
            return ok;
        }
    }

    return false;
}

bool DSDSyncSearcher::push(float y, int64_t sampleIndex, Detection& detection)
{
    // Into the buffer of this sample's phase, twice, and the window is the m_windowLength
    // samples of that phase ending here
    const int phase = (int) (m_pushed % m_sps);
    const int slot = (int) ((m_pushed / m_sps) % m_windowLength);
    float *buffer = m_phaseBuffers.data() + (size_t) phase * 2 * m_windowLength;
    buffer[slot] = y;
    buffer[slot + m_windowLength] = y;
    m_window = buffer + slot + 1;
    m_sampleIndex = sampleIndex;
    m_pushed++;
    bool emitted = false;

    // Emit the candidate once the peak is clearly behind us
    if (m_hasCandidate && (sampleIndex - m_candidateSample >= m_sps / 2))
    {
        detection = m_candidate;
        // Parabolic interpolation of |rho| around the peak: history holds rho at peak-1, peak, peak+1
        const PatternState& p = m_patterns[m_candidatePattern];
        float r0 = std::fabs(m_candidateRhoBefore);
        float r1 = std::fabs(m_candidateRho);
        float r2 = std::fabs(p.m_rhoHistory[2]);
        float denominator = r0 - 2.0f * r1 + r2;
        float fraction = (std::fabs(denominator) > 1e-9f) ? 0.5f * (r0 - r2) / denominator : 0.0f;
        detection.m_fraction = std::max(-0.5f, std::min(0.5f, fraction)) * m_stride;  // the evaluations are m_stride apart
        m_hasCandidate = false;
        emitted = true;
    }

    if (m_pushed % m_stride != 0) {
        return emitted;
    }

    updateSums();

    for (size_t i = 0; i < m_patterns.size(); i++)
    {
        PatternState& p = m_patterns[i];

        if (m_pushed < (long long) p.m_length * m_sps + m_settle) {
            continue;
        }

        Detection d;
        float rho;

        if (!fitAt(p, d, rho)) {
            rho = 0.0f;
        }

        float previous = p.m_rhoHistory[0];
        p.m_rhoHistory[0] = rho;

        // rhoHistory[1] is the rho at the candidate sample, [2] the one at the evaluation after it
        if (m_hasCandidate && ((int) i == m_candidatePattern) && (sampleIndex == m_candidateSample + m_stride)) {
            p.m_rhoHistory[2] = rho;
        }

        // A non inverted fit is preferred over an inverted one of the same strength: some
        // patterns are the negation of another (DMR voice and data syncs) so the sign alone is
        // ambiguous, and a normal polarity signal is the usual case
        float score = std::fabs(rho) + (rho > 0.0f ? m_polarityBias : 0.0f);
        float candidateScore = std::fabs(m_candidateRho) + (m_candidateRho > 0.0f ? m_polarityBias : 0.0f);

        if ((std::fabs(rho) >= thresholdFor(m_threshold, DVSync::get(p.m_pattern).m_length))
            && (!m_hasCandidate || (score > candidateScore)))
        {
            m_hasCandidate = true;
            m_candidatePattern = (int) i;
            m_candidateSample = sampleIndex;
            m_candidateRho = rho;
            m_candidateRhoBefore = previous;
            m_candidate = d;
            m_candidate.m_sample = sampleIndex;
            p.m_rhoHistory[1] = rho;
            p.m_rhoHistory[2] = rho;
        }
    }

    return emitted;
}
