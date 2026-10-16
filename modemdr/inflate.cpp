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

#include <algorithm>
#include <array>

#include "crc.h"
#include "inflate.h"

namespace modemdr
{

// Raw DEFLATE (RFC 1951): stored, fixed-Huffman and dynamic-Huffman blocks.
class Inflater
{
public:
    Inflater(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output, std::size_t maximumOutput) :
        m_data(data),
        m_size(size),
        m_output(output),
        m_maximumOutput(maximumOutput)
    {
    }

    bool run()
    {
        bool last = false;

        while (!last)
        {
            int value = 0;

            if (!bits(1, value)) {
                return false;
            }

            last = value != 0;

            if (!bits(2, value)) {
                return false;
            }

            bool ok = false;

            if (value == 0) {
                ok = stored();
            } else if (value == 1) {
                ok = fixed();
            } else if (value == 2) {
                ok = dynamic();
            }

            if (!ok) {
                return false;
            }
        }

        return true;
    }

private:
    // A canonical Huffman code: how many codes of each length, and the
    // symbols in code order.
    struct Huffman
    {
        std::array<int, 16> counts {};
        std::vector<int> symbols;

        bool build(const int* lengths, int n)
        {
            counts.fill(0);

            for (int symbol = 0; symbol < n; ++symbol) {
                ++counts[static_cast<std::size_t>(lengths[symbol])];
            }

            // An over-subscribed code is invalid; an incomplete one is allowed
            // (a code with a single symbol, for example).
            int left = 1;

            for (int length = 1; length < 16; ++length)
            {
                left = 2 * left - counts[static_cast<std::size_t>(length)];

                if (left < 0) {
                    return false;
                }
            }

            std::array<int, 16> offsets {};

            for (int length = 1; length < 15; ++length) {
                offsets[static_cast<std::size_t>(length + 1)] = offsets[static_cast<std::size_t>(length)] + counts[static_cast<std::size_t>(length)];
            }

            symbols.assign(static_cast<std::size_t>(n), 0);

            for (int symbol = 0; symbol < n; ++symbol)
            {
                if (lengths[symbol] != 0) {
                    symbols[static_cast<std::size_t>(offsets[static_cast<std::size_t>(lengths[symbol])]++)] = symbol;
                }
            }

            return true;
        }
    };

    bool bits(int count, int& value)
    {
        std::uint32_t result = m_bitBuffer;

        while (m_bitCount < count)
        {
            if (m_position >= m_size) {
                return false;
            }

            result |= static_cast<std::uint32_t>(m_data[m_position++]) << m_bitCount;
            m_bitCount += 8;
        }

        m_bitBuffer = result >> count;
        m_bitCount -= count;
        value = static_cast<int>(result & ((1U << count) - 1U));
        return true;
    }

    bool decode(const Huffman& huffman, int& symbol)
    {
        // Codes are sent most significant bit first, one bit at a time.
        int code = 0;
        int first = 0;
        int index = 0;

        for (int length = 1; length < 16; ++length)
        {
            int bit = 0;

            if (!bits(1, bit)) {
                return false;
            }

            code |= bit;
            const int count = huffman.counts[static_cast<std::size_t>(length)];

            if (code - count < first)
            {
                symbol = huffman.symbols[static_cast<std::size_t>(index + (code - first))];
                return true;
            }

            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }

        return false;
    }

    bool stored()
    {
        // Byte aligned: LEN and its complement, then the bytes.
        m_bitBuffer = 0;
        m_bitCount = 0;

        if (m_position + 4U > m_size) {
            return false;
        }

        const unsigned int length = m_data[m_position] | (m_data[m_position + 1U] << 8);
        const unsigned int complement = m_data[m_position + 2U] | (m_data[m_position + 3U] << 8);
        m_position += 4U;

        if (length != (~complement & 0xffffU) || m_position + length > m_size || m_output.size() + length > m_maximumOutput) {
            return false;
        }

        m_output.insert(m_output.end(), m_data + m_position, m_data + m_position + length);
        m_position += length;
        return true;
    }

