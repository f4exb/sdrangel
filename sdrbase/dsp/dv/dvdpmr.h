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

#ifndef INCLUDE_DSP_DV_DVDPMR_H
#define INCLUDE_DSP_DV_DVDPMR_H

#include <cstdint>

#include "export.h"

/**
 * dPMR (ETSI TS 102 490 / TS 102 658) channel coding of the header information
 * (HI), control channel (CCH) and END fields: CRC, shortened Hamming(12,8), block
 * interleaving and scrambling (clause 7). Bit arrays hold one bit per byte, MSB
 * first, in transmission order.
 */
class SDRBASE_API DVDPMR
{
public:
    static const int m_frameBits = 384;          //!< Payload or header frame (80 ms)
    static const int m_fs1Bits = 48;
    static const int m_fs2Bits = 24;
    static const int m_fs3Bits = 24;
    static const int m_ccBits = 24;              //!< Colour code field
    static const int m_hiInfoBits = 72;          //!< Header information before CRC8
    static const int m_hiCodedBits = 120;
    static const int m_cchInfoBits = 41;         //!< CCH information before CRC7
    static const int m_cchCodedBits = 72;
    static const int m_endInfoBits = 17;         //!< END information before CRC7
    static const int m_endCodedBits = 72;
    static const int m_tchBits = 72;             //!< One AMBE+2 vocoder frame

    // ---- Coding primitives (clause 7) --------------------------------------------
    //! Append a CRC7 (x^7 + x^3 + 1) to bits[0..nbBits-1]
    static void crc7Append(uint8_t *bits, int nbBits);
    //! Append a CRC8 (x^8 + x^2 + x + 1) to bits[0..nbBits-1]
    static void crc8Append(uint8_t *bits, int nbBits);
    static bool crc7Check(const uint8_t *bits, int nbBits);
    static bool crc8Check(const uint8_t *bits, int nbBits);
    //! Encode nbBlocks bytes of data (8 bits each) into nbBlocks 12 bit Hamming(12,8) codewords
    static void hammingBlockEncode(const uint8_t *data, int nbBlocks, uint8_t *coded);
    //! Decode nbBlocks codewords in place and extract the data bits. Returns corrected bits or -1.
    static int hammingBlockDecode(uint8_t *coded, int nbBlocks, uint8_t *data);
    //! 12 x nbBlocks interleave (tables 7.2 and 7.3): out[i] = in[12 * (i % nbBlocks) + i / nbBlocks]
    static void interleave(const uint8_t *in, int nbBlocks, uint8_t *out);
    static void deinterleave(const uint8_t *in, int nbBlocks, uint8_t *out);
    //! XOR bits with the scrambling sequence restarted from its initial state (clause 7.4)
    static void scramble(uint8_t *bits, int nbBits);

    // ---- Fields ------------------------------------------------------------------
    //! CRC8 + Hamming + 12 x 10 interleave + scrambling: 72 -> 120 bits (clause 7.7)
    static void headerInfoEncode(const uint8_t *hi72, uint8_t *coded120);
    //! 120 -> 72 bits. Returns corrected bits or -1 when uncorrectable or the CRC fails.
    static int headerInfoDecode(const uint8_t *coded120, uint8_t *hi72);
    //! CRC7 + Hamming + 12 x 6 interleave + scrambling: 41 -> 72 bits (clause 7.6)
    static void cchEncode(const uint8_t *cch41, uint8_t *coded72);
    static int cchDecode(const uint8_t *coded72, uint8_t *cch41);
    //! CRC7 + Hamming + scrambling of 36 bits then repeated: 17 -> 72 bits (clause 7.8)
    static void endEncode(const uint8_t *end17, uint8_t *coded72);
    //! Decodes the first copy, or the second when the first fails
    static int endDecode(const uint8_t *coded72, uint8_t *end17);

    // ---- Colour codes (table 6.1) ------------------------------------------------
    //! 24 bit di-bit encoded colour code field of colour code 0..63
    static uint32_t colourCodeSequence(int colourCode);
    static void colourCodeBits(int colourCode, uint8_t *bits24);
    //! Colour code number of a received 24 bit field, matched on symbol signs. Returns -1 if none within 2 symbols.
    static int colourCodeMatch(const uint8_t *bits24, int& errors);

private:
    static const uint32_t m_colourCodes[64];
};

#endif // INCLUDE_DSP_DV_DVDPMR_H
