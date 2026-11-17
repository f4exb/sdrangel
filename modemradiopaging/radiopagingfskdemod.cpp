///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include "radiopagingfskdemod.h"

#include <algorithm>
#include <cmath>

namespace {

// Timing loop gains, as the fraction of a half symbol corrected per unit
// normalised timing error
const double timingGainAcquire = 0.10;
const double timingGainTrack = 0.02;
// Symbols of the hunting means, and of the history the known patterns are fitted to
const int defaultHuntWindow = 64;
const size_t historySize = 256;
// Smoothing of the symbol power used to normalise the timing error
const double timingPowerAlpha = 0.01;

} // namespace

RadioPagingFskDemod::RadioPagingFskDemod() :
    m_samplesPerSymbol(1.0),
    m_levels(2),
    m_locked(false),
    m_holdLevels(false),
    m_matchedFilter(1),
    m_huntDc(defaultHuntWindow),
    m_huntDeviation(defaultHuntWindow),
    m_history(historySize, 0.0f),
    m_inputPos(0)
{
    reset();
}

void RadioPagingFskDemod::setHuntWindow(int symbols)
{
    m_huntDc.resize(std::max(1, symbols));
    m_huntDeviation.resize(std::max(1, symbols));
}

void RadioPagingFskDemod::offsetLevels(float delta)
{
    m_dc += delta;

    for (float& x : m_history) {
        x += delta;
    }

    // The hunting means continue over the moved symbol values
    size_t n = std::min((size_t) m_huntDc.size(), m_historyCount);

    m_huntDc.reset();
    m_huntDeviation.reset();

    for (size_t i = 0; i < n; i++)
    {
        float raw = m_history[(m_historyPos + historySize - n + i) % historySize];
        m_huntDc(raw);
        m_huntDeviation(std::fabs(raw - (float) m_huntDc.instantAverage()));
    }
}

// Least squares fit of value = dc + deviation * level over the known symbols
bool RadioPagingFskDemod::fitLevels(const std::vector<float>& known, size_t symbolsAgo)
{
    size_t n = known.size();

    if ((n < 2) || (n + symbolsAgo > m_historyCount)) {
        return false;
    }

    double sx = 0.0, sl = 0.0, sxl = 0.0, sll = 0.0;

    for (size_t i = 0; i < n; i++)
    {
        double x = m_history[(m_historyPos + historySize - symbolsAgo - n + i) % historySize];
        double l = known[i];
        sx += x;
        sl += l;
        sxl += x * l;
        sll += l * l;
    }

    double ml = sl / n;
    double mx = sx / n;
    double var = sll / n - ml * ml;

    if (var < 1e-6) {
        return false;
    }

    double deviation = (sxl / n - mx * ml) / var;

    // A pattern that is not what was received (a false sync, a decoder of the other
    // polarity taking a pattern for another) fits with a deviation unlike the one known:
    // rejected, the levels are kept
    if ((deviation < 0.05) || (deviation < 0.6 * m_deviation) || (deviation > 1.6 * m_deviation)) {
        return false;
    }

    m_deviation = (float) std::min(deviation, 20.0);
    m_dc = (float) (mx - deviation * ml);
    m_holdLevels = true;

    return true;
}

void RadioPagingFskDemod::create(double sampleRate, double symbolRate, int levels)
{
    m_samplesPerSymbol = sampleRate / symbolRate;
    m_levels = (levels == 4) ? 4 : 2;
    m_matchedFilter.resize(std::max(1, (int) std::lround(m_samplesPerSymbol)));
    m_input.assign((size_t) std::ceil(m_samplesPerSymbol) + 4, 0.0f);
    reset();
}

// Linear interpolation between the samples: the part of each sample between back1 and back2 counts
float RadioPagingFskDemod::inputMean(double back1, double back2) const
{
    const size_t size = m_input.size();
    double sum = 0.0;
    double weight = 0.0;

    for (int k = (int) std::floor(back1); (k <= (int) std::ceil(back2)) && (k < (int) size); k++)
    {
        // Sample k samples before the last one covers [k - 0.5, k + 0.5] of that axis
        double lo = std::max(back1, k - 0.5);
        double hi = std::min(back2, k + 0.5);

        if ((hi > lo) && (k >= 0))
        {
            sum += m_input[(m_inputPos + size - 1 - (size_t) k) % size] * (hi - lo);
            weight += hi - lo;
        }
    }

    return (weight > 0.0) ? (float) (sum / weight) : 0.0f;
}

