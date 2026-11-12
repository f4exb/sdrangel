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

#include "dvsync.h"

// Sync words: ETSI TS 102 361-1 table 9.2 (DMR), Icom D-STAR specification, Yaesu
// System Fusion, NXDN TS 1-A 4.5.1, ETSI TS 102 490 clause 8 (dPMR), TIA-102.BAAA (P25), TIA-102.BBAC (P25 Phase 2).
// Each hex value is converted to levels at first use.

namespace
{
    struct Levels
    {
        int8_t m_levels[DVSync::m_maxLength];

        Levels(uint64_t value, int nbBits)
        {
            DVSync::valueToLevels(value, nbBits, m_levels);
        }

        Levels(const int8_t *levels, int n)
        {
            for (int i = 0; i < n; i++) {
                m_levels[i] = levels[i];
            }
        }
    };

    // D-STAR patterns (bit 0 = +3): the last 24 symbols of the header sync are the end of the
    // 1010... bit sync followed by the 15 bit frame sync 111011001010000
    const int8_t dstarHeader[24] = { 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, -3, -3, 3, -3, -3, 3, 3, -3, 3, -3, 3, 3, 3, 3 };
    // Voice frame sync: 101010101110010010000... as levels
    const int8_t dstarVoice[24] = { -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, -3, 3, -3, 3, 3, 3, -3, -3, 3, -3, 3, 3, 3 };
    // End of transmission: 32 bit sync symbols then the 16 bit end pattern
    const int8_t dstarEnd[48] = {
        -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3,
        -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3, -3, 3,
        3, 3, 3, -3, 3, 3, -3, -3, 3, -3, 3, -3, -3, -3, -3, 3
    };
    // NXDN preamble tail (last 9 symbols of 0x57 0x75 0xFD repeated) then the FSW 0xCDF59
    const int8_t nxdnPreambleFsw[19] = { -3, 3, -3, 3, 3, -3, -3, -3, 3, -3, 1, -3, 3, -3, -3, 3, 3, -1, 3 };

    const Levels dmrDataBS(0xDFF57D75DF5DULL, 48);
    const Levels dmrVoiceBS(0x755FD7DF75F7ULL, 48);
    const Levels dmrDataMS(0xD5D7F77FD757ULL, 48);
    const Levels dmrVoiceMS(0x7F7D5DD57DFDULL, 48);
    const Levels dmrDataTS1(0xF7FDD5DDFD55ULL, 48);
    const Levels dmrVoiceTS1(0x5D577F7757FFULL, 48);
    const Levels dmrDataTS2(0xD7557F5FF7F5ULL, 48);
    const Levels dmrVoiceTS2(0x7DFFD5F55D5FULL, 48);
    const Levels dmrRCMS(0x77D55F7DFD77ULL, 48);
    const Levels ysf(0xD471C9634DULL, 40);
    const Levels nxdnFsw(0xCDF59ULL, 20);
    const Levels dpmrFs1(0x57FF5F75D577ULL, 48);
    const Levels dpmrFs2(0x5FF77DULL, 24);
    const Levels dpmrFs3(0x7DDFF5ULL, 24);
    const Levels dpmrFs4(0xFD55F5DF7FDDULL, 48);
    const Levels p25(0x5575F5FF77FFULL, 48);
    const Levels p25p2(0x575D57F7FFULL, 40);
    const Levels m17Lsf(0x55F7ULL, 16);
    const Levels m17Stream(0xFF5DULL, 16);
    const Levels m17Packet(0x75FFULL, 16);
    const Levels m17Eot(0x555DULL, 16);
}

const DVSync::Info DVSync::m_patterns[DVSync::NbPatterns] = {
    { DMRDataBS,       "DMR BS data",        24, dmrDataBS.m_levels,   4800, true },
    { DMRVoiceBS,      "DMR BS voice",       24, dmrVoiceBS.m_levels,  4800, true },
    { DMRDataMS,       "DMR MS data",        24, dmrDataMS.m_levels,   4800, true },
    { DMRVoiceMS,      "DMR MS voice",       24, dmrVoiceMS.m_levels,  4800, true },
    { DMRDataTS1,      "DMR TS1 data",       24, dmrDataTS1.m_levels,  4800, true },
    { DMRVoiceTS1,     "DMR TS1 voice",      24, dmrVoiceTS1.m_levels, 4800, true },
    { DMRDataTS2,      "DMR TS2 data",       24, dmrDataTS2.m_levels,  4800, true },
    { DMRVoiceTS2,     "DMR TS2 voice",      24, dmrVoiceTS2.m_levels, 4800, true },
    { DMRRCMS,         "DMR MS RC",          24, dmrRCMS.m_levels,     4800, true },
    { DStarHeader,     "D-STAR header",      24, dstarHeader,          4800, false },
    { DStarVoice,      "D-STAR voice",       24, dstarVoice,           4800, false },
    { DStarEnd,        "D-STAR end",         48, dstarEnd,             4800, false },
    { YSF,             "YSF",                20, ysf.m_levels,         4800, true },
    { NXDNFSW,         "NXDN FSW",           10, nxdnFsw.m_levels,     2400, true },
    { NXDNPreambleFSW, "NXDN preamble FSW",  19, nxdnPreambleFsw,      2400, true },
    { DPMRFS1,         "dPMR FS1",           24, dpmrFs1.m_levels,     2400, true },
    { DPMRFS2,         "dPMR FS2",           12, dpmrFs2.m_levels,     2400, true },
    { DPMRFS3,         "dPMR FS3",           12, dpmrFs3.m_levels,     2400, true },
    { DPMRFS4,         "dPMR FS4",           24, dpmrFs4.m_levels,     2400, true },
    { P25,             "P25",                24, p25.m_levels,         4800, true },
    { P25P2,           "P25 Phase 2",        20, p25p2.m_levels,       6000, true },
    { M17LSF,          "M17 link setup",      8, m17Lsf.m_levels,      4800, true },
    { M17Stream,       "M17 stream",          8, m17Stream.m_levels,   4800, true },
    { M17Packet,       "M17 packet",          8, m17Packet.m_levels,   4800, true },
    { M17EOT,          "M17 end",             8, m17Eot.m_levels,      4800, true },
};

const DVSync::Info& DVSync::get(Pattern pattern)
{
    return m_patterns[(int) pattern];
}

int8_t DVSync::dibitToLevel(uint8_t dibit)
{
    static const int8_t levels[4] = { 1, 3, -1, -3 };
    return levels[dibit & 3];
}

uint8_t DVSync::levelToDibit(int8_t level)
{
    if (level >= 3) {
        return 1;
    } else if (level >= 1) {
        return 0;
    } else if (level >= -1) {
        return 2;
    } else {
        return 3;
    }
}

void DVSync::bitsToLevels(const uint8_t *bits, int nbBits, int8_t *levels)
{
    for (int i = 0; i + 1 < nbBits; i += 2) {
        levels[i / 2] = dibitToLevel((uint8_t) (((bits[i] & 1) << 1) | (bits[i + 1] & 1)));
    }
}

void DVSync::valueToLevels(uint64_t value, int nbBits, int8_t *levels)
{
    for (int i = 0; i < nbBits / 2; i++) {
        levels[i] = dibitToLevel((uint8_t) ((value >> (nbBits - 2 - 2 * i)) & 3));
    }
}

void DVSync::gmskBitsToLevels(const uint8_t *bits, int nbBits, int8_t *levels)
{
    for (int i = 0; i < nbBits; i++) {
        levels[i] = (bits[i] & 1) ? -3 : 3;
    }
}
