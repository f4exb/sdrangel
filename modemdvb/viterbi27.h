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

#ifndef MODEMDVB_VITERBI27_H
#define MODEMDVB_VITERBI27_H

#include <cstdint>
#include <vector>

#include "export.h"

namespace dvbt {

struct EncoderOutputs;
struct BranchCosts;

// Convolutional code, rate 1/2, K = 7, G1 = 171 (X), G2 = 133 (Y) octal, as used by DVB-S and DVB-T
// The encoder is never terminated, so the decoder runs continuously, tracing back over each block
// it is given and holding back the most recent bits until the following block has arrived
class MODEMDVB_API Viterbi27
{
public:
    Viterbi27();

    void reset();
    //! Decode nbBits information bits from soft symbols llrs[2*i] (X) and llrs[2*i+1] (Y),
    //! positive meaning 0 and 0 meaning erased. Appends decoded bits (one per byte) to out.
    //! Output lags input by m_tracebackLength bits
    void decode(const int8_t *llrs, int nbBits, std::vector<uint8_t>& out);
    //! Encode bits (one per byte) into X, Y pairs (one bit per byte). Used for testing
    static void encode(const uint8_t *bits, int nbBits, uint8_t& state, std::vector<uint8_t>& out);

    static const int m_tracebackLength = 192;

private:
    friend struct EncoderOutputs;

    void forward(const int8_t *llrs, int nbBits);
    void forwardScalar(int nbBits, uint64_t *decisions);
    static int parity(int v);

    // State s holds the previous six input bits, the most recent in bit 0. Input u moves state s to
    // ((s << 1) | u) & 63, so new states 2j and 2j+1 both have predecessors j and j+32 (a butterfly)
    // X = u ^ s0 ^ s1 ^ s2 ^ s5 (171 octal), Y = u ^ s1 ^ s2 ^ s4 ^ s5 (133 octal)
    static const int m_maskX = 0x27;
    static const int m_maskY = 0x36;
    static const EncoderOutputs m_encoderOutputs;
    static const BranchCosts m_branchCosts;

    alignas(32) uint8_t m_metrics[64]; //!< 8-bit path metrics, smallest best
    //! One word per step not yet output (the first m_nbDecisions), bit s set when state s came from s/2 + 32. Only
    //! grows, so its words aren't zeroed each call before the kernels write them (190 kB a symbol in 8K)
    std::vector<uint64_t> m_decisions;
    int m_nbDecisions = 0;
    std::vector<uint32_t> m_costs;     //!< Per step, the branch costs for encoder outputs 0..3 in bytes 0..3
};

} // namespace dvbt

#endif // MODEMDVB_VITERBI27_H
