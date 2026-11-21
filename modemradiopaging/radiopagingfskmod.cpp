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

#include <algorithm>
#include <cmath>

#include "radiopagingfskmod.h"

const int RadioPagingFskMod::m_internalRate;

namespace {

const int shapingSpan = 2;          // Gaussian over +/-1 / cutoff: +/-7.5 standard deviations
const int interpolatorPhases = 48;
const double interpolatorTapsPerPhase = 3.0;
const double interpolatorCutoff = 0.45 * RadioPagingFskMod::m_internalRate;

} // namespace

RadioPagingFskMod::RadioPagingFskMod() :
    m_deviation(4500.0),
    m_shaping(2400.0),
    m_channelSampleRate(m_internalRate),
    m_symbol(0),
    m_blockStart(true),
    m_sampleInSymbol(0),
    m_hold(0.0f),
    m_tail(0),
    m_gaussianDelay(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_phase(0.0),
    m_phaseSensitivity(0.0),
    m_level(0.0f),
    m_levelSample(0.0f, 0.0f)
{
    applyShaping();
    applyChannelSampleRate();
}

// Only the FM sensitivity changes: the filters and the symbols queued are kept
void RadioPagingFskMod::setDeviation(double deviation)
{
    m_deviation = deviation;
    m_phaseSensitivity = 2.0 * M_PI * m_deviation / (double) m_channelSampleRate;
}

void RadioPagingFskMod::setShaping(double cutoff)
{
    m_shaping = cutoff;
    applyShaping();
}

void RadioPagingFskMod::setChannelSampleRate(int sampleRate)
{
    m_channelSampleRate = sampleRate;
    applyChannelSampleRate();
}

// The Gaussian cutoff is in Hz: one "symbol" of the filter is 1 / cutoff, with BT = 1. An NRZ
// level through it has no overshoot and reaches the deviation in about 0.6 / cutoff
void RadioPagingFskMod::applyShaping()
{
    int samples = std::max(1, (int) std::lround(m_internalRate / std::max(m_shaping, 1.0)));

    m_gaussian.create(1.0, shapingSpan, samples);
    m_gaussianDelay = (shapingSpan * samples) / 2;
}

// The shaped level is band limited to a few kHz and sampled at 38400 Hz, so the Interpolator
// resamples it with a wide cutoff
void RadioPagingFskMod::applyChannelSampleRate()
{
    m_interpolator.create(interpolatorPhases, m_internalRate, interpolatorCutoff, interpolatorTapsPerPhase);
    m_interpolatorDistance = (Real) m_internalRate / (Real) m_channelSampleRate;
    m_interpolatorDistanceRemain = 0.0f;
    m_phaseSensitivity = 2.0 * M_PI * m_deviation / (double) m_channelSampleRate;
}

void RadioPagingFskMod::reset()
{
    m_blocks.clear();
    m_symbol = 0;
    m_blockStart = true;
    m_sampleInSymbol = 0;
    m_hold = 0.0f;
    m_tail = 0;
    m_phase = 0.0;
    m_level = 0.0f;
    m_levelSample = Complex(0.0f, 0.0f);
    applyShaping();
    applyChannelSampleRate();
}

bool RadioPagingFskMod::push(double baud, const std::vector<float>& levels, double shaping, double deviation)
{
    if ((baud < 1.0) || (baud > (double) m_internalRate)) {
        return false;
    }

    if (!levels.empty()) {
        m_blocks.push_back(Block{(double) m_internalRate / baud, levels, shaping, deviation});
    }

    return true;
}

size_t RadioPagingFskMod::queued() const
{
    size_t n = 0;

    for (const Block& block : m_blocks) {
        n += block.m_levels.size();
    }

    return n - std::min(n, m_symbol);
}

// One shaped level at the internal rate. After the last symbol its level is held while the
// filter delay runs out
void RadioPagingFskMod::shapeSample()
{
    float level;

    if (!m_blocks.empty())
    {
        Block& block = m_blocks.front();

        // A block with its own shaping: the filter is designed again at its first symbol, its
        // history filled with the level held so that the deviation does not jump
        // The first sample of a block (a flag: the fraction of a symbol carried over from the
        // block before is not zero after a rate that does not divide 38400)
        bool blockStart = m_blockStart;
        m_blockStart = false;

        if (blockStart && (block.m_shaping > 0.0) && (block.m_shaping != m_shaping))
        {
            m_shaping = block.m_shaping;
            applyShaping();

            for (int i = 0; i <= 2 * m_gaussianDelay; i++) {
                m_gaussian.filter(m_hold);
            }
        }

        // A block with its own deviation: the FM sensitivity from its first symbol
        if (blockStart && (block.m_deviation > 0.0) && (block.m_deviation != m_deviation)) {
            setDeviation(block.m_deviation);
        }

        level = block.m_levels[m_symbol];

        // The symbol ends when its samples are sent; the fraction over is carried to the next
        // symbol, so the block has exactly its length on average
        m_sampleInSymbol += 1.0;

        if (m_sampleInSymbol >= block.m_samplesPerSymbol - 1e-6)
        {
            m_sampleInSymbol = std::max(0.0, m_sampleInSymbol - block.m_samplesPerSymbol);

            if (++m_symbol >= block.m_levels.size())
            {
                // After the last symbol its level is held at full power for the filter delay and one
                // symbol more: the receiver sees the whole last symbol and its end before the ramp down
                int guard = (int) std::ceil(block.m_samplesPerSymbol);
                m_blocks.pop_front();
                m_symbol = 0;
                m_blockStart = true;

                if (m_blocks.empty()) {
                    m_tail = m_gaussianDelay + guard;
                }
            }
        }

        m_hold = level;
    }
    else
    {
        level = m_hold;

        if (m_tail > 0) {
            m_tail--;
        }
    }

    m_level = m_gaussian.filter(level);
    m_levelSample = Complex(m_level, 0.0f);
}

Complex RadioPagingFskMod::next()
{
    Complex ci;

    if (m_interpolatorDistance > 1.0f) // decimate
    {
        shapeSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_levelSample, &ci)) {
            shapeSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_levelSample, &ci)) {
            shapeSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    // FM at the channel sample rate
    m_phase += m_phaseSensitivity * ci.real();

    if (m_phase > M_PI) {
        m_phase -= 2.0 * M_PI;
    } else if (m_phase < -M_PI) {
        m_phase += 2.0 * M_PI;
    }

    return Complex((Real) std::cos(m_phase), (Real) std::sin(m_phase));
}
