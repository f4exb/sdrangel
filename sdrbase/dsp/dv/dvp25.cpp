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
#include "dvp25.h"
#include "dvreedsolomon.h"

// ---- NID --------------------------------------------------------------------------

uint64_t DVP25::nidEncode(int nac, int duid)
{
    uint16_t data = (uint16_t) (((nac & 0xFFF) << 4) | (duid & 0xF));
    uint64_t parity = ((duid >> 1) ^ duid) & 1;
    return (DVBCH::p25_63_16().encodeWord(data) << 1) | parity;
}

int DVP25::nidDecode(const uint8_t *bits, int& nac, int& duid)
{
    uint8_t cw[63];
    std::copy(bits, bits + 63, cw);
    int errors = DVBCH::p25_63_16().decode(cw);
    nac = (int) DVBits::getBits(cw, 0, 12);
    duid = (int) DVBits::getBits(cw, 12, 4);
    return errors;
}

// ---- Hamming(10,6) hexbit blocks -----------------------------------------------------

void DVP25::hammingEncodeHexbits(const uint8_t *hexbits, int nbHexbits, uint8_t *bits)
{
    for (int i = 0; i < nbHexbits; i++) {
        DVBits::putBits(bits, 10 * i, DVBlockCode::hamming106().encodeWord(hexbits[i]), 10);
    }
}

int DVP25::hammingDecodeHexbits(const uint8_t *bits, int nbHexbits, uint8_t *hexbits)
{
    int corrected = 0;

    for (int i = 0; i < nbHexbits; i++)
    {
        uint32_t cw = (uint32_t) DVBits::getBits(bits, 10 * i, 10);
        int e = DVBlockCode::hamming106().decodeWord(cw);

        if (e > 0) {
            corrected += e;
        }

        hexbits[i] = (uint8_t) (cw >> 4);
    }

    return corrected;
}

// ---- Link control ----------------------------------------------------------------

void DVP25::lcEncode(const uint8_t *lc9, uint8_t *bits240)
{
    uint8_t lcBits[72];
    uint8_t hexbits[24];
    DVBits::bytesToBitsMSB(lc9, 9, lcBits);
    DVBits::bitsToHexbits(lcBits, 12, hexbits);
    DVReedSolomon::p25_24_12().encodeParity(hexbits, 12, hexbits + 12);
    hammingEncodeHexbits(hexbits, 24, bits240);
}

int DVP25::lcDecode(const uint8_t *bits240, uint8_t *lc9)
{
    uint8_t hexbits[24];
    uint8_t lcBits[72];
    hammingDecodeHexbits(bits240, 24, hexbits);
    int errors = DVReedSolomon::p25_24_12().decode(hexbits, 24);
    DVBits::hexbitsToBits(hexbits, 12, lcBits);
    DVBits::bitsToBytesMSB(lcBits, 72, lc9);
    return errors;
}

void DVP25::essEncode(const uint8_t *mi9, uint8_t algid, uint16_t kid, uint8_t *bits240)
{
    uint8_t bits[96];
    uint8_t hexbits[24];
    DVBits::bytesToBitsMSB(mi9, 9, bits);
    DVBits::putBits(bits, 72, algid, 8);
    DVBits::putBits(bits, 80, kid, 16);
    DVBits::bitsToHexbits(bits, 16, hexbits);
    DVReedSolomon::p25_24_16().encodeParity(hexbits, 16, hexbits + 16);
    hammingEncodeHexbits(hexbits, 24, bits240);
}

int DVP25::essDecode(const uint8_t *bits240, uint8_t *mi9, uint8_t& algid, uint16_t& kid)
{
    uint8_t hexbits[24];
    uint8_t bits[96];
    hammingDecodeHexbits(bits240, 24, hexbits);
    int errors = DVReedSolomon::p25_24_16().decode(hexbits, 24);
    DVBits::hexbitsToBits(hexbits, 16, bits);
    DVBits::bitsToBytesMSB(bits, 72, mi9);
    algid = (uint8_t) DVBits::getBits(bits, 72, 8);
    kid = (uint16_t) DVBits::getBits(bits, 80, 16);
    return errors;
}

