///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef MODEMDVB_VITERBI27KERNEL_H
#define MODEMDVB_VITERBI27KERNEL_H

// Internal to modemdvb: the Viterbi decoder's add-compare-select for instruction sets chosen at run time

#include <cstdint>

namespace dvbt {

#if defined(MODEMDVB_X86)
//! With 8-bit path metrics, smallest best. metrics: 64, updated. outputs: the encoder's outputs (X | Y << 1) from
//! states 0..31 with input 0. costs: per step, the branch costs for outputs 0..3 in bytes 0..3. decisions: nbBits
//! words, bit s set when state s came from s/2 + 32.
//! The smallest metric at the start of each pair of steps (or of a last step on its own, for an odd nbBits) is
//! subtracted at its end (metrics can't get smaller), so finding it overlaps the steps. From the best state one branch costs at most half the step's costs (63), so the
//! smallest metric is at most 126 after subtracting and 252 before: the best paths never saturate at 255
void viterbi27Forward8AVX2(uint8_t *metrics, const uint8_t *outputs, const uint32_t *costs, int nbBits,
    uint64_t *decisions);
void viterbi27Forward8SSE41(uint8_t *metrics, const uint8_t *outputs, const uint32_t *costs, int nbBits,
    uint64_t *decisions);
#endif

} // namespace dvbt

#endif // MODEMDVB_VITERBI27KERNEL_H
