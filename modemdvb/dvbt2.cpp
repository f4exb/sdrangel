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
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <tuple>

#include "dvbt2.h"
#include "dvbt2tables.h"

namespace dvbt2 {

const char *toString(GuardInterval guard)
{
    static const char *names[] = {"1/32", "1/16", "1/8", "1/4", "1/128", "19/128", "19/256"};
    return names[(int) guard];
}

const char *toString(PilotPattern pattern)
{
    static const char *names[] = {"PP1", "PP2", "PP3", "PP4", "PP5", "PP6", "PP7", "PP8"};
    return names[(int) pattern];
}

const char *toString(Constellation constellation)
{
    static const char *names[] = {"QPSK", "16-QAM", "64-QAM", "256-QAM"};
    return names[(int) constellation];
}

const char *toString(L1Constellation constellation)
{
    static const char *names[] = {"BPSK", "QPSK", "16-QAM", "64-QAM"};
    return names[(int) constellation];
}

const char *toString(CodeRate rate)
{
    static const char *names[] = {"1/2", "3/5", "2/3", "3/4", "4/5", "5/6", "1/3", "2/5", "1/4", "8/9", "9/10",
        "13/45", "9/20", "11/20", "26/45", "28/45", "23/36", "25/36", "13/18", "7/9", "90/180", "96/180", "100/180",
        "104/180", "116/180", "124/180", "128/180", "132/180", "135/180", "140/180", "154/180", "18/30", "20/30",
        "22/30", "11/45", "4/15", "14/45", "7/15", "8/15", "32/45", "2/9", "1/5"};
    return ((int) rate >= 0) && ((int) rate < nbCodeRates) ? names[(int) rate] : "?";
}

const char *fftSizeString(int fftSize)
{
    switch (fftSize)
    {
    case 1024: return "1K";
    case 2048: return "2K";
    case 4096: return "4K";
    case 8192: return "8K";
    case 16384: return "16K";
    case 32768: return "32K";
    default: return "?";
    }
}

double sampleRate(int bandwidthHz)
{
    if (bandwidthHz == 1700000) {
        return 131e6 / 71.0;
    }

    return 8.0 * bandwidthHz / 7.0;
}

int guardSamples(int fftSize, GuardInterval guard)
{
    switch (guard)
    {
    case GuardInterval::G1_32: return fftSize / 32;
    case GuardInterval::G1_16: return fftSize / 16;
    case GuardInterval::G1_8: return fftSize / 8;
    case GuardInterval::G1_4: return fftSize / 4;
    case GuardInterval::G1_128: return fftSize / 128;
    case GuardInterval::G19_128: return 19 * fftSize / 128;
    case GuardInterval::G19_256: return 19 * fftSize / 256;
    }

    return 0;
}

bool guardAllowed(int fftSize, GuardInterval guard)
{
    bool newGuard = (guard == GuardInterval::G1_128) || (guard == GuardInterval::G19_128) || (guard == GuardInterval::G19_256);

    switch (fftSize)
    {
    case 32768: return guard != GuardInterval::G1_4;
    case 16384:
    case 8192: return true;
    case 4096:
    case 2048: return !newGuard;
    case 1024: return !newGuard && (guard != GuardInterval::G1_32);
    default: return false;
    }
}

int numP2(int fftSize)
{
    switch (fftSize)
    {
    case 1024: return 16;
    case 2048: return 8;
    case 4096: return 4;
    case 8192: return 2;
    default: return 1;
    }
}

int kMaxNormal(int fftSize)
{
    switch (fftSize)
    {
    case 1024: return 852;
    case 2048: return 1704;
    case 4096: return 3408;
    case 8192: return 6816;
    case 16384: return 13632;
    case 32768: return 27264;
    default: return 0;
    }
}

int kExt(int fftSize)
{
    switch (fftSize)
    {
    case 8192: return 48;
    case 16384: return 144;
    case 32768: return 288;
    default: return 0;
    }
}

int dx(PilotPattern pattern)
{
    static const int values[] = {3, 6, 6, 12, 12, 24, 24, 6};
    return values[(int) pattern];
}

int dy(PilotPattern pattern)
{
    static const int values[] = {4, 2, 4, 2, 4, 2, 4, 16};
    return values[(int) pattern];
}

// Table 64: used when time interpolation is needed, i.e. Dx Dy is at least the reciprocal of the guard interval
// fraction, except with PP8
bool frameClosingSymbol(int fftSize, GuardInterval guard, PilotPattern pattern, bool miso)
{
    if (pattern == PilotPattern::PP8) {
        return false;
    }

    if (miso) {
        return true;
    }

    return dx(pattern) * dy(pattern) * guardSamples(fftSize, guard) >= fftSize;
}

int bitsPerCell(Constellation constellation)
{
    return 2 * ((int) constellation + 1);
}

int bitsPerCell(L1Constellation constellation)
{
    static const int values[] = {1, 2, 4, 6};
    return values[(int) constellation];
}

int nldpc(FECType type)
{
    return type == FECType::Normal ? 64800 : type == FECType::Medium ? 32400 : 16200;
}

int kldpc(FECType type, CodeRate rate, CodeFamily family)
{
    const bool s2 = family == CodeFamily::S2;

    // The medium FECFRAME's (EN 302 307-2 table 5: VL-SNR's 1/5, 11/45 and 1/3), only S2's
    if (type == FECType::Medium) {
        return !s2 ? 0 : rate == CodeRate::R1_5 ? 6480 : rate == CodeRate::R11_45 ? 7920 : rate == CodeRate::R1_3 ? 10800 : 0;
    }

    // S2X's (EN 302 307-2 tables 4 and 6: Nbch), only in S2's family, each in the frame sizes it has
    if (isS2X(rate))
    {
        static const int kNormal[] = {18720, 29160, 35640, 37440, 40320, 41400, 45000, 46800, 50400, 32400, 34560, 36000,
            37440, 41760, 44640, 46080, 47520, 48600, 50400, 55440, 38880, 43200, 47520, 0, 0, 0, 0, 0, 0, 14400, 0};
        static const int kShort[] = {0, 0, 0, 9360, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3960, 4320, 5040, 7560, 8640, 11520, 0, 0};
        const int i = (int) rate - (int) CodeRate::R13_45;
        return !s2 ? 0 : (type == FECType::Normal ? kNormal : kShort)[i];
    }

    if (type == FECType::Normal)
    {
        static const int kT2[] = {32400, 38880, 43200, 48600, 51840, 54000, 0, 0, 0, 0, 0};
        static const int kS2[] = {32400, 38880, 43200, 48600, 51840, 54000, 21600, 25920, 16200, 57600, 58320};
        return (s2 ? kS2 : kT2)[(int) rate];
    }
    else
    {
        static const int kT2[] = {7200, 9720, 10800, 11880, 12600, 13320, 5400, 6480, 3240, 0, 0};
        static const int kS2[] = {7200, 9720, 10800, 11880, 12600, 13320, 5400, 6480, 3240, 14400, 0};
        return (s2 ? kS2 : kT2)[(int) rate];
    }
}

int bchT(FECType type, CodeRate rate, CodeFamily family)
{
    if (isS2X(rate) || (type == FECType::Medium)) {
        return 12; // Tables 4 to 6
    }

    if (type == FECType::Normal)
    {
        if ((family == CodeFamily::S2) && ((rate == CodeRate::R8_9) || (rate == CodeRate::R9_10))) {
            return 8;
        }

        if ((rate == CodeRate::R2_3) || (rate == CodeRate::R5_6)) {
            return 10;
        }
    }

    return 12;
}

int kbch(FECType type, CodeRate rate, CodeFamily family)
{
    // Each of the BCH code's t polynomials has the field's degree: 16, 15 (medium) or 14
    return kldpc(type, rate, family) - bchT(type, rate, family) * (type == FECType::Normal ? 16 : type == FECType::Medium ? 15 : 14);
}

int cellsPerFECBlock(FECType type, Constellation constellation)
{
    return nldpc(type) / bitsPerCell(constellation);
}

int fftSizeFromS2(int s2, bool& newGuards, bool lite)
{
    static const int sizes[] = {2048, 8192, 4096, 1024, 16384, 32768, 8192, 32768};
    static const int liteSizes[] = {2048, 8192, 4096, 16384, 16384, 0, 8192, 0}; // No 1K or 32K
    int field1 = (s2 >> 1) & 7;
    newGuards = lite ? (field1 == 3) || (field1 == 6) : field1 >= 6;
    return lite ? liteSizes[field1] : sizes[field1];
}

int P1Signalling::fftSize() const
{
    if (!isT2()) {
        return 0;
    }

    bool newGuards;
    return fftSizeFromS2(m_s2, newGuards, isT2Lite());
}

// Bit fields, MSB first, one bit per byte

uint32_t L1Pre::getBits(const uint8_t *bits, int& offset, int nb)
{
    uint32_t value = 0;

    for (int i = 0; i < nb; i++) {
        value = (value << 1) | (bits[offset + i] & 1);
    }

    offset += nb;
    return value;
}

void L1Pre::putBits(std::vector<uint8_t>& bits, uint32_t value, int nb)
{
    for (int i = nb - 1; i >= 0; i--) {
        bits.push_back((value >> i) & 1);
    }
}

uint32_t crc32(const uint8_t *bits, int nbBits)
{
    uint32_t crc = 0xffffffff;

    for (int i = 0; i < nbBits; i++)
    {
        uint32_t bit = ((crc >> 31) ^ bits[i]) & 1;
        crc = (crc << 1) ^ (bit ? 0x04c11db7 : 0);
    }

    return crc;
}

// A byte at a time, as every TS packet's is checked: the CRC of each byte value, from 0
uint8_t crc8(const uint8_t *bytes, int nbBytes)
{
    static const std::array<uint8_t, 256> table = []() {
        std::array<uint8_t, 256> t;

        for (int v = 0; v < 256; v++)
        {
            uint8_t crc = (uint8_t) v;

            for (int b = 0; b < 8; b++) {
                crc = (uint8_t) ((crc << 1) ^ (crc & 0x80 ? 0xd5 : 0));
            }

            t[v] = crc;
        }

        return t;
    }();
    uint8_t crc = 0;

    for (int i = 0; i < nbBytes; i++) {
        crc = table[crc ^ bytes[i]];
    }

    return crc;
}

void scramble(uint8_t *bits, int nbBits)
{
    unsigned int st = 0251; // 100101010000000, bit-reversed

    for (int i = 0; i < nbBits; i++)
    {
        unsigned int bit = ((st >> 13) ^ (st >> 14)) & 1;
        st = (st << 1) | bit;
        bits[i] ^= bit;
    }
}

void scrambleBytes(uint8_t *bytes, int nbBytes)
{
    static std::vector<uint8_t> sequence;
    static std::once_flag once;

    std::call_once(once, []() {
        const int maxBytes = 58192 / 8; // DVB-S2's largest BBFRAME (9/10), T2's 53840
        sequence.resize(maxBytes);
        unsigned int st = 0251;

        for (int i = 0; i < maxBytes; i++)
        {
            uint8_t out = 0;

            for (int n = 0; n < 8; n++)
            {
                unsigned int bit = ((st >> 13) ^ (st >> 14)) & 1;
                out = (uint8_t) ((out << 1) | bit);
                st = (st << 1) | bit;
            }

            sequence[i] = out;
        }
    });

    for (int i = 0; i < nbBytes; i++) {
        bytes[i] ^= sequence[i];
    }
}

bool L1Pre::decode(const uint8_t *bits)
{
    int o = 0;
    m_type = getBits(bits, o, 8);
    m_bwtExt = getBits(bits, o, 1);
    m_s1 = getBits(bits, o, 3);
    m_s2 = getBits(bits, o, 4);
    m_l1RepetitionFlag = getBits(bits, o, 1);
    int guard = getBits(bits, o, 3);
    m_guard = (GuardInterval) std::min(guard, 6);
    m_papr = getBits(bits, o, 4);
    int l1Mod = getBits(bits, o, 4);
    m_l1Mod = (L1Constellation) std::min(l1Mod, 3);
    m_l1Cod = getBits(bits, o, 2);
    m_l1FecType = getBits(bits, o, 2);
    m_l1PostSize = getBits(bits, o, 18);
    m_l1PostInfoSize = getBits(bits, o, 18);
    int pattern = getBits(bits, o, 4);
    m_pilotPattern = (PilotPattern) std::min(pattern, 7);
    m_txIdAvailability = getBits(bits, o, 8);
    m_cellId = getBits(bits, o, 16);
    m_networkId = getBits(bits, o, 16);
    m_t2SystemId = getBits(bits, o, 16);
    m_numT2Frames = getBits(bits, o, 8);
    m_numDataSymbols = getBits(bits, o, 12);
    m_regenFlag = getBits(bits, o, 3);
    m_l1PostExtension = getBits(bits, o, 1);
    m_numRf = getBits(bits, o, 3);
    m_currentRfIdx = getBits(bits, o, 3);
    m_t2Version = getBits(bits, o, 4);
    m_l1PostScrambled = getBits(bits, o, 1);
    m_t2BaseLite = getBits(bits, o, 1);
    o += 4; // Reserved
    uint32_t crc = getBits(bits, o, 32);

    if (m_t2Version < 2)
    {
        m_l1PostScrambled = false;
        m_t2BaseLite = false;
    }

    return (crc == crc32(bits, size - 32)) && (guard <= 6) && (l1Mod <= 3) && (pattern <= 7);
}

void L1Pre::encode(uint8_t *bits) const
{
    std::vector<uint8_t> v;
    putBits(v, m_type, 8);
    putBits(v, m_bwtExt, 1);
    putBits(v, m_s1, 3);
    putBits(v, m_s2, 4);
    putBits(v, m_l1RepetitionFlag, 1);
    putBits(v, (int) m_guard, 3);
    putBits(v, m_papr, 4);
    putBits(v, (int) m_l1Mod, 4);
    putBits(v, m_l1Cod, 2);
    putBits(v, m_l1FecType, 2);
    putBits(v, m_l1PostSize, 18);
    putBits(v, m_l1PostInfoSize, 18);
    putBits(v, (int) m_pilotPattern, 4);
    putBits(v, m_txIdAvailability, 8);
    putBits(v, m_cellId, 16);
    putBits(v, m_networkId, 16);
    putBits(v, m_t2SystemId, 16);
    putBits(v, m_numT2Frames, 8);
    putBits(v, m_numDataSymbols, 12);
    putBits(v, m_regenFlag, 3);
    putBits(v, m_l1PostExtension, 1);
    putBits(v, m_numRf, 3);
    putBits(v, m_currentRfIdx, 3);
    putBits(v, m_t2Version, 4);
    putBits(v, m_l1PostScrambled, 1);
    putBits(v, m_t2BaseLite, 1);
    putBits(v, 0, 4);
    putBits(v, crc32(v.data(), (int) v.size()), 32);
    std::copy(v.begin(), v.end(), bits);
}

int L1Pre::fftSize() const
{
    bool newGuards;
    return fftSizeFromS2(m_s2, newGuards, (m_s1 == (int) PreambleType::T2LiteSISO) || (m_s1 == (int) PreambleType::T2LiteMISO));
}

bool L1Post::decode(const uint8_t *bits, const L1Pre& pre)
{
    const int size = pre.m_l1PostInfoSize;
    uint32_t crc = 0;

    for (int i = 0; i < 32; i++) {
        crc = (crc << 1) | (bits[size + i] & 1);
    }

    if ((crc != crc32(bits, size)) || (size < 35)) {
        return false;
    }

    int o = 0;
    m_subSlicesPerFrame = L1Pre::getBits(bits, o, 15);
    int numPlp = L1Pre::getBits(bits, o, 8);
    m_numAux = L1Pre::getBits(bits, o, 4);
    m_auxConfigRfu = L1Pre::getBits(bits, o, 8);

    // The fields' length, from the numbers of loops, must fit: the bits beyond aren't there
    const int fields = 35 + 35 * pre.m_numRf + ((pre.m_s2 & 1) ? 34 : 0) + 89 * numPlp + 32 + 32 * m_numAux + 71 + 48 * numPlp;

    if (fields > size) {
        return false;
    }

    // Code rates, modulations and FEC types reserved for the profile (tables 32 to 34): such a PLP can't be decoded
    const bool lite = (pre.m_s1 == (int) PreambleType::T2LiteSISO) || (pre.m_s1 == (int) PreambleType::T2LiteMISO);
    m_frequencies.assign(pre.m_numRf, 0);

    for (int i = 0; i < pre.m_numRf; i++)
    {
        int index = L1Pre::getBits(bits, o, 3);
        uint32_t frequency = L1Pre::getBits(bits, o, 32);

        if (index < pre.m_numRf) {
            m_frequencies[index] = frequency;
        }
    }

    if (pre.m_s2 & 1)
    {
        m_fefType = L1Pre::getBits(bits, o, 4);
        m_fefLength = L1Pre::getBits(bits, o, 22);
        m_fefInterval = L1Pre::getBits(bits, o, 8);
    }

    m_plps.resize(numPlp);

    for (PLP& plp : m_plps)
    {
        plp.m_id = L1Pre::getBits(bits, o, 8);
        plp.m_type = L1Pre::getBits(bits, o, 3);
        plp.m_payloadType = L1Pre::getBits(bits, o, 5);
        plp.m_ffFlag = L1Pre::getBits(bits, o, 1);
        plp.m_firstRfIdx = L1Pre::getBits(bits, o, 3);
        plp.m_firstFrameIdx = L1Pre::getBits(bits, o, 8);
        plp.m_groupId = L1Pre::getBits(bits, o, 8);
        int cod = L1Pre::getBits(bits, o, 3);
        int mod = L1Pre::getBits(bits, o, 3);
        plp.m_rotation = L1Pre::getBits(bits, o, 1);
        int fec = L1Pre::getBits(bits, o, 2);
        plp.m_codeRate = (CodeRate) cod;
        plp.m_constellation = (Constellation) std::min(3, mod);
        plp.m_fecType = (FECType) std::min(1, fec);
        plp.m_valid = (mod <= 3) && (lite ? (fec == 0) && (cod != 4) && (cod != 5) : (fec <= 1) && (cod <= 5));
        plp.m_numBlocksMax = L1Pre::getBits(bits, o, 10);
        plp.m_frameInterval = L1Pre::getBits(bits, o, 8);
        plp.m_timeIlLength = L1Pre::getBits(bits, o, 8);
        plp.m_timeIlType = L1Pre::getBits(bits, o, 1);
        plp.m_inBandA = L1Pre::getBits(bits, o, 1);
        plp.m_inBandB = L1Pre::getBits(bits, o, 1);
        plp.m_reserved1 = L1Pre::getBits(bits, o, 11);
        plp.m_plpMode = L1Pre::getBits(bits, o, 2);
        plp.m_staticFlag = L1Pre::getBits(bits, o, 1);
        plp.m_staticPaddingFlag = L1Pre::getBits(bits, o, 1);
    }

    m_fefLengthMsb = L1Pre::getBits(bits, o, 2);
    o += 30; // RESERVED_2
    o += 32 * m_numAux; // AUX_STREAM_TYPE, AUX_PRIVATE_CONF

    // Dynamic, for the current frame
    m_frameIdx = L1Pre::getBits(bits, o, 8);
    m_subSliceInterval = L1Pre::getBits(bits, o, 22);
    m_type2Start = L1Pre::getBits(bits, o, 22);
    m_l1ChangeCounter = L1Pre::getBits(bits, o, 8);
    m_startRfIdx = L1Pre::getBits(bits, o, 3);
    o += 8; // RESERVED_1

    for (PLP& plp : m_plps)
    {
        int id = L1Pre::getBits(bits, o, 8);
        int start = L1Pre::getBits(bits, o, 22);
        int numBlocks = L1Pre::getBits(bits, o, 10);
        o += 8; // RESERVED_2

        // The loop is in the same order as the configurable one
        if (id == plp.m_id)
        {
            plp.m_start = start;
            plp.m_numBlocks = numBlocks;
        }
    }

    return o <= size;
}

void L1Post::encode(std::vector<uint8_t>& bits, L1Pre& pre) const
{
    bits.clear();
    L1Pre::putBits(bits, m_subSlicesPerFrame, 15);
    L1Pre::putBits(bits, (uint32_t) m_plps.size(), 8);
    L1Pre::putBits(bits, m_numAux, 4);
    L1Pre::putBits(bits, m_auxConfigRfu, 8);

    for (int i = 0; i < pre.m_numRf; i++)
    {
        L1Pre::putBits(bits, i, 3);
        L1Pre::putBits(bits, i < (int) m_frequencies.size() ? m_frequencies[i] : 0, 32);
    }

    if (pre.m_s2 & 1)
    {
        L1Pre::putBits(bits, m_fefType, 4);
        L1Pre::putBits(bits, m_fefLength, 22);
        L1Pre::putBits(bits, m_fefInterval, 8);
    }

    for (const PLP& plp : m_plps)
    {
        L1Pre::putBits(bits, plp.m_id, 8);
        L1Pre::putBits(bits, plp.m_type, 3);
        L1Pre::putBits(bits, plp.m_payloadType, 5);
        L1Pre::putBits(bits, plp.m_ffFlag, 1);
        L1Pre::putBits(bits, plp.m_firstRfIdx, 3);
        L1Pre::putBits(bits, plp.m_firstFrameIdx, 8);
        L1Pre::putBits(bits, plp.m_groupId, 8);
        L1Pre::putBits(bits, (int) plp.m_codeRate, 3);
        L1Pre::putBits(bits, (int) plp.m_constellation, 3);
        L1Pre::putBits(bits, plp.m_rotation, 1);
        L1Pre::putBits(bits, (int) plp.m_fecType, 2);
        L1Pre::putBits(bits, plp.m_numBlocksMax, 10);
        L1Pre::putBits(bits, plp.m_frameInterval, 8);
        L1Pre::putBits(bits, plp.m_timeIlLength, 8);
        L1Pre::putBits(bits, plp.m_timeIlType, 1);
        L1Pre::putBits(bits, plp.m_inBandA, 1);
        L1Pre::putBits(bits, plp.m_inBandB, 1);
        L1Pre::putBits(bits, plp.m_reserved1, 11);
        L1Pre::putBits(bits, plp.m_plpMode, 2);
        L1Pre::putBits(bits, plp.m_staticFlag, 1);
        L1Pre::putBits(bits, plp.m_staticPaddingFlag, 1);
    }

    L1Pre::putBits(bits, m_fefLengthMsb, 2);
    L1Pre::putBits(bits, 0, 30);

    for (int i = 0; i < m_numAux; i++) {
        L1Pre::putBits(bits, 0, 32);
    }

    L1Pre::putBits(bits, m_frameIdx, 8);
    L1Pre::putBits(bits, m_subSliceInterval, 22);
    L1Pre::putBits(bits, m_type2Start, 22);
    L1Pre::putBits(bits, m_l1ChangeCounter, 8);
    L1Pre::putBits(bits, m_startRfIdx, 3);
    L1Pre::putBits(bits, 0, 8);

    for (const PLP& plp : m_plps)
    {
        L1Pre::putBits(bits, plp.m_id, 8);
        L1Pre::putBits(bits, plp.m_start, 22);
        L1Pre::putBits(bits, plp.m_numBlocks, 10);
        L1Pre::putBits(bits, 0, 8);
    }

    L1Pre::putBits(bits, 0, 8); // RESERVED_3

    for (int i = 0; i < m_numAux; i++) {
        L1Pre::putBits(bits, 0, 48);
    }

    pre.m_l1PostInfoSize = (int) bits.size();
    uint32_t crc = crc32(bits.data(), (int) bits.size());
    L1Pre::putBits(bits, crc, 32);
}

int framePN(int l)
{
    if ((l < 0) || (l >= framePNLength)) {
        return 0;
    }

    int digit = l / 4;
    char c = tables::frameLevelPN[digit];
    int value = c <= '9' ? c - '0' : c - 'A' + 10;
    return (value >> (3 - (l % 4))) & 1;
}

int Tables::fftIndex(int fftSize)
{
    switch (fftSize)
    {
    case 1024: return 0;
    case 2048: return 1;
    case 4096: return 2;
    case 8192: return 3;
    case 16384: return 4;
    default: return 5;
    }
}

// Table 50: active cells in the frame closing symbol, without tone reservation
int Tables::frameClosingActiveCells(int fftSize, bool extended, PilotPattern pattern)
{
    static const int normal[6][8] = {
        {402, 654, 490, 707, 544, 0, 0, 0},
        {804, 1309, 980, 1415, 1088, 0, 1396, 0},
        {1609, 2619, 1961, 2831, 2177, 0, 2792, 0},
        {3218, 5238, 3922, 5662, 4354, 0, 5585, 0},
        {6437, 10476, 7845, 11324, 8709, 11801, 11170, 0},
        {0, 20952, 0, 22649, 0, 23603, 0, 0},
    };
    static const int ext[6][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {3264, 5312, 3978, 5742, 4416, 0, 5664, 0},
        {6573, 10697, 8011, 11563, 8893, 12051, 11406, 0},
        {0, 21395, 0, 23127, 0, 24102, 0, 0},
    };
    return (extended ? ext : normal)[fftIndex(fftSize)][(int) pattern];
}

const Tables& Tables::get(int fftSize, bool extended, PilotPattern pattern, bool toneReservation, bool miso)
{
    static std::mutex mutex;
    static std::map<std::tuple<int, bool, int, bool, bool>, Tables*> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto key = std::make_tuple(fftSize, extended, (int) pattern, toneReservation, miso);
    auto it = cache.find(key);

    if (it != cache.end()) {
        return *it->second;
    }

    Tables *tables = new Tables(fftSize, extended, pattern, toneReservation, miso);
    cache[key] = tables;
    return *tables;
}

Tables::Tables(int fftSize, bool extended, PilotPattern pattern, bool toneReservation, bool miso) :
    m_fftSize(fftSize),
    m_extended(extended && (kExt(fftSize) > 0)),
    m_pattern(pattern),
    m_toneReservation(toneReservation),
    m_miso(miso),
    m_kExt(m_extended ? kExt(fftSize) : 0),
    m_dx(dx(pattern)),
    m_dy(dy(pattern))
{
    m_kMax = kMaxNormal(fftSize) + 2 * m_kExt;
    const int nbCarriers = m_kMax + 1;
    const int fi = fftIndex(fftSize);

    // Symbol level PRBS x^11 + x^2 + 1, from all ones. Its first bit is on the first carrier of extended mode
    const int offset = kExt(fftSize) - m_kExt;
    std::vector<uint8_t> prbs(nbCarriers + offset);
    unsigned int reg = 0x7ff;

    for (size_t k = 0; k < prbs.size(); k++)
    {
        prbs[k] = reg & 1;
        unsigned int bit = (reg ^ (reg >> 2)) & 1;
        reg = (reg >> 1) | (bit << 10);
    }

    m_w.assign(prbs.begin() + offset, prbs.end());

    // P2 symbols (9.2.6): pilots every 6 carriers in 32K SISO, else every 3, plus the extended carriers, and in MISO
    // the two carriers next to each edge of the normal carriers (9.2.8)
    const bool siso32K = (fftSize == 32768) && !miso;
    const int p2Spacing = siso32K ? 6 : 3;
    const float p2Amplitude = std::sqrt(siso32K ? 37.0f : 31.0f) / 5.0f;
    std::vector<uint8_t> p2Reserved(nbCarriers, 0);

    for (int i = 0; i < tables::p2ReservedCarriersIndex[fi][1]; i++) {
        p2Reserved[tables::p2ReservedCarriers[tables::p2ReservedCarriersIndex[fi][0] + i] + m_kExt] = 1;
    }

    for (int k = 0; k < nbCarriers; k++)
    {
        bool pilot = (k % p2Spacing == 0) || (k < m_kExt) || (k > m_kMax - m_kExt);

        // And next to the reserved carriers, unless reserved too
        if (miso && ((k == m_kExt + 1) || (k == m_kExt + 2) || (k == m_kMax - m_kExt - 2) || (k == m_kMax - m_kExt - 1)
            || (!p2Reserved[k] && (((k > 0) && p2Reserved[k - 1]) || ((k < m_kMax) && p2Reserved[k + 1])))))
        {
            pilot = true;
        }

        if (pilot) {
            m_p2Pilots.push_back({k, p2Amplitude});
        } else if (!p2Reserved[k]) {
            m_p2Data.push_back(k);
        }
    }

    // Continual pilots (9.2.4): from the 32K indices, modulo Kmod in the smaller modes, plus extended carrier mode's
    // 1K has group 1 (CP1), 2K groups 1 and 2, and so on, up to 32K's six
    static const int kMod[] = {1632, 1632, 3264, 6528, 13056, 0};
    const int nbGroups = fi + 1;
    std::vector<uint8_t> continual(nbCarriers, 0);

    for (int g = 0; g < nbGroups; g++)
    {
        const int *index = tables::continualPilotIndex[g][(int) pattern];

        for (int i = 0; i < index[1]; i++)
        {
            int k = tables::continualPilots[index[0] + i];
            k = kMod[fi] ? k % kMod[fi] : k;

            if (k <= m_kMax) {
                continual[k] = 1;
            }
        }
    }

    if (m_extended)
    {
        const int *index = tables::extendedContinualPilotIndex[fi - 3][(int) pattern];

        for (int i = 0; i < index[1]; i++) {
            continual[tables::extendedContinualPilots[index[0] + i]] = 1;
        }
    }

    for (int k = 0; k < nbCarriers; k++)
    {
        if (continual[k]) {
            m_continual.push_back(k);
        }
    }

    // Normal symbols (9.2.3 to 9.2.5, 9.3)
    static const float spAmplitudes[] = {4.0f / 3.0f, 4.0f / 3.0f, 7.0f / 4.0f, 7.0f / 4.0f, 7.0f / 3.0f, 7.0f / 3.0f, 7.0f / 3.0f, 7.0f / 3.0f};
    static const float cpAmplitudes[] = {4.0f / 3.0f, 4.0f / 3.0f, 4.0f * std::sqrt(2.0f) / 3.0f, 8.0f / 3.0f, 8.0f / 3.0f, 8.0f / 3.0f};
    const float spAmplitude = spAmplitudes[(int) pattern];
    const float cpAmplitude = cpAmplitudes[fi];
    const int period = m_dx * m_dy;

    for (int phase = 0; phase < m_dy; phase++)
    {
        std::vector<uint8_t> reserved(nbCarriers, 0);

        // Reserved carriers move with the scattered pilots, which in extended mode are offset by Kext
        if (toneReservation)
        {
            for (int i = 0; i < tables::reservedCarriersIndex[fi][1]; i++)
            {
                int k = tables::reservedCarriers[tables::reservedCarriersIndex[fi][0] + i] + m_dx * ((phase + m_kExt / m_dx) % m_dy);

                if (k <= m_kMax) {
                    reserved[k] = 1;
                }
            }
        }

        for (int k = 0; k < nbCarriers; k++)
        {
            bool scattered = (((k - m_kExt) % period + period) % period) == m_dx * phase;

            if (scattered || (k == 0) || (k == m_kMax)) {
                m_pilots[phase].push_back({k, spAmplitude});
            } else if (continual[k]) {
                m_pilots[phase].push_back({k, cpAmplitude});
            } else if (!reserved[k]) {
                m_data[phase].push_back(k);
            }
        }

        // For frequency interpolation from this symbol's pilots alone
        const int spOffset = (m_kExt + m_dx * phase) % period;

        for (const Pilot& pilot : m_pilots[phase])
        {
            if ((pilot.m_k - spOffset) % period == 0) {
                m_scattered[phase].push_back(pilot);
            }
        }
    }

    // Frame closing symbol (9.2.7): pilots every Dx carriers, and the edges. Tone reservation as for P2
    for (int k = 0; k < nbCarriers; k++)
    {
        bool extra = (k == m_kMax - 1) && (((fftSize == 1024) && ((pattern == PilotPattern::PP4) || (pattern == PilotPattern::PP5)))
            || ((fftSize == 2048) && (pattern == PilotPattern::PP7)));

        if ((k % m_dx == 0) || (k == m_kMax) || extra) {
            m_fcPilots.push_back({k, spAmplitude});
        } else if (!(toneReservation && p2Reserved[k])) {
            m_fcData.push_back(k);
        }
    }

    m_cfc = frameClosingActiveCells(fftSize, m_extended, pattern);

    if (toneReservation) {
        m_cfc -= tables::reservedCarriersIndex[fi][1];
    }

    interleaver((int) m_p2Data.size(), m_p2Read);
    interleaver((int) m_data[0].size(), m_dataRead);
    interleaver((int) m_fcData.size(), m_fcRead);
}

// Frequency interleaver (8.5). read[parity][p] is the index of the data carrier holding cell p
void Tables::interleaver(int ndata, std::vector<int> read[2]) const
{
    // Bit permutations of tables 53, R' bit j to R bit perm[j]
    static const int perm1K[2][9] = {{8, 7, 6, 5, 0, 1, 2, 3, 4}, {6, 8, 7, 4, 1, 0, 5, 2, 3}};
    static const int perm2K[2][10] = {{4, 3, 9, 6, 2, 8, 1, 5, 7, 0}, {6, 9, 4, 8, 5, 1, 0, 7, 2, 3}};
    static const int perm4K[2][11] = {{6, 3, 0, 9, 4, 2, 1, 8, 5, 10, 7}, {5, 9, 1, 4, 3, 0, 8, 10, 7, 2, 6}};
    static const int perm8K[2][12] = {{7, 1, 4, 2, 9, 6, 8, 10, 0, 3, 11, 5}, {11, 4, 9, 3, 1, 2, 5, 0, 6, 7, 10, 8}};
    static const int perm16K[2][13] = {{9, 7, 6, 10, 12, 5, 1, 11, 0, 2, 3, 4, 8}, {6, 8, 10, 12, 2, 0, 4, 1, 11, 3, 5, 9, 7}};
    static const int perm32K[14] = {7, 13, 3, 4, 9, 2, 12, 11, 1, 8, 10, 0, 5, 6};

    const int nr = (int) std::lround(std::log2((double) m_fftSize));
    const int mMax = m_fftSize;
    const int fi = fftIndex(m_fftSize);

    for (int parity = 0; parity < 2; parity++)
    {
        const int *perm;

        switch (fi)
        {
        case 0: perm = perm1K[parity]; break;
        case 1: perm = perm2K[parity]; break;
        case 2: perm = perm4K[parity]; break;
        case 3: perm = perm8K[parity]; break;
        case 4: perm = perm16K[parity]; break;
        default: perm = perm32K; break;
        }

        std::vector<int> h(ndata);
        unsigned int rp = 0;

        for (int i = 0, p = 0; (i < mMax) && (p < ndata); i++)
        {
            if (i < 2) {
                rp = 0;
            } else if (i == 2) {
                rp = 1;
            }
            else
            {
                unsigned int bit;

                switch (fi)
                {
                case 0: bit = (rp ^ (rp >> 4)) & 1; break;
                case 1: bit = (rp ^ (rp >> 3)) & 1; break;
                case 2: bit = (rp ^ (rp >> 2)) & 1; break;
                case 3: bit = (rp ^ (rp >> 1) ^ (rp >> 4) ^ (rp >> 6)) & 1; break;
                case 4: bit = (rp ^ (rp >> 1) ^ (rp >> 4) ^ (rp >> 5) ^ (rp >> 9) ^ (rp >> 11)) & 1; break;
                default: bit = (rp ^ (rp >> 1) ^ (rp >> 2) ^ (rp >> 12)) & 1; break;
                }

                rp = (rp >> 1) | (bit << (nr - 2));
            }

            unsigned int r = 0;

            for (int j = 0; j < nr - 1; j++) {
                r |= ((rp >> j) & 1) << perm[j];
            }

            int value = (i % 2) * (1 << (nr - 1)) + (int) r;

            if (value < ndata) {
                h[p++] = value;
            }
        }

        // 1K-16K: a[p] = x[H(p)], with H0 on even and H1 on odd symbols.
        // 32K: a[H(p)] = x[p] on even symbols and a[p] = x[H(p)] on odd
        read[parity].resize(ndata);

        for (int p = 0; p < ndata; p++)
        {
            if ((fi == 5) && (parity == 0)) {
                read[parity][p] = h[p];
            } else {
                read[parity][h[p]] = p;
            }
        }
    }
}

const P1Tables& P1Tables::get()
{
    static P1Tables tables;
    return tables;
}

P1Tables::P1Tables()
{
    std::copy(tables::p1Carriers, tables::p1Carriers + 384, m_carriers);

    auto hexBits = [](const char *hex, uint8_t *bits) {
        for (int i = 0; hex[i]; i++)
        {
            int value = hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'A' + 10;

            for (int b = 0; b < 4; b++) {
                bits[4 * i + b] = (value >> (3 - b)) & 1;
            }
        }
    };

    for (int i = 0; i < 8; i++) {
        hexBits(tables::p1S1[i], m_s1[i]);
    }

    for (int i = 0; i < 16; i++) {
        hexBits(tables::p1S2[i], m_s2[i]);
    }

    // PRBS of 5.2.4 from 100111001000110
    unsigned int st = 0x3139;

    for (int i = 0; i < 384; i++)
    {
        unsigned int bit = ((st >> 13) ^ (st >> 14)) & 1;
        st = (st << 1) | bit;
        m_scrambling[i] = bit;
    }
}

// 9.8.2.2: CSS_S1, CSS_S2, CSS_S1, DBPSK from +1, then scrambled
void P1Tables::modulation(int s1, int s2, float *values) const
{
    uint8_t sequence[384];
    std::copy(m_s1[s1], m_s1[s1] + 64, sequence);
    std::copy(m_s2[s2], m_s2[s2] + 256, sequence + 64);
    std::copy(m_s1[s1], m_s1[s1] + 64, sequence + 320);
    float diff = 1.0f;

    for (int i = 0; i < 384; i++)
    {
        diff = sequence[i] ? -diff : diff;
        values[i] = m_scrambling[i] ? -diff : diff;
    }
}

} // namespace dvbt2
