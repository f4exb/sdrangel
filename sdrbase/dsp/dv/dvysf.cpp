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
#include "dvconvolutional.h"
#include "dvcrc.h"
#include "dvscrambler.h"
#include "dvysf.h"

// Interleave tables of the YSF specification: the on-air bit i holds raw bit table[i]

const int DVYSF::m_fichInterleave[100] = {
        0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95,
        1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61, 66, 71, 76, 81, 86, 91, 96,
        2, 7, 12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 77, 82, 87, 92, 97,
        3, 8, 13, 18, 23, 28, 33, 38, 43, 48, 53, 58, 63, 68, 73, 78, 83, 88, 93, 98,
        4, 9, 14, 19, 24, 29, 34, 39, 44, 49, 54, 59, 64, 69, 74, 79, 84, 89, 94, 99,
};

const int DVYSF::m_dchInterleave[180] = {
        0,   9,  18,  27,  36,  45,  54,  63,  72,  81,  90,  99, 108, 117, 126, 135, 144, 153, 162, 171,
        1,  10,  19,  28,  37,  46,  55,  64,  73,  82,  91, 100, 109, 118, 127, 136, 145, 154, 163, 172,
        2,  11,  20,  29,  38,  47,  56,  65,  74,  83,  92, 101, 110, 119, 128, 137, 146, 155, 164, 173,
        3,  12,  21,  30,  39,  48,  57,  66,  75,  84,  93, 102, 111, 120, 129, 138, 147, 156, 165, 174,
        4,  13,  22,  31,  40,  49,  58,  67,  76,  85,  94, 103, 112, 121, 130, 139, 148, 157, 166, 175,
        5,  14,  23,  32,  41,  50,  59,  68,  77,  86,  95, 104, 113, 122, 131, 140, 149, 158, 167, 176,
        6,  15,  24,  33,  42,  51,  60,  69,  78,  87,  96, 105, 114, 123, 132, 141, 150, 159, 168, 177,
        7,  16,  25,  34,  43,  52,  61,  70,  79,  88,  97, 106, 115, 124, 133, 142, 151, 160, 169, 178,
        8,  17,  26,  35,  44,  53,  62,  71,  80,  89,  98, 107, 116, 125, 134, 143, 152, 161, 170, 179
};

const int DVYSF::m_vd2Interleave[104] = {
         0,  26,  52,  78,
         1,  27,  53,  79,
         2,  28,  54,  80,
         3,  29,  55,  81,
         4,  30,  56,  82,
         5,  31,  57,  83,
         6,  32,  58,  84,
         7,  33,  59,  85,
         8,  34,  60,  86,
         9,  35,  61,  87,
        10,  36,  62,  88,
        11,  37,  63,  89,
        12,  38,  64,  90,
        13,  39,  65,  91,
        14,  40,  66,  92,
        15,  41,  67,  93,
        16,  42,  68,  94,
        17,  43,  69,  95,
        18,  44,  70,  96,
        19,  45,  71,  97,
        20,  46,  72,  98,
        21,  47,  73,  99,
        22,  48,  74, 100,
        23,  49,  75, 101,
        24,  50,  76, 102,
        25,  51,  77, 103
};

// ---- FICH ------------------------------------------------------------------------

void DVYSF::fichToBits(const FICH& fich, uint8_t *bits)
{
    // FI(2) CS(2) CM(2) BN(2) BT(2) FN(3) FT(3) reserved(1) Dev(1) MR(3) VoIP(1) DT(2) SQL(1) SQ(7)
    int p = 0;
    p = DVBits::putBits(bits, p, fich.m_fi, 2);
    p = DVBits::putBits(bits, p, fich.m_cs, 2);
    p = DVBits::putBits(bits, p, fich.m_cm, 2);
    p = DVBits::putBits(bits, p, fich.m_bn, 2);
    p = DVBits::putBits(bits, p, fich.m_bt, 2);
    p = DVBits::putBits(bits, p, fich.m_fn, 3);
    p = DVBits::putBits(bits, p, fich.m_ft, 3);
    p = DVBits::putBits(bits, p, 0, 1);
    p = DVBits::putBits(bits, p, fich.m_dev, 1);
    p = DVBits::putBits(bits, p, fich.m_mr, 3);
    p = DVBits::putBits(bits, p, fich.m_voip, 1);
    p = DVBits::putBits(bits, p, fich.m_dt, 2);
    p = DVBits::putBits(bits, p, fich.m_sql, 1);
    DVBits::putBits(bits, p, fich.m_sq, 7);
}

