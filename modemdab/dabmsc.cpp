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

#include "dabprofile.h"
#include "dabmsc.h"

namespace modemdab
{

DABSubchannelDecoder::DABSubchannelDecoder() :
    m_codedBits(0),
    m_head(0),
    m_filled(0),
    m_lastDamaged(false),
    m_bitErrors(0),
    m_comparedBits(0),
    m_countErrors(true)
{
    reset();
}

void DABSubchannelDecoder::configure(const DABSubchannel& subchannel)
{
    m_subchannel = subchannel;
    m_profile = subchannel.profile();
    m_codedBits = subchannel.sizeCU * ModeI::CUBits;
    for (auto& h : m_history) {
        h.assign(m_codedBits, 0.0f);
    }
    reset();
}

void DABSubchannelDecoder::reset()
{
    m_head = 0;
    m_filled = 0;
    m_historyLost.fill(true);
    m_lastDamaged = false;
    m_bitErrors = 0;
}

bool DABSubchannelDecoder::push(const float *cif, std::vector<uint8_t>& logicalFrame)
{
    DABProfileScope profile(DABProfile::MSC);
    if (!configured() || m_profile.parts.empty()) {
        return false;
    }

    m_head = (m_head + 1) % Depth;
    std::vector<float>& slot = m_history[m_head];

    if (cif)
    {
        const float *start = cif + m_subchannel.startCU * ModeI::CUBits;
        std::copy(start, start + m_codedBits, slot.begin());
        m_historyLost[m_head] = false;
    }
    else
    {
        std::fill(slot.begin(), slot.end(), 0.0f);
        m_historyLost[m_head] = true;
    }

    if (m_filled < Depth) {
        m_filled++;
    }
    if (m_filled < Depth) {
        return false;
    }

    // Bit i of the codeword sent 15 CIFs ago arrived DABTables::timeInterleavingDelay(i) CIFs later
    m_codeword.resize(m_codedBits);
    m_lastDamaged = false;
    for (int r = 0; r < Depth; r++) {
        m_lastDamaged = m_lastDamaged || m_historyLost[r];
    }
    for (int i = 0; i < m_codedBits; i++)
    {
        const int age = 15 - DABTables::timeInterleavingDelay(i);
        m_codeword[i] = m_history[(m_head - age + Depth) % Depth][i];
    }

    if (m_countErrors) {
        m_viterbi.decodePunctured(m_codeword.data(), m_codedBits, m_profile, m_bits, &m_bitErrors, &m_comparedBits);
    } else {
        m_viterbi.decodePunctured(m_codeword.data(), m_codedBits, m_profile, m_bits);
    }

    // Energy dispersal, and bytes
    const std::vector<uint8_t>& prbs = DABTables::prbs();
    const int bytes = static_cast<int>(m_bits.size()) / 8;

    logicalFrame.resize(bytes);
    for (int i = 0; i < bytes; i++)
    {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            byte = static_cast<uint8_t>((byte << 1) | ((m_bits[i * 8 + b] ^ prbs[i * 8 + b]) & 1));
        }
        logicalFrame[i] = byte;
    }
    return true;
}

void DABPacketDecoder::reset()
{
    m_dataGroup.clear();
    m_dataGroupValid = false;
    m_continuity = -1;
    m_validPackets = 0;
    m_invalidPackets = 0;
}

void DABPacketDecoder::frameLost()
{
    m_dataGroup.clear();
    m_dataGroupValid = false;
    m_continuity = -1;
}

void DABPacketDecoder::push(const uint8_t *data, int size, int address, bool dataGroups, std::vector<std::vector<uint8_t>>& completed)
{
    static const int packetLengths[4] = {24, 48, 72, 96};
    int position = 0;

    while (position + 3 <= size)
    {
        const int length = packetLengths[data[position] >> 6];
        if (position + length > size) {
            break;
        }
        const uint8_t *packet = &data[position];
        position += length;

        const int packetAddress = ((packet[0] & 0x03) << 8) | packet[1];
        if (packetAddress != address) {
            continue;
        }
        if (!DABTables::crc16Check(packet, length))
        {
            m_invalidPackets++;
            // The data group this packet belonged to is lost
            m_dataGroupValid = false;
            continue;
        }
        m_validPackets++;

        const int continuity = (packet[0] >> 4) & 0x03;
        const bool first = (packet[0] & 0x08) != 0;
        const bool last = (packet[0] & 0x04) != 0;
        const bool command = (packet[2] & 0x80) != 0;
        const int usefulLength = std::min(packet[2] & 0x7f, length - 5);
        const uint8_t *useful = &packet[3];

        const bool continuous = (m_continuity >= 0) && (((m_continuity + 1) & 3) == continuity);
        m_continuity = continuity;

        if (command) {
            continue;
        }

        if (!dataGroups)
        {
            completed.emplace_back(useful, useful + usefulLength);
            continue;
        }

        if (first)
        {
            m_dataGroup.assign(useful, useful + usefulLength);
            m_dataGroupValid = true;
        }
        else if (m_dataGroupValid && continuous)
        {
            m_dataGroup.insert(m_dataGroup.end(), useful, useful + usefulLength);
        }
        else
        {
            m_dataGroupValid = false;
        }

        if (m_dataGroupValid && (m_dataGroup.size() > MaximumDataGroupBytes)) {
            m_dataGroupValid = false;
        }

        if (last && m_dataGroupValid)
        {
            completed.push_back(m_dataGroup);
            m_dataGroup.clear();
            m_dataGroupValid = false;
        }
    }
}

} // namespace modemdab
