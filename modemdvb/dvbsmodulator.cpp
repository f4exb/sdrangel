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
#include <cstring>

#include "dvbsmodulator.h"
#include "dvbtouter.h"
#include "viterbi27.h"

namespace dvbs {

PulseShaper::PulseShaper() :
    m_sampleRate(2.0),
    m_symbolRate(1.0),
    m_span(16),
    m_time(0.0),
    m_step(0.5),
    m_gain(1.0f)
{
    configure(2.0, 1.0, 0.35);
}

void PulseShaper::configure(double sampleRate, double symbolRate, double rollOff, int span)
{
    m_sampleRate = sampleRate;
    m_symbolRate = symbolRate;
    // A low roll-off's pulse falls off over about 1 / roll-off symbols: spanned 3 times that, else truncated it's ISI
    m_span = std::max(span, 2 * (int) std::ceil(1.5 / std::max(0.01, rollOff)));
    m_step = symbolRate / sampleRate;
    const int points = m_span * m_resolution + 1;
    m_table.resize(points);
    double energy = 0.0;

    for (int i = 0; i < points; i++)
    {
        const double t = (double) i / m_resolution - m_span / 2.0;
        m_table[i] = (float) rootRaisedCosine(t, rollOff);
    }

    // Unit power out for unit power symbols: the average power is the pulse's energy (the integral of h^2 over t, in
    // symbols) a symbol, whatever the sample rate
    for (int i = 0; i < points; i += 1) {
        energy += (double) m_table[i] * m_table[i];
    }

    energy /= m_resolution;
    m_gain = (float) std::sqrt(1.0 / energy);
    m_history.assign(m_span, Cf(0.0f, 0.0f));
    m_time = m_span / 2.0;
}

void PulseShaper::setClockOffset(double ppm)
{
    m_step = m_symbolRate * (1.0 + ppm * 1e-6) / m_sampleRate;
}

void PulseShaper::process(const Cf *symbols, int nbSymbols, std::vector<Cf>& samples)
{
    m_history.insert(m_history.end(), symbols, symbols + nbSymbols);
    const int n = (int) m_history.size();
    const int half = m_span / 2;

    // Output at m_time needs symbols from m_time - half to m_time + half
    while (m_time + half + 1 < n)
    {
        const int centre = (int) std::floor(m_time);
        float re = 0.0f, im = 0.0f;

        for (int k = centre - half + 1; k <= centre + half; k++)
        {
            const double t = m_time - k + m_span / 2.0; // From the table's start
            const int index = (int) std::lround(t * m_resolution);

            if ((index >= 0) && (index < (int) m_table.size()))
            {
                const float h = m_table[index];
                re += m_history[k].real() * h;
                im += m_history[k].imag() * h;
            }
        }

        samples.emplace_back(re * m_gain, im * m_gain);
        m_time += m_step;
    }

    const int consumed = std::max(0, std::min(n, (int) std::floor(m_time) - half));
    m_history.erase(m_history.begin(), m_history.begin() + consumed);
    m_time -= consumed;
}

Modulator::Modulator(CodeRate rate) :
    m_period(1),
    m_encoderState(0)
{
    puncturePattern(rate, m_pattern, m_period);

    // The interleaver starts full of random bytes, as if the transmitter had been running: zeros would make the first
    // packets a run of the same symbol, an unmodulated carrier
    uint32_t lcg = 12345;

    for (int j = 0; j < 12; j++)
    {
        m_fifo[j].resize(17 * j);

        for (uint8_t& byte : m_fifo[j])
        {
            lcg = lcg * 1664525u + 1013904223u;
            byte = (uint8_t) (lcg >> 24);
        }

        m_fifoPos[j] = 0;
    }
}

double Modulator::symbolsPerPacket() const
{
    return dvbt::rsPacketSize * 8.0 * m_pattern.size() / m_period / 2.0;
}

void Modulator::modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& symbols)
{
    static const float a = 0.70710678f;

    for (int p = 0; p < nbPackets; p++)
    {
        m_pending.insert(m_pending.end(), packets + p * dvbt::tsPacketSize, packets + (p + 1) * dvbt::tsPacketSize);

        if (m_pending.size() == 8 * dvbt::tsPacketSize)
        {
            encodeGroup();
            m_pending.clear();
        }
    }

    // Puncture whole periods, then a symbol for each pair: I = 1 - 2 x, Q = 1 - 2 y
    const int periods = (int) m_bits.size() / m_period;
    std::vector<uint8_t> xy;
    dvbt::Viterbi27::encode(m_bits.data(), periods * m_period, m_encoderState, xy);
    m_bits.erase(m_bits.begin(), m_bits.begin() + periods * m_period);

    for (int g = 0; g < periods; g++)
    {
        for (const PunctureEntry& e : m_pattern) {
            m_serial.push_back(xy[2 * (g * m_period + e.m_bit) + e.m_y]);
        }
    }

    size_t i = 0;

    for (; i + 2 <= m_serial.size(); i += 2) {
        symbols.emplace_back(m_serial[i] ? -a : a, m_serial[i + 1] ? -a : a);
    }

    m_serial.erase(m_serial.begin(), m_serial.begin() + i);
}

// Randomisation of 8 packets, RS encoding, outer interleaving, and their bits for the convolutional code
void Modulator::encodeGroup()
{
    uint8_t group[8][dvbt::tsPacketSize];

    for (int k = 0; k < 8; k++) {
        std::memcpy(group[k], &m_pending[k * dvbt::tsPacketSize], dvbt::tsPacketSize);
    }

    dvbt::OuterDecoder::randomize(group);

    for (int k = 0; k < 8; k++)
    {
        uint8_t rs[dvbt::rsPacketSize];
        dvbt::OuterDecoder::rsEncode(group[k], rs);

        for (int b = 0; b < dvbt::rsPacketSize; b++)
        {
            const int j = b % 12;
            uint8_t out = rs[b];

            if (j > 0)
            {
                std::vector<uint8_t>& fifo = m_fifo[j];
                out = fifo[m_fifoPos[j]];
                fifo[m_fifoPos[j]] = rs[b];
                m_fifoPos[j] = (m_fifoPos[j] + 1) % (int) fifo.size();
            }

            for (int bit = 7; bit >= 0; bit--) {
                m_bits.push_back((out >> bit) & 1);
            }
        }
    }
}

} // namespace dvbs
