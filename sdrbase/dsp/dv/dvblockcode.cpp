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

// The parity tables are those of ETSI TS 102 361-1 V2.5.1 Annex B (DMR),
// ETSI TS 102 490 clause 7 (dPMR) and TIA-102.BAAA-A (P25). The cyclic codes
// are given by their generator polynomials.

#include <algorithm>
#include <vector>

#include "dvbits.h"
#include "dvblockcode.h"

DVBlockCode::DVBlockCode(int k, int nbParity, const char *const *parityRows) :
    m_k(k),
    m_p(nbParity),
    m_n(k + nbParity)
{
    m_parityOfData.resize(k);

    for (int i = 0; i < k; i++)
    {
        uint32_t row = 0;

        for (int j = 0; j < nbParity; j++) {
            row = (row << 1) | (parityRows[i][j] == '1' ? 1 : 0);
        }

        m_parityOfData[i] = row;
    }

    build();
}

DVBlockCode::DVBlockCode(int k, int nbParity, uint32_t generator, bool overallParity) :
    m_k(k),
    m_p(nbParity + (overallParity ? 1 : 0)),
    m_n(k + nbParity + (overallParity ? 1 : 0))
{
    m_parityOfData.resize(k);

    for (int i = 0; i < k; i++)
    {
        // Remainder of x^(k-1-i) x^p divided by g(x)
        uint32_t r = 1u << (nbParity + (k - 1 - i));

        for (int b = nbParity + (k - 1 - i); b >= nbParity; b--)
        {
            if (r & (1u << b)) {
                r ^= generator << (b - nbParity);
            }
        }

        uint32_t row = r & ((1u << nbParity) - 1);

        if (overallParity) {
            row = (row << 1) | ((1 + DVBits::popcount(row)) & 1); // even parity over data bit i and its parity bits
        }

        m_parityOfData[i] = row;
    }

    build();
}

void DVBlockCode::build()
{
    // All codewords: minimum distance and the soft decoding table
    m_codewords.resize(1u << m_k);
    m_dmin = m_n;

    for (uint32_t d = 0; d < (1u << m_k); d++)
    {
        uint32_t cw = (d << m_p) | parityOf(d);
        m_codewords[d] = cw;

        if (d != 0) {
            m_dmin = std::min(m_dmin, DVBits::popcount(cw));
        }
    }

    m_t = (m_dmin - 1) / 2;

    // Syndrome table: every error pattern of weight 1..t
    m_syndromeTable.assign(1u << m_p, m_noPattern);
    m_syndromeTable[0] = 0;
    std::vector<uint32_t> patterns(1, 0);

    for (int weight = 1; weight <= m_t; weight++)
    {
        std::vector<uint32_t> next;

        for (uint32_t p : patterns)
        {
            int lowest = p ? 0 : m_n;

            while (p && !((p >> lowest) & 1)) {
                lowest++;
            }

            for (int b = 0; b < (p ? lowest : m_n); b++)
            {
                uint32_t pattern = p | (1u << b);
                uint32_t s = getSyndrome(pattern);

                if (m_syndromeTable[s] == m_noPattern) {
                    m_syndromeTable[s] = pattern;
                }

                next.push_back(pattern);
            }
        }

        patterns.swap(next);
    }
}

uint32_t DVBlockCode::parityOf(uint32_t data) const
{
    uint32_t parity = 0;

    for (int i = 0; i < m_k; i++)
    {
        if ((data >> (m_k - 1 - i)) & 1) {
            parity ^= m_parityOfData[i];
        }
    }

    return parity;
}

uint32_t DVBlockCode::getSyndrome(uint32_t codeword) const
{
    return parityOf(codeword >> m_p) ^ (codeword & ((1u << m_p) - 1));
}

uint32_t DVBlockCode::encodeWord(uint32_t data) const
{
    data &= (1u << m_k) - 1;
    return (data << m_p) | parityOf(data);
}

