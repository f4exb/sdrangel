///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGBCH_H
#define MODEMRADIOPAGING_RADIOPAGINGBCH_H

#include <cstdint>

#include "export.h"

// BCH(31,21) code with an even parity bit, the 32-bit codeword used by POCSAG and FLEX.
//
// Word layout (POCSAG transmission order, bit 31 sent first):
//   bits 31-11  21 data bits
//   bits 10-1   10 check bits, generator g(x) = x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + 1
//   bit 0       even parity over bits 31-1
// FLEX sends the same codeword least significant bit first: reverse() converts between
// the two orders.
class MODEMRADIOPAGING_API RadioPagingBCH
{
public:
    struct Result
    {
        uint32_t m_word;        //!< Corrected codeword (unchanged if uncorrectable)
        int m_bitErrors;        //!< Bits corrected in bits 31-1, -1 if uncorrectable
        bool m_parityError;     //!< Even parity fails after correction
    };

    //!< Codeword for 21 data bits (bits 20-0 of data), check and parity bits set
    static uint32_t encode(uint32_t data);
    //!< Corrects up to two bit errors in the 32 bits and detects three (minimum distance 6 with the
    //!< parity bit). Two errors in bits 31-1 are corrected when the parity then holds; a wrong parity
    //!< after correcting at most one bit in bits 31-1 is an error of the parity bit, which is repaired
    //!< (m_parityError set). Two corrected bits and a wrong parity is uncorrectable (m_bitErrors -1)
    static Result decode(uint32_t word);
    //!< 21 data bits of a codeword
    static uint32_t data(uint32_t word) { return word >> 11; }
    //!< Syndrome of bits 31-1, 0 for a valid codeword
    static uint32_t syndrome(uint32_t word);
    static bool evenParity(uint32_t word);
    static uint32_t reverse(uint32_t word);

    static const uint32_t m_generator = 0x769;
};

#endif // MODEMRADIOPAGING_RADIOPAGINGBCH_H
