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
#include "dvnxdn.h"
#include "dvscrambler.h"

// Received bit k is deinterleaved to position table[k] (NXDN TS 1-A 4.4.x)
const int DVNXDN::m_sacchInterleave[60] = {
    0, 12, 24, 36, 48,
    1, 13, 25, 37, 49,
    2, 14, 26, 38, 50,
    3, 15, 27, 39, 51,
    4, 16, 28, 40, 52,
    5, 17, 29, 41, 53,
    6, 18, 30, 42, 54,
    7, 19, 31, 43, 55,
    8, 20, 32, 44, 56,
    9, 21, 33, 45, 57,
    10, 22, 34, 46, 58,
    11, 23, 35, 47, 59,
};

// Indexes into the 72 coded bits removed by puncturing
const int DVNXDN::m_sacchPunctureList[12] = { 5, 11, 17, 23, 29, 35, 41, 47, 53, 59, 65, 71 };

const int DVNXDN::m_facch1Interleave[144] = {
    0, 16, 32, 48, 64, 80, 96, 112, 128,
    1, 17, 33, 49, 65, 81, 97, 113, 129,
    2, 18, 34, 50, 66, 82, 98, 114, 130,
    3, 19, 35, 51, 67, 83, 99, 115, 131,
    4, 20, 36, 52, 68, 84, 100, 116, 132,
    5, 21, 37, 53, 69, 85, 101, 117, 133,
    6, 22, 38, 54, 70, 86, 102, 118, 134,
    7, 23, 39, 55, 71, 87, 103, 119, 135,
    8, 24, 40, 56, 72, 88, 104, 120, 136,
    9, 25, 41, 57, 73, 89, 105, 121, 137,
    10, 26, 42, 58, 74, 90, 106, 122, 138,
    11, 27, 43, 59, 75, 91, 107, 123, 139,
    12, 28, 44, 60, 76, 92, 108, 124, 140,
    13, 29, 45, 61, 77, 93, 109, 125, 141,
    14, 30, 46, 62, 78, 94, 110, 126, 142,
    15, 31, 47, 63, 79, 95, 111, 127, 143,
};

// Indexes into the 192 coded bits removed by puncturing
const int DVNXDN::m_facch1PunctureList[48] = {
    1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
    81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145,
    149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189,
};

// UDCH: 12 rows of 29 (TS 1-A 4.4.4)
const int DVNXDN::m_udchInterleave[348] = {
    0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180, 192, 204, 216, 228, 240, 252, 264, 276, 288, 300, 312, 324, 336,
    1, 13, 25, 37, 49, 61, 73, 85, 97, 109, 121, 133, 145, 157, 169, 181, 193, 205, 217, 229, 241, 253, 265, 277, 289, 301, 313, 325, 337,
    2, 14, 26, 38, 50, 62, 74, 86, 98, 110, 122, 134, 146, 158, 170, 182, 194, 206, 218, 230, 242, 254, 266, 278, 290, 302, 314, 326, 338,
    3, 15, 27, 39, 51, 63, 75, 87, 99, 111, 123, 135, 147, 159, 171, 183, 195, 207, 219, 231, 243, 255, 267, 279, 291, 303, 315, 327, 339,
    4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124, 136, 148, 160, 172, 184, 196, 208, 220, 232, 244, 256, 268, 280, 292, 304, 316, 328, 340,
    5, 17, 29, 41, 53, 65, 77, 89, 101, 113, 125, 137, 149, 161, 173, 185, 197, 209, 221, 233, 245, 257, 269, 281, 293, 305, 317, 329, 341,
    6, 18, 30, 42, 54, 66, 78, 90, 102, 114, 126, 138, 150, 162, 174, 186, 198, 210, 222, 234, 246, 258, 270, 282, 294, 306, 318, 330, 342,
    7, 19, 31, 43, 55, 67, 79, 91, 103, 115, 127, 139, 151, 163, 175, 187, 199, 211, 223, 235, 247, 259, 271, 283, 295, 307, 319, 331, 343,
    8, 20, 32, 44, 56, 68, 80, 92, 104, 116, 128, 140, 152, 164, 176, 188, 200, 212, 224, 236, 248, 260, 272, 284, 296, 308, 320, 332, 344,
    9, 21, 33, 45, 57, 69, 81, 93, 105, 117, 129, 141, 153, 165, 177, 189, 201, 213, 225, 237, 249, 261, 273, 285, 297, 309, 321, 333, 345,
    10, 22, 34, 46, 58, 70, 82, 94, 106, 118, 130, 142, 154, 166, 178, 190, 202, 214, 226, 238, 250, 262, 274, 286, 298, 310, 322, 334, 346,
    11, 23, 35, 47, 59, 71, 83, 95, 107, 119, 131, 143, 155, 167, 179, 191, 203, 215, 227, 239, 251, 263, 275, 287, 299, 311, 323, 335, 347
};