void DVYSF::bitsToFich(const uint8_t *bits, FICH& fich)
{
    fich.m_fi = (int) DVBits::getBits(bits, 0, 2);
    fich.m_cs = (int) DVBits::getBits(bits, 2, 2);
    fich.m_cm = (int) DVBits::getBits(bits, 4, 2);
    fich.m_bn = (int) DVBits::getBits(bits, 6, 2);
    fich.m_bt = (int) DVBits::getBits(bits, 8, 2);
    fich.m_fn = (int) DVBits::getBits(bits, 10, 3);
    fich.m_ft = (int) DVBits::getBits(bits, 13, 3);
    fich.m_dev = (int) DVBits::getBits(bits, 17, 1);
    fich.m_mr = (int) DVBits::getBits(bits, 18, 3);
    fich.m_voip = (int) DVBits::getBits(bits, 21, 1);
    fich.m_dt = (int) DVBits::getBits(bits, 22, 2);
    fich.m_sql = (int) DVBits::getBits(bits, 24, 1);
    fich.m_sq = (int) DVBits::getBits(bits, 25, 7);
}

void DVYSF::fichEncode(const FICH& fich, uint8_t *dibits)
{
    // 32 bits + CRC-16 = 48 bits -> 4 x Golay(24,12) = 96 bits + 4 flush bits = 100 bits
    // -> K=5 rate 1/2 -> 100 dibits -> interleave
    uint8_t info[48];
    uint8_t bytes[4];
    uint8_t coded[100];
    uint8_t raw[100];

    fichToBits(fich, info);
    DVBits::bitsToBytesMSB(info, 32, bytes);
    DVBits::putBits(info, 32, DVCRC::ysf16(bytes, 4), 16);

    for (int b = 0; b < 4; b++) {
        DVBlockCode::golay2412().encode(&info[12 * b], &coded[24 * b]);
    }

    std::fill(coded + 96, coded + 100, 0);
    DVConvolutional::k5().encodeDibits(coded, 100, raw);

    for (int i = 0; i < m_fichDibits; i++) {
        dibits[i] = raw[m_fichInterleave[i]];
    }
}

int DVYSF::fichFromCoded(const uint8_t *coded, FICH& fich)
{
    uint8_t info[48];
    uint8_t bytes[4];
    int corrected = 0;

    for (int b = 0; b < 4; b++)
    {
        uint8_t cw[24];
        std::copy(&coded[24 * b], &coded[24 * b + 24], cw);
        int e = DVBlockCode::golay2412().decode(cw);

        if (e < 0) {
            return -1;
        }

        corrected += e;
        std::copy(cw, cw + 12, &info[12 * b]);
    }

    DVBits::bitsToBytesMSB(info, 32, bytes);

    if (DVCRC::ysf16(bytes, 4) != (uint16_t) DVBits::getBits(info, 32, 16)) {
        return -1;
    }

    bitsToFich(info, fich);
    return corrected;
}

int DVYSF::fichDecode(const uint8_t *dibits, FICH& fich)
{
    uint8_t raw[100];
    uint8_t coded[100];

    for (int i = 0; i < m_fichDibits; i++) {
        raw[m_fichInterleave[i]] = dibits[i];
    }

    DVConvolutional::k5().decodeDibits(raw, 100, coded, true);
    return fichFromCoded(coded, fich);
}

