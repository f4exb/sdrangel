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

#ifndef INCLUDE_DSP_DV_DVCONVOLUTIONAL_H
#define INCLUDE_DSP_DV_DVCONVOLUTIONAL_H

#include <cstdint>
#include <vector>

#include "export.h"

/**
 * Rate 1/2 binary convolutional code with constraint length K and a soft input
 * Viterbi decoder.
 *
 * Encoder convention (the one the digital voice protocols share): a K bit shift
 * register receives each new bit at its most significant end,
 * r = (r >> 1) | (bit << (K - 1)), and output j is the parity of r & poly[j].
 * The two outputs of each input bit are transmitted poly 0 first. The encoder
 * starts in state 0; a protocol that appends K - 1 zero tail bits returns it to
 * state 0, which the decoder exploits when terminated is true.
 */
class SDRBASE_API DVConvolutional
{
public:
    DVConvolutional(int k, uint32_t poly0, uint32_t poly1);

    int getK() const { return m_k; }

    //! nbBits data bits -> 2 * nbBits coded bits
    void encode(const uint8_t *bits, int nbBits, uint8_t *coded) const;
    //! nbBits data bits -> nbBits dibits, poly 0 output in the MSB
    void encodeDibits(const uint8_t *bits, int nbBits, uint8_t *dibits) const;

    //! Decode nbBits data bits from 2 * nbBits LLRs (positive favours 0, zero for a punctured
    //! or erased position). Returns the number of coded hard decisions the survivor path disagrees with.
    int decodeSoft(const float *llr, int nbBits, uint8_t *bits, bool terminated = true) const;
    //! Decode from hard coded bits. Returns the number of coded bits corrected.
    int decodeHard(const uint8_t *coded, int nbBits, uint8_t *bits, bool terminated = true) const;
    //! Decode from hard dibits (poly 0 output in the MSB)
    int decodeDibits(const uint8_t *dibits, int nbBits, uint8_t *bits, bool terminated = true) const;

    static const DVConvolutional& dstar(); //!< K=3, polynomials 0x7 and 0x5 (D-STAR radio header)
    static const DVConvolutional& k5();    //!< K=5, polynomials 0x13 and 0x1D (YSF, NXDN)

private:
    int m_k;
    int m_nbStates;
    std::vector<uint8_t> m_outputs; //!< outputs[(state << 1) | bit] = (out0 << 1) | out1

    static int parity(uint32_t v);
};

#endif // INCLUDE_DSP_DV_DVCONVOLUTIONAL_H
