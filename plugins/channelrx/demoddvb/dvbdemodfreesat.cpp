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

#include "dvbdemodfreesat.h"

// Reads the bits of the coded string, MSB first, with zeros past the end
class BitReader
{
public:
    BitReader(const uint8_t *data, int length) : m_data(data), m_length(length), m_bit(0) {}
    bool atEnd() const { return m_bit >= 8 * m_length; }
    //! The next 32 bits, without consuming them
    uint32_t peek() const
    {
        uint32_t value = 0;

        for (int i = 0; i < 32; i++)
        {
            const int bit = m_bit + i;
            const int byte = bit >> 3;
            const uint32_t b = byte < m_length ? (m_data[byte] >> (7 - (bit & 7))) & 1 : 0;
            value = (value << 1) | b;
        }

        return value;
    }
    void skip(int bits) { m_bit += bits; }

private:
    const uint8_t *m_data;
    int m_length;
    int m_bit;
};

bool DVBDemodFreesat::decode(const uint8_t *text, int length, std::string& out)
{
    if ((length < 2) || (text[0] != 0x1F) || ((text[1] != 1) && (text[1] != 2))) {
        return false;
    }

    const int number = text[1];
    const Code *codes = table(number);
    const uint16_t *starts = index(number);
    const int nbPrevious = indexSize(number) - 1;
    BitReader bits(text + 2, length - 2);
    uint8_t previous = 0; // The start of the string has the same codes as after its end

    // The string ends with the code for the end, or when the bits run out
    while (!bits.atEnd())
    {
        if (previous == m_escape)
        {
            // Literal bytes, until one that is ASCII, after which coding resumes from it
            const uint8_t c = (uint8_t) (bits.peek() >> 24);
            bits.skip(8);

            if (c & 0x80)
            {
                out.push_back((char) c);
                continue;
            }

            if (c == m_endOfString) {
                break;
            }

            // Control characters aren't output, but are the previous character, so an escape can follow
            if ((c >= 0x20) || (c == '\n')) {
                out.push_back((char) c);
            }

            previous = c;
            continue;
        }

        if (previous >= nbPrevious) {
            return false;
        }

        const uint32_t value = bits.peek();
        bool found = false;

        for (int j = starts[previous]; j < starts[previous + 1]; j++)
        {
            const uint32_t mask = codes[j].m_bits >= 32 ? 0xFFFFFFFF : ~(0xFFFFFFFFu >> codes[j].m_bits);

            if ((value & mask) == codes[j].m_value)
            {
                bits.skip(codes[j].m_bits);
                previous = codes[j].m_next;
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }

        if (previous == m_endOfString) {
            break;
        }

        if (previous != m_escape) {
            out.push_back((char) previous);
        }
    }

    return true;
}
