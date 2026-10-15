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
#include <numeric>

#include "lookaheadlimiter.h"

LookaheadLimiter::LookaheadLimiter()
{
    configure(1, 1);
}

void LookaheadLimiter::configure(int lookaheadSamples, int releaseSamples)
{
    m_lookahead = std::max(lookaheadSamples, 1);
    m_release = 1.0f - std::exp(-1.0f / std::max(releaseSamples, 1));
    m_delay.resize(m_lookahead);
    m_minCount.resize(m_lookahead + 2);
    m_minGain.resize(m_lookahead + 2);
    m_box.resize(m_lookahead);
    reset();
}

void LookaheadLimiter::reset()
{
    std::fill(m_delay.begin(), m_delay.end(), Complex(0.0f, 0.0f));
    m_delayIdx = 0;
    m_minHead = 0;
    m_minSize = 0;
    std::fill(m_box.begin(), m_box.end(), 1.0f);
    m_boxSum = m_lookahead;
    m_boxIdx = 0;
    m_count = 0;
    m_gain = 1.0f;
}

Complex LookaheadLimiter::process(const Complex& input)
{
    const int capacity = (int) m_minGain.size();
    const float peak = std::max(std::fabs(input.real()), std::fabs(input.imag()));
    const float required = peak > 1.0f ? 1.0f / peak : 1.0f;

    // Minimum required gain over samples m_count - m_lookahead to m_count
    while ((m_minSize > 0) && (m_minGain[(m_minHead + m_minSize - 1) % capacity] >= required)) {
        m_minSize--;
    }
    const int tail = (m_minHead + m_minSize) % capacity;
    m_minCount[tail] = m_count;
    m_minGain[tail] = required;
    m_minSize++;
    while (m_minCount[m_minHead] < m_count - m_lookahead)
    {
        m_minHead = (m_minHead + 1) % capacity;
        m_minSize--;
    }
    const float windowMin = m_minGain[m_minHead];

    // Each of the last m_lookahead window minimums covers the sample now leaving
    // the delay line, so their average is no more than the gain that sample needs
    m_boxSum += windowMin - m_box[m_boxIdx];
    m_box[m_boxIdx] = windowMin;
    m_boxIdx = (m_boxIdx + 1) % m_lookahead;
    if (m_boxIdx == 0) { // Avoid accumulating rounding errors
        m_boxSum = std::accumulate(m_box.begin(), m_box.end(), 0.0);
    }
    const float boxGain = (float) (m_boxSum / m_lookahead);

    // Recover slowly, but never above the gain required by the coming samples
    m_gain = std::min(boxGain, m_gain + (1.0f - m_gain) * m_release);

    const Complex delayed = m_delay[m_delayIdx];
    m_delay[m_delayIdx] = input;
    m_delayIdx = (m_delayIdx + 1) % m_lookahead;
    m_count++;

    // Rounding of the average could exceed the limit by a tiny amount
    return Complex(std::clamp(delayed.real() * m_gain, -1.0f, 1.0f),
                   std::clamp(delayed.imag() * m_gain, -1.0f, 1.0f));
}