int DVYSF::fichDecodeSoft(const float *llr, FICH& fich)
{
    float raw[200];
    uint8_t coded[100];

    for (int i = 0; i < m_fichDibits; i++)
    {
        raw[2 * m_fichInterleave[i]] = llr[2 * i];
        raw[2 * m_fichInterleave[i] + 1] = llr[2 * i + 1];
    }

    DVConvolutional::k5().decodeSoft(raw, 100, coded, true);
    return fichFromCoded(coded, fich);
}

// ---- DCH -------------------------------------------------------------------------

void DVYSF::dchEncode(const uint8_t *data, int nbBytes, uint8_t *dibits)
{
    // The CRC is computed over the whitened data bytes and is not whitened itself.
    // nbBytes + 2 bytes + 4 flush bits -> K=5 rate 1/2 -> interleave
    uint8_t bytes[22];
    uint8_t bits[180];
    uint8_t raw[180];
    int nbBits = 8 * (nbBytes + 2) + 4;

    for (int i = 0; i < nbBytes; i++) {
        bytes[i] = data[i] ^ DVScrambler::PN95::ysf().getByte(i);
    }

    uint16_t crc = DVCRC::ysf16(bytes, nbBytes);
    bytes[nbBytes] = crc >> 8;
    bytes[nbBytes + 1] = crc & 0xff;
    DVBits::bytesToBitsMSB(bytes, nbBytes + 2, bits);
    std::fill(bits + 8 * (nbBytes + 2), bits + nbBits, 0);
    DVConvolutional::k5().encodeDibits(bits, nbBits, raw);

    const int *table = (nbBytes == 10) ? m_fichInterleave : m_dchInterleave;

    for (int i = 0; i < nbBits; i++) {
        dibits[i] = raw[table[i]];
    }
}

bool DVYSF::dchFromDecoded(const uint8_t *bits, int nbBytes, uint8_t *data)
{
    uint8_t bytes[22];
    DVBits::bitsToBytesMSB(bits, 8 * (nbBytes + 2), bytes);
    uint16_t crc = (uint16_t) ((bytes[nbBytes] << 8) | bytes[nbBytes + 1]);
    bool ok = DVCRC::ysf16(bytes, nbBytes) == crc;

    for (int i = 0; i < nbBytes; i++) {
        data[i] = bytes[i] ^ DVScrambler::PN95::ysf().getByte(i);
    }

    return ok;
}

bool DVYSF::dchDecode(const uint8_t *dibits, int nbBytes, uint8_t *data)
{
    uint8_t raw[180];
    uint8_t bits[180];
    int nbBits = 8 * (nbBytes + 2) + 4;
    const int *table = (nbBytes == 10) ? m_fichInterleave : m_dchInterleave;

    for (int i = 0; i < nbBits; i++) {
        raw[table[i]] = dibits[i];
    }

    DVConvolutional::k5().decodeDibits(raw, nbBits, bits, true);
    return dchFromDecoded(bits, nbBytes, data);
}

bool DVYSF::dchDecodeSoft(const float *llr, int nbBytes, uint8_t *data)
{
    float raw[360];
    uint8_t bits[180];
    int nbBits = 8 * (nbBytes + 2) + 4;
    const int *table = (nbBytes == 10) ? m_fichInterleave : m_dchInterleave;

    for (int i = 0; i < nbBits; i++)
    {
        raw[2 * table[i]] = llr[2 * i];
        raw[2 * table[i] + 1] = llr[2 * i + 1];
    }

    DVConvolutional::k5().decodeSoft(raw, nbBits, bits, true);
    return dchFromDecoded(bits, nbBytes, data);
}

// ---- V/D type 2 voice --------------------------------------------------------------

