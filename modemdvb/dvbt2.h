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

#ifndef MODEMDVB_DVBT2_H
#define MODEMDVB_DVBT2_H

#include <algorithm>
#include <complex>
#include <cstdint>
#include <vector>

#include "export.h"

// DVB-T2 (ETSI EN 302 755 V1.4.1) parameters, L1 signalling and carrier tables. T2-base profile, SISO
namespace dvbt2 {

typedef std::complex<float> Cf;

//! S1 field of P1 (table 18)
enum class PreambleType { T2SISO = 0, T2MISO = 1, NonT2 = 2, T2LiteSISO = 3, T2LiteMISO = 4 };

//! GUARD_INTERVAL field of L1-pre (table 22)
enum class GuardInterval { G1_32 = 0, G1_16 = 1, G1_8 = 2, G1_4 = 3, G1_128 = 4, G19_128 = 5, G19_256 = 6 };

//! PILOT_PATTERN field (table 27)
enum class PilotPattern { PP1 = 0, PP2, PP3, PP4, PP5, PP6, PP7, PP8 };

//! PLP_MOD field (table 33)
enum class Constellation { QPSK = 0, QAM16 = 1, QAM64 = 2, QAM256 = 3 };

//! L1_MOD field (table 24)
enum class L1Constellation { BPSK = 0, QPSK = 1, QAM16 = 2, QAM64 = 3 };

//! PLP_COD field (table 32). 1/3 and 2/5 are T2-Lite only. 1/4 is only for L1-pre
//! LDPC code rates. 8/9 and 9/10 are DVB-S2's alone
//! T2's, then S2's (from R1_3), then S2X's (EN 302 307-2, from R13_45: its LDPC code identifiers; 26/45 and 32/45 are
//! codes in both frame sizes, the others in one)
enum class CodeRate { R1_2 = 0, R3_5 = 1, R2_3 = 2, R3_4 = 3, R4_5 = 4, R5_6 = 5, R1_3 = 6, R2_5 = 7, R1_4 = 8, R8_9 = 9, R9_10 = 10,
    R13_45, R9_20, R11_20, R26_45, R28_45, R23_36, R25_36, R13_18, R7_9, R90_180, R96_180, R100_180, R104_180,
    R116_180, R124_180, R128_180, R132_180, R135_180, R140_180, R154_180, R18_30, R20_30, R22_30,
    R11_45, R4_15, R14_45, R7_15, R8_15, R32_45, R2_9, R1_5 };  // 2/9 normal and 1/5 medium: S2X's VL-SNR codes
static const int nbCodeRates = (int) CodeRate::R1_5 + 1;
//! One of S2X's
inline bool isS2X(CodeRate rate) { return (int) rate >= (int) CodeRate::R13_45; }

//! PLP_FEC_TYPE field (table 34)
//! 16K and 64K LDPC. Medium (32 400 bits) is only S2X's, for VL-SNR (EN 302 307-2 table 5)
enum class FECType { Short = 0, Normal = 1, Medium = 2 };

//! Whose LDPC and BCH codes: DVB-T2's (EN 302 755) or DVB-S2's (EN 302 307), which share most, but S2's 64K 2/3 and 16K
//! 3/5 codes are different, it has rates T2 hasn't (64K 1/4, 1/3, 2/5, 8/9, 9/10 and 16K 8/9), and its 8/9 and 9/10
//! have t = 8 BCH
enum class CodeFamily { T2 = 0, S2 = 1 };

MODEMDVB_API const char *toString(GuardInterval guard);
MODEMDVB_API const char *toString(PilotPattern pattern);
MODEMDVB_API const char *toString(Constellation constellation);
MODEMDVB_API const char *toString(L1Constellation constellation);
MODEMDVB_API const char *toString(CodeRate rate);
MODEMDVB_API const char *fftSizeString(int fftSize);

MODEMDVB_API double sampleRate(int bandwidthHz);              //!< 1/T (table 65)
MODEMDVB_API int guardSamples(int fftSize, GuardInterval guard);
MODEMDVB_API bool guardAllowed(int fftSize, GuardInterval guard); //!< Table 67
MODEMDVB_API int numP2(int fftSize);                          //!< Table 51
MODEMDVB_API int kMaxNormal(int fftSize);                     //!< Table 66
MODEMDVB_API int kExt(int fftSize);
MODEMDVB_API int dx(PilotPattern pattern);                    //!< Table 57
MODEMDVB_API int dy(PilotPattern pattern);
//! Table 64 for SISO. In MISO, always but with PP8 (9.2.7)
MODEMDVB_API bool frameClosingSymbol(int fftSize, GuardInterval guard, PilotPattern pattern, bool miso = false);
MODEMDVB_API int bitsPerCell(Constellation constellation);
MODEMDVB_API int bitsPerCell(L1Constellation constellation);
MODEMDVB_API int nldpc(FECType type);
//! Table 6 (EN 302 307 tables 5a and 5b for S2). 0 if the family hasn't the code
MODEMDVB_API int kldpc(FECType type, CodeRate rate, CodeFamily family = CodeFamily::T2);
MODEMDVB_API int kbch(FECType type, CodeRate rate, CodeFamily family = CodeFamily::T2);
//! Error correction capability
MODEMDVB_API int bchT(FECType type, CodeRate rate, CodeFamily family = CodeFamily::T2);
MODEMDVB_API int cellsPerFECBlock(FECType type, Constellation constellation); //!< Table 17
//! FFT size from S2 field 1 (table 19(a)) of T2-base or T2-Lite, or 0 if reserved, and whether the guard interval is one
//! of 1/128, 19/256, 19/128
MODEMDVB_API int fftSizeFromS2(int s2, bool& newGuards, bool lite = false);

//! Which T2 profile to receive: Auto is T2-base, or T2-Lite when it's alone (its P1s don't signal other preambles)
enum class Profile { Auto = 0, Base = 1, Lite = 2 };

//! The P1 signalling
struct MODEMDVB_API P1Signalling
{
    int m_s1 = 0;
    int m_s2 = 0;               //!< Field 1 in bits 3-1, mixed flag in bit 0
    int fftSize() const;        //!< 0 if not T2-base or T2-Lite, or reserved
    bool isT2Base() const { return (m_s1 == (int) PreambleType::T2SISO) || (m_s1 == (int) PreambleType::T2MISO); }
    bool isT2Lite() const { return (m_s1 == (int) PreambleType::T2LiteSISO) || (m_s1 == (int) PreambleType::T2LiteMISO); }
    bool isT2() const { return isT2Base() || isT2Lite(); }
    bool isMISO() const { return (m_s1 == (int) PreambleType::T2MISO) || (m_s1 == (int) PreambleType::T2LiteMISO); }
    bool hasFEF() const { return (m_s2 & 1) != 0; }
};

//! L1-pre's BPSK cells: the first l1PreCells / NP2 of each P2 symbol (8.3.5)
static const int l1PreCells = 1840;

//! L1-pre signalling (7.2.2), 200 bits including the CRC
struct MODEMDVB_API L1Pre
{
    static const int size = 200;

