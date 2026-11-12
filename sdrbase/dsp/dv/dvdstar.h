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

#ifndef INCLUDE_DSP_DV_DVDSTAR_H
#define INCLUDE_DSP_DV_DVDSTAR_H

#include <cstdint>

#include "export.h"

/**
 * D-STAR radio header coding: 41 header bytes (flags, callsigns, CRC) sent LSB
 * first through a rate 1/2 K=3 convolutional code, a 660 bit block interleaver
 * and the x^7 + x^4 + 1 scrambler. Bit arrays hold one bit per byte in
 * transmission order.
 */
class SDRBASE_API DVDStar
{
public:
    static constexpr int m_headerBytes = 41;
    static constexpr int m_headerDataBits = 330;   //!< 328 header bits + 2 flush bits
    static constexpr int m_headerBits = 660;
    static constexpr int m_voiceBits = 72;
    static constexpr int m_dataBits = 24;          //!< Slow data per frame
    static constexpr int m_framesPerSuperframe = 21;

    //! CRC over the first 39 header bytes; stored in bytes 39 (low) and 40 (high)
    static uint16_t headerCRC(const uint8_t *header);
    static bool headerCRCCheck(const uint8_t *header);

    //! 41 header bytes -> 660 coded, interleaved and scrambled bits
    static void headerEncode(const uint8_t *header, uint8_t *bits);
    //! 660 received bits -> 41 header bytes. Returns the number of coded bits corrected by the
    //! Viterbi decoder, or -1 when the header CRC fails.
    static int headerDecode(const uint8_t *bits, uint8_t *header);
    //! Soft input variant from 660 LLRs
    static int headerDecodeSoft(const float *llr, uint8_t *header);

    //! 660 bit block interleaver and its inverse
    static void interleave(const uint8_t *in, uint8_t *out);
    static void deinterleave(const uint8_t *in, uint8_t *out);

    //! Slow data scrambling: the 3 data bytes of each frame (sent LSB first) are XORed with 0x70 0x4F 0x93
    static void scrambleSlowData(uint8_t *bits24);

private:
    static int headerFromCoded(const float *llr, uint8_t *header);
};

#endif // INCLUDE_DSP_DV_DVDSTAR_H
