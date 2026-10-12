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

#ifndef MODEMDVB_DVBT2LDPCTABLES_H
#define MODEMDVB_DVBT2LDPCTABLES_H

#include <cstdint>

#include "dvbt2fec.h"

// Internal to modemdvb: the LDPC codes' tables of parity bit accumulator addresses, for ldpcTable() (T2's,
// dvbt2ldpctables.cpp), ldpcTableS2() (dvbs2ldpctables.cpp) and ldpcTableS2X() (dvbs2xldpctables.cpp)

namespace dvbt2 {

class LDPCTables
{
private:
    friend MODEMDVB_API const LDPCTable *ldpcTable(FECType type, CodeRate rate, CodeFamily family);
    friend MODEMDVB_API const LDPCTable *ldpcTableS2(FECType type, CodeRate rate);
    friend MODEMDVB_API const LDPCTable *ldpcTableS2X(FECType type, CodeRate rate);

    // EN 302 755 annex A (Nldpc = 64 800) and annex B (Nldpc = 16 200)
    static const uint16_t m_tableA1[];
    static const uint16_t m_tableA2[];
    static const uint16_t m_tableA3[];
    static const uint16_t m_tableA4[];
    static const uint16_t m_tableA5[];
    static const uint16_t m_tableA6[];
    static const uint16_t m_tableB1[];
    static const uint16_t m_tableB2[];
    static const uint16_t m_tableB3[];
    static const uint16_t m_tableB4[];
    static const uint16_t m_tableB5[];
    static const uint16_t m_tableB6[];
    static const uint16_t m_tableB7[];
    static const uint16_t m_tableB8[];
    static const uint16_t m_tableB9[];
    // EN 302 307 annexes B and C: S2's not shared with T2
    static const uint16_t m_s2TableB1[];
    static const uint16_t m_s2TableB2[];
    static const uint16_t m_s2TableB3[];
    static const uint16_t m_s2TableB6[];
    static const uint16_t m_s2TableB10[];
    static const uint16_t m_s2TableB11[];
    static const uint16_t m_s2TableC5[];
    static const uint16_t m_s2TableC10[];
    // EN 302 307-2 annexes B and C: S2X's
    static const uint16_t m_s2xTableB2[];
    static const uint16_t m_s2xTableB3[];
    static const uint16_t m_s2xTableB4[];
    static const uint16_t m_s2xTableB5[];
    static const uint16_t m_s2xTableB6[];
    static const uint16_t m_s2xTableB7[];
    static const uint16_t m_s2xTableB8[];
    static const uint16_t m_s2xTableB9[];
    static const uint16_t m_s2xTableB10[];
    static const uint16_t m_s2xTableB11[];
    static const uint16_t m_s2xTableB12[];
    static const uint16_t m_s2xTableB13[];
    static const uint16_t m_s2xTableB14[];
    static const uint16_t m_s2xTableB15[];
    static const uint16_t m_s2xTableB16[];
    static const uint16_t m_s2xTableB17[];
    static const uint16_t m_s2xTableB18[];
    static const uint16_t m_s2xTableB19[];
    static const uint16_t m_s2xTableB20[];
    static const uint16_t m_s2xTableB21[];
    static const uint16_t m_s2xTableB22[];
    static const uint16_t m_s2xTableB23[];
    static const uint16_t m_s2xTableB24[];
    static const uint16_t m_s2xTableC1[];
    static const uint16_t m_s2xTableC2[];
    static const uint16_t m_s2xTableC3[];
    static const uint16_t m_s2xTableC4[];
    static const uint16_t m_s2xTableC5[];
    static const uint16_t m_s2xTableC6[];
    static const uint16_t m_s2xTableC7[];
    // VL-SNR's
    static const uint16_t m_tableS2X_B1_2_9[];
    static const uint16_t m_tableS2X_C8_1_5[];
    static const uint16_t m_tableS2X_C9_11_45[];
    static const uint16_t m_tableS2X_C10_1_3[];
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2LDPCTABLES_H
