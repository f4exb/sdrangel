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

#include "drmbitutils.h"

#include <array>
#include <stdexcept>

namespace modemdrm
{

namespace
{

// P(X) = X^9 + X^5 + 1 from the all-ones state has period 2^9 - 1 = 511.
constexpr std::size_t PRBSPeriod = 511U;

const std::array<std::uint8_t, PRBSPeriod>& prbsPeriod()
{
    static const std::array<std::uint8_t, PRBSPeriod> period = [] {
        std::array<std::uint8_t, PRBSPeriod> result {};
        std::uint16_t registerBits = 0x01ffU;
        for (std::size_t i = 0; i < PRBSPeriod; ++i)
        {
            // With stages numbered from the input, feedback/output is stage 4
            // XOR stage 8. This yields the normative first sixteen bits
            // 0000011110111110 from the all-ones state.
            const std::uint8_t bit = static_cast<std::uint8_t>(
                ((registerBits >> 4) ^ (registerBits >> 8)) & 1U
            );
            result[i] = bit;
            registerBits = static_cast<std::uint16_t>(((registerBits << 1) & 0x01feU) | bit);
        }

        return result;
    }();
    return period;
}

// Byte-at-a-time tables for the MSB-first CRCs below.
template <typename Word, int Degree, std::uint32_t Polynomial>
const std::array<Word, 256>& crcTable()
{
    static const std::array<Word, 256> table = [] {
        std::array<Word, 256> result {};
        constexpr std::uint32_t mask = (1U << Degree) - 1U;
        constexpr std::uint32_t topBit = 1U << (Degree - 1);
        for (std::uint32_t byte = 0; byte < 256U; ++byte)
        {
            std::uint32_t remainder = byte << (Degree - 8);
            for (int bit = 0; bit < 8; ++bit)
            {
                remainder = (remainder & topBit) ? ((remainder << 1) ^ Polynomial) : (remainder << 1);
                remainder &= mask;
            }

            result[byte] = static_cast<Word>(remainder);
        }

        return result;
    }();
    return table;
}

template <typename Word, int Degree, std::uint32_t Polynomial>
Word crcBytes(const std::uint8_t* bytes, std::size_t byteCount)
{
    if (bytes == nullptr && byteCount != 0) {
        throw std::invalid_argument("Invalid DRM CRC parameters");
    }

    constexpr std::uint32_t mask = (1U << Degree) - 1U;
    const std::array<Word, 256>& table = crcTable<Word, Degree, Polynomial>();
    std::uint32_t remainder = mask;
    for (std::size_t i = 0; i < byteCount; ++i) {
        remainder = ((remainder << 8) ^ table[((remainder >> (Degree - 8)) ^ bytes[i]) & 0xffU]) & mask;
    }

    return static_cast<Word>((~remainder) & mask);
}

} // namespace

std::vector<std::uint8_t> EnergyDispersal::sequence(std::size_t length)
{
    const std::array<std::uint8_t, PRBSPeriod>& period = prbsPeriod();
    std::vector<std::uint8_t> result(length);
    for (std::size_t i = 0, phase = 0; i < length; ++i)
    {
        result[i] = period[phase];
        if (++phase == PRBSPeriod) {
            phase = 0;
        }
    }

    return result;
}

void EnergyDispersal::apply(std::vector<std::uint8_t>& bits)
{
    const std::array<std::uint8_t, PRBSPeriod>& period = prbsPeriod();
    for (std::size_t i = 0, phase = 0; i < bits.size(); ++i)
    {
        bits[i] = static_cast<std::uint8_t>((bits[i] ^ period[phase]) & 1U);
        if (++phase == PRBSPeriod) {
            phase = 0;
        }
    }
}

std::uint32_t CRC::compute(
    const std::uint8_t* bits,
    std::size_t bitCount,
    int degree,
    std::uint32_t polynomialWithoutLeadingTerm
)
{
    if (degree < 1 || degree > 31 || (bits == nullptr && bitCount != 0)) {
        throw std::invalid_argument("Invalid DRM CRC parameters");
    }

    const std::uint32_t mask = (1U << degree) - 1U;
    const std::uint32_t topBit = 1U << (degree - 1);
    std::uint32_t remainder = mask;

    for (std::size_t i = 0; i < bitCount; ++i)
    {
        const bool feedback = ((remainder & topBit) != 0) ^ ((bits[i] & 1U) != 0);
        remainder = (remainder << 1) & mask;
        if (feedback) {
            remainder ^= polynomialWithoutLeadingTerm;
        }
    }

    return (~remainder) & mask;
}

std::uint8_t CRC::crc8(const std::uint8_t* bits, std::size_t bitCount)
{
    // G8(X) = X^8 + X^4 + X^3 + X^2 + 1.
    return static_cast<std::uint8_t>(compute(bits, bitCount, 8, 0x1dU));
}

std::uint16_t CRC::crc16(const std::uint8_t* bits, std::size_t bitCount)
{
    // G16(X) = X^16 + X^12 + X^5 + 1.
    return static_cast<std::uint16_t>(compute(bits, bitCount, 16, 0x1021U));
}

std::uint8_t CRC::crc8Bytes(const std::uint8_t* bytes, std::size_t byteCount)
{
    return crcBytes<std::uint8_t, 8, 0x1dU>(bytes, byteCount);
}

std::uint16_t CRC::crc16Bytes(const std::uint8_t* bytes, std::size_t byteCount)
{
    return crcBytes<std::uint16_t, 16, 0x1021U>(bytes, byteCount);
}

} // namespace modemdrm