// Indexes into the 406 coded bits removed by puncturing (two of every 14)
const int DVNXDN::m_udchPunctureList[58] = {
    3, 11, 17, 25, 31, 39, 45, 53, 59, 67, 73, 81, 87, 95, 101, 109, 115, 123, 129, 137, 143, 151, 157, 165, 171, 179, 185, 193, 199, 207, 213, 221, 227, 235, 241, 249, 255, 263, 269, 277, 283, 291, 297, 305, 311, 319, 325, 333, 339, 347, 353, 361, 367, 375, 381, 389, 395, 403
};

// ---- Convolutional code --------------------------------------------------------------

void DVNXDN::convEncode(const uint8_t *in, int nbBits, uint8_t *out)
{
    // The same code as DVConvolutional::k5(): register bit 4 = current, G1 mask 0x13, G2 mask 0x1D
    DVConvolutional::k5().encode(in, nbBits, out);
}

void DVNXDN::punctureInterleave(const uint8_t *coded, int nbCoded, const int *punctureList, int nbPuncture,
    const int *interleave, int nbTx, uint8_t *tx)
{
    uint8_t punctured[512];
    int n = 0;
    int p = 0;

    for (int i = 0; (i < nbCoded) && (n < nbTx); i++)
    {
        if ((p < nbPuncture) && (i == punctureList[p])) {
            p++;
        } else {
            punctured[n++] = coded[i];
        }
    }

    for (int k = 0; k < nbTx; k++) {
        tx[k] = punctured[interleave[k]];
    }
}

void DVNXDN::deinterleaveDepuncture(const float *rx, int nbTx, const int *interleave, const int *punctureList, int nbPuncture,
    int nbCoded, float *coded)
{
    float punctured[512];

    for (int k = 0; k < nbTx; k++) {
        punctured[interleave[k]] = rx[k];
    }

    int n = 0;
    int p = 0;

    for (int i = 0; i < nbCoded; i++)
    {
        if ((p < nbPuncture) && (i == punctureList[p]))
        {
            coded[i] = 0.0f;
            p++;
        }
        else
        {
            coded[i] = (n < nbTx) ? punctured[n] : 0.0f;
            n++;
        }
    }
}

// ---- LICH ---------------------------------------------------------------------------

void DVNXDN::lichEncode(int rfChannelType, int functionalType, int option, int direction, uint8_t *bits)
{
    uint8_t info[8];
    info[0] = (rfChannelType >> 1) & 1;
    info[1] = rfChannelType & 1;
    info[2] = (functionalType >> 1) & 1;
    info[3] = functionalType & 1;
    info[4] = (option >> 1) & 1;
    info[5] = option & 1;
    info[6] = direction & 1;
    // Even parity over the 4 most significant bits (TS 1-A 4.5.3, TS 1-E: the RF channel and
    // functional types, not the option: over six, option 01 and 10 frames failed their LICH)
    info[7] = (uint8_t) ((info[0] + info[1] + info[2] + info[3]) & 1);

    // Each information bit is the MSB of one symbol; the LSB is set so that only +3/-3 symbols are used
    for (int i = 0; i < 8; i++)
    {
        bits[2 * i] = info[i];
        bits[2 * i + 1] = 1;
    }
}

bool DVNXDN::lichDecode(const uint8_t *bits, int& rfChannelType, int& functionalType, int& option, int& direction)
{
    uint8_t info[8];

    for (int i = 0; i < 8; i++) {
        info[i] = bits[2 * i] & 1;
    }

    rfChannelType = (info[0] << 1) | info[1];
    functionalType = (info[2] << 1) | info[3];
    option = (info[4] << 1) | info[5];
    direction = info[6];
    return ((info[0] + info[1] + info[2] + info[3] + info[7]) & 1) == 0;
}

