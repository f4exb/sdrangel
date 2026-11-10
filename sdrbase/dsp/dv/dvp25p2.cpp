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

// The ISCH and DUID codewords and the scrambler are those OP25 (p25p2_isch.cc,
// p25p2_duid.cc, lfsr.py, KA1RBI, GPL-3.0-or-later) uses; the two block codes are expressed
// here by their generators, which the OP25 tables reduce to.

#include <algorithm>
#include <cstring>

#include "dvbits.h"
#include "dvcrc.h"
#include "dvreedsolomon.h"

#include "dvp25p2.h"

namespace
{
    // ISCH: codeword = offset xor the generator rows of the set message bits
    // (cnt bits 0-1, fr bit 2, loc bits 3-4, chn bits 5-6)
    const uint64_t ischOffset = 0x184229D461ULL;
    const uint64_t ischGenerator[7] = {
        0x00343D8597ULL, 0x0058CBAA4EULL, 0x009DA3A171ULL, 0x09048D9B72ULL,
        0x020807F7FFULL, 0x0C00DED18EULL, 0x100F4B1758ULL
    };

    // DUID (8,4): generator rows of DUID bits 0-3
    const uint8_t duidGenerator[4] = { 0x17, 0x2E, 0x4B, 0x8D };

    // Punctured (never transmitted) hexbits of the RS(63,35) codeword
    const int sacchErasures[11] = { 0, 1, 2, 3, 4, 57, 58, 59, 60, 61, 62 };
    const int facchErasures[18] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 54, 55, 56, 57, 58, 59, 60, 61, 62 };
}

const int DVP25P2::m_duidDibits[4] = { 20, 57, 142, 179 };
const int DVP25P2::m_voiceDibits[4] = { 21, 58, 106, 143 };

int DVP25P2::timeslotChannel(int timeslot)
{
    static const int channel[12] = { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0 };
    return channel[timeslot % 12];
}

// ---- ISCH and DUID ------------------------------------------------------------------------

uint64_t DVP25P2::ischEncode(const ISCH& isch)
{
    int value = (isch.m_count & 3) | ((isch.m_frame & 1) << 2) | ((isch.m_location & 3) << 3) | ((isch.m_channel & 3) << 5);
    uint64_t codeword = ischOffset;

    for (int k = 0; k < 7; k++)
    {
        if ((value >> k) & 1) {
            codeword ^= ischGenerator[k];
        }
    }

    return codeword;
}

DVP25P2::ISCHType DVP25P2::ischDecode(const float *llr, ISCH& isch, int& distance)
{
    // Correlate the LLRs with every codeword: positive LLR favours 0
    float best = -1e30f;
    int bestValue = -1;
    uint64_t bestCodeword = 0;

    for (int value = -1; value < 128; value++)
    {
        uint64_t codeword;

        if (value < 0) {
            codeword = m_syncISCH;
        }
        else
        {
            ISCH i;
            i.m_count = value & 3;
            i.m_frame = (value >> 2) & 1;
            i.m_location = (value >> 3) & 3;
            i.m_channel = (value >> 5) & 3;
            codeword = ischEncode(i);
        }

        float metric = 0.0f;

        for (int b = 0; b < 40; b++) {
            metric += ((codeword >> (39 - b)) & 1) ? -llr[b] : llr[b];
        }

        if (metric > best)
        {
            best = metric;
            bestValue = value;
            bestCodeword = codeword;
        }
    }

    uint64_t hard = 0;

    for (int b = 0; b < 40; b++) {
        hard = (hard << 1) | (llr[b] < 0.0f ? 1 : 0);
    }

    distance = DVBits::popcount64(hard ^ bestCodeword);

    if (bestValue < 0) {
        return ISCHSync;
    }

    isch.m_count = bestValue & 3;
    isch.m_frame = (bestValue >> 2) & 1;
    isch.m_location = (bestValue >> 3) & 3;
    isch.m_channel = (bestValue >> 5) & 3;
    return ISCHInfo;
}

uint8_t DVP25P2::duidEncode(int duid)
{
    uint8_t codeword = 0;

    for (int k = 0; k < 4; k++)
    {
        if ((duid >> k) & 1) {
            codeword ^= duidGenerator[k];
        }
    }

    return codeword;
}

int DVP25P2::duidDecode(const float *llr, int& distance)
{
    float best = -1e30f;
    int bestDuid = 0;

    for (int duid = 0; duid < 16; duid++)
    {
        uint8_t codeword = duidEncode(duid);
        float metric = 0.0f;

        for (int b = 0; b < 8; b++) {
            metric += ((codeword >> (7 - b)) & 1) ? -llr[b] : llr[b];
        }

        if (metric > best)
        {
            best = metric;
            bestDuid = duid;
        }
    }

    uint8_t hard = 0;

    for (int b = 0; b < 8; b++) {
        hard = (hard << 1) | (llr[b] < 0.0f ? 1 : 0);
    }

    distance = DVBits::popcount(hard ^ duidEncode(bestDuid));
    return bestDuid;
}