void DVYSF::vd2Bits(const uint8_t *ambe49, uint8_t *bits104)
{
    // Bits 0..80 carry the first 27 parameter bits each repeated 3 times consecutively,
    // bits 81..102 the remaining 22, bit 103 is 0
    for (int k = 0; k < 27; k++)
    {
        bits104[3 * k] = ambe49[k] & 1;
        bits104[3 * k + 1] = ambe49[k] & 1;
        bits104[3 * k + 2] = ambe49[k] & 1;
    }

    for (int k = 0; k < 22; k++) {
        bits104[81 + k] = ambe49[27 + k] & 1;
    }

    bits104[103] = 0;
}

void DVYSF::vd2VoiceEncode(const uint8_t *ambe49, uint8_t *dibits)
{
    uint8_t raw[m_vd2VoiceBits];
    uint8_t onAir[m_vd2VoiceBits];
    vd2Bits(ambe49, raw);

    // Whitening is applied in the de-interleaved domain with the PN bit at the de-interleaved index
    DVScrambler::PN95::ysf().apply(raw, m_vd2VoiceBits);

    for (int i = 0; i < m_vd2VoiceBits; i++) {
        onAir[i] = raw[m_vd2Interleave[i]];
    }

    DVBits::bitsToDibits(onAir, m_vd2VoiceDibits, dibits);
}

void DVYSF::vd2VoiceDecode(const uint8_t *dibits, uint8_t *ambe49)
{
    uint8_t onAir[m_vd2VoiceBits];
    uint8_t raw[m_vd2VoiceBits];
    DVBits::dibitsToBits(dibits, m_vd2VoiceDibits, onAir);

    for (int i = 0; i < m_vd2VoiceBits; i++) {
        raw[m_vd2Interleave[i]] = onAir[i];
    }

    DVScrambler::PN95::ysf().apply(raw, m_vd2VoiceBits);

    for (int k = 0; k < 27; k++) {
        ambe49[k] = (raw[3 * k] + raw[3 * k + 1] + raw[3 * k + 2]) >= 2 ? 1 : 0;
    }

    for (int k = 0; k < 22; k++) {
        ambe49[27 + k] = raw[81 + k];
    }
}

void DVYSF::vd2VoiceDecodeSoft(const float *llr, uint8_t *ambe49, int *errors)
{
    float raw[m_vd2VoiceBits];

    for (int i = 0; i < m_vd2VoiceBits; i++) {
        raw[m_vd2Interleave[i]] = DVScrambler::PN95::ysf().getBit(m_vd2Interleave[i]) ? -llr[i] : llr[i];
    }

    int outvoted = 0;

    for (int k = 0; k < 27; k++)
    {
        ambe49[k] = (raw[3 * k] + raw[3 * k + 1] + raw[3 * k + 2]) < 0.0f ? 1 : 0;

        for (int r = 0; r < 3; r++) {
            outvoted += (raw[3 * k + r] < 0.0f ? 1 : 0) != ambe49[k];
        }
    }

    if (errors) {
        *errors = outvoted;
    }

    for (int k = 0; k < 22; k++) {
        ambe49[27 + k] = raw[81 + k] < 0.0f ? 1 : 0;
    }
}

// ---- Voice full rate ---------------------------------------------------------------------------