// ---- Header ----------------------------------------------------------------------

void DVP25::headerEncode(const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits648)
{
    uint8_t bits[120];
    uint8_t hexbits[36];
    DVBits::bytesToBitsMSB(mi9, 9, bits);
    int pos = DVBits::putBits(bits, 72, mfid, 8);
    pos = DVBits::putBits(bits, pos, algid, 8);
    pos = DVBits::putBits(bits, pos, kid, 16);
    DVBits::putBits(bits, pos, tgid, 16);
    DVBits::bitsToHexbits(bits, 20, hexbits);
    DVReedSolomon::p25_36_20().encodeParity(hexbits, 20, hexbits + 20);

    for (int i = 0; i < 36; i++) {
        DVBits::putBits(bits648, 18 * i, DVBlockCode::golay186().encodeWord(hexbits[i]), 18);
    }
}

int DVP25::headerDecode(const uint8_t *bits648, uint8_t *mi9, uint8_t& mfid, uint8_t& algid, uint16_t& kid, uint16_t& tgid)
{
    uint8_t hexbits[36];
    uint8_t bits[120];

    for (int i = 0; i < 36; i++)
    {
        uint32_t cw = (uint32_t) DVBits::getBits(bits648, 18 * i, 18);
        DVBlockCode::golay186().decodeWord(cw);
        hexbits[i] = (uint8_t) (cw >> 12);
    }

    int errors = DVReedSolomon::p25_36_20().decode(hexbits, 36);
    DVBits::hexbitsToBits(hexbits, 20, bits);
    DVBits::bitsToBytesMSB(bits, 72, mi9);
    mfid = (uint8_t) DVBits::getBits(bits, 72, 8);
    algid = (uint8_t) DVBits::getBits(bits, 80, 8);
    kid = (uint16_t) DVBits::getBits(bits, 88, 16);
    tgid = (uint16_t) DVBits::getBits(bits, 104, 16);
    return errors;
}

// ---- Low speed data --------------------------------------------------------------

void DVP25::lsdEncode(uint16_t lsd, uint8_t *bits32)
{
    DVBits::putBits(bits32, 0, DVBlockCode::cyclic168().encodeWord(lsd >> 8), 16);
    DVBits::putBits(bits32, 16, DVBlockCode::cyclic168().encodeWord(lsd & 0xFF), 16);
}

// ---- Trunking and data blocks -----------------------------------------------------------------

namespace
{
    // Bit interleave of a 196 bit block and the state transition table of the 1/2 rate trellis
    // (4 states, one per previous dibit; the codeword of a dibit d from state s is nextWords[s][d]),
    // as OP25 (p25p1_fdma.cc, from wireshark's packet-p25cai.c by Michael Ossmann) has them
    const uint16_t deinterleave[196] = {
        0,  1,  2,  3,  52, 53, 54, 55, 100,101,102,103, 148,149,150,151,
        4,  5,  6,  7,  56, 57, 58, 59, 104,105,106,107, 152,153,154,155,
        8,  9, 10, 11,  60, 61, 62, 63, 108,109,110,111, 156,157,158,159,
        12, 13, 14, 15,  64, 65, 66, 67, 112,113,114,115, 160,161,162,163,
        16, 17, 18, 19,  68, 69, 70, 71, 116,117,118,119, 164,165,166,167,
        20, 21, 22, 23,  72, 73, 74, 75, 120,121,122,123, 168,169,170,171,
        24, 25, 26, 27,  76, 77, 78, 79, 124,125,126,127, 172,173,174,175,
        28, 29, 30, 31,  80, 81, 82, 83, 128,129,130,131, 176,177,178,179,
        32, 33, 34, 35,  84, 85, 86, 87, 132,133,134,135, 180,181,182,183,
        36, 37, 38, 39,  88, 89, 90, 91, 136,137,138,139, 184,185,186,187,
        40, 41, 42, 43,  92, 93, 94, 95, 140,141,142,143, 188,189,190,191,
        44, 45, 46, 47,  96, 97, 98, 99, 144,145,146,147, 192,193,194,195,
        48, 49, 50, 51
    };
    const uint8_t nextWords[4][4] = {
        { 0x2, 0xC, 0x1, 0xF },
        { 0xE, 0x0, 0xD, 0x3 },
        { 0x9, 0x7, 0xA, 0x4 },
        { 0x5, 0xB, 0x6, 0x8 }
    };
    // The rate 3/4 trellis (TIA-102.BAAA 7.5, the same tables as the DMR rate 3/4 code): 8 states,
    // one per previous tribit; the constellation point of a tribit t from state s is
    // points34[s][t], and a point's two dibits as bits (dibit +3 = 01, +1 = 00, -1 = 10, -3 = 11)
    // are pointWords[point]. Tables as MMDVMHost's P25Trellis has them.
    const uint8_t points34[8][8] = {
        { 0,  8, 4, 12, 2, 10, 6, 14 },
        { 4, 12, 2, 10, 6, 14, 0,  8 },
        { 1,  9, 5, 13, 3, 11, 7, 15 },
        { 5, 13, 3, 11, 7, 15, 1,  9 },
        { 3, 11, 7, 15, 1,  9, 5, 13 },
        { 7, 15, 1,  9, 5, 13, 3, 11 },
        { 2, 10, 6, 14, 0,  8, 4, 12 },
        { 6, 14, 0,  8, 4, 12, 2, 10 }
    };
    const uint8_t pointWords[16] = { 0x2, 0xA, 0x7, 0xF, 0xE, 0x6, 0xB, 0x3, 0xD, 0x5, 0x8, 0x0, 0x1, 0x9, 0x4, 0xC };
}

