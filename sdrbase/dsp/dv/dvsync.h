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

#ifndef INCLUDE_DSP_DV_DVSYNC_H
#define INCLUDE_DSP_DV_DVSYNC_H

#include <cstdint>

#include "export.h"

/**
 * Synchronisation patterns of the digital voice protocols as symbol levels, and
 * the dibit to level mapping they share.
 *
 * Levels are in units of a third of the peak deviation: +3, +1, -1, -3 for 4FSK
 * (dibit 01, 00, 10, 11: the ETSI and TIA C4FM convention) and +3, -3 for
 * D-STAR GMSK (bit 0 positive). Patterns are given for the non inverted
 * signal; an inverted one has all levels negated.
 */
class SDRBASE_API DVSync
{
public:
    enum Pattern
    {
        DMRDataBS,      //!< DF F5 7D 75 DF 5D
        DMRVoiceBS,     //!< 75 5F D7 DF 75 F7
        DMRDataMS,      //!< D5 D7 F7 7F D7 57
        DMRVoiceMS,     //!< 7F 7D 5D D5 7D FD
        DMRDataTS1,     //!< F7 FD D5 DD FD 55 (direct mode time slot 1)
        DMRVoiceTS1,    //!< 5D 57 7F 77 57 FF
        DMRDataTS2,     //!< D7 55 7F 5F F7 F5 (direct mode time slot 2)
        DMRVoiceTS2,    //!< 7D FF D5 F5 5D 5F
        DMRRCMS,        //!< 77 D5 5F 7D FD 77 (reverse channel)
        DStarHeader,    //!< Last 24 symbols of the header sync: bit sync tail and 15 bit frame sync
        DStarVoice,     //!< 24 bit voice frame sync (slow data field of the first frame of a superframe)
        DStarEnd,       //!< 48 symbol end of transmission (32 bit sync bits then the end pattern)
        YSF,            //!< D4 71 C9 63 4D
        NXDNFSW,        //!< 0xCDF59 (10 symbols)
        NXDNPreambleFSW,//!< Last 9 preamble symbols followed by the FSW (19 symbols)
        DPMRFS1,        //!< 57 FF 5F 75 D5 77 (header)
        DPMRFS2,        //!< 5F F7 7D (superframe)
        DPMRFS3,        //!< 7D DF F5 (end)
        DPMRFS4,        //!< FD 55 F5 DF 7F DD (packet data header)
        P25,            //!< 55 75 F5 FF 77 FF
        P25P2,          //!< 57 5D 57 F7 FF (Phase 2 ISCH sync word, 20 symbols at 6000 symbols/s)
        M17LSF,         //!< 55 F7 link setup (8 symbols at 4800 symbols/s)
        M17Stream,      //!< FF 5D stream frame
        M17Packet,      //!< 75 FF packet frame
        M17EOT,         //!< 55 5D end of transmission marker
        NbPatterns
    };

    struct Info
    {
        Pattern m_pattern;
        const char *m_name;
        int m_length;          //!< Symbols
        const int8_t *m_levels;
        int m_symbolRate;      //!< 4800, 2400 or 6000
        bool m_fourLevel;      //!< 4FSK (false: GMSK)
    };

    static const int m_maxLength = 48;

    static const Info& get(Pattern pattern);

    //! Dibit to 4FSK level: 01 -> +3, 00 -> +1, 10 -> -1, 11 -> -3
    static int8_t dibitToLevel(uint8_t dibit);
    //! 4FSK level to dibit
    static uint8_t levelToDibit(int8_t level);
    //! Bits (MSB of each pair first) to 4FSK levels
    static void bitsToLevels(const uint8_t *bits, int nbBits, int8_t *levels);
    //! The low nbBits of a hex value (MSB first) to 4FSK levels
    static void valueToLevels(uint64_t value, int nbBits, int8_t *levels);
    //! D-STAR bits to GMSK levels: bit 0 -> +3, bit 1 -> -3
    static void gmskBitsToLevels(const uint8_t *bits, int nbBits, int8_t *levels);

private:
    static const Info m_patterns[NbPatterns];
};

#endif // INCLUDE_DSP_DV_DVSYNC_H
