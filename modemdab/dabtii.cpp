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
#include <cstdlib>

#include "dabtii.h"
#include "dabtables.h"

namespace modemdab
{

DABTIIDecoder::DABTIIDecoder()
{
    reset();
}

void DABTIIDecoder::reset()
{
    m_sum.assign(ModeI::FFTSize, 0.0);
    m_count = 0;
    m_detections = 0;
    m_transmitters.clear();
}

void DABTIIDecoder::push(const std::vector<float>& nullSpectrum)
{
    if (nullSpectrum.size() != static_cast<std::size_t>(ModeI::FFTSize)) {
        return;
    }
    for (int i = 0; i < ModeI::FFTSize; i++) {
        m_sum[i] += nullSpectrum[i];
    }
    m_count++;
    if (m_count >= Average)
    {
        detect();
        m_detections++;
        std::fill(m_sum.begin(), m_sum.end(), 0.0);
        m_count = 0;
    }
}

void DABTIIDecoder::detect()
{
    // Each comb c has eight positions b of carrier pairs, repeated in four blocks of
    // 384 carriers. A transmitter switches on the four positions of its pattern.
    static const int blockStart[4] = {-768, -384, 1, 385};
    double energy[24][8];
    std::vector<double> all;

    for (int c = 0; c < 24; c++)
    {
        for (int b = 0; b < 8; b++)
        {
            double e = 0.0;
            int pairs = 0;
            for (int q = 0; q < 4; q++)
            {
                const int k = blockStart[q] + 2 * c + 48 * b;
                if ((std::abs(k) <= DCExclusion) || (std::abs(k + 1) <= DCExclusion)) {
                    continue;
                }
                e += m_sum[(k + ModeI::FFTSize) % ModeI::FFTSize] + m_sum[(k + 1 + ModeI::FFTSize) % ModeI::FFTSize];
                pairs++;
            }
            // Scaled to four pairs, where one was left out near DC
            energy[c][b] = e * 4.0 / pairs;
            all.push_back(e);
        }
    }

    // Most positions are empty: their median is the noise floor
    std::vector<double> sorted = all;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double floor = std::max(sorted[sorted.size() / 2], 1.0e-30);

    std::vector<DABTIITransmitter> found;
    double strongest = 0.0;
    std::vector<double> levels;

    for (int c = 0; c < 24; c++)
    {
        int order[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        std::sort(order, order + 8, [&](int a, int b) { return energy[c][a] > energy[c][b]; });

        // Four positions clearly above the floor and above the other four. Each TII carrier
        // has the power of an ensemble carrier, and the floor is the mean of 64 noise bins
        // (8 frames, 4 blocks, 2 carriers), so 3 times it is far above the noise.
        const double fourth = energy[c][order[3]];
        const double fifth = energy[c][order[4]];
        if ((fourth < 3.0 * floor) || (fourth < 2.0 * fifth)) {
            continue;
        }

        uint8_t pattern = 0;
        double level = 0.0;
        for (int i = 0; i < 4; i++)
        {
            pattern |= static_cast<uint8_t>(0x80 >> order[i]);
            level += energy[c][order[i]];
        }
        for (int p = 0; p < 70; p++)
        {
            if (DABTables::tiiPattern(p) == pattern)
            {
                DABTIITransmitter t;
                t.mainId = p;
                t.subId = c;
                found.push_back(t);
                levels.push_back(level);
                strongest = std::max(strongest, level);
                break;
            }
        }
    }

    for (std::size_t i = 0; i < found.size(); i++) {
        found[i].levelDB = static_cast<float>(10.0 * std::log10(levels[i] / strongest));
    }
    std::sort(found.begin(), found.end(), [](const DABTIITransmitter& a, const DABTIITransmitter& b) { return a.levelDB > b.levelDB; });
    m_transmitters = found;
}

} // namespace modemdab