uint16_t DVP25::blockCrc(const uint8_t *bytes, int nbBytes)
{
    const uint32_t poly = (1 << 12) | (1 << 5) | 1;
    uint32_t crc = 0;

    for (int i = 0; i < nbBytes; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            uint32_t bit = (bytes[i] >> (7 - j)) & 1;
            crc = ((crc << 1) | bit) & 0x1FFFF;

            if (crc & 0x10000) {
                crc = (crc & 0xFFFF) ^ poly;
            }
        }
    }

    // The transmitted CRC makes the register of the 12 bytes read all ones: this is the value
    // that appends after 10 bytes, and the check of 12 bytes gives 0
    return (uint16_t) (crc ^ 0xFFFF);
}

void DVP25::blockEncode(const uint8_t *bytes12, uint8_t *bits196)
{
    uint8_t block[12];
    std::copy(bytes12, bytes12 + 10, block);
    // The CRC is the augmented register after the 10 bytes, so that the 12 bytes check to zero:
    // running the register over the 10 bytes and 16 zero bits gives it
    uint8_t padded[12];
    std::copy(bytes12, bytes12 + 10, padded);
    padded[10] = 0;
    padded[11] = 0;
    uint16_t crc = blockCrc(padded, 12);
    block[10] = (uint8_t) (crc >> 8);
    block[11] = (uint8_t) crc;
    blockEncodeRaw(block, bits196);
}

void DVP25::blockEncodeRaw(const uint8_t *block, uint8_t *bits196)
{
    // 48 data dibits then a flush dibit, each through the trellis; the 4 bit codewords in order,
    // then interleaved
    uint8_t coded[196];
    int state = 0;

    for (int d = 0; d < 49; d++)
    {
        int dibit = (d < 48) ? (block[d >> 2] >> (6 - 2 * (d & 3))) & 3 : 0;
        uint8_t word = nextWords[state][dibit];

        for (int b = 0; b < 4; b++) {
            coded[4 * d + b] = (word >> (3 - b)) & 1;
        }

        state = dibit;
    }

    for (int i = 0; i < 196; i++) {
        bits196[deinterleave[i]] = coded[i];
    }
}

