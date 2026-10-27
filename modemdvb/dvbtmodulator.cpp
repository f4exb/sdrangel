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

#include <cmath>
#include <cstring>

#include <QString>

#include "dsp/fftengine.h"

#include "dvbtmodulator.h"
#include "dvbtouter.h"
#include "viterbi27.h"

namespace dvbt {

Modulator::Modulator(Mode mode, Constellation constellation, CodeRate codeRate, GuardInterval guard, int cellId,
    Hierarchy hierarchy, CodeRate lpCodeRate) :
    m_mode(mode),
    m_constellation(constellation),
    m_codeRate(codeRate),
    m_guard(guard),
    m_cellId(cellId),
    m_tables(Tables::get(mode)),
    m_hierarchy(constellation == Constellation::QPSK ? Hierarchy::None : hierarchy),
    m_lpCodeRate(lpCodeRate),
    m_tpsBits{},
    m_symbolIndex(0),
    m_frameNumber(0)
{
    m_inner.configure(mode, constellation, codeRate, m_hierarchy, false);
    m_fft = FFTEngine::create(QString());
    m_fft->configure(m_tables.m_fftSize, true);
    m_cells.resize(m_tables.m_nbData);
    m_tpsValue.resize(m_tables.m_tpsCarriers.size());
    m_streams[0].m_rate = codeRate;
    m_streams[0].m_infoBits = m_inner.infoBitsPerSymbol();

    if (m_hierarchy != Hierarchy::None)
    {
        InnerDecoder lp;
        lp.configure(mode, constellation, lpCodeRate, m_hierarchy, true);
        m_streams[1].m_rate = lpCodeRate;
        m_streams[1].m_infoBits = lp.infoBitsPerSymbol();
    }

    for (Stream& stream : m_streams)
    {
        for (int j = 0; j < 12; j++) {
            stream.m_fifo[j].assign(j * 17, 0);
        }
    }
}

Modulator::~Modulator()
{
    delete m_fft;
}

void Modulator::Stream::encode(const uint8_t *packets, int nbPackets)
{
    for (int g = 0; g + 8 <= nbPackets; g += 8)
    {
        uint8_t group[8][tsPacketSize];
        memcpy(group, packets + g * tsPacketSize, sizeof(group));
        OuterDecoder::randomize(group);

        for (int p = 0; p < 8; p++)
        {
            uint8_t rsPacket[rsPacketSize];
            OuterDecoder::rsEncode(group[p], rsPacket);
            uint8_t bits[rsPacketSize * 8];

            for (int i = 0; i < rsPacketSize; i++)
            {
                // Outer interleave
                int j = m_bytePos % 12;
                uint8_t byte = rsPacket[i];

                if (j > 0)
                {
                    uint8_t out = m_fifo[j][m_fifoPos[j]];
                    m_fifo[j][m_fifoPos[j]] = byte;
                    m_fifoPos[j] = (m_fifoPos[j] + 1) % (int) m_fifo[j].size();
                    byte = out;
                }

                m_bytePos = (m_bytePos + 1) % rsPacketSize;

                for (int b = 0; b < 8; b++) {
                    bits[i * 8 + b] = (byte >> (7 - b)) & 1;
                }
            }

            Viterbi27::encode(bits, rsPacketSize * 8, m_encoderState, m_xy);
        }
    }
}

void Modulator::modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& samples, const uint8_t *lpPackets, int nbLpPackets)
{
    m_streams[0].encode(packets, nbPackets);
    const bool hierarchical = m_hierarchy != Hierarchy::None;
    const bool lpGiven = lpPackets && (nbLpPackets > 0);
    std::vector<uint8_t> nullPackets;

    if (hierarchical && lpGiven) {
        m_streams[1].encode(lpPackets, nbLpPackets);
    }

    if (hierarchical && !lpGiven)
    {
        // Null packets (PID 0x1fff) for the low priority stream, so the high priority one's are still sent
        nullPackets.assign(8 * tsPacketSize, (uint8_t) 0xff);

        for (int p = 0; p < 8; p++)
        {
            uint8_t *packet = &nullPackets[p * tsPacketSize];
            packet[0] = 0x47;
            packet[1] = 0x1f;
            packet[2] = 0xff;
            packet[3] = 0x10;
        }
    }

    // Symbols, while each stream has the bits for one
    while (true)
    {
        if (!nullPackets.empty() && ((int) m_streams[0].m_xy.size() >= 2 * m_streams[0].m_infoBits))
        {
            while ((int) m_streams[1].m_xy.size() < 2 * m_streams[1].m_infoBits) {
                m_streams[1].encode(nullPackets.data(), 8);
            }
        }

        for (int s = 0; s < (hierarchical ? 2 : 1); s++)
        {
            if ((int) m_streams[s].m_xy.size() < 2 * m_streams[s].m_infoBits) {
                return;
            }
        }

        for (int s = 0; s < (hierarchical ? 2 : 1); s++)
        {
            Stream& stream = m_streams[s];
            stream.m_punctured.clear();
            InnerDecoder::puncture(stream.m_rate, stream.m_xy.data(), stream.m_infoBits, stream.m_punctured);
            stream.m_xy.erase(stream.m_xy.begin(), stream.m_xy.begin() + 2 * stream.m_infoBits);
        }

        modulateSymbol(samples);
    }
}

