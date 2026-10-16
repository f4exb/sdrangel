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

#ifndef MODEMDAB_DABTABLES_H_
#define MODEMDAB_DABTABLES_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

namespace modemdab
{

// Transmission mode I (ETSI EN 300 401 V2.1.1 clause 14, table 22). Times are in
// samples of the elementary period T = 1/2048000 s.
namespace ModeI
{
    constexpr int SampleRate = 2048000;
    constexpr int FFTSize = 2048;           // Tu
    constexpr int GuardSize = 504;          // Delta
    constexpr int SymbolSize = 2552;        // Ts
    constexpr int NullSize = 2656;          // Tnull
    constexpr int FrameSize = 196608;       // Tf (96 ms)
    constexpr int SymbolsPerFrame = 76;     // L, the null symbol excluded
    constexpr int Carriers = 1536;          // K
    constexpr int BitsPerSymbol = 2 * Carriers;
    constexpr int FICSymbols = 3;           // l = 2..4
    constexpr int MSCSymbols = 72;          // l = 5..76
    constexpr int CIFsPerFrame = 4;
    constexpr int CUBits = 64;
    constexpr int CUsPerCIF = 864;
    constexpr int CIFBits = CUsPerCIF * CUBits;                         // 55296
    constexpr int FICBlockBits = FICSymbols * BitsPerSymbol / CIFsPerFrame; // 2304
    constexpr int FIBsPerCIF = 3;
    constexpr int FIBBytes = 32;
    constexpr int FIBDataBytes = 30;
    constexpr int CIFCountModulus = 5000;
}

// Puncturing index (1..24) and the number of 128-bit mother code blocks punctured with it.
struct PuncturingPart
{
    int blocks;
    int index;
};

// A protection profile (EN 300 401 clause 11): the puncturing of each part of a mother
// codeword of 4 * I + 24 bits, followed by the 24-bit tail punctured with VT and padding.
struct MODEMDAB_API ProtectionProfile
{
    std::vector<PuncturingPart> parts;
    int paddingBits = 0;

    int informationBits() const;    // I = 32 * total blocks
    int puncturedBits() const;      // M, including the tail and padding
};

// Table 8: a short-form (UEP) sub-channel.
struct UEPTableEntry
{
    int sizeCU;
    int protectionLevel;            // 1..5
    int bitrate;                    // kbit/s
};

class MODEMDAB_API DABTables
{
public:
    // Phase reference symbol, indexed by carrier k + 768 (k = -768..768; k = 0 is zero).
    static const std::array<std::complex<float>, ModeI::Carriers + 1>& phaseReference();
    // Frequency interleaving: carrier index k (-768..768, never 0) for QPSK symbol n.
    static const std::array<int16_t, ModeI::Carriers>& frequencyInterleaving();
    // Puncturing vector VPI (index 1..24) as 32 flags, and the tail vector VT (24 flags).
    static const std::array<uint8_t, 32>& puncturingVector(int index);
    static const std::array<uint8_t, 24>& tailPuncturingVector();
    // Time interleaving: CIF delay for bit index i, by i mod 16 (table 21).
    static int timeInterleavingDelay(int bitIndex) { return m_timeInterleavingDelays[bitIndex & 15]; }

    static ProtectionProfile ficProfile();
    static bool uepTableEntry(int tableIndex, UEPTableEntry& entry);
    static ProtectionProfile uepProfile(int tableIndex);
    // EEP: set A (option 0) uses bit rates of 8n kbit/s, set B (option 1) 32n kbit/s.
    // protectionLevel is 0..3 for levels 1..4.
    static ProtectionProfile eepProfile(bool setB, int protectionLevel, int sizeCU);
    static int eepBitrate(bool setB, int protectionLevel, int sizeCU);

    // Energy dispersal PRBS (x^9 + x^5 + 1, all ones initial state); at least 43776 bits.
    static const std::vector<uint8_t>& prbs();

    // CRC-16 (x^16 + x^12 + x^5 + 1, all ones initial state, complemented), as used by FIBs,
    // packets, data groups, X-PAD data groups and DAB+ AUs.
    static uint16_t crc16(const uint8_t *data, std::size_t length);
    // True when the last two bytes of data are the CRC of the bytes before them.
    static bool crc16Check(const uint8_t *data, std::size_t lengthWithCRC);
    // DAB+ superframe Fire code (TS 102 563 5.2) over bytes 2..10, checked against bytes 0..1.
    static bool fireCodeCheck(const uint8_t *superframe);
    // MPEG audio CRC-16 (x^16 + x^15 + x^2 + 1, all ones initial state) over bits.
    static uint16_t crc16MPEG(const uint8_t *data, int startBit, int bitCount, uint16_t crc = 0xffff);

    // TII pattern a_b(p) for main identifier p (0..69), bit b (0..7) is (pattern >> (7 - b)) & 1.
    static uint8_t tiiPattern(int mainId);

    // Text
    static std::string labelToUTF8(const uint8_t *data, std::size_t length, int charset);
    static std::string programmeTypeName(int pty, bool northAmerica);
    static std::string languageName(int language);
    // Announcement types (TS 101 756 table 14), by bit of the ASu and ASw flags
    static std::string announcementTypeName(int type);
    static std::string userApplicationName(int userApplicationType);

private:
    static const int m_timeInterleavingDelays[16];
};

} // namespace modemdab

#endif // MODEMDAB_DABTABLES_H_