int DVBlockCode::decodeWord(uint32_t& codeword) const
{
    codeword &= (m_n == 32) ? 0xFFFFFFFFu : ((1u << m_n) - 1);
    uint32_t pattern = m_syndromeTable[getSyndrome(codeword)];

    if (pattern == m_noPattern) {
        return -1;
    }

    codeword ^= pattern;
    return DVBits::popcount(pattern);
}

void DVBlockCode::encode(const uint8_t *data, uint8_t *codeword) const
{
    uint32_t d = (uint32_t) DVBits::getBits(data, 0, m_k);
    DVBits::putBits(codeword, 0, encodeWord(d), m_n);
}

int DVBlockCode::decode(uint8_t *codeword) const
{
    uint32_t cw = (uint32_t) DVBits::getBits(codeword, 0, m_n);
    int errors = decodeWord(cw);

    if (errors > 0) {
        DVBits::putBits(codeword, 0, cw, m_n);
    }

    return errors;
}

int DVBlockCode::decode(uint8_t *codeword, uint8_t *data) const
{
    int errors = decode(codeword);
    std::copy(codeword, codeword + m_k, data);
    return errors;
}

int DVBlockCode::decodeSoft(const float *llr, uint8_t *data, uint8_t *codeword) const
{
    // Correlation with the bipolar codeword: maximise sum (1 - 2 c_i) llr_i. The sum splits at
    // the middle of the codeword: each half's correlation for every pattern of its bits is
    // tabled first (one addition an entry, from the pattern with its lowest set bit cleared),
    // so each codeword costs two look ups rather than n branches - for Golay (24,12) 12 k
    // operations a word rather than 100 k, which is most of what a UW candidate costs the
    // Angel61 receiver on noise (up to 24 header attempts of two words each)
    const int lowBits = m_n / 2;
    const int highBits = m_n - lowBits;
    float best = -1e30f;
    uint32_t bestIndex = 0;

    if ((((size_t) 1 << m_k) * m_n) <= 4 * (((size_t) 1 << highBits) + ((size_t) 1 << lowBits)))
    {
        // Few codewords for their length: the tables would cost more than the codewords
        for (uint32_t d = 0; d < (1u << m_k); d++)
        {
            uint32_t cw = m_codewords[d];
            float metric = 0.0f;

            for (int i = 0; i < m_n; i++) {
                metric += ((cw >> (m_n - 1 - i)) & 1) ? -llr[i] : llr[i];
            }

            if (metric > best)
            {
                best = metric;
                bestIndex = d;
            }
        }
    }
    else
    {
        thread_local std::vector<float> high, low;
        high.resize((size_t) 1 << highBits);
        low.resize((size_t) 1 << lowBits);

        auto table = [](std::vector<float>& t, const float *l, int bits) {
            float all = 0.0f;
            for (int i = 0; i < bits; i++) {
                all += l[i];
            }
            t[0] = all;
            for (uint32_t p = 1; p < (1u << bits); p++)
            {
                // Bit b of the pattern (from the LSB) is position bits - 1 - b of the half
                int b = 0;
                while (!((p >> b) & 1)) {
                    b++;
                }
                t[p] = t[p & (p - 1)] - 2.0f * l[bits - 1 - b];
            }
        };

        table(high, llr, highBits);
        table(low, llr + highBits, lowBits);
        const uint32_t lowMask = (1u << lowBits) - 1;

        for (uint32_t d = 0; d < (1u << m_k); d++)
        {
            uint32_t cw = m_codewords[d];
            float metric = high[cw >> lowBits] + low[cw & lowMask];

            if (metric > best)
            {
                best = metric;
                bestIndex = d;
            }
        }
    }

    uint32_t cw = m_codewords[bestIndex];
    uint32_t hard = 0;

    for (int i = 0; i < m_n; i++) {
        hard = (hard << 1) | (llr[i] < 0.0f ? 1 : 0);
    }

    DVBits::putBits(data, 0, bestIndex, m_k);

    if (codeword) {
        DVBits::putBits(codeword, 0, cw, m_n);
    }

    return DVBits::popcount(cw ^ hard);
}