int DVP25::blockDecode(const float *llr196, uint8_t *bytes12, bool checkCrc)
{
    // Soft Viterbi over the 4 states; the metric of a branch is the correlation of its codeword
    // with the LLRs
    float metric[4] = { 0.0f, -1e30f, -1e30f, -1e30f };
    uint8_t predecessor[49][4];
    float llr[196];

    for (int i = 0; i < 196; i++) {
        llr[i] = llr196[deinterleave[i]];
    }

    for (int d = 0; d < 49; d++)
    {
        float next[4];
        const float *w = llr + 4 * d;

        for (int dibit = 0; dibit < 4; dibit++)
        {
            next[dibit] = -1e30f;

            for (int s = 0; s < 4; s++)
            {
                uint8_t word = nextWords[s][dibit];
                float m = metric[s];

                for (int b = 0; b < 4; b++) {
                    m += ((word >> (3 - b)) & 1) ? -w[b] : w[b];
                }

                if (m > next[dibit])
                {
                    next[dibit] = m;
                    predecessor[d][dibit] = (uint8_t) s;
                }
            }
        }

        std::copy(next, next + 4, metric);
    }

    // The flush dibit is 0: trace back from state 0
    int state = 0;
    uint8_t dibits[49];

    for (int d = 48; d >= 0; d--)
    {
        dibits[d] = (uint8_t) state;
        state = predecessor[d][state];
    }

    std::fill(bytes12, bytes12 + 12, 0);

    for (int d = 0; d < 48; d++) {
        bytes12[d >> 2] |= dibits[d] << (6 - 2 * (d & 3));
    }

    if (checkCrc && (blockCrc(bytes12, 12) != 0)) {
        return -1;
    }

    // Disagreements between the decoded codewords and the hard decisions
    int differences = 0;
    state = 0;

    for (int d = 0; d < 49; d++)
    {
        uint8_t word = nextWords[state][dibits[d]];

        for (int b = 0; b < 4; b++) {
            differences += (((word >> (3 - b)) & 1) != 0) != (llr[4 * d + b] < 0.0f);
        }

        state = dibits[d];
    }

    return differences;
}

void DVP25::block34Encode(const uint8_t *bytes18, uint8_t *bits196)
{
    // The CRC-9 covers the 7 bit serial number then the 16 data bytes (the same code as the DMR
    // confirmed data blocks); it sits in the low bit of byte 0 and byte 1
    uint8_t block[18];
    std::copy(bytes18, bytes18 + 18, block);
    uint8_t covered[7 + 128];

    for (int i = 0; i < 7; i++) {
        covered[i] = (block[0] >> (7 - i)) & 1;
    }

    DVBits::bytesToBitsMSB(block + 2, 16, covered + 7);
    uint16_t crc = DVCRC::dmr9(covered, 7 + 128);
    block[0] = (block[0] & 0xFE) | ((crc >> 8) & 1);
    block[1] = (uint8_t) crc;

    // 48 data tribits then a flush tribit, each through the trellis; the points' dibits in order,
    // then the interleave of the rate 1/2 blocks
    uint8_t coded[196];
    int state = 0;

    for (int t = 0; t < 49; t++)
    {
        int tribit = 0;

        if (t < 48)
        {
            for (int b = 0; b < 3; b++)
            {
                int bit = 3 * t + b;
                tribit = (tribit << 1) | ((block[bit >> 3] >> (7 - (bit & 7))) & 1);
            }
        }

        uint8_t word = pointWords[points34[state][tribit]];

        for (int b = 0; b < 4; b++) {
            coded[4 * t + b] = (word >> (3 - b)) & 1;
        }

        state = tribit;
    }

    for (int i = 0; i < 196; i++) {
        bits196[deinterleave[i]] = coded[i];
    }
}