// ---- SACCH ------------------------------------------------------------------------------

void DVNXDN::sacchEncode(const uint8_t *info, uint8_t *tx)
{
    uint8_t data[m_sacchCodedBits];
    uint8_t coded[2 * m_sacchCodedBits];

    std::copy(info, info + m_sacchInfoBits, data);
    DVBits::putBits(data, m_sacchInfoBits, DVCRC::nxdn6(info, m_sacchInfoBits), 6);
    std::fill(data + 32, data + m_sacchCodedBits, 0); // tail bits
    convEncode(data, m_sacchCodedBits, coded);
    punctureInterleave(coded, 2 * m_sacchCodedBits, m_sacchPunctureList, 12, m_sacchInterleave, m_sacchTxBits, tx);
}

bool DVNXDN::sacchFromCoded(const float *coded, uint8_t *info)
{
    uint8_t data[m_sacchCodedBits];
    DVConvolutional::k5().decodeSoft(coded, m_sacchCodedBits, data, true);
    std::copy(data, data + m_sacchInfoBits, info);
    return DVCRC::nxdn6(data, m_sacchInfoBits) == (uint8_t) DVBits::getBits(data, m_sacchInfoBits, 6);
}

bool DVNXDN::sacchDecodeSoft(const float *llr, uint8_t *info)
{
    float coded[2 * m_sacchCodedBits];
    deinterleaveDepuncture(llr, m_sacchTxBits, m_sacchInterleave, m_sacchPunctureList, 12, 2 * m_sacchCodedBits, coded);
    return sacchFromCoded(coded, info);
}

bool DVNXDN::sacchDecode(const uint8_t *rx, uint8_t *info)
{
    float llr[m_sacchTxBits];
    DVBits::bitsToLlr(rx, m_sacchTxBits, llr);
    return sacchDecodeSoft(llr, info);
}

// ---- FACCH1 -----------------------------------------------------------------------------

void DVNXDN::facch1Encode(const uint8_t *info, uint8_t *tx)
{
    uint8_t data[m_facch1CodedBits];
    uint8_t coded[2 * m_facch1CodedBits];

    std::copy(info, info + m_facch1InfoBits, data);
    DVBits::putBits(data, m_facch1InfoBits, DVCRC::nxdn12(info, m_facch1InfoBits), 12);
    std::fill(data + 92, data + m_facch1CodedBits, 0); // tail bits
    convEncode(data, m_facch1CodedBits, coded);
    punctureInterleave(coded, 2 * m_facch1CodedBits, m_facch1PunctureList, 48, m_facch1Interleave, m_facch1TxBits, tx);
}

bool DVNXDN::facch1FromCoded(const float *coded, uint8_t *info)
{
    uint8_t data[m_facch1CodedBits];
    DVConvolutional::k5().decodeSoft(coded, m_facch1CodedBits, data, true);
    std::copy(data, data + m_facch1InfoBits, info);
    return DVCRC::nxdn12(data, m_facch1InfoBits) == (uint16_t) DVBits::getBits(data, m_facch1InfoBits, 12);
}

bool DVNXDN::facch1DecodeSoft(const float *llr, uint8_t *info)
{
    float coded[2 * m_facch1CodedBits];
    deinterleaveDepuncture(llr, m_facch1TxBits, m_facch1Interleave, m_facch1PunctureList, 48, 2 * m_facch1CodedBits, coded);
    return facch1FromCoded(coded, info);
}

// ---- UDCH -------------------------------------------------------------------------------

void DVNXDN::udchEncode(const uint8_t *info, uint8_t *tx)
{
    uint8_t data[m_udchCodedBits];
    uint8_t coded[2 * m_udchCodedBits];

    std::copy(info, info + m_udchInfoBits, data);
    DVBits::putBits(data, m_udchInfoBits, DVCRC::nxdn15(info, m_udchInfoBits), 15);
    std::fill(data + 199, data + m_udchCodedBits, 0); // tail bits
    convEncode(data, m_udchCodedBits, coded);
    punctureInterleave(coded, 2 * m_udchCodedBits, m_udchPunctureList, 58, m_udchInterleave, m_udchTxBits, tx);
}