// ---- Scrambler ----------------------------------------------------------------------------

void DVP25P2::scramblerMask(uint32_t wacn, uint16_t sysid, uint16_t nac, uint8_t *bits)
{
    // The 44 bit seed is premultiplied by the matrix of the standard (row i has ones at
    // columns i, i+4, i+9, i+15, i+20, i+34), then the register is stepped as six coupled
    // sub-registers of 4, 5, 6, 5, 14 and 10 bits, the output being its most significant bit
    uint64_t seed = ((uint64_t) (wacn & 0xFFFFF) << 24) | ((uint64_t) (sysid & 0xFFF) << 12) | (nac & 0xFFF);
    static const int taps[6] = { 0, 4, 9, 15, 20, 34 };
    uint64_t reg = 0;

    for (int j = 0; j < 44; j++)
    {
        int v = 0;

        for (int t = 0; t < 6; t++)
        {
            int i = j - taps[t];

            if (i >= 0) {
                v ^= (int) ((seed >> (43 - i)) & 1);
            }
        }

        reg = (reg << 1) | v;
    }

    for (int k = 0; k < 4320; k++)
    {
        bits[k] = (uint8_t) ((reg >> 43) & 1);

        uint32_t s1 = (reg >> 40) & 0xF;
        uint32_t s2 = (reg >> 35) & 0x1F;
        uint32_t s3 = (reg >> 29) & 0x3F;
        uint32_t s4 = (reg >> 24) & 0x1F;
        uint32_t s5 = (reg >> 10) & 0x3FFF;
        uint32_t s6 = reg & 0x3FF;
        uint32_t cy1 = (s1 >> 3) & 1;
        uint32_t cy2 = (s2 >> 4) & 1;
        uint32_t cy3 = (s3 >> 5) & 1;
        uint32_t cy4 = (s4 >> 4) & 1;
        uint32_t cy5 = (s5 >> 13) & 1;
        uint32_t cy6 = (s6 >> 9) & 1;
        s1 = ((s1 << 1) & 0xF) | (cy1 ^ cy2);
        s2 = ((s2 << 1) & 0x1F) | (cy1 ^ cy3);
        s3 = ((s3 << 1) & 0x3F) | (cy1 ^ cy4);
        s4 = ((s4 << 1) & 0x1F) | (cy1 ^ cy5);
        s5 = ((s5 << 1) & 0x3FFF) | (cy1 ^ cy6);
        s6 = ((s6 << 1) & 0x3FF) | cy1;
        reg = ((uint64_t) s1 << 40) | ((uint64_t) s2 << 35) | ((uint64_t) s3 << 29) | ((uint64_t) s4 << 24) | ((uint64_t) s5 << 10) | s6;
    }
}

// ---- SACCH and FACCH ----------------------------------------------------------------------

int DVP25P2::sacchDibit(int k)
{
    // Payload dibits in order, the DUID dibits skipped
    int d = 21 + k;

    if (d >= 57) {
        d++;
    }

    if (d >= 142) {
        d++;
    }

    return d;
}

int DVP25P2::facchDibit(int k)
{
    int d = 21 + k;

    if (d >= 57) {
        d++;
    }

    if (d >= 89) {
        d += 21; // dibits 89-109 are not part of the FACCH
    }

    if (d >= 142) {
        d++;
    }

    return d;
}

uint16_t DVP25P2::crc12(const uint8_t *bits, int nbBits)
{
    return (uint16_t) DVCRC::compute(bits, nbBits, 12, 0x897, 0, 0xFFF);
}

int DVP25P2::acchDecode(const uint8_t *bits, int nbBits, int firstHexbit, uint8_t *pdu, int pduBits, int crcBits)
{
    uint8_t hexbits[63];
    std::memset(hexbits, 0, sizeof(hexbits));
    DVBits::bitsToHexbits(bits, nbBits / 6, hexbits + firstHexbit);
    int corrected;

    if (firstHexbit == 5) {
        corrected = DVReedSolomon::p25_63_35().decode(hexbits, 63, sacchErasures, 11);
    } else {
        corrected = DVReedSolomon::p25_63_35().decode(hexbits, 63, facchErasures, 18);
    }

    if (corrected < 0) {
        return -1;
    }

    uint8_t decoded[336];
    DVBits::hexbitsToBits(hexbits + firstHexbit, (pduBits + crcBits) / 6, decoded);
    const uint16_t crc = crcBits == 16 ? crc16(decoded, pduBits) : crc12(decoded, pduBits);

    if (crc != (uint16_t) DVBits::getBits(decoded, pduBits, crcBits)) {
        return -1;
    }

    std::memcpy(pdu, decoded, pduBits);
    return corrected;
}

