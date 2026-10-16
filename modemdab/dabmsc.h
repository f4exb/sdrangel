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

#ifndef MODEMDAB_DABMSC_H_
#define MODEMDAB_DABMSC_H_

#include <array>
#include <cstdint>
#include <vector>

#include "export.h"
#include "dabconvolutional.h"
#include "dabensemble.h"

namespace modemdab
{

// One MSC sub-channel: time deinterleaving, depuncturing, Viterbi and energy dispersal
// (EN 300 401 clauses 10.3, 11.3 and 12).
class MODEMDAB_API DABSubchannelDecoder
{
public:
    DABSubchannelDecoder();

    // Restarts the time deinterleaver
    void configure(const DABSubchannel& subchannel);
    const DABSubchannel& subchannel() const { return m_subchannel; }
    bool configured() const { return m_subchannel.id >= 0; }
    void reset();

    // Push a CIF (55296 soft bits) or, if cif is null, a lost one. Returns true when a logical
    // frame (bitrate * 3 bytes) is output, which is 15 CIFs after its first bits arrived.
    bool push(const float *cif, std::vector<uint8_t>& logicalFrame);
    // The logical frame output was decoded from lost CIFs only in part (or completely)
    bool lastFrameDamaged() const { return m_lastDamaged; }
    int lastBitErrors() const { return m_bitErrors; }
    int lastComparedBits() const { return m_comparedBits; }
    // The Viterbi output of the last logical frame, before energy dispersal is removed
    const std::vector<uint8_t>& lastBits() const { return m_bits; }
    // Soft output: whether each bit of lastBits() is reliable (see DABViterbi)
    void setReliabilityLimit(float limit) { m_viterbi.setReliabilityLimit(limit); }
    // Whether to estimate the channel bit errors (lastBitErrors()); on by default
    void setCountErrors(bool count) { m_countErrors = count; }
    const std::vector<uint8_t>& lastReliable() const { return m_viterbi.reliable(); }

private:
    static constexpr int Depth = 16;

    DABSubchannel m_subchannel;
    ProtectionProfile m_profile;
    int m_codedBits;                // M, the sub-channel's bits in a CIF
    std::array<std::vector<float>, Depth> m_history;
    std::array<bool, Depth> m_historyLost;
    int m_head;
    int m_filled;
    std::vector<float> m_codeword;
    std::vector<uint8_t> m_bits;
    DABViterbi m_viterbi;
    bool m_lastDamaged;
    int m_bitErrors;
    int m_comparedBits;
    bool m_countErrors;
};

// Packet mode (EN 300 401 clause 5.3.2, 5.3.3): packets of one address, assembled into MSC
// data groups when used.
class MODEMDAB_API DABPacketDecoder
{
public:
    void reset();
    // Split a logical frame of the sub-channel into packets, and append the data groups (or,
    // without data groups, the packet data) of the given address that it completes.
    void push(const uint8_t *data, int size, int address, bool dataGroups, std::vector<std::vector<uint8_t>>& completed);
    // A logical frame was lost
    void frameLost();

    int validPackets() const { return m_validPackets; }
    int invalidPackets() const { return m_invalidPackets; }

private:
    static constexpr std::size_t MaximumDataGroupBytes = 8192U + 64U;

    std::vector<uint8_t> m_dataGroup;
    bool m_dataGroupValid = false;
    int m_continuity = -1;
    int m_validPackets = 0;
    int m_invalidPackets = 0;
};

} // namespace modemdab

#endif // MODEMDAB_DABMSC_H_
