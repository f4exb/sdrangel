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

#ifndef INCLUDE_DSP_DV_DVBITS_H
#define INCLUDE_DSP_DV_DVBITS_H

#include <cstdint>

#include "export.h"

/**
 * Bit array helpers shared by the digital voice modulator and demodulator.
 *
 * Throughout the dv library a bit array holds one bit (0 or 1) per byte, in
 * transmission order (first on air first). A dibit array holds one dibit
 * (0..3) per byte, the first transmitted bit being the most significant.
 * A hexbit array holds one 6 bit symbol per byte.
 *
 * Soft decisions are log likelihood ratios (LLR) as floats, one per bit,
 * positive when bit 0 is more likely and negative when bit 1 is more likely,
 * the same convention as mbelib. Zero means no information (an erasure).
 */
class SDRBASE_API DVBits
{
public:
    //! Unpack bytes into bits, MSB of each byte first
    static void bytesToBitsMSB(const uint8_t *bytes, int nbBytes, uint8_t *bits);
    //! Unpack bytes into bits, LSB of each byte first
    static void bytesToBitsLSB(const uint8_t *bytes, int nbBytes, uint8_t *bits);
    //! Pack bits into bytes, MSB first. Zero fills the last byte.
    static void bitsToBytesMSB(const uint8_t *bits, int nbBits, uint8_t *bytes);
    //! Pack bits into bytes, LSB first. Zero fills the last byte.
    static void bitsToBytesLSB(const uint8_t *bits, int nbBits, uint8_t *bytes);

    //! Write the low nbBits of value at bits[pos] (MSB first). Returns the position after the field.
    static int putBits(uint8_t *bits, int pos, uint64_t value, int nbBits);
    //! Read nbBits from bits[pos] (MSB first)
    static uint64_t getBits(const uint8_t *bits, int pos, int nbBits);

    //! Pack a bit array (MSB first) into hexbits
    static void bitsToHexbits(const uint8_t *bits, int nbHexbits, uint8_t *hexbits);
    //! Unpack hexbits into bits (MSB first)
    static void hexbitsToBits(const uint8_t *hexbits, int nbHexbits, uint8_t *bits);

    //! Bits to dibits: dibit i = (bits[2i] << 1) | bits[2i+1]
    static void bitsToDibits(const uint8_t *bits, int nbDibits, uint8_t *dibits);
    //! Dibits to bits
    static void dibitsToBits(const uint8_t *dibits, int nbDibits, uint8_t *bits);

    //! Hard decisions from LLRs: 1 when llr < 0
    static void llrToBits(const float *llr, int nbBits, uint8_t *bits);
    //! LLRs from hard bits with a fixed magnitude
    static void bitsToLlr(const uint8_t *bits, int nbBits, float *llr, float magnitude = 1.0f);

    static int popcount(uint32_t v);
    static int popcount64(uint64_t v);
};

#endif // INCLUDE_DSP_DV_DVBITS_H
