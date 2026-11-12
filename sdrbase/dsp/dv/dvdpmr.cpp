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
#include "dvblockcode.h"
#include "dvcrc.h"
#include "dvdpmr.h"
#include "dvscrambler.h"

// Colour (channel) code sequences by code number (TS 102 658 table 6.1, TS 102 490 table 6.1).
// Each of the 12 code bits is di-bit encoded: 0 -> 01 (+3), 1 -> 11 (-3) (clause 7.1)
const uint32_t DVDPMR::m_colourCodes[64] = {
    0x575F77, 0x577577, 0x57DD75, 0x57F775, 0x55577D, 0x557D7D, 0x55D57F, 0x55FF7F, // 0..7
    0x5F555F, 0x5F7F5F, 0x5FD75D, 0x5FFD5D, 0x5D5D55, 0x5D7755, 0x5DDF57, 0x5DF557, // 8..15
    0x775DD7, 0x7777D7, 0x77DFD5, 0x77F5D5, 0x7555DD, 0x757FDD, 0x75D7DF, 0x75FDDF, // 16..23
    0x7F57FF, 0x7F7DFF, 0x7FD5FD, 0x7FFFFD, 0x7D5FF5, 0x7D75F5, 0x7DDDF7, 0x7DF7F7, // 24..31
    0xD755F7, 0xD77FF7, 0xD7D7F5, 0xD7FDF5, 0xD55DFD, 0xD577FD, 0xD5DFFF, 0xD5F5FF, // 32..39
    0xDF5FDF, 0xDF75DF, 0xDFDDDD, 0xDFF7DD, 0xDD57D5, 0xDD7DD5, 0xDDD5D7, 0xDDFFD7, // 40..47
    0xF75757, 0xF77D57, 0xF7D555, 0xF7FF55, 0xF55F5D, 0xF5755D, 0xF5DD5F, 0xF5F75F, // 48..55
    0xFF5D7F, 0xFF777F, 0xFFDF7D, 0xFFF57D, 0xFD5575, 0xFD7F75, 0xFDD777, 0xFDFD77  // 56..63
};

// ---- Primitives ------------------------------------------------------------------

void DVDPMR::crc7Append(uint8_t *bits, int nbBits)
{
    DVCRC::append(bits, nbBits, 7, 0x09, 0, 0);
}

void DVDPMR::crc8Append(uint8_t *bits, int nbBits)
{
    DVCRC::append(bits, nbBits, 8, 0x07, 0, 0);
}

bool DVDPMR::crc7Check(const uint8_t *bits, int nbBits)
{
    return DVCRC::check(bits, nbBits, 7, 0x09, 0, 0);
}

bool DVDPMR::crc8Check(const uint8_t *bits, int nbBits)
{
    return DVCRC::check(bits, nbBits, 8, 0x07, 0, 0);
}

void DVDPMR::hammingBlockEncode(const uint8_t *data, int nbBlocks, uint8_t *coded)
{
    for (int b = 0; b < nbBlocks; b++) {
        DVBlockCode::hamming128().encode(&data[8 * b], &coded[12 * b]);
    }
}

int DVDPMR::hammingBlockDecode(uint8_t *coded, int nbBlocks, uint8_t *data)
{
    int corrected = 0;
    bool ok = true;

    for (int b = 0; b < nbBlocks; b++)
    {
        int e = DVBlockCode::hamming128().decode(&coded[12 * b]);

        if (e < 0) {
            ok = false;
        } else {
            corrected += e;
        }

        std::copy(&coded[12 * b], &coded[12 * b + 8], &data[8 * b]);
    }

    return ok ? corrected : -1;
}

void DVDPMR::interleave(const uint8_t *in, int nbBlocks, uint8_t *out)
{
    // Transmit data enters the 12 row x nbBlocks column matrix column by column (codeword by
    // codeword) and leaves it row by row (clause 7.5)
    int n = 12 * nbBlocks;

    for (int i = 0; i < n; i++) {
        out[i] = in[12 * (i % nbBlocks) + i / nbBlocks];
    }
}

void DVDPMR::deinterleave(const uint8_t *in, int nbBlocks, uint8_t *out)
{
    int n = 12 * nbBlocks;

    for (int i = 0; i < n; i++) {
        out[12 * (i % nbBlocks) + i / nbBlocks] = in[i];
    }
}

