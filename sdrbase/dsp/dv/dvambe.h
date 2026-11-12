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

#ifndef INCLUDE_DSP_DV_DVAMBE_H
#define INCLUDE_DSP_DV_DVAMBE_H

#include <cstdint>

#include "export.h"

/**
 * AMBE+2 3600x2450 voice frame layout as transmitted by DMR, NXDN, dPMR and YSF
 * V/D type 1: 72 on-air bits carrying 49 parameter bits protected by Golay(24,12)
 * (C0), scrambled Golay(23,12) (C1) and two unprotected blocks (C2, C3).
 *
 * The bit plane form fr[4][24] and the parameter bit order ambe_d[49] are those
 * of mbelib, so the results can be handed to its data path decoders. The
 * vocoder itself lives in mbelib; this class only moves bits around.
 */
class SDRBASE_API DVAMBE
{
public:
    static constexpr int m_frameBits = 72;
    static constexpr int m_frameBytes = 9;
    static constexpr int m_paramBits = 49;

    //! 72 on-air bits -> fr[4][24] bit planes (DMR interleave)
    static void bitsToFrame(const uint8_t *bits, uint8_t fr[4][24]);
    //! fr[4][24] bit planes -> 72 on-air bits
    static void frameToBits(const uint8_t fr[4][24], uint8_t *bits);

    //! 72 on-air bits -> 49 parameter bits: Golay decoding of C0 and C1, descrambling of C1.
    //! Returns the number of corrected bits, or -1 if C0 is uncorrectable.
    static int bitsToParams(const uint8_t *bits, uint8_t *params);
    //! 49 parameter bits -> 72 on-air bits (Golay encoding and C1 scrambling)
    static void paramsToBits(const uint8_t *params, uint8_t *bits);
    //! 72 LLRs (positive favours 0) -> 49 parameter bits by maximum likelihood decoding of C0 and C1.
    //! Returns the number of hard decisions the decoded codewords differ from.
    static int llrToParams(const float *llr, uint8_t *params);
    //! 9 byte frame (MSB first) -> 49 parameter bits. Returns as bitsToParams().
    static int bytesToParams(const uint8_t *frame, uint8_t *params);
    //! 49 parameter bits -> 9 byte frame (MSB first)
    static void paramsToBytes(const uint8_t *params, uint8_t *frame);

    //! C1 scrambling sequence from the 12 C0 data bits: 23 bits, bit k applies to fr[1][22 - k]
    static void pnSequence(const uint8_t *c0, uint8_t *pn23);

    //! 49 parameter bits -> 7 byte DVSI raw 2450 word (MSB first, the AMBE-3000 rate 34 channel format)
    static void paramsToDvsi(const uint8_t *params, uint8_t *bytes7);
    //! 7 byte DVSI raw 2450 word -> 49 parameter bits
    static void dvsiToParams(const uint8_t *bytes7, uint8_t *params);
    //! Parameter bit i is bit m_dvsiOrder[i] of the 49 bit DVSI word
    static const int m_dvsiOrder[49];

    static const int m_rW[36]; //!< Bit 1 of dibit i -> fr[rW][rX]
    static const int m_rX[36];
    static const int m_rY[36]; //!< Bit 0 of dibit i -> fr[rY][rZ]
    static const int m_rZ[36];
};

#endif // INCLUDE_DSP_DV_DVAMBE_H
