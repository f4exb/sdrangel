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

#include <cstring>

#include "dvambe.h"
#include "dvbits.h"
#include "dvblockcode.h"

// AMBE 3600x2450 interleave (ETSI TS 102 361-1 B.2, the mbelib rW/rX/rY/rZ tables)
const int DVAMBE::m_rW[36] = {
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int DVAMBE::m_rX[36] = {
  23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

const int DVAMBE::m_rY[36] = {
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int DVAMBE::m_rZ[36] = {
  5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

// Parameter bit i of ambe_d[49] is bit m_dvsiOrder[i] (MSB first) of the DVSI raw 2450 word
const int DVAMBE::m_dvsiOrder[49] = {
    0, 3, 6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 41, 43, 45, 47,
    1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 42, 44, 46, 48,
    2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38
};

void DVAMBE::paramsToDvsi(const uint8_t *params, uint8_t *bytes7)
{
    uint8_t raw[56] = { 0 };

    for (int i = 0; i < 49; i++) {
        raw[m_dvsiOrder[i]] = params[i] & 1;
    }

    DVBits::bitsToBytesMSB(raw, 56, bytes7);
}

void DVAMBE::dvsiToParams(const uint8_t *bytes7, uint8_t *params)
{
    uint8_t raw[56];
    DVBits::bytesToBitsMSB(bytes7, 7, raw);

    for (int i = 0; i < 49; i++) {
        params[i] = raw[m_dvsiOrder[i]];
    }
}

void DVAMBE::bitsToFrame(const uint8_t *bits, uint8_t fr[4][24])
{
    std::memset(fr, 0, 4 * 24);

    for (int i = 0; i < 36; i++)
    {
        fr[m_rW[i]][m_rX[i]] = bits[2 * i] & 1;
        fr[m_rY[i]][m_rZ[i]] = bits[2 * i + 1] & 1;
    }
}

void DVAMBE::frameToBits(const uint8_t fr[4][24], uint8_t *bits)
{
    for (int i = 0; i < 36; i++)
    {
        bits[2 * i] = fr[m_rW[i]][m_rX[i]];
        bits[2 * i + 1] = fr[m_rY[i]][m_rZ[i]];
    }
}

void DVAMBE::pnSequence(const uint8_t *c0, uint8_t *pn23)
{
    // pr[0] = 16 * C0 data, pr[i] = (173 pr[i-1] + 13849) mod 65536, bit i = pr[i] >> 15
    uint32_t seed = 0;

    for (int i = 0; i < 12; i++) {
        seed = (seed << 1) | (c0[i] & 1);
    }

    uint32_t pr = 16 * seed;

    for (int k = 0; k < 23; k++)
    {
        pr = ((173 * pr) + 13849) & 0xffff;
        pn23[k] = (uint8_t) (pr >> 15);
    }
}

int DVAMBE::bitsToParams(const uint8_t *bits, uint8_t *params)
{
    // Parameter order (mbelib ambe_d[49]): C0 data fr[0][23..12], C1 data fr[1][22..11] after
    // descrambling, C2 fr[2][10..0], C3 fr[3][13..0]
    uint8_t fr[4][24];
    bitsToFrame(bits, fr);

    // C0: Golay(24,12) with the codeword read from fr[0][23] (data MSB) down to fr[0][0] (parity)
    uint8_t c0[24];

    for (int k = 0; k < 24; k++) {
        c0[k] = fr[0][23 - k];
    }

    int errors = DVBlockCode::golay2412().decode(c0);

    if (errors < 0) {
        return -1;
    }

    for (int k = 0; k < 12; k++) {
        params[k] = c0[k];
    }

    // C1: Golay(23,12) scrambled with the sequence derived from C0
    uint8_t pn[23];
    uint8_t c1[23];
    pnSequence(params, pn);

    for (int k = 0; k < 23; k++) {
        c1[k] = fr[1][22 - k] ^ pn[k];
    }

    int errors1 = DVBlockCode::golay2312().decode(c1);

    if (errors1 > 0) {
        errors += errors1;
    }

    for (int k = 0; k < 12; k++) {
        params[12 + k] = c1[k];
    }

    for (int k = 0; k < 11; k++) {
        params[24 + k] = fr[2][10 - k];
    }

    for (int k = 0; k < 14; k++) {
        params[35 + k] = fr[3][13 - k];
    }

    return errors;
}

int DVAMBE::llrToParams(const float *llr, uint8_t *params)
{
    // Soft bit planes: the same interleave as the bits
    float fr[4][24] = { { 0.0f } };

    for (int i = 0; i < 36; i++)
    {
        fr[m_rW[i]][m_rX[i]] = llr[2 * i];
        fr[m_rY[i]][m_rZ[i]] = llr[2 * i + 1];
    }

    float c0[24];

    for (int k = 0; k < 24; k++) {
        c0[k] = fr[0][23 - k];
    }

    int changed = DVBlockCode::golay2412().decodeSoft(c0, params);

    // C1: descramble the LLRs (a scrambler bit inverts the sign) then decode
    uint8_t pn[23];
    float c1[23];
    pnSequence(params, pn);

    for (int k = 0; k < 23; k++) {
        c1[k] = pn[k] ? -fr[1][22 - k] : fr[1][22 - k];
    }

    changed += DVBlockCode::golay2312().decodeSoft(c1, params + 12);

    for (int k = 0; k < 11; k++) {
        params[24 + k] = fr[2][10 - k] < 0.0f ? 1 : 0;
    }

    for (int k = 0; k < 14; k++) {
        params[35 + k] = fr[3][13 - k] < 0.0f ? 1 : 0;
    }

    return changed;
}

void DVAMBE::paramsToBits(const uint8_t *params, uint8_t *bits)
{
    uint8_t fr[4][24];
    uint8_t coded[24];
    std::memset(fr, 0, sizeof(fr));

    DVBlockCode::golay2412().encode(&params[0], coded);

    for (int k = 0; k < 24; k++) {
        fr[0][23 - k] = coded[k];
    }

    DVBlockCode::golay2312().encode(&params[12], coded);
    uint8_t pn[23];
    pnSequence(&params[0], pn);

    for (int k = 0; k < 23; k++) {
        fr[1][22 - k] = coded[k] ^ pn[k];
    }

    for (int k = 0; k < 11; k++) {
        fr[2][10 - k] = params[24 + k];
    }

    for (int k = 0; k < 14; k++) {
        fr[3][13 - k] = params[35 + k];
    }

    frameToBits(fr, bits);
}

int DVAMBE::bytesToParams(const uint8_t *frame, uint8_t *params)
{
    uint8_t bits[m_frameBits];
    DVBits::bytesToBitsMSB(frame, m_frameBytes, bits);
    return bitsToParams(bits, params);
}

void DVAMBE::paramsToBytes(const uint8_t *params, uint8_t *frame)
{
    uint8_t bits[m_frameBits];
    paramsToBits(params, bits);
    DVBits::bitsToBytesMSB(bits, m_frameBits, frame);
}