const int *DVYSF::imbeYSFToP25Table()
{
    // Both layouts give each on-air bit a cell (row, column) of the IMBE frame: P25 Phase 1 the dibit
    // tables (iW, iX) and (iY, iZ) of its two bits; YSF a position in the 23, 23, 23, 23, 15, 15, 15, 7
    // codeword sequence, each codeword MSB first
    static const uint8_t iW[72] = { 0,2,4,1,3,5, 0,2,4,1,3,6, 0,2,4,1,3,6, 0,2,4,1,3,6, 0,2,4,1,3,6, 0,2,4,1,3,6,
                                    0,2,5,1,3,6, 0,2,5,1,3,6, 0,2,5,1,3,7, 0,2,5,1,3,7, 0,2,5,1,4,7, 0,3,5,2,4,7 };
    static const uint8_t iX[72] = { 22,20,10,20,18,0, 20,18,8,18,16,13, 18,16,6,16,14,11, 16,14,4,14,12,9, 14,12,2,12,10,7, 12,10,0,10,8,5,
                                    10,8,13,8,6,3, 8,6,11,6,4,1, 6,4,9,4,2,6, 4,2,7,2,0,4, 2,0,5,0,13,2, 0,21,3,21,11,0 };
    static const uint8_t iY[72] = { 1,3,5,0,2,4, 1,3,6,0,2,4, 1,3,6,0,2,4, 1,3,6,0,2,4, 1,3,6,0,2,4, 1,3,6,0,2,5,
                                    1,3,6,0,2,5, 1,3,6,0,2,5, 1,3,6,0,2,5, 1,3,7,0,2,5, 1,4,7,0,3,5, 2,4,7,1,3,5 };
    static const uint8_t iZ[72] = { 21,19,1,21,19,9, 19,17,14,19,17,7, 17,15,12,17,15,5, 15,13,10,15,13,3, 13,11,8,13,11,1, 11,9,6,11,9,14,
                                    9,7,4,9,7,12, 7,5,2,7,5,10, 5,3,0,5,3,8, 3,1,5,3,1,6, 1,14,3,1,22,4, 22,12,1,22,20,2 };
    static const uint8_t ysfVW[144] = {
          0, 24, 48, 72, 96,120, 25,  1, 73, 49,121, 97,   2, 26, 50, 74, 98,122, 27,  3, 75, 51,123, 99,
          4, 28, 52, 76,100,124, 29,  5, 77, 53,125,101,   6, 30, 54, 78,102,126, 31,  7, 79, 55,127,103,
          8, 32, 56, 80,104,128, 33,  9, 81, 57,129,105,  10, 34, 58, 82,106,130, 35, 11, 83, 59,131,107,
         12, 36, 60, 84,108,132, 37, 13, 85, 61,133,109,  14, 38, 62, 86,110,134, 39, 15, 87, 63,135,111,
         16, 40, 64, 88,112,136, 41, 17, 89, 65,137,113,  18, 42, 66, 90,114,138, 43, 19, 91, 67,139,115,
         20, 44, 68, 92,116,140, 45, 21, 93, 69,141,117,  22, 46, 70, 94,118,142, 47, 23, 95, 71,143,119 };
    static int table[144];
    static bool built = false;

    if (!built)
    {
        int p25At[8 * 23];
        std::fill(p25At, p25At + 8 * 23, -1);

        for (int d = 0; d < 72; d++)
        {
            p25At[23 * iW[d] + iX[d]] = 2 * d;
            p25At[23 * iY[d] + iZ[d]] = 2 * d + 1;
        }

        for (int i = 0; i < 144; i++)
        {
            const int pos = ysfVW[i];
            const int row = pos < 92 ? pos / 23 : pos < 137 ? 4 + (pos - 92) / 15 : 7;
            const int column = pos < 92 ? 22 - pos % 23 : pos < 137 ? 14 - (pos - 92) % 15 : 6 - (pos - 137);
            table[i] = p25At[23 * row + column];
        }

        built = true;
    }

    return table;
}

void DVYSF::imbeYSFToP25(const uint8_t *ysf, uint8_t *p25)
{
    const int *table = imbeYSFToP25Table();

    for (int i = 0; i < 144; i++) {
        p25[table[i]] = ysf[i];
    }
}

void DVYSF::imbeYSFToP25(const float *ysf, float *p25)
{
    const int *table = imbeYSFToP25Table();

    for (int i = 0; i < 144; i++) {
        p25[table[i]] = ysf[i];
    }
}

void DVYSF::imbeP25ToYSF(const uint8_t *p25, uint8_t *ysf)
{
    const int *table = imbeYSFToP25Table();

    for (int i = 0; i < 144; i++) {
        ysf[i] = p25[table[i]];
    }
}