    int m_type = 0;
    bool m_bwtExt = false;
    int m_s1 = 0;
    int m_s2 = 0;
    bool m_l1RepetitionFlag = false;
    GuardInterval m_guard = GuardInterval::G1_32;
    int m_papr = 0;
    L1Constellation m_l1Mod = L1Constellation::BPSK;
    int m_l1Cod = 0;
    int m_l1FecType = 0;
    int m_l1PostSize = 0;       //!< In cells
    int m_l1PostInfoSize = 0;   //!< In bits, excluding the CRC
    PilotPattern m_pilotPattern = PilotPattern::PP1;
    int m_txIdAvailability = 0;
    int m_cellId = 0;
    int m_networkId = 0;
    int m_t2SystemId = 0;
    int m_numT2Frames = 0;
    int m_numDataSymbols = 0;
    int m_regenFlag = 0;
    bool m_l1PostExtension = false;
    int m_numRf = 1;
    int m_currentRfIdx = 0;
    int m_t2Version = 0;
    bool m_l1PostScrambled = false;
    bool m_t2BaseLite = false;

    //! bits: one per byte, MSB first. Returns false if the CRC is wrong
    bool decode(const uint8_t *bits);
    void encode(uint8_t *bits) const;
    int fftSize() const;
    //! Tone reservation in the data symbols (table 23)
    bool toneReservation() const { return (m_papr & 2) != 0; }

private:
    friend struct L1Post;

    //! Bit fields, MSB first, one bit per byte
    static uint32_t getBits(const uint8_t *bits, int& offset, int nb);
    static void putBits(std::vector<uint8_t>& bits, uint32_t value, int nb);
};

//! A PLP, from the configurable and dynamic L1-post signalling
struct MODEMDVB_API PLP
{
    int m_id = 0;
    int m_type = 1;             //!< 0 common, 1 data type 1, 2 data type 2
    int m_payloadType = 3;      //!< 3 TS
    bool m_ffFlag = false;
    int m_firstRfIdx = 0;
    int m_firstFrameIdx = 0;
    int m_groupId = 0;
    CodeRate m_codeRate = CodeRate::R2_3;
    Constellation m_constellation = Constellation::QAM256;
    bool m_rotation = false;
    FECType m_fecType = FECType::Normal;
    int m_numBlocksMax = 0;
    int m_frameInterval = 1;
    int m_timeIlLength = 0;
    bool m_timeIlType = false;
    bool m_inBandA = false;
    bool m_inBandB = false;
    int m_reserved1 = 0;
    int m_plpMode = 0;
    bool m_staticFlag = false;
    bool m_staticPaddingFlag = false;
    bool m_valid = true;        //!< Its code rate, modulation and FEC type aren't reserved for the profile, so it can be decoded
    // Dynamic
    int m_start = 0;            //!< Cell address
    int m_numBlocks = 0;        //!< FEC blocks in the interleaving frame

