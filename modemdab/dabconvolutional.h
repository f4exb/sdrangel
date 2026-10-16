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

#ifndef MODEMDAB_DABCONVOLUTIONAL_H_
#define MODEMDAB_DABCONVOLUTIONAL_H_

#include <array>
#include <cstdint>
#include <vector>

#include "export.h"
#include "modemdr/viterbi.h"
#include "dabtables.h"

namespace modemdab
{

// EN 300 401 clause 11: K = 7, rate 1/4 mother code with generators 133, 171, 145, 133 (octal).
// Soft values are positive for a transmitted zero and negative for a one; zero is an erasure.
class MODEMDAB_API DABConvolutional
{
public:
    static constexpr int ConstraintLength = 7;
    static constexpr int TailBits = ConstraintLength - 1;

    // Mother codeword of 4 * (bits + 6) bits, terminated with six zero bits.
    static std::vector<uint8_t> encodeMother(const std::vector<uint8_t>& bits);
    // Mother code, puncturing, tail and padding: the convolutional codeword of the profile.
    static std::vector<uint8_t> encode(const std::vector<uint8_t>& bits, const ProtectionProfile& profile);
    // Expand a punctured codeword to the mother codeword, with zeros where bits were removed.
    // Returns false if softCount is shorter than the profile needs.
    static bool depuncture(const float *soft, int softCount, const ProtectionProfile& profile, std::vector<float>& mother);
    // Whether each bit of the convolutional codeword (as encode() orders them) is reliable,
    // from the information bits: when every bit in the encoder register is. Padding is.
    static std::vector<uint8_t> codedReliable(const std::vector<uint8_t>& reliable, const ProtectionProfile& profile);
    // encode() and codedReliable() in one pass: each codeword bit, or -1 where it is unreliable
    static void encodeKnown(const std::vector<uint8_t>& bits, const std::vector<uint8_t>& reliable,
        const ProtectionProfile& profile, std::vector<int8_t>& codeword);
};

class MODEMDAB_API DABViterbi
{
public:
    DABViterbi();

    // Decode a mother codeword of 4 * (informationBits + 6) soft values into informationBits bits.
    void decode(const float *mother, int informationBits, std::vector<uint8_t>& bits);
    // Depuncture and decode. If bitErrors is given, it receives the number of transmitted bits
    // whose hard decision disagrees with the re-encoded result (a channel bit error estimate),
    // and codedBits the number compared.
    bool decodePunctured(
        const float *soft,
        int softCount,
        const ProtectionProfile& profile,
        std::vector<uint8_t>& bits,
        int *bitErrors = nullptr,
        int *codedBits = nullptr
    );

    // Soft output, as a thresholded SOVA: with a limit above zero, decoding also marks each
    // information bit reliable when every path that decides it differently has a metric
    // worse by at least limit times the mean magnitude of the soft values. Only one bit per
    // state and step is kept (whether the losing path was that close), so it costs little.
    void setReliabilityLimit(float limit) { m_reliabilityLimit = limit; }
    const std::vector<uint8_t>& reliable() const { return m_reliable; }

private:
    modemdr::Viterbi m_core;                // The forward pass, shared with DRM
    std::vector<uint64_t> m_decisions;
    std::vector<float> m_mother;
    float m_reliabilityLimit;
    float m_closeLimit;                     // The limit in metric units, for this codeword
    std::vector<uint64_t> m_close;          // By step: states where the losing path was within the limit
    std::vector<uint8_t> m_states;          // Of the decoded path, after each step
    std::vector<uint8_t> m_reliable;

};

} // namespace modemdab

#endif // MODEMDAB_DABCONVOLUTIONAL_H_
