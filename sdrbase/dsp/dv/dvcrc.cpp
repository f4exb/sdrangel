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

#include "dvbits.h"
#include "dvcrc.h"

uint32_t DVCRC::compute(const uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor)
{
    uint32_t mask = (order == 32) ? 0xFFFFFFFFu : ((1u << order) - 1);
    uint32_t crc = init & mask;

    for (int i = 0; i < nbBits; i++)
    {
        uint32_t feedback = ((crc >> (order - 1)) & 1) ^ (bits[i] & 1);
        crc = (crc << 1) & mask;

        if (feedback) {
            crc ^= polynomial;
        }
    }

    return (crc ^ finalXor) & mask;
}

void DVCRC::append(uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor)
{
    uint32_t crc = compute(bits, nbBits, order, polynomial, init, finalXor);
    DVBits::putBits(bits, nbBits, crc, order);
}

bool DVCRC::check(const uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor)
{
    uint32_t crc = compute(bits, nbBits, order, polynomial, init, finalXor);
    return crc == (uint32_t) DVBits::getBits(bits, nbBits, order);
}

uint16_t DVCRC::dstarHeader(const uint8_t *header)
{
    return dstar16(header, 39);
}

uint16_t DVCRC::dstar16(const uint8_t *header, int nbBytes)
{
    uint16_t crc = 0xffff;

    for (int n = 0; n < nbBytes; n++)
    {
        for (int m = 0; m < 8; m++)
        {
            crc ^= (header[n] >> m) & 1;

            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8408;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc ^ 0xffff;
}

uint16_t DVCRC::ccitt16(const uint8_t *bytes, int nbBytes, uint16_t init, uint16_t finalXor)
{
    uint16_t crc = init;

    for (int i = 0; i < nbBytes; i++)
    {
        crc ^= (uint16_t) (bytes[i] << 8);

        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t) ((crc << 1) ^ 0x1021) : (uint16_t) (crc << 1);
        }
    }

    return crc ^ finalXor;
}

uint32_t DVCRC::crc32(const uint8_t *bytes, int nbBytes)
{
    uint32_t crc = 0xffffffff;

    for (int i = 0; i < nbBytes; i++)
    {
        crc ^= bytes[i];

        for (int b = 0; b < 8; b++) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xedb88320) : (crc >> 1);
        }
    }

    return crc ^ 0xffffffff;
}

uint8_t DVCRC::dmrLcChecksum(const uint8_t *lc)
{
    unsigned int sum = 0;

    for (int i = 0; i < 9; i++) {
        sum += lc[i];
    }

    return (uint8_t) (sum % 31);
}