int DVP25::block34Decode(const float *llr196, uint8_t *bytes18)
{
    // Soft Viterbi over the 8 states, the metric of a branch the correlation of its point's bits
    // with the LLRs
    float metric[8] = { 0.0f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f };
    uint8_t predecessor[49][8];
    float llr[196];

    for (int i = 0; i < 196; i++) {
        llr[i] = llr196[deinterleave[i]];
    }

    for (int t = 0; t < 49; t++)
    {
        float next[8];
        const float *w = llr + 4 * t;

        for (int tribit = 0; tribit < 8; tribit++)
        {
            next[tribit] = -1e30f;

            for (int s = 0; s < 8; s++)
            {
                uint8_t word = pointWords[points34[s][tribit]];
                float m = metric[s];

                for (int b = 0; b < 4; b++) {
                    m += ((word >> (3 - b)) & 1) ? -w[b] : w[b];
                }

                if (m > next[tribit])
                {
                    next[tribit] = m;
                    predecessor[t][tribit] = (uint8_t) s;
                }
            }
        }

        std::copy(next, next + 8, metric);
    }

    // The flush tribit is 0: trace back from state 0
    int state = 0;
    uint8_t tribits[49];

    for (int t = 48; t >= 0; t--)
    {
        tribits[t] = (uint8_t) state;
        state = predecessor[t][state];
    }

    std::fill(bytes18, bytes18 + 18, 0);

    for (int t = 0; t < 48; t++)
    {
        for (int b = 0; b < 3; b++)
        {
            int bit = 3 * t + b;
            bytes18[bit >> 3] |= ((tribits[t] >> (2 - b)) & 1) << (7 - (bit & 7));
        }
    }

    uint8_t covered[7 + 128];

    for (int i = 0; i < 7; i++) {
        covered[i] = (bytes18[0] >> (7 - i)) & 1;
    }

    DVBits::bytesToBitsMSB(bytes18 + 2, 16, covered + 7);

    if (DVCRC::dmr9(covered, 7 + 128) != ((((uint16_t) bytes18[0] & 1) << 8) | bytes18[1])) {
        return -1;
    }

    int differences = 0;
    state = 0;

    for (int t = 0; t < 49; t++)
    {
        uint8_t word = pointWords[points34[state][tribits[t]]];

        for (int b = 0; b < 4; b++) {
            differences += (((word >> (3 - b)) & 1) != 0) != (llr[4 * t + b] < 0.0f);
        }

        state = tribits[t];
    }

    return differences;
}

