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
#include <cstdlib>

#include "dsdsymbolclock.h"

DSDSymbolClock::DSDSymbolClock() :
    m_sps(10),
    m_strobe(0.0),
    m_rate(0.0),
    m_gainPhase(0.02f),
    m_gainRate(0.0002f),
    m_hold(false),
    m_acquisitionLength(0),
    m_acquisition(0),
    m_acquisitionSum(0.0),
    m_slope(0.96f),
    m_correction(0.0),
    m_correctionSymbols(0),
    m_buffer(16384, 0.0f), // a P25 voice unit is 8640 samples
    m_newest(-1),
    m_first(-1),
    m_lastU(0.0f),
    m_lastD(0),
    m_lastReliability(0.0f),
    m_lastReliable(false)
{
}

void DSDSymbolClock::setSamplesPerSymbol(int sps)
{
    m_sps = sps;
    setLoopBandwidth(0.01f);
}

void DSDSymbolClock::setLoopBandwidth(float bandwidth)
{
    // Second order loop with damping 1: gains scaled to samples per unit timing error. The M&M
    // error in normalised units is 0.96 per sample of offset for 4FSK through the raised cosine
    // composite (measured in dsddemodtest --unit).
    m_slope = 0.96f;
    m_gainPhase = 4.0f * bandwidth / m_slope;
    m_gainRate = 0.0f; // the rate is measured over bursts and syncs (setRate), not integrated here
}

void DSDSymbolClock::reset()
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    m_newest = -1;
    m_first = -1;
    m_strobe = 0.0;
    m_rate = 0.0;
    m_lastReliable = false;
    m_hold = false;
}

void DSDSymbolClock::resetLoop()
{
    m_rate = 0.0;
    m_lastReliable = false;
    m_hold = false;
    m_acquisition = 0;
    m_acquisitionSum = 0.0;
    m_correction = 0.0;
    m_correctionSymbols = 0;
}

void DSDSymbolClock::setHold(bool hold)
{
    if (m_hold && !hold)
    {
        m_acquisition = m_acquisitionLength;
        m_acquisitionSum = 0.0;
        m_lastReliable = false;
    }

    m_hold = hold;
}

double DSDSymbolClock::takeCorrection(int& symbols)
{
    double c = m_correction;
    symbols = m_correctionSymbols;
    m_correction = 0.0;
    m_correctionSymbols = 0;
    return c;
}

void DSDSymbolClock::setRate(double rate)
{
    m_rate = std::max(-0.02 * m_sps, std::min(0.02 * m_sps, rate));
}

void DSDSymbolClock::push(float y, int64_t sampleIndex)
{
    m_newest = sampleIndex;
    m_first = m_first < 0 ? sampleIndex : m_first;
    m_buffer[sampleIndex % (long long) m_buffer.size()] = y;
}

float DSDSymbolClock::at(int64_t sampleIndex) const
{
    if ((sampleIndex < std::max<int64_t>(0, m_first)) || (sampleIndex > m_newest) || (m_newest - sampleIndex >= (long long) m_buffer.size())) {
        return 0.0f;
    }

    return m_buffer[sampleIndex % (long long) m_buffer.size()];
}

void DSDSymbolClock::setPhase(double symbolCentre)
{
    m_strobe = symbolCentre + m_sps + m_rate;
    m_lastReliable = false;
}

void DSDSymbolClock::adjust(double samples)
{
    m_strobe += std::max(-2.0, std::min(2.0, samples));
}

bool DSDSymbolClock::ready() const
{
    return m_newest >= (long long) std::floor(m_strobe) + 2;
}

float DSDSymbolClock::interpolateAt(double position) const
{
    int64_t m = (long long) std::floor(position);
    float mu = (float) (position - m);
    float p0 = at(m - 1), p1 = at(m), p2 = at(m + 1), p3 = at(m + 2);
    // Cubic Lagrange interpolation at m + mu
    float a = p3 - p2 - p0 + p1;
    float b = p0 - p1 - a;
    float c = p2 - p0;
    return ((a * mu + b) * mu + c) * mu + p1;
}

float DSDSymbolClock::next(double& centre)
{
    float y = interpolateAt(m_strobe);
    centre = m_strobe;
    m_strobe += m_sps + m_rate;
    return y;
}

void DSDSymbolClock::update(float u, int d, float reliability)
{
    if (m_lastReliable && !m_hold)
    {
        // Mueller and Muller: positive when the strobe is early. Unreliable decisions are
        // weighted down rather than ignored, otherwise a large offset (every decision doubtful)
        // could never be pulled in.
        float e = u * (float) m_lastD - m_lastU * (float) d;
        e = std::max(-4.0f, std::min(4.0f, e));
        e *= 0.25f + 0.75f * reliability * m_lastReliability;

        if (m_acquisition > 0)
        {
            // After a gap: average the error over a few symbols, then apply one correction
            m_acquisitionSum += e;

            if (--m_acquisition == 0)
            {
                double step = std::max(-3.0, std::min(3.0, m_acquisitionSum / m_acquisitionLength / m_slope));
                m_strobe += step;
                m_correction += step;
            }
        }
        else
        {
            m_strobe += m_gainPhase * e;
            m_correction += m_gainPhase * e;
            m_rate += m_gainRate * e;
            m_rate = std::max(-0.02 * m_sps, std::min(0.02 * m_sps, m_rate));
        }
    }

    m_correctionSymbols++; // every symbol period, held or not: the drift accumulates through gaps too

    m_lastU = u;
    m_lastD = d;
    m_lastReliability = reliability;
    m_lastReliable = true;
}