void Modulator::modulateSymbol(std::vector<Cf>& samples)
{
    const int n = m_tables.m_fftSize;
    const int kMax = m_tables.m_kMax;
    const int phase = m_symbolIndex % 4;
    const float boost = 4.0f / 3.0f;

    if (m_symbolIndex == 0)
    {
        TPS tps;
        tps.m_frameNumber = m_frameNumber;
        tps.m_constellation = m_constellation;
        tps.m_hierarchy = m_hierarchy;
        tps.m_hpCodeRate = m_codeRate;
        tps.m_lpCodeRate = m_hierarchy != Hierarchy::None ? m_lpCodeRate : CodeRate::R1_2; // 000 (4.6.2.7)
        tps.m_guard = m_guard;
        tps.m_mode = m_mode;

        if (m_cellId >= 0)
        {
            tps.m_lengthIndicator = 31;
            tps.m_cellIdByte = (m_frameNumber & 1) ? (m_cellId & 0xff) : (m_cellId >> 8);
        }

        tps.encode(m_tpsBits);
    }

    m_inner.encode(m_symbolIndex, m_streams[0].m_punctured.data(), m_cells.data(),
        m_hierarchy != Hierarchy::None ? m_streams[1].m_punctured.data() : nullptr);

    Cf *in = m_fft->in();
    std::fill(in, in + n, Cf(0.0f, 0.0f));
    const std::vector<uint8_t>& type = m_tables.m_carrierType[phase];
    const std::vector<int>& data = m_tables.m_dataCarriers[phase];
    auto bin = [n, kMax](int k) { int i = k - kMax / 2; return i < 0 ? i + n : i; };

    for (int k = 0; k <= kMax; k++)
    {
        if (type[k] == 1) {
            in[bin(k)] = Cf(m_tables.m_prbs[k] ? -boost : boost, 0.0f);
        }
    }

    for (size_t i = 0; i < data.size(); i++) {
        in[bin(data[i])] = m_cells[i];
    }

    // TPS: absolute in symbol 0, then DBPSK (4.6)
    for (size_t i = 0; i < m_tables.m_tpsCarriers.size(); i++)
    {
        int k = m_tables.m_tpsCarriers[i];

        if (m_symbolIndex == 0) {
            m_tpsValue[i] = m_tables.m_prbs[k] ? -1.0f : 1.0f;
        } else if (m_tpsBits[m_symbolIndex]) {
            m_tpsValue[i] = -m_tpsValue[i];
        }

        in[bin(k)] = Cf(m_tpsValue[i], 0.0f);
    }

    m_fft->transform();
    const Cf *out = m_fft->out();
    const int ng = guardSamples(m_mode, m_guard);
    const float scale = 1.0f / std::sqrt((float) (kMax + 1));

    for (int i = n - ng; i < n; i++) {
        samples.push_back(out[i] * scale);
    }

    for (int i = 0; i < n; i++) {
        samples.push_back(out[i] * scale);
    }

    if (++m_symbolIndex == symbolsPerFrame)
    {
        m_symbolIndex = 0;
        m_frameNumber = (m_frameNumber + 1) % framesPerSuperFrame;
    }
}

} // namespace dvbt
