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

#include <array>

#include "crc.h"

namespace modemdr
{

namespace
{

const std::array<std::uint16_t, 256>& crc16Table()
{
    static const std::array<std::uint16_t, 256> table = [] {
        std::array<std::uint16_t, 256> t {};
        for (int i = 0; i < 256; i++)
        {
            std::uint16_t crc = static_cast<std::uint16_t>(i << 8);
            for (int b = 0; b < 8; b++) {
                crc = (crc & 0x8000U) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021U) : static_cast<std::uint16_t>(crc << 1);
            }
            t[static_cast<std::size_t>(i)] = crc;
        }
        return t;
    }();
    return table;
}

} // anonymous namespace

std::uint16_t CRC::crc16(const std::uint8_t *data, std::size_t length)
{
    const std::array<std::uint16_t, 256>& table = crc16Table();
    std::uint16_t crc = 0xffffU;

    for (std::size_t i = 0; i < length; i++) {
        crc = static_cast<std::uint16_t>((crc << 8) ^ table[((crc >> 8) ^ data[i]) & 0xffU]);
    }
    return static_cast<std::uint16_t>(~crc);
}

bool CRC::crc16Check(const std::uint8_t *data, std::size_t lengthWithCRC)
{
    if (lengthWithCRC < 2) {
        return false;
    }
    const std::uint16_t crc = static_cast<std::uint16_t>((data[lengthWithCRC - 2] << 8) | data[lengthWithCRC - 1]);
    return crc16(data, lengthWithCRC - 2) == crc;
}

std::uint32_t CRC::crc32(const std::uint8_t *data, std::size_t length)
{
    std::uint32_t crc = 0xffffffffU;

    for (std::size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

} // namespace modemdr
