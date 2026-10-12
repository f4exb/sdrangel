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

#include "droneiddecoder.h"
#include "droneiddetector.h"

DroneIDDetector::DroneIDDetector() :
    m_threshold(0.20f),
    m_x(m_historySize),
    m_c(m_productSize),
    m_c2(m_productSize),
    m_e(m_productSize),
    m_p(m_corrSize),
    m_p2(m_corrSize),
    m_r(m_corrSize),
    m_rr(m_corrSize)
{
    std::vector<int> starts;
    DroneIDDecoder::symbolStarts(starts);
    const int last = DroneIDDecoder::m_nbSymbols - 1;
    for (int k = 0; k < DroneIDDecoder::m_nbSymbols; k++)
    {
        m_offsets[k] = (starts[last] + DroneIDDecoder::m_cpLengths[last])
            - (starts[k] + DroneIDDecoder::m_cpLengths[k]);
    }
    reset();
}

void DroneIDDetector::reset()
{
    std::fill(m_x.begin(), m_x.end(), cf(0.0f, 0.0f));
    std::fill(m_c.begin(), m_c.end(), cf(0.0f, 0.0f));
    std::fill(m_c2.begin(), m_c2.end(), cf(0.0f, 0.0f));
    std::fill(m_e.begin(), m_e.end(), 0.0f);
    std::fill(m_p.begin(), m_p.end(), cf(0.0f, 0.0f));
    std::fill(m_p2.begin(), m_p2.end(), cf(0.0f, 0.0f));
    std::fill(m_r.begin(), m_r.end(), 0.0f);
    std::fill(m_rr.begin(), m_rr.end(), 0.0f);
    m_pSum = 0.0;
    m_p2Sum = 0.0;
    m_rSum = 0.0;
    m_n = 0;
    m_state = Searching;
    m_peakMetric = 0.0f;
    m_peakScore = 0.0f;
    m_peakIndex = 0;
    m_searchStart = 0;
    m_captureEnd = 0;
    m_holdoffEnd = 0;
    m_burstMetric = 0.0f;
    m_burstIndex = 0;
}

// Recalculate running sums from stored products, to prevent accumulation of rounding errors
void DroneIDDetector::recalculate()
{
    m_pSum = 0.0;
    m_p2Sum = 0.0;
    m_rSum = 0.0;
    for (int i = 0; i < m_window; i++)
    {
        int64_t n = m_n - i;
        m_pSum += (std::complex<double>) m_c[n & m_productMask];
        m_p2Sum += (std::complex<double>) m_c2[n & m_productMask];
        m_rSum += m_e[n & m_productMask];
    }
}

// Frame correlation for frame whose last symbol's CP correlation ends at sample n.
// Returns normalised metric, and sets score to the unnormalised value.
// If gate is true, returns 0 as soon as the metric is known to be below the threshold,
// to save computation while searching.
float DroneIDDetector::metric(int64_t n, float& score, bool gate) const
{
    cf f = 0.0f;
    float e = 0.0f;

    for (int k = 0; k < DroneIDDecoder::m_nbSymbols; k++)
    {
        int i = (int) ((n - m_offsets[k]) & m_corrMask);
        f += m_p[i];
        e += m_rr[i];
    }

    // The metric can not exceed |f|/e, so skip the square roots when below the threshold
    float te = m_threshold * e;
    if (gate && (std::norm(f) <= te * te))
    {
        score = 0.0f;
        return 0.0f;
    }

    cf f2 = 0.0f;
    for (int k = 0; k < DroneIDDecoder::m_nbSymbols; k++) {
        f2 += m_p2[(n - m_offsets[k]) & m_corrMask];
    }
    score = std::abs(f) - std::abs(f2);
    if (e <= 0.0f) {
        return 0.0f;
    }
    return score / e;
}