bool DVNXDN::udchFromCoded(const float *coded, uint8_t *info)
{
    uint8_t data[m_udchCodedBits];
    DVConvolutional::k5().decodeSoft(coded, m_udchCodedBits, data, true);
    std::copy(data, data + m_udchInfoBits, info);
    return DVCRC::nxdn15(data, m_udchInfoBits) == (uint16_t) DVBits::getBits(data, m_udchInfoBits, 15);
}

bool DVNXDN::udchDecodeSoft(const float *llr, uint8_t *info)
{
    float coded[2 * m_udchCodedBits];
    deinterleaveDepuncture(llr, m_udchTxBits, m_udchInterleave, m_udchPunctureList, 58, 2 * m_udchCodedBits, coded);
    return udchFromCoded(coded, info);
}

// The CAC's puncture (coded bits 3 and 11 of every 14, as the UDCH's) and interleave (written
// in rows of 12, read in columns of 25): OP25 and dsd-fme PERM_12_25, SDRTrunk Interleaver(12, 25)
static const int *cacPunctureList()
{
    static int list[50];
    static bool built = false;

    if (!built)
    {
        for (int i = 0, n = 0; (i < 2 * DVNXDN::m_cacCodedBits) && (n < 50); i++)
        {
            if ((i % 14 == 3) || (i % 14 == 11)) {
                list[n++] = i;
            }
        }

        built = true;
    }

    return list;
}

static const int *cacInterleave()
{
    static int table[DVNXDN::m_cacTxBits];
    static bool built = false;

    if (!built)
    {
        for (int k = 0; k < DVNXDN::m_cacTxBits; k++) {
            table[k] = (k % 25) * 12 + k / 25;
        }

        built = true;
    }

    return table;
}

void DVNXDN::cacEncode(const uint8_t *info, uint8_t *tx)
{
    uint8_t data[m_cacCodedBits];
    uint8_t coded[2 * m_cacCodedBits];

    std::copy(info, info + m_cacInfoBits, data);
    DVBits::putBits(data, m_cacInfoBits, DVCRC::nxdn16(info, m_cacInfoBits), 16);
    std::fill(data + m_cacInfoBits + 16, data + m_cacCodedBits, 0); // tail bits
    convEncode(data, m_cacCodedBits, coded);
    punctureInterleave(coded, 2 * m_cacCodedBits, cacPunctureList(), 50, cacInterleave(), m_cacTxBits, tx);
}

bool DVNXDN::cacDecodeSoft(const float *llr, uint8_t *info)
{
    float coded[2 * m_cacCodedBits];
    deinterleaveDepuncture(llr, m_cacTxBits, cacInterleave(), cacPunctureList(), 50, 2 * m_cacCodedBits, coded);
    uint8_t data[m_cacCodedBits];
    DVConvolutional::k5().decodeSoft(coded, m_cacCodedBits, data, true);
    std::copy(data, data + m_cacInfoBits, info);
    return DVCRC::nxdn16(data, m_cacInfoBits) == (uint16_t) DVBits::getBits(data, m_cacInfoBits, 16);
}

bool DVNXDN::udchDecode(const uint8_t *rx, uint8_t *info)
{
    float llr[m_udchTxBits];
    DVBits::bitsToLlr(rx, m_udchTxBits, llr);
    return udchDecodeSoft(llr, info);
}

bool DVNXDN::facch1Decode(const uint8_t *rx, uint8_t *info)
{
    float llr[m_facch1TxBits];
    DVBits::bitsToLlr(rx, m_facch1TxBits, llr);
    return facch1DecodeSoft(llr, info);
}

// ---- Scrambling ----------------------------------------------------------------------------

void DVNXDN::scramble(uint8_t *bits)
{
    const DVScrambler::PN95& pn = DVScrambler::PN95::nxdn();

    for (int i = 0; i < m_scrambledSymbols; i++) {
        bits[2 * i] ^= pn.getBit(i);
    }
}

void DVNXDN::scrambleSoft(float *llr)
{
    const DVScrambler::PN95& pn = DVScrambler::PN95::nxdn();

    for (int i = 0; i < m_scrambledSymbols; i++)
    {
        if (pn.getBit(i)) {
            llr[2 * i] = -llr[2 * i];
        }
    }
}