    int interleavingFrames() const { return m_timeIlType ? m_timeIlLength : 1; } //!< PI, T2-frames per interleaving frame
    int tiBlocks() const { return m_timeIlType ? 1 : std::max(1, m_timeIlLength); } //!< NTI
};

//! L1-post signalling (7.2.3): configurable and dynamic parts
struct MODEMDVB_API L1Post
{
    int m_subSlicesPerFrame = 1;
    int m_numAux = 0;
    int m_auxConfigRfu = 0;
    std::vector<uint32_t> m_frequencies;    //!< Indexed by RF_IDX
    int m_fefType = 0;
    int m_fefLength = 0;
    int m_fefInterval = 0;
    std::vector<PLP> m_plps;
    int m_fefLengthMsb = 0;
    // Dynamic
    int m_frameIdx = 0;
    int m_subSliceInterval = 0;
    int m_type2Start = 0;
    int m_l1ChangeCounter = 0;
    int m_startRfIdx = 0;

    //! Parse L1_POST_INFO_SIZE + 32 bits (one per byte), checking the CRC
    bool decode(const uint8_t *bits, const L1Pre& pre);
    //! Configurable, dynamic and CRC bits. Sets the info size in pre
    void encode(std::vector<uint8_t>& bits, L1Pre& pre) const;
};

MODEMDVB_API uint32_t crc32(const uint8_t *bits, int nbBits);  //!< Annex F, one bit per byte
MODEMDVB_API uint8_t crc8(const uint8_t *bytes, int nbBytes);  //!< Annex F

//! Energy dispersal (5.2.4) of nbBits bits, one per byte, starting from the initial state
MODEMDVB_API void scramble(uint8_t *bits, int nbBits);
//! The same, on bytes, MSB first
MODEMDVB_API void scrambleBytes(uint8_t *bytes, int nbBytes);

//! Carrier types and frequency interleaver for one combination of FFT size, carrier mode, pilot pattern and
//! tone reservation. Carriers are indexed from 0 to kMax (extended mode includes Kext each side)
class MODEMDVB_API Tables
{
public:
    struct Pilot
    {
        int m_k;
        float m_amplitude;
    };

    static const Tables& get(int fftSize, bool extended, PilotPattern pattern, bool toneReservation, bool miso = false);

    int m_fftSize;
    bool m_extended;
    PilotPattern m_pattern;
    bool m_toneReservation;
    bool m_miso;                //!< P2 pilots for MISO
    int m_kMax;
    int m_kExt;                 //!< Used for the extended mode offsets, 0 in normal mode
    int m_dx;
    int m_dy;
    std::vector<uint8_t> m_w;   //!< Symbol level PRBS for each carrier (9.2.2.1)

    // P2 symbols
    std::vector<int> m_p2Data;
    std::vector<Pilot> m_p2Pilots;
    // Normal data symbols, for each value of l mod Dy (l counting from the first P2 symbol)
    std::vector<int> m_data[16];
    std::vector<Pilot> m_pilots[16];
    std::vector<Pilot> m_scattered[16];     //!< Of m_pilots, those on the scattered pilots' grid (Dx Dy carriers apart)
    // Frame closing symbol: NFC data cells, the last NFC-CFC of which are unmodulated
    std::vector<int> m_fcData;
    std::vector<Pilot> m_fcPilots;
    int m_cfc;
    std::vector<int> m_continual;   //!< Continual pilot carriers, for frequency synchronisation

    //! Frequency interleaver (8.5): data cell x[p] is carrier data[read[p]]. For symbol l of the frame
    const std::vector<int>& p2Deinterleave(int l) const { return m_p2Read[l & 1]; }
    const std::vector<int>& dataDeinterleave(int l) const { return m_dataRead[l & 1]; }
    const std::vector<int>& fcDeinterleave(int l) const { return m_fcRead[l & 1]; }

private:
    Tables(int fftSize, bool extended, PilotPattern pattern, bool toneReservation, bool miso);
    static int fftIndex(int fftSize);
    //! Table 50: active cells in the frame closing symbol, without tone reservation
    static int frameClosingActiveCells(int fftSize, bool extended, PilotPattern pattern);
    void interleaver(int ndata, std::vector<int> read[2]) const;

    std::vector<int> m_p2Read[2];
    std::vector<int> m_dataRead[2];
    std::vector<int> m_fcRead[2];
};

//! Frame level PN sequence bit for symbol l (9.2.2.2), 0 past its end
MODEMDVB_API int framePN(int l);
static const int framePNLength = 2624;     //!< Table 56: the most symbols a T2-frame can have (10 MHz)

//! P1 tables (9.8)
struct MODEMDVB_API P1Tables
{
    static const P1Tables& get();
    int m_carriers[384];
    uint8_t m_s1[8][64];
    uint8_t m_s2[16][256];
    uint8_t m_scrambling[384];
    //! Modulation of the active carriers for given S1 and S2, before scrambling removed: +/-1
    void modulation(int s1, int s2, float *values) const;

private:
    P1Tables();
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2_H