bool DroneIDDetector::process(cf x)
{
    m_n++;
    const int64_t n = m_n;

    m_x[n & m_historyMask] = x;

    // Products of this sample with earlier samples. These are stored, so they don't need to be
    // recalculated when they leave the window, and exactly what was added is subtracted
    const cf xl = m_x[(n - m_lag) & m_historyMask];
    const cf xl2 = m_x[(n - m_lag2) & m_historyMask];
    const cf c(x.real() * xl.real() + x.imag() * xl.imag(), x.imag() * xl.real() - x.real() * xl.imag());
    const cf c2(x.real() * xl2.real() + x.imag() * xl2.imag(), x.imag() * xl2.real() - x.real() * xl2.imag());
    const float e = x.real() * x.real() + x.imag() * x.imag();
    const int pi = (int) (n & m_productMask);
    const int po = (int) ((n - m_window) & m_productMask);

    // Update windowed correlations and energy
    m_pSum += std::complex<double>((double) c.real() - (double) m_c[po].real(), (double) c.imag() - (double) m_c[po].imag());
    m_p2Sum += std::complex<double>((double) c2.real() - (double) m_c2[po].real(), (double) c2.imag() - (double) m_c2[po].imag());
    m_rSum += (double) e - (double) m_e[po];
    m_c[pi] = c;
    m_c2[pi] = c2;
    m_e[pi] = e;

    if ((n & (m_recalcInterval - 1)) == 0) {
        recalculate();
    }

    const int ci = (int) (n & m_corrMask);
    const float r = (float) std::max(m_rSum, 0.0);
    m_p[ci] = cf((float) m_pSum.real(), (float) m_pSum.imag());
    m_p2[ci] = cf((float) m_p2Sum.real(), (float) m_p2Sum.imag());
    m_r[ci] = r;
    m_rr[ci] = 0.5f * (r + m_r[(n - m_lag) & m_corrMask]);

    // Wait until there's enough history for a frame
    if (n < DroneIDDecoder::m_frameLength + m_lag + m_window) {
        return false;
    }

    // Frame start, if last symbol's correlation peak is at n
    const int lastPeakOffset = m_offsets[0] + DroneIDDecoder::m_cpLengths[0] + m_lag - 1;

    switch (m_state)
    {
    case Searching:
    {
        if ((n < m_holdoffEnd) || ((n % m_searchStride) != 0)) {
            break;
        }
        float score;
        float m = metric(n, score, true);
        if (m > m_threshold)
        {
            m_peakMetric = m;
            m_peakScore = score;
            m_peakIndex = n;
            m_searchStart = n;
            m_state = FindingPeak;
        }
        break;
    }
    case FindingPeak:
    {
        float score;
        float m = metric(n, score, false);
        if ((m > m_threshold) && (score > m_peakScore))
        {
            m_peakMetric = m;
            m_peakScore = score;
            m_peakIndex = n;
        }
        if (n - m_searchStart >= m_peakSearchLength)
        {
            int64_t frameStart = m_peakIndex - lastPeakOffset;
            m_captureEnd = frameStart + DroneIDDecoder::m_frameLength + m_margin;
            m_state = Capturing;
        }
        else
        {
            break;
        }
    }
    [[fallthrough]];
    case Capturing:
    {
        if (n >= m_captureEnd)
        {
            int64_t frameStart = m_peakIndex - lastPeakOffset;
            int64_t start = frameStart - m_margin;
            int length = (int) (m_captureEnd - start + 1);

            m_burst.resize(length);
            for (int i = 0; i < length; i++) {
                m_burst[i] = m_x[(start + i) & m_historyMask];
            }
            m_burstMetric = m_peakMetric;
            m_burstIndex = start;
            // Partial alignments with the end of this frame give high metrics for another frame's duration
            m_holdoffEnd = m_peakIndex + DroneIDDecoder::m_frameLength;
            m_state = Searching;
            return true;
        }
        break;
    }
    }

    return false;
}
