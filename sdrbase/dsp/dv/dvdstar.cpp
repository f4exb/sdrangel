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

#include "dvbits.h"
#include "dvconvolutional.h"
#include "dvcrc.h"
#include "dvdstar.h"
#include "dvscrambler.h"

uint16_t DVDStar::headerCRC(const uint8_t *header)
{
    return DVCRC::dstarHeader(header);
}

bool DVDStar::headerCRCCheck(const uint8_t *header)
{
    uint16_t crc = headerCRC(header);
    return (header[39] == (crc & 0xff)) && (header[40] == ((crc >> 8) & 0xff));
}

void DVDStar::interleave(const uint8_t *in, uint8_t *out)
{
    // 660 bits written into a block of 24 columns and read out column by column;
    // the last 12 columns have 27 rows, the first 12 have 28
    int k = 0;

    for (int loop = 0; loop < m_headerBits; loop++)
    {
        out[loop] = in[k];
        k += 24;

        if (k >= 672) {
            k -= 671;
        } else if (k >= 660) {
            k -= 647;
        }
    }
}

void DVDStar::deinterleave(const uint8_t *in, uint8_t *out)
{
    int k = 0;

    for (int loop = 0; loop < m_headerBits; loop++)
    {
        out[k] = in[loop];
        k += 24;

        if (k >= 672) {
            k -= 671;
        } else if (k >= 660) {
            k -= 647;
        }
    }
}

void DVDStar::headerEncode(const uint8_t *header, uint8_t *bits)
{
    uint8_t dataBits[m_headerDataBits];
    uint8_t codedBits[m_headerBits];
    uint8_t interleavedBits[m_headerBits];

    // 41 bytes LSB first plus two flush bits (the encoder returns to state 0)
    DVBits::bytesToBitsLSB(header, m_headerBytes, dataBits);
    dataBits[328] = 0;
    dataBits[329] = 0;
    DVConvolutional::dstar().encode(dataBits, m_headerDataBits, codedBits);
    interleave(codedBits, interleavedBits);
    DVScrambler::dstarHeader(interleavedBits, bits, m_headerBits);
}

int DVDStar::headerFromCoded(const float *llr, uint8_t *header)
{
    uint8_t dataBits[m_headerDataBits];
    int corrected = DVConvolutional::dstar().decodeSoft(llr, m_headerDataBits, dataBits, true);
    DVBits::bitsToBytesLSB(dataBits, 328, header);
    return headerCRCCheck(header) ? corrected : -1;
}

int DVDStar::headerDecode(const uint8_t *bits, uint8_t *header)
{
    float llr[m_headerBits];
    DVBits::bitsToLlr(bits, m_headerBits, llr);
    return headerDecodeSoft(llr, header);
}

int DVDStar::headerDecodeSoft(const float *llr, uint8_t *header)
{
    // Descramble by negating the LLRs where the scrambler bit is set, then deinterleave
    uint8_t zero[m_headerBits];
    uint8_t scrambler[m_headerBits];
    float descrambled[m_headerBits];
    float coded[m_headerBits];
    std::fill(zero, zero + m_headerBits, 0);
    DVScrambler::dstarHeader(zero, scrambler, m_headerBits);

    for (int i = 0; i < m_headerBits; i++) {
        descrambled[i] = scrambler[i] ? -llr[i] : llr[i];
    }

    int k = 0;

    for (int loop = 0; loop < m_headerBits; loop++)
    {
        coded[k] = descrambled[loop];
        k += 24;

        if (k >= 672) {
            k -= 671;
        } else if (k >= 660) {
            k -= 647;
        }
    }

    return headerFromCoded(coded, header);
}

void DVDStar::scrambleSlowData(uint8_t *bits24)
{
    // The slow data bytes are sent LSB first, so the mask bits are applied in that order
    static const uint8_t mask[3] = { 0x70, 0x4F, 0x93 };

    for (int i = 0; i < m_dataBits; i++) {
        bits24[i] ^= (mask[i / 8] >> (i % 8)) & 1;
    }
}