void DVP25P2::acchEncode(const uint8_t *pdu, int pduBits, int firstHexbit, uint8_t *bits, int nbBits, int crcBits)
{
    uint8_t coded[336];
    std::memcpy(coded, pdu, pduBits);
    DVBits::putBits(coded, pduBits, crcBits == 16 ? crc16(pdu, pduBits) : crc12(pdu, pduBits), crcBits);
    uint8_t hexbits[63];
    std::memset(hexbits, 0, sizeof(hexbits));
    DVBits::bitsToHexbits(coded, (pduBits + crcBits) / 6, hexbits + firstHexbit);
    DVReedSolomon::p25_63_35().encodeParity(hexbits, 35, hexbits + 35);
    DVBits::hexbitsToBits(hexbits + firstHexbit, nbBits / 6, bits);
}

int DVP25P2::sacchDecode(const uint8_t *bits, uint8_t *pdu)
{
    return acchDecode(bits, m_sacchBits, 5, pdu, m_sacchPduBits);
}

int DVP25P2::facchDecode(const uint8_t *bits, uint8_t *pdu)
{
    return acchDecode(bits, m_facchBits, 9, pdu, m_facchPduBits);
}

int DVP25P2::lcchDecode(const uint8_t *bits, uint8_t *pdu)
{
    return acchDecode(bits, m_sacchBits, 5, pdu, m_lcchPduBits, 16);
}

void DVP25P2::lcchEncode(const uint8_t *pdu, uint8_t *bits)
{
    acchEncode(pdu, m_lcchPduBits, 5, bits, m_sacchBits, 16);
}

uint16_t DVP25P2::crc16(const uint8_t *bits, int nbBits)
{
    return (uint16_t) DVCRC::compute(bits, nbBits, 16, 0x1021, 0, 0xFFFF);
}

void DVP25P2::sacchEncode(const uint8_t *pdu, uint8_t *bits)
{
    acchEncode(pdu, m_sacchPduBits, 5, bits, m_sacchBits);
}

void DVP25P2::facchEncode(const uint8_t *pdu, uint8_t *bits)
{
    acchEncode(pdu, m_facchPduBits, 9, bits, m_facchBits);
}

// ---- Encryption sync ----------------------------------------------------------------------

int DVP25P2::essDecode(const uint8_t *essB, const uint8_t *essA, uint8_t& algid, uint16_t& keyid, uint8_t *mi)
{
    // Shortened RS(44,16): the 16 data hexbits (algid 8, key ID 16, MI 72) then 28 parity
    uint8_t codeword[44];
    std::memcpy(codeword, essB, 16);
    std::memcpy(codeword + 16, essA, 28);
    int corrected = DVReedSolomon::p25_63_35().decode(codeword, 44);

    if (corrected < 0) {
        return -1;
    }

    uint8_t bits[96];
    DVBits::hexbitsToBits(codeword, 16, bits);
    algid = (uint8_t) DVBits::getBits(bits, 0, 8);
    keyid = (uint16_t) DVBits::getBits(bits, 8, 16);
    DVBits::bitsToBytesMSB(bits + 24, 72, mi);
    return corrected;
}

void DVP25P2::essEncode(uint8_t algid, uint16_t keyid, const uint8_t *mi, uint8_t *essB, uint8_t *essA)
{
    uint8_t bits[96];
    DVBits::putBits(bits, 0, algid, 8);
    DVBits::putBits(bits, 8, keyid, 16);
    DVBits::bytesToBitsMSB(mi, 9, bits + 24);
    uint8_t codeword[44];
    DVBits::bitsToHexbits(bits, 16, codeword);
    DVReedSolomon::p25_63_35().encodeParity(codeword, 16, codeword + 16);
    std::memcpy(essB, codeword, 16);
    std::memcpy(essA, codeword + 16, 28);
}

// ---- Names --------------------------------------------------------------------------------

const char *DVP25P2::duidName(int duid)
{
    switch (duid)
    {
    case DUID4V: return "4V";
    case DUIDSACCHScrambled: return "SACCH";
    case DUIDLCCHScrambled: return "LCCH";
    case DUID2V: return "2V";
    case DUIDFACCHScrambled: return "FACCH";
    case DUIDSACCH: return "SACCH";
    case DUIDLCCH: return "LCCH";
    case DUIDFACCH: return "FACCH";
    default: return "?";
    }
}

const char *DVP25P2::macOpcodeName(int opcode)
{
    switch (opcode)
    {
    case MacSignal: return "SIGNAL";
    case MacPTT: return "PTT";
    case MacEndPTT: return "END";
    case MacIdle: return "IDLE";
    case MacActive: return "ACTIVE";
    case MacHangtime: return "HANGTIME";
    default: return "?";
    }
}

const char *DVP25P2::algorithmName(int algid)
{
    switch (algid)
    {
    case 0x80: return "clear";
    case 0x81: return "DES-OFB";
    case 0x83: return "3DES";
    case 0x84: return "AES-256";
    case 0x85: return "AES-128";
    case 0x9F: return "DES-XL";
    case 0xAA: return "ADP";
    default: return "encrypted";
    }
}