void RadioPagingFskDemod::reset()
{
    m_matchedFilter.reset();
    m_prevFiltered = 0.0f;
    m_timeToNext = m_samplesPerSymbol / 2.0;
    m_nextIsSymbol = true;
    m_prevSymbolValue = 0.0f;
    m_midValue = 0.0f;
    m_haveMid = false;
    m_timingPower = 1.0;
    m_dc = 0.0f;
    m_deviation = 1.0f;
    m_locked = false;
    m_holdLevels = false;
    m_huntDc.reset();
    m_huntDeviation.reset();
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    m_historyPos = 0;
    m_historyCount = 0;
    std::fill(m_input.begin(), m_input.end(), 0.0f);
    m_inputPos = 0;
}

void RadioPagingFskDemod::setLevels(float dc, float deviation)
{
    m_dc = dc;
    m_deviation = std::max(deviation, 1e-3f);
}

void RadioPagingFskDemod::startSymbols(double samplesToNextSymbol)
{
    m_timeToNext = samplesToNextSymbol;
    m_nextIsSymbol = true;
    m_haveMid = false;
}

float RadioPagingFskDemod::levelValue(int symbol, int levels)
{
    if (levels == 4)
    {
        static const float values[4] = {-1.0f, -1.0f/3.0f, 1.0f/3.0f, 1.0f};
        return values[symbol & 3];
    }
    else
    {
        return symbol ? 1.0f : -1.0f;
    }
}

int RadioPagingFskDemod::decide(float normalised) const
{
    if (m_levels == 4)
    {
        if (normalised > 2.0f/3.0f) {
            return 3;
        } else if (normalised > 0.0f) {
            return 2;
        } else if (normalised > -2.0f/3.0f) {
            return 1;
        } else {
            return 0;
        }
    }
    else
    {
        return normalised >= 0.0f ? 1 : 0;
    }
}

bool RadioPagingFskDemod::process(float sample, Symbol& symbol)
{
    bool produced = false;

    m_matchedFilter(sample);
    float filtered = m_matchedFilter.asFloat();

    if (!m_input.empty())
    {
        m_input[m_inputPos] = sample;
        m_inputPos = (m_inputPos + 1) % m_input.size();
    }

    m_timeToNext -= 1.0;

    while (m_timeToNext <= 0.0)
    {
        // The instant lies between the previous input sample (fraction 0) and this one (fraction 1)
        float fraction = (float) (1.0 + m_timeToNext);
        float value = m_prevFiltered + (filtered - m_prevFiltered) * fraction - m_dc;
        double halfSymbol = m_samplesPerSymbol / 2.0;

        if (m_nextIsSymbol)
        {
            double correction = 0.0;

            if (m_haveMid)
            {
                // Gardner timing error: positive when sampling early, negative when late
                double error = (m_prevSymbolValue - value) * m_midValue / std::max(m_timingPower, 1e-6);
                error = std::max(-1.0, std::min(1.0, error));
                correction = (m_locked ? timingGainTrack : timingGainAcquire) * error * halfSymbol;
            }

            float normalised = value / m_deviation;
            int decided = decide(normalised);
            float level = levelValue(decided, m_levels);

            // The symbol value with its DC offset: history for the known pattern fits, and
            // the hunting means. Unless held, the levels follow the hunting means (2-level)
            float raw = value + m_dc;
            m_history[m_historyPos] = raw;
            m_historyPos = (m_historyPos + 1) % historySize;
            m_historyCount = std::min(m_historyCount + 1, historySize);
            m_huntDc(raw);
            m_huntDeviation(std::fabs(raw - (float) m_huntDc.instantAverage()));

            if (!m_holdLevels)
            {
                m_dc = (float) m_huntDc.instantAverage();
                m_deviation = std::max(0.05f, std::min(20.0f, (float) m_huntDeviation.instantAverage()));
            }

            m_timingPower += timingPowerAlpha * ((double) value * value - m_timingPower);
            m_prevSymbolValue = value;

            symbol.m_value = decided;
            symbol.m_soft = normalised;
            symbol.m_error = normalised - level;
            symbol.m_offset = (float) -m_timeToNext;

            // The matched filter at the instant averages the symbol: the input from b to b + one symbol before the
            // last sample (a sample k before it covers k - 0.5 to k + 0.5). Its centre half without the transitions
            const double b = (1.0 - fraction) - 0.5;
            const float centre = inputMean(b + m_samplesPerSymbol / 4.0, b + 3.0 * m_samplesPerSymbol / 4.0) - m_dc;
            symbol.m_centreSoft = centre / m_deviation;
            symbol.m_centreValue = decide(symbol.m_centreSoft);
            produced = true;

            m_timeToNext += halfSymbol + correction;
        }
        else
        {
            m_midValue = value;
            m_haveMid = true;
            m_timeToNext += halfSymbol;
        }

        m_nextIsSymbol = !m_nextIsSymbol;
    }

    m_prevFiltered = filtered;

    return produced;
}