uint32_t DVP25::packetCrc(const uint8_t *bytes, int nbBytes)
{
    uint32_t crc = 0;

    for (int i = 0; i < nbBytes; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            uint32_t feedback = ((crc >> 31) ^ (bytes[i] >> (7 - j))) & 1;
            crc <<= 1;

            if (feedback) {
                crc ^= 0x04C11DB7u;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

const char *DVP25::tsbkOpcodeName(int opcode)
{
    switch (opcode)
    {
    case 0x00: return "GRP_V_CH_GRANT";
    case 0x02: return "GRP_V_CH_GRANT_UPDT";
    case 0x03: return "GRP_V_CH_GRANT_UPDT_EXP";
    case 0x04: return "UU_V_CH_GRANT";
    case 0x05: return "UU_ANS_REQ";
    case 0x06: return "UU_V_CH_GRANT_UPDT";
    case 0x08: return "TELE_INT_CH_GRANT";
    case 0x09: return "TELE_INT_CH_GRANT_UPDT";
    case 0x0A: return "TELE_INT_ANS_REQ";
    case 0x14: return "SN_DATA_CHN_GNT";
    case 0x15: return "SN_DATA_CHN_ANN_EXP";
    case 0x16: return "SN_DATA_CHN_ANN";
    case 0x18: return "STS_UPDT";
    case 0x1A: return "STS_Q";
    case 0x1C: return "MSG_UPDT";
    case 0x1D: return "RAD_MON_CMD";
    case 0x1F: return "CALL_ALRT";
    case 0x20: return "ACK_RSP_FNE";
    case 0x21: return "QUE_RSP";
    case 0x24: return "EXT_FNCT_CMD";
    case 0x27: return "DENY_RSP";
    case 0x28: return "GRP_AFF_RSP";
    case 0x29: return "SCCB_EXP";
    case 0x2A: return "GRP_AFF_Q";
    case 0x2B: return "LOC_REG_RSP";
    case 0x2C: return "U_REG_RSP";
    case 0x2D: return "U_REG_CMD";
    case 0x2E: return "AUTH_CMD";
    case 0x2F: return "U_DE_REG_ACK";
    case 0x30: return "SYNC_BCST";
    case 0x31: return "AUTH_DMD";
    case 0x32: return "AUTH_FNE_RESP";
    case 0x33: return "IDEN_UP_TDMA";
    case 0x34: return "IDEN_UP_VU";
    case 0x35: return "TIME_DATE_ANN";
    case 0x36: return "ROAM_ADDR_CMD";
    case 0x37: return "ROAM_ADDR_UPDT";
    case 0x38: return "SYS_SRV_BCST";
    case 0x39: return "SCCB";
    case 0x3A: return "RFSS_STS_BCST";
    case 0x3B: return "NET_STS_BCST";
    case 0x3C: return "ADJ_STS_BCST";
    case 0x3D: return "IDEN_UP";
    case 0x3E: return "P_PARM_BCST";
    case 0x3F: return "P_PARM_UPDT";
    default: return nullptr;
    }
}

int DVP25::lsdDecode(const uint8_t *bits32, uint16_t& lsd)
{
    uint32_t hi = (uint32_t) DVBits::getBits(bits32, 0, 16);
    uint32_t lo = (uint32_t) DVBits::getBits(bits32, 16, 16);
    int e1 = DVBlockCode::cyclic168().decodeWord(hi);
    int e2 = DVBlockCode::cyclic168().decodeWord(lo);
    lsd = (uint16_t) (((hi >> 8) << 8) | (lo >> 8));
    return ((e1 < 0) || (e2 < 0)) ? -1 : e1 + e2;
}

// ---- TDULC -------------------------------------------------------------------------

void DVP25::tdulcEncode(const uint8_t *lc9, uint8_t *bits288)
{
    uint8_t lcBits[72];
    uint8_t hexbits[24];
    DVBits::bytesToBitsMSB(lc9, 9, lcBits);
    DVBits::bitsToHexbits(lcBits, 12, hexbits);
    DVReedSolomon::p25_24_12().encodeParity(hexbits, 12, hexbits + 12);

    for (int i = 0; i < 12; i++)
    {
        uint32_t data = ((uint32_t) hexbits[2 * i] << 6) | hexbits[2 * i + 1];
        DVBits::putBits(bits288, 24 * i, DVBlockCode::golay2412().encodeWord(data), 24);
    }
}

int DVP25::tdulcDecode(const uint8_t *bits288, uint8_t *lc9)
{
    uint8_t hexbits[24];
    uint8_t lcBits[72];

    for (int i = 0; i < 12; i++)
    {
        uint32_t cw = (uint32_t) DVBits::getBits(bits288, 24 * i, 24);
        DVBlockCode::golay2412().decodeWord(cw);
        hexbits[2 * i] = (uint8_t) ((cw >> 18) & 0x3F);
        hexbits[2 * i + 1] = (uint8_t) ((cw >> 12) & 0x3F);
    }

    int errors = DVReedSolomon::p25_24_12().decode(hexbits, 24);
    DVBits::hexbitsToBits(hexbits, 12, lcBits);
    DVBits::bitsToBytesMSB(lcBits, 72, lc9);
    return errors;
}

// ---- Status symbols -------------------------------------------------------------------

int DVP25::insertStatusSymbols(const uint8_t *data, int nbDataBits, uint8_t status, uint8_t *out)
{
    int nbData = ((nbDataBits + 69) / 70) * 70;
    int pos = 0;

    for (int i = 0; i < nbData; i += 2)
    {
        out[pos++] = (i < nbDataBits) ? data[i] : 0;
        out[pos++] = (i + 1 < nbDataBits) ? data[i + 1] : 0;

        if (((i / 2) % 35) == 34)
        {
            out[pos++] = (status >> 1) & 1;
            out[pos++] = status & 1;
        }
    }

    return pos;
}

int DVP25::removeStatusSymbols(const uint8_t *in, int nbBits, uint8_t *data)
{
    int pos = 0;

    for (int i = 0; i + 1 < nbBits; i += 2)
    {
        if (((i / 2) % 36) == 35) {
            continue; // status dibit
        }

        data[pos++] = in[i];
        data[pos++] = in[i + 1];
    }

    return pos;
}
