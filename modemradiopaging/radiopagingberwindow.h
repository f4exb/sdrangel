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

#ifndef MODEMRADIOPAGING_RADIOPAGINGBERWINDOW_H
#define MODEMRADIOPAGING_RADIOPAGINGBERWINDOW_H

#include <cstdint>

// Bit error rate of the codewords received recently: the bits a code corrected over the
// bits received, over a window that halves every m_windowCodewords codewords. A codeword
// the code could not correct counts with m_uncorrectableBits errors (its real number of
// errors is not known) and is counted on its own, so a reception with errors beyond the
// correction capacity can be told apart from one whose errors were all corrected.
struct RadioPagingBerWindow
{
    static const int m_windowCodewords = 64;
    static const int m_uncorrectableBits = 3;

    uint32_t m_errorBits = 0;
    uint32_t m_bits = 0;
    uint32_t m_uncorrectable = 0;
    int m_codewords = 0;

    void reset()
    {
        m_errorBits = 0;
        m_bits = 0;
        m_uncorrectable = 0;
        m_codewords = 0;
    }

    //!< One codeword of nbBits bits with errorBits corrected, or uncorrectable
    void add(int errorBits, int nbBits, bool uncorrectable)
    {
        m_errorBits += uncorrectable ? m_uncorrectableBits : (uint32_t) errorBits;
        m_bits += (uint32_t) nbBits;
        m_uncorrectable += uncorrectable ? 1 : 0;

        if (++m_codewords >= m_windowCodewords)
        {
            m_errorBits /= 2;
            m_bits /= 2;
            m_uncorrectable /= 2;
            m_codewords = m_windowCodewords / 2;
        }
    }

    bool valid() const { return m_bits > 0; }
    float ber() const { return (m_bits > 0) ? (float) m_errorBits / (float) m_bits : 0.0f; }
};

#endif // MODEMRADIOPAGING_RADIOPAGINGBERWINDOW_H