// ---- Shared instances -------------------------------------------------------------------

static const char *const HAMMING_7_4_P[4] = { "101", "111", "110", "011" };                                            // Table B.17
static const char *const HAMMING_13_9_P[9] = { "1111", "1110", "0111", "1010", "0101", "1011", "1100", "0110", "0011" }; // Table B.14
static const char *const HAMMING_15_11_P[11] = {                                                                       // Table B.15
    "1001", "1101", "1111", "1110", "0111", "1010", "0101", "1011", "1100", "0110", "0011"
};
static const char *const HAMMING_16_11_4_P[11] = {                                                                     // Table B.16
    "10011", "11010", "11111", "11100", "01110", "10101", "01011", "10110", "11001", "01101", "00111"
};
static const char *const GOLAY_20_8_P[8] = {                                                                           // Table B.11
    "001111011010", "110110011001", "011011001101", "001101100111",
    "110111000110", "101010010111", "100100111110", "100011101011"
};
static const char *const QR_16_7_6_P[7] = {                                                                            // Table B.12
    "001001111", "100011110", "110110111", "111100010", "111001001", "011100101", "001110011"
};
static const char *const HAMMING_12_8_P[8] = {                                                                         // TS 102 490 table 7.1
    "1110", "0111", "1010", "0101", "1011", "1100", "0110", "0011"
};
static const char *const HAMMING_10_6_P[6] = {                                                                         // TIA-102.BAAA-A
    "1110", "1101", "1011", "0111", "0011", "1100"
};

const DVBlockCode& DVBlockCode::hamming74()    { static const DVBlockCode code(4, 3, HAMMING_7_4_P); return code; }
const DVBlockCode& DVBlockCode::hamming139()   { static const DVBlockCode code(9, 4, HAMMING_13_9_P); return code; }
const DVBlockCode& DVBlockCode::hamming1511()  { static const DVBlockCode code(11, 4, HAMMING_15_11_P); return code; }
const DVBlockCode& DVBlockCode::hamming16114() { static const DVBlockCode code(11, 5, HAMMING_16_11_4_P); return code; }
const DVBlockCode& DVBlockCode::golay208()     { static const DVBlockCode code(8, 12, GOLAY_20_8_P); return code; }
const DVBlockCode& DVBlockCode::qr1676()       { static const DVBlockCode code(7, 9, QR_16_7_6_P); return code; }
const DVBlockCode& DVBlockCode::hamming128()   { static const DVBlockCode code(8, 4, HAMMING_12_8_P); return code; }
const DVBlockCode& DVBlockCode::hamming106()   { static const DVBlockCode code(6, 4, HAMMING_10_6_P); return code; }
const DVBlockCode& DVBlockCode::golay2312()    { static const DVBlockCode code(12, 11, 0xC75u, false); return code; }
const DVBlockCode& DVBlockCode::golay2412()    { static const DVBlockCode code(12, 11, 0xC75u, true); return code; }
const DVBlockCode& DVBlockCode::cyclic168()    { static const DVBlockCode code(8, 8, 0x139u, false); return code; }

const DVBlockCode& DVBlockCode::golay186()
{
    // Golay(24,12) shortened by the 6 leading data bits: the parity of hexbit bit i is that of data bit 6 + i
    struct Rows
    {
        char text[6][13];
        const char *rows[6];

        Rows()
        {
            const DVBlockCode& full = golay2412();

            for (int i = 0; i < 6; i++)
            {
                uint32_t cw = full.encodeWord(1u << (5 - i)); // data bit 6 + i set

                for (int j = 0; j < 12; j++) {
                    text[i][j] = ((cw >> (11 - j)) & 1) ? '1' : '0';
                }

                text[i][12] = '\0';
                rows[i] = text[i];
            }
        }
    };

    static const Rows rows;
    static const DVBlockCode code(6, 12, rows.rows);
    return code;
}
