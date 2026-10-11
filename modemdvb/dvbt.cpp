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

#include "dvbt.h"

namespace dvbt {

int fftSize(Mode mode)
{
    return mode == Mode::Mode2K ? 2048 : 8192;
}

int kMax(Mode mode)
{
    return mode == Mode::Mode2K ? 1704 : 6816;
}

int dataCarriers(Mode mode)
{
    return mode == Mode::Mode2K ? 1512 : 6048;
}

int guardSamples(Mode mode, GuardInterval guard)
{
    return fftSize(mode) / (32 >> (int) guard);
}

int bitsPerCell(Constellation constellation)
{
    return 2 * ((int) constellation + 1);
}

void codeRateFraction(CodeRate rate, int& num, int& den)
{
    static const int nums[] = {1, 2, 3, 5, 7};
    num = nums[(int) rate];
    den = num + 1;
}

double sampleRate(int bandwidthHz)
{
    return 8.0 * bandwidthHz / 7.0; // Elementary period T = 7/64 us for 8 MHz
}

const char *toString(Mode mode)
{
    return mode == Mode::Mode2K ? "2K" : "8K";
}

const char *toString(Constellation constellation)
{
    static const char *names[] = {"QPSK", "16-QAM", "64-QAM"};
    return names[(int) constellation];
}

const char *toString(Hierarchy hierarchy)
{
    static const char *names[] = {"None", "a=1", "a=2", "a=4"};
    return names[(int) hierarchy];
}

const char *toString(CodeRate rate)
{
    static const char *names[] = {"1/2", "2/3", "3/4", "5/6", "7/8"};
    return names[(int) rate];
}

const char *toString(GuardInterval guard)
{
    static const char *names[] = {"1/32", "1/16", "1/8", "1/4"};
    return names[(int) guard];
}

double netBitRate(int bandwidthHz, Mode mode, Constellation constellation, CodeRate rate, GuardInterval guard)
{
    int num, den;
    codeRateFraction(rate, num, den);
    double symbolDuration = (fftSize(mode) + guardSamples(mode, guard)) / sampleRate(bandwidthHz);
    double bits = dataCarriers(mode) * bitsPerCell(constellation) * (double) num / den * tsPacketSize / rsPacketSize;
    return bits / symbolDuration;
}

// TPS

TPS::TPS() :
    m_frameNumber(0),
    m_constellation(Constellation::QAM64),
    m_hierarchy(Hierarchy::None),
    m_hpCodeRate(CodeRate::R2_3),
    m_lpCodeRate(CodeRate::R1_2),
    m_guard(GuardInterval::G1_32),
    m_mode(Mode::Mode8K),
    m_lengthIndicator(23),
    m_cellIdByte(0),
    m_inDepth(false),
    m_bchOK(false)
{
}

bool TPS::sameParameters(const TPS& other) const
{
    return (m_constellation == other.m_constellation)
        && (m_hierarchy == other.m_hierarchy)
        && (m_hpCodeRate == other.m_hpCodeRate)
        && (m_lpCodeRate == other.m_lpCodeRate)
        && (m_guard == other.m_guard)
        && (m_mode == other.m_mode)
        && (m_inDepth == other.m_inDepth);
}

int TPS::getBits(const uint8_t *bits, int start, int count)
{
    int v = 0;

    for (int i = 0; i < count; i++) {
        v = (v << 1) | (bits[start + i] & 1);
    }

    return v;
}

void TPS::setBits(uint8_t *bits, int start, int count, int value)
{
    for (int i = 0; i < count; i++) {
        bits[start + i] = (value >> (count - 1 - i)) & 1;
    }
}

// BCH(67,53, t=2), shortened from BCH(127,113): g(x) = x^14 + x^9 + x^8 + x^6 + x^5 + x^4 + x^2 + x + 1 (4.6.2.11)
// Remainder of the polynomial with coefficients bits[start..start+count-1] (highest degree first) times x^14
uint16_t TPS::bchRemainder(const uint8_t *bits, int start, int count)
{
    const uint16_t g = 0x0377; // g(x) without x^14
    uint16_t rem = 0;

    for (int i = 0; i < count; i++)
    {
        int feedback = ((rem >> 13) & 1) ^ (bits[start + i] & 1);
        rem = (rem << 1) & 0x3fff;

        if (feedback) {
            rem ^= g;
        }
    }

    return rem;
}

bool TPS::checkBCH(const uint8_t bits[symbolsPerFrame])
{
    return bchRemainder(bits, 1, 67) == 0; // s1..s67
}

void TPS::addBCH(uint8_t bits[symbolsPerFrame])
{
    setBits(bits, 54, 14, bchRemainder(bits, 1, 53)); // Parity of s1..s53 into s54..s67
}

bool TPS::decode(const uint8_t bits[symbolsPerFrame])
{
    int sync = getBits(bits, 1, 16);
    int frameNumber = getBits(bits, 23, 2);
    bool oddFrame = (frameNumber & 1) == 0; // Frames 1 and 3 have the non-inverted sync word

    if (sync != (oddFrame ? m_syncWord : (~m_syncWord & 0xffff))) {
        return false;
    }

    int constellation = getBits(bits, 25, 2);
    int hierarchy = getBits(bits, 28, 2); // s27 is the in-depth interleaver flag (DVB-H, F.3.4.2.2)
    int hpCodeRate = getBits(bits, 30, 3);
    int lpCodeRate = getBits(bits, 33, 3);
    int mode = getBits(bits, 38, 2);

    if ((constellation > 2) || (hpCodeRate > 4) || (lpCodeRate > 4) || (mode > 1)) {
        return false;
    }

    m_frameNumber = frameNumber;
    m_lengthIndicator = getBits(bits, 17, 6);
    m_constellation = (Constellation) constellation;
    m_hierarchy = (Hierarchy) hierarchy;
    m_inDepth = (bits[27] & 1) != 0;
    m_hpCodeRate = (CodeRate) hpCodeRate;
    m_lpCodeRate = (CodeRate) lpCodeRate;
    m_guard = (GuardInterval) getBits(bits, 36, 2);
    m_mode = (Mode) mode;
    m_cellIdByte = getBits(bits, 40, 8);
    m_bchOK = checkBCH(bits);

    return true;
}

void TPS::encode(uint8_t bits[symbolsPerFrame]) const
{
    bool oddFrame = (m_frameNumber & 1) == 0;

    bits[0] = 0;
    setBits(bits, 1, 16, oddFrame ? m_syncWord : (~m_syncWord & 0xffff));
    setBits(bits, 17, 6, m_lengthIndicator);
    setBits(bits, 23, 2, m_frameNumber);
    setBits(bits, 25, 2, (int) m_constellation);
    setBits(bits, 27, 3, (m_inDepth ? 4 : 0) | (int) m_hierarchy);
    setBits(bits, 30, 3, (int) m_hpCodeRate);
    setBits(bits, 33, 3, (int) m_lpCodeRate);
    setBits(bits, 36, 2, (int) m_guard);
    setBits(bits, 38, 2, (int) m_mode);
    setBits(bits, 40, 8, m_cellIdByte);
    setBits(bits, 48, 6, 0);
    addBCH(bits);
}

// Tables

// Continual pilot carrier indices for 2K mode (4.5.3, table 7). The 8K positions are these repeated
// every 1704 carriers, plus 6816
const int Tables::m_continualPilots2K[] = {
    0, 48, 54, 87, 141, 156, 192, 201, 255, 279, 282, 333, 432, 450, 483, 525, 531, 618, 636, 714, 759, 765,
    780, 804, 873, 888, 918, 939, 942, 969, 984, 1050, 1101, 1107, 1110, 1137, 1140, 1146, 1206, 1269, 1323,
    1377, 1491, 1683, 1704
};

// TPS carrier indices for 2K mode (4.6, table 8). The 8K positions are these repeated every 1704 carriers
const int Tables::m_tpsCarriers2K[] = {
    34, 50, 209, 346, 413, 569, 595, 688, 790, 901, 1073, 1219, 1262, 1286, 1469, 1594, 1687
};

const Tables& Tables::get(Mode mode)
{
    static const Tables tables2K(Mode::Mode2K);
    static const Tables tables8K(Mode::Mode8K);

    return mode == Mode::Mode2K ? tables2K : tables8K;
}

Tables::Tables(Mode mode) :
    m_mode(mode),
    m_fftSize(fftSize(mode)),
    m_kMax(kMax(mode)),
    m_nbData(dataCarriers(mode))
{
    const int nbCPs2K = sizeof(m_continualPilots2K) / sizeof(m_continualPilots2K[0]);
    const int nbTPS2K = sizeof(m_tpsCarriers2K) / sizeof(m_tpsCarriers2K[0]);
    const int repeats = (m_kMax + 1) / 1704;

    for (int r = 0; r < repeats; r++)
    {
        for (int i = 0; i < nbCPs2K - 1; i++) {
            m_continualPilots.push_back(m_continualPilots2K[i] + 1704 * r);
        }

        for (int i = 0; i < nbTPS2K; i++) {
            m_tpsCarriers.push_back(m_tpsCarriers2K[i] + 1704 * r);
        }
    }

    m_continualPilots.push_back(m_kMax);

    // Pilot reference sequence: x^11 + x^2 + 1, register initialised to all ones (4.5.2)
    m_prbs.resize(m_kMax + 1);
    unsigned int reg = 0x7ff;

    for (int k = 0; k <= m_kMax; k++)
    {
        m_prbs[k] = reg & 1;
        unsigned int bit = (reg ^ (reg >> 2)) & 1;
        reg = (reg >> 1) | (bit << 10);
    }

    // Carrier types for each of the four scattered pilot phases (4.5.3)
    for (int p = 0; p < 4; p++)
    {
        m_carrierType[p].assign(m_kMax + 1, 0);

        for (int k = 3 * p; k <= m_kMax; k += 12) {
            m_carrierType[p][k] = 1;
        }

        for (int k : m_continualPilots) {
            m_carrierType[p][k] = 1;
        }

        for (int k : m_tpsCarriers) {
            m_carrierType[p][k] = 2;
        }

        for (int k = 0; k <= m_kMax; k++)
        {
            if (m_carrierType[p][k] == 0) {
                m_dataCarriers[p].push_back(k);
            }
        }
    }

    // Symbol interleaver permutation H(q) (4.3.4.2)
    const int nr = mode == Mode::Mode2K ? 11 : 13;
    const int mMax = 1 << nr;
    static const int perm2K[] = {4, 3, 9, 6, 2, 8, 1, 5, 7, 0};               // R' bit j goes to R bit perm[j]
    static const int perm8K[] = {7, 1, 4, 2, 9, 6, 8, 10, 0, 3, 11, 5};
    const int *perm = mode == Mode::Mode2K ? perm2K : perm8K;
    unsigned int rp = 0; // R'_i

    m_symbolInterleaver.resize(m_nbData);

    for (int i = 0, q = 0; i < mMax; i++)
    {
        if (i < 2) {
            rp = 0;
        } else if (i == 2) {
            rp = 1;
        }
        else
        {
            unsigned int bit = mode == Mode::Mode2K
                ? ((rp >> 0) ^ (rp >> 3)) & 1
                : ((rp >> 0) ^ (rp >> 1) ^ (rp >> 4) ^ (rp >> 6)) & 1;
            rp = (rp >> 1) | (bit << (nr - 2));
        }

        unsigned int r = 0;

        for (int j = 0; j < nr - 1; j++) {
            r |= ((rp >> j) & 1) << perm[j];
        }

        int h = (i % 2) * (1 << (nr - 1)) + r;

        if (h < m_nbData) {
            m_symbolInterleaver[q++] = h;
        }
    }
}

} // namespace dvbt