void DVDPMR::scramble(uint8_t *bits, int nbBits)
{
    DVScrambler::dpmr(bits, nbBits);
}

// ---- Fields --------------------------------------------------------------------------

void DVDPMR::headerInfoEncode(const uint8_t *hi72, uint8_t *coded120)
{
    uint8_t data[80];
    uint8_t coded[120];
    std::copy(hi72, hi72 + m_hiInfoBits, data);
    crc8Append(data, m_hiInfoBits);
    hammingBlockEncode(data, 10, coded);
    interleave(coded, 10, coded120);
    scramble(coded120, m_hiCodedBits);
}

int DVDPMR::headerInfoDecode(const uint8_t *coded120, uint8_t *hi72)
{
    uint8_t scrambled[120];
    uint8_t coded[120];
    uint8_t data[80];
    std::copy(coded120, coded120 + m_hiCodedBits, scrambled);
    scramble(scrambled, m_hiCodedBits);
    deinterleave(scrambled, 10, coded);
    int errors = hammingBlockDecode(coded, 10, data);
    std::copy(data, data + m_hiInfoBits, hi72);
    return ((errors < 0) || !crc8Check(data, m_hiInfoBits)) ? -1 : errors;
}

void DVDPMR::cchEncode(const uint8_t *cch41, uint8_t *coded72)
{
    uint8_t data[48];
    uint8_t coded[72];
    std::copy(cch41, cch41 + m_cchInfoBits, data);
    crc7Append(data, m_cchInfoBits);
    hammingBlockEncode(data, 6, coded);
    interleave(coded, 6, coded72);
    scramble(coded72, m_cchCodedBits);
}

int DVDPMR::cchDecode(const uint8_t *coded72, uint8_t *cch41)
{
    uint8_t scrambled[72];
    uint8_t coded[72];
    uint8_t data[48];
    std::copy(coded72, coded72 + m_cchCodedBits, scrambled);
    scramble(scrambled, m_cchCodedBits);
    deinterleave(scrambled, 6, coded);
    int errors = hammingBlockDecode(coded, 6, data);
    std::copy(data, data + m_cchInfoBits, cch41);
    return ((errors < 0) || !crc7Check(data, m_cchInfoBits)) ? -1 : errors;
}

void DVDPMR::endEncode(const uint8_t *end17, uint8_t *coded72)
{
    // The 36 bit block is sent twice with the scrambler restarted for each copy
    uint8_t data[24];
    std::copy(end17, end17 + m_endInfoBits, data);
    crc7Append(data, m_endInfoBits);
    hammingBlockEncode(data, 3, coded72);
    scramble(coded72, 36);
    std::copy(coded72, coded72 + 36, coded72 + 36);
}

int DVDPMR::endDecode(const uint8_t *coded72, uint8_t *end17)
{
    for (int copy = 0; copy < 2; copy++)
    {
        uint8_t coded[36];
        uint8_t data[24];
        std::copy(coded72 + 36 * copy, coded72 + 36 * copy + 36, coded);
        scramble(coded, 36);
        int errors = hammingBlockDecode(coded, 3, data);

        if ((errors >= 0) && crc7Check(data, m_endInfoBits))
        {
            std::copy(data, data + m_endInfoBits, end17);
            return errors;
        }
    }

    return -1;
}

// ---- Colour codes -------------------------------------------------------------------

uint32_t DVDPMR::colourCodeSequence(int colourCode)
{
    return m_colourCodes[colourCode & 63];
}

void DVDPMR::colourCodeBits(int colourCode, uint8_t *bits24)
{
    DVBits::putBits(bits24, 0, colourCodeSequence(colourCode), m_ccBits);
}

int DVDPMR::colourCodeMatch(const uint8_t *bits24, int& errors)
{
    // Compare the symbol signs (MSB of each dibit) with each code's 12 bits
    int best = -1;
    errors = 13;

    for (int code = 0; code < 64; code++)
    {
        uint32_t seq = m_colourCodes[code];
        int e = 0;

        for (int i = 0; i < 12; i++) {
            e += ((seq >> (23 - 2 * i)) & 1) != (bits24[2 * i] & 1);
        }

        if (e < errors)
        {
            errors = e;
            best = code;
        }
    }

    return (errors <= 2) ? best : -1;
}
