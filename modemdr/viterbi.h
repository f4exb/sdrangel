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

#ifndef MODEMDR_VITERBI_H_
#define MODEMDR_VITERBI_H_

#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdr
{

// The forward pass of a Viterbi decoder for the K = 7 mother codes of DAB (EN 300 401 clause 11:
// rate 1/4, generators 133, 171, 145, 133 octal) and DRM (ES 201 980 clause 7.3.3: rate 1/6,
// generators 133, 171, 145, 133, 171, 145). The register is (input << 6) | state, where the state
// holds the six previous inputs, the newest in bit 5; the next state is (state >> 1) |
// (input << 5). Every generator taps both the input and the oldest bit, so one branch metric
// and its negation serve the four transitions of each butterfly (SSE2 or NEON where available).
class MODEMDR_API Viterbi
{
public:
    static constexpr int States = 64;
    static constexpr int MaximumOutputs = 6;

    // Generator polynomials, as above: four for DAB, six for DRM
    explicit Viterbi(const std::vector<unsigned int>& generators);
    static const std::vector<unsigned int>& dabGenerators();
    static const std::vector<unsigned int>& drmGenerators();

    int outputs() const { return m_outputs; }
    // The mother code's output bits for a register value, output k in bit k
    unsigned int registerOutputs(unsigned int reg) const;

    // steps steps of outputs() soft values each: positive for a transmitted zero, negative for
    // a one, zero for a punctured bit. Bit s of decisions[t] is set when the odd predecessor of
    // state s wins at step t (the even one on a tie). With closeLimit above zero, bit s of
    // close[t] is set when the two paths into s are within closeLimit.
    void forward(const float *soft, int steps, std::uint64_t *decisions, std::uint64_t *close = nullptr,
        float closeLimit = 0.0f) const;

    // The path ending in state 0: its information bits (the input of each of the first
    // informationBits steps) and, if states is given, the state after each step.
    static void traceback(const std::uint64_t *decisions, int steps, int informationBits, std::uint8_t *bits,
        std::uint8_t *states = nullptr);

private:
    int m_outputs;
    std::vector<unsigned int> m_generators;
    // The sign (+1 for 0, -1 for 1) of output k of the register (0 << 6) | (2j), by k and j
    alignas(16) float m_signs[MaximumOutputs][32];

    void forwardScalar(const float *soft, int steps, std::uint64_t *decisions, std::uint64_t *close, float closeLimit) const;
};

} // namespace modemdr

#endif // MODEMDR_VITERBI_H_