    bool codes(const Huffman& lengthCodes, const Huffman& distanceCodes)
    {
        static constexpr int LengthBase[29] = {
            3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static constexpr int LengthExtra[29] = {
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
            3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static constexpr int DistanceBase[30] = {
            1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
            257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static constexpr int DistanceExtra[30] = {
            0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
            7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

        for (;;)
        {
            int symbol = 0;

            if (!decode(lengthCodes, symbol)) {
                return false;
            }

            if (symbol < 256)
            {
                if (m_output.size() >= m_maximumOutput) {
                    return false;
                }

                m_output.push_back(static_cast<std::uint8_t>(symbol));
                continue;
            }

            if (symbol == 256) {
                return true; // end of block
            }

            symbol -= 257;

            if (symbol >= 29) {
                return false;
            }

            int extra = 0;

            if (!bits(LengthExtra[symbol], extra)) {
                return false;
            }

            const std::size_t length = static_cast<std::size_t>(LengthBase[symbol] + extra);

            if (!decode(distanceCodes, symbol) || symbol >= 30 || !bits(DistanceExtra[symbol], extra)) {
                return false;
            }

            const std::size_t distance = static_cast<std::size_t>(DistanceBase[symbol] + extra);

            if (distance > m_output.size() || m_output.size() + length > m_maximumOutput) {
                return false;
            }

            // Byte by byte: a copy may overlap what it produces.
            for (std::size_t i = 0; i < length; ++i) {
                m_output.push_back(m_output[m_output.size() - distance]);
            }
        }
    }

    bool fixed()
    {
        std::array<int, 288> lengths {};
        std::fill(lengths.begin(), lengths.begin() + 144, 8);
        std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
        std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
        std::fill(lengths.begin() + 280, lengths.end(), 8);
        std::array<int, 30> distances {};
        distances.fill(5);
        Huffman lengthCodes;
        Huffman distanceCodes;
        return lengthCodes.build(lengths.data(), 288) && distanceCodes.build(distances.data(), 30)
            && codes(lengthCodes, distanceCodes);
    }

    bool dynamic()
    {
        int literalCount = 0;
        int distanceCount = 0;
        int codeLengthCount = 0;

        if (!bits(5, literalCount) || !bits(5, distanceCount) || !bits(4, codeLengthCount)) {
            return false;
        }

        literalCount += 257;
        distanceCount += 1;
        codeLengthCount += 4;

        if (literalCount > 286 || distanceCount > 30) {
            return false;
        }

        static constexpr int Order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        std::array<int, 19> codeLengths {};

        for (int i = 0; i < codeLengthCount; ++i)
        {
            if (!bits(3, codeLengths[static_cast<std::size_t>(Order[i])])) {
                return false;
            }
        }

        Huffman lengthHuffman;

        if (!lengthHuffman.build(codeLengths.data(), 19)) {
            return false;
        }

        std::array<int, 316> lengths {};
        int index = 0;

        while (index < literalCount + distanceCount)
        {
            int symbol = 0;

            if (!decode(lengthHuffman, symbol)) {
                return false;
            }

            if (symbol < 16)
            {
                lengths[static_cast<std::size_t>(index++)] = symbol;
                continue;
            }

            int repeat = 0;
            int value = 0;

            if (symbol == 16)
            {
                // Repeat the previous length 3-6 times.
                if (index == 0 || !bits(2, repeat)) {
                    return false;
                }

                value = lengths[static_cast<std::size_t>(index - 1)];
                repeat += 3;
            }
            else if (symbol == 17)
            {
                if (!bits(3, repeat)) {
                    return false;
                }

                repeat += 3;
            }
            else
            {
                if (!bits(7, repeat)) {
                    return false;
                }

                repeat += 11;
            }

            if (index + repeat > literalCount + distanceCount) {
                return false;
            }

            while (repeat-- > 0) {
                lengths[static_cast<std::size_t>(index++)] = value;
            }
        }

        if (lengths[256] == 0) {
            return false; // no end of block code
        }

        Huffman lengthCodes;
        Huffman distanceCodes;
        return lengthCodes.build(lengths.data(), literalCount)
            && distanceCodes.build(lengths.data() + literalCount, distanceCount)
            && codes(lengthCodes, distanceCodes);
    }

    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
    std::uint32_t m_bitBuffer = 0;
    int m_bitCount = 0;
    std::vector<std::uint8_t>& m_output;
    std::size_t m_maximumOutput;
};

bool inflateRaw(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output, std::size_t maximumOutput)
{
    output.clear();
    Inflater inflater(data, size, output, maximumOutput);
    return inflater.run();
}

bool gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output, std::size_t maximumOutput)
{
    // RFC 1952: a header, raw DEFLATE data, then CRC-32 and size
    if ((size < 18U) || (data[0] != 0x1fU) || (data[1] != 0x8bU) || (data[2] != 8U)) {
        return false;
    }

    const int flags = data[3];
    std::size_t position = 10U;

    if (flags & 0x04)
    {
        // FEXTRA
        if (position + 2U > size) {
            return false;
        }
        position += 2U + (data[position] | (data[position + 1U] << 8));
    }

    for (int field : {0x08, 0x10})
    {
        // FNAME and FCOMMENT, each zero terminated
        if (flags & field)
        {
            while ((position < size) && (data[position] != 0U)) {
                ++position;
            }
            ++position;
        }
    }

    if (flags & 0x02) {
        position += 2U; // FHCRC
    }

    if (position + 8U > size) {
        return false;
    }

    const std::uint32_t crc = data[size - 8U] | (data[size - 7U] << 8) | (data[size - 6U] << 16)
        | (static_cast<std::uint32_t>(data[size - 5U]) << 24);
    const std::size_t expected = data[size - 4U] | (data[size - 3U] << 8) | (data[size - 2U] << 16)
        | (static_cast<std::size_t>(data[size - 1U]) << 24);
    return inflateRaw(data + position, size - position - 8U, output, maximumOutput) && (output.size() == expected)
        && (CRC::crc32(output.data(), output.size()) == crc);
}

} // namespace modemdr
