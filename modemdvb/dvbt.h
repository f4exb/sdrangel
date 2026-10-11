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

#ifndef MODEMDVB_DVBT_H
#define MODEMDVB_DVBT_H

#include <cstdint>
#include <complex>
#include <vector>

#include "export.h"

// DVB-T as defined in ETSI EN 300 744. Section references in this library are to that document

namespace dvbt {

typedef std::complex<float> Cf;

// Values are those used in the TPS (EN 300 744 4.6.2)
enum class Mode { Mode2K = 0, Mode8K = 1 };
enum class Constellation { QPSK = 0, QAM16 = 1, QAM64 = 2 };
enum class Hierarchy { None = 0, Alpha1 = 1, Alpha2 = 2, Alpha4 = 3 };
enum class CodeRate { R1_2 = 0, R2_3 = 1, R3_4 = 2, R5_6 = 3, R7_8 = 4 };
enum class GuardInterval { G1_32 = 0, G1_16 = 1, G1_8 = 2, G1_4 = 3 };

static const int symbolsPerFrame = 68;
static const int framesPerSuperFrame = 4;
static const int tsPacketSize = 188;
static const int rsPacketSize = 204;

MODEMDVB_API int fftSize(Mode mode);
MODEMDVB_API int kMax(Mode mode);                //!< Index of highest carrier (Kmin is 0)
MODEMDVB_API int dataCarriers(Mode mode);        //!< Data cells per OFDM symbol
MODEMDVB_API int guardSamples(Mode mode, GuardInterval guard);
MODEMDVB_API int bitsPerCell(Constellation constellation);
MODEMDVB_API void codeRateFraction(CodeRate rate, int& num, int& den);
MODEMDVB_API double sampleRate(int bandwidthHz); //!< 64/7 MS/s for 8 MHz channels
MODEMDVB_API const char *toString(Mode mode);
MODEMDVB_API const char *toString(Constellation constellation);
MODEMDVB_API const char *toString(Hierarchy hierarchy);
MODEMDVB_API const char *toString(CodeRate rate);
MODEMDVB_API const char *toString(GuardInterval guard);
MODEMDVB_API double netBitRate(int bandwidthHz, Mode mode, Constellation constellation, CodeRate rate, GuardInterval guard);

// Transmission Parameter Signalling (4.6)
struct MODEMDVB_API TPS
{
    int m_frameNumber;          //!< 0..3 within the super-frame
    Constellation m_constellation;
    Hierarchy m_hierarchy;
    CodeRate m_hpCodeRate;
    CodeRate m_lpCodeRate;
    GuardInterval m_guard;
    Mode m_mode;
    int m_lengthIndicator;      //!< 23, or 31 if the cell id is signalled (33 with DVB-H signalling)
    int m_cellIdByte;           //!< Half of the cell id carried in this frame: high byte in frames 1 and 3
    bool m_inDepth;             //!< DVB-H's in-depth inner interleaver (s27, annex F), for 2K (and 4K) only
    bool m_bchOK;

    TPS();
    bool sameParameters(const TPS& other) const;    //!< Compare the fields that set up the receiver
    //! Decode a 68-bit TPS block, s0..s67. Returns false if the sync word or a field is invalid
    bool decode(const uint8_t bits[symbolsPerFrame]);
    //! Encode into a 68-bit TPS block (s0 is set to 0)
    void encode(uint8_t bits[symbolsPerFrame]) const;

    static const uint16_t m_syncWord = 0x35ee; //!< 0011010111101110, frames 1 and 3
    static bool checkBCH(const uint8_t bits[symbolsPerFrame]);
    static void addBCH(uint8_t bits[symbolsPerFrame]);

private:
    static int getBits(const uint8_t *bits, int start, int count);
    static void setBits(uint8_t *bits, int start, int count, int value);
    static uint16_t bchRemainder(const uint8_t *bits, int start, int count);
};

// Per-mode carrier tables (4.5)
class MODEMDVB_API Tables
{
public:
    static const Tables& get(Mode mode);

    Mode m_mode;
    int m_fftSize;
    int m_kMax;
    int m_nbData;
    std::vector<int> m_continualPilots;      //!< Carrier indices
    std::vector<int> m_tpsCarriers;          //!< Carrier indices
    std::vector<uint8_t> m_prbs;             //!< w_k for k = 0..Kmax (4.5.2)
    //! Per scattered pilot phase (l mod 4): carrier type, 0 = data, 1 = scattered/continual pilot, 2 = TPS
    std::vector<uint8_t> m_carrierType[4];
    //! Per scattered pilot phase: indices of the data carriers, in increasing order
    std::vector<int> m_dataCarriers[4];
    std::vector<int> m_symbolInterleaver;    //!< H(q), 4.3.4.2

private:
    explicit Tables(Mode mode);

    static const int m_continualPilots2K[];  //!< 2K mode (4.5.3, table 7)
    static const int m_tpsCarriers2K[];      //!< 2K mode (4.6, table 8)
};

} // namespace dvbt

#endif // MODEMDVB_DVBT_H
