///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_PAGERDEMODBCH_H
#define INCLUDE_PAGERDEMODBCH_H

#include <QtGlobal>

// BCH(31,21) code plus an even parity bit, as used by both POCSAG and FLEX.
//
// Codewords are in POCSAG bit order: bit 31 is transmitted first, bits 31-11 are data,
// bits 10-1 are the BCH check bits and bit 0 is even parity over the whole word.
// FLEX transmits codewords LSB first, so FLEX words are bit reversed with reverse()
// before and after decoding.
class PagerDemodBCH
{
public:
    //!< Returns cw with the BCH check bits (10-1) calculated. Parity bit 0 is cleared
    static quint32 encode(quint32 cw);
    //!< Corrects up to two bit errors in bits 31-1. Bit 0 is passed through unchanged.
    //!< Returns false if the errors are uncorrectable, in which case correctedCW = cw
    static bool decode(quint32 cw, quint32& correctedCW);
    static quint32 reverse(quint32 x);
    static int xorBits(quint32 word, int firstBit, int lastBit);
    static bool evenParity(quint32 word, int firstBit, int lastBit, int parityBit);
};

#endif // INCLUDE_PAGERDEMODBCH_H
