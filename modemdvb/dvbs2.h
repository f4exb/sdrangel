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

#ifndef MODEMDVB_DVBS2_H
#define MODEMDVB_DVBS2_H

#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "export.h"
#include "dvbt2.h"

// DVB-S2 (EN 302 307 V1.2.1) and S2X (EN 302 307-2 V1.3.1, its broadcast MODCODs and VL-SNR frames: not superframes
// or channel bonding): MODCODs, the physical layer frame's structure and signalling, scrambling, constellations, bit interleaving
// and demapping. The LDPC and BCH codes are DVB-T2's (dvbt2fec.h), as CodeFamily::S2
//
// A MODCOD's id is S2's MODCOD (0 to 31) or, for S2X's (the PLS code's b0 = 1), 32 + its b1..b6 (32 to 95). An S2X
// MODCOD is for one frame size, its TYPE's MSB set from it; its b7 is the pilots, the TYPE's LSB as S2's

namespace dvbs2 {

typedef std::complex<float> Cf;
using dvbt2::CodeFamily;
using dvbt2::CodeRate;
using dvbt2::FECType;

//! The constellation's size and kind. S2X's 16 and 32APSK are S2's (4+12, 4+12+16) or others (8+8, 4+12+16rb,
//! 4+8+4+16) by code rate, its 64APSK one of three (16+16+16+16, 8+16+20+20, 4+12+20+28) and 8APSK is 2+4+2.
//! BPSK is pi/2 BPSK, VL-SNR's alone
enum class Modulation { QPSK = 0, PSK8 = 1, APSK16 = 2, APSK32 = 3, APSK8 = 4, APSK64 = 5, APSK128 = 6, APSK256 = 7, BPSK = 8 };

static const int slotSymbols = 90;      //!< M (5.5)
static const int headerSymbols = 90;    //!< The PLHEADER, one slot
static const int timeSlicingHeaderSymbols = 180;    //!< EN 302 307-1 Annex M's (time slicing), two slots
static const int timeSlicingCodeBits = 77;          //!< Its (77, 16) convolutional code's, each sent twice
static const int sofSymbols = 26;
static const int pilotSymbols = 36;     //!< P, a pilot block
static const int pilotInterval = 16;    //!< Slots between pilot blocks
static const int dummySlots = 36;       //!< A dummy PLFRAME's
static const int dummyModcod = 0;
static const int s2xModcodBase = 32;    //!< S2X MODCODs' ids: this plus b1..b6
static const int nbModcods = 96;        //!< Ids 0 to 95
static const int maxPoints = 256;       //!< In a constellation
static const int maxBitsPerSymbol = 8;

MODEMDVB_API const char *toString(Modulation modulation);
MODEMDVB_API int bitsPerSymbol(Modulation modulation);
//! Table 12 (S2's MODCOD 1 to 28) and EN 302 307-2's table 17a (S2X's): the modulation and code rate (the LDPC code's,
//! as the implementation's name has it). Returns false for 0 (dummy), S2's reserved 29 to 31 and S2X's reserved ones
MODEMDVB_API bool modcodInfo(int modcod, Modulation& modulation, CodeRate& rate);
//! The MODCOD of a modulation, code rate and frame size, 0 if there isn't one
MODEMDVB_API int modcod(Modulation modulation, CodeRate rate, FECType type = FECType::Normal);
//! "8PSK 2/3", or S2X's canonical name, as "16APSK 2/3-L"
MODEMDVB_API const char *modcodString(int modcod);
inline bool isS2X(int modcod) { return modcod >= s2xModcodBase; }
//! An S2X MODCOD's frame size, its code's. False for S2's, whose TYPE says
MODEMDVB_API bool fixedFECType(int modcod, FECType& type);
//! The frame size a MODCOD is sent with: an S2X one's own, else the one given
MODEMDVB_API FECType modcodFECType(int modcod, FECType type);
//! A MODCOD that can be decoded at that frame size: S2's 1 to 28 at either (but 9/10 only normal), S2X's at its own
MODEMDVB_API bool validModcod(int modcod, FECType type);
//! The PLS code's 8 bits (b0 the MSB) for a MODCOD and TYPE
MODEMDVB_API int plsCode(int modcod, int type);
//! S, the slots of an XFECFRAME (table 11; 128APSK's padded to 103)
MODEMDVB_API int slotCount(Modulation modulation, FECType type);
//! Pilot blocks in a PLFRAME of that many slots: one after every 16, but not at the end
MODEMDVB_API int pilotBlocks(int nbSlots, bool pilots);
//! Symbols in a PLFRAME, PLHEADER included: 90 (S + 1) + 36 int((S - 1) / 16) with pilots; a dummy's; that of an S2X
//! reserved PLS code (EN 302 307-2 table 17b: a receiver keeps lock through it); a VL-SNR set's. 0 if there's no length
//! (S2's reserved MODCODs, a MODCOD at a frame size it isn't for)
MODEMDVB_API int plFrameSymbols(int modcod, FECType type, bool pilots);
//! A PLFRAME's TYPE field: the frame size in its MSB, pilots in its LSB
inline int frameType(FECType type, bool pilots) { return (type == FECType::Short ? 2 : 0) | (pilots ? 1 : 0); }
//! Ideal Es/N0 at quasi error free (table 13, 64800 frames; EN 302 307-2's tables 20a and 20b), dB
MODEMDVB_API double idealEsN0(int modcod);
//! Useful bit rate: kbch less the BBHEADER of each frame, at a symbol rate
MODEMDVB_API double usefulBitRate(double symbolRate, int modcod, FECType type, bool pilots);

//! The PLHEADER's bits (5.5.2): the SOF then the scrambled PLS code of the MODCOD and TYPE, one per byte
MODEMDVB_API void plHeaderBits(int modcod, int type, uint8_t bits[headerSymbols]);
//! The PLHEADER's pi/2 BPSK symbols. S2X's PLS code's are turned by pi/2 (multiplied by j) after the SOF
MODEMDVB_API void plHeaderSymbols(int modcod, int type, Cf symbols[headerSymbols]);
//! The pi/2 BPSK symbol for a 0 at header position n (counting from 0): e^j pi/4, then e^j 3pi/4, alternately
MODEMDVB_API Cf headerSymbolZero(int n);
//! The SOF's bits
MODEMDVB_API const uint8_t *sofBits();
//! The PLS code's scrambling sequence, 64 bits
MODEMDVB_API const uint8_t *plsScrambling();
//! The 32 bits of the (32, 7) code for b0..b6 (b0 the MSB of value), each 0 or 1 (S2's figure 13b, with S2X's first
//! row, EN 302 307-2 figure 20). With b0 = 0, S2's (32, 6) code for b1..b6
MODEMDVB_API void plsCodeword(int value, uint8_t bits[32]);
//! The (32, 7) code's row for b0
static const uint32_t plsB0Row = 0x90AC2DDDu;

// EN 302 307-1 Annex M, time slicing: the PLHEADER's two slots, the SOF then the PLS code's 16 bits u0..u15 (the PLS code's
// b0..b7, as plsCode's, then the 8 bit time slice number, TSN) in a k = 5 rate 1/5 tail biting convolutional code, p4
// punctured after u3, u8 and u13 (77 bits), each bit sent twice and scrambled: 154 pi/2 BPSK symbols, without S2X's
// quarter turn

//! The code's five bits (p0 in bit 4) for an input bit from a state (S0 in bit 3 .. S3 in bit 0, S0 the last input).
//! The next state is (input << 3) | (state >> 1)
MODEMDVB_API int timeSlicingParity(int input, int state);
//! Whether the code's input bit j has its p4 (it's punctured for u3, u8 and u13)
inline bool timeSlicingHasP4(int j) { return (j != 3) && (j != 8) && (j != 13); }
//! The 77 bits of the code for a 16 bit word (u0 its MSB), each 0 or 1
MODEMDVB_API void timeSlicingCodeword(int word, uint8_t bits[timeSlicingCodeBits]);
//! The PLS code's scrambling sequence, 154 bits
MODEMDVB_API const uint8_t *timeSlicingScrambling();
//! The PLHEADER's bits: the SOF, then the code of the MODCOD's and TYPE's PLS code and the TSN, repeated and scrambled
MODEMDVB_API void timeSlicingHeaderBits(int modcod, int type, int tsn, uint8_t bits[timeSlicingHeaderSymbols]);
//! The PLHEADER's pi/2 BPSK symbols
MODEMDVB_API void timeSlicingHeader(int modcod, int type, int tsn, Cf symbols[timeSlicingHeaderSymbols]);

// EN 302 307-2 5.5.2, VL-SNR frames: a PLHEADER of PLS code 129 (set 1, MODCOD id 32 with pilots) or 131 (set 2, 33),
// then a 900 symbol pi/2 BPSK VL-SNR header whose Walsh-Hadamard sequence (index 0 to 15) says the MODCOD, then the
// XFECFRAME (shortened and punctured to fit) with extra pilot blocks between S2's: QPSK normal's length with pilots
// (set 1) or 16APSK normal's (set 2), so legacy receivers skip them. The headers aren't scrambled; the scrambling starts
// after the VL-SNR header, pi/2 BPSK data's by (-1)^R

static const int vlsnrHeaderSymbols = 900;
static const int vlsnrHeaderLength = headerSymbols + vlsnrHeaderSymbols;    //!< Before the scrambling starts
static const int vlsnrIndexes = 16;
static const int vlsnrDummy = 12;               //!< The VL-SNR dummy frame's index (set 2)

struct VLSNRModcod
{
    bool m_valid;               //!< Assigned (the dummy's not)
    int m_set;                  //!< 1 or 2
    Modulation m_modulation;    //!< QPSK or BPSK (pi/2)
    CodeRate m_rate;            //!< The LDPC code's
    FECType m_type;
    int m_spreading;            //!< Each bit sent this many times
    int m_xs;                   //!< Information bits shortened (zeros, not sent)
    int m_p;                    //!< Every P'th parity bit from the first punctured,
    int m_xp;                   //!< this many
    double m_esn0;              //!< Ideal Es/N0 (EN 302 307-2 tables 20a to 20c)
    const char *m_name;         //!< Canonical (table 18a)
};

//! Table 18b's MODCOD for a VL-SNR header's index (0 to 15)
MODEMDVB_API const VLSNRModcod& vlsnrModcod(int index);
//! A PLHEADER's MODCOD id and TYPE are VL-SNR's: its set (1 or 2), else 0
inline int vlsnrSet(int modcod, int type) { return (type & 1) && ((modcod == s2xModcodBase) || (modcod == s2xModcodBase + 1)) ? modcod - s2xModcodBase + 1 : 0; }
//! The PLHEADER's MODCOD id for a set (with pilots)
inline int vlsnrSetModcod(int set) { return s2xModcodBase + set - 1; }
//! Symbols in a set's PLFRAME, headers included: 33 282 or 16 686
MODEMDVB_API int vlsnrFrameSymbols(int set);
//! Its data symbols: 30 780 or 14 976
MODEMDVB_API int vlsnrDataSymbols(int set);
//! For each symbol after the VL-SNR header, 1 if it's a pilot (figures 17 and 18): S2's blocks of 36 after every 16
//! slots (from the PLHEADER), and between them blocks of 34 (set 1's first 18 groups), 32 (set 2's first 9) or 36
MODEMDVB_API const std::vector<uint8_t>& vlsnrPilots(int set);
//! The header's 900 bits for an index: 00, the 896 bit pattern's 16 rows each inverted by the sequence's minus, 00
MODEMDVB_API void vlsnrHeaderBits(int index, uint8_t bits[vlsnrHeaderSymbols]);
//! Its pi/2 BPSK symbols
MODEMDVB_API void vlsnrHeader(int index, Cf symbols[vlsnrHeaderSymbols]);
//! The BBFRAME's bits, Kbch less the shortening (not whole bytes for the medium codes: the BBFRAME's last bits padding)
MODEMDVB_API int vlsnrKbch(int index);
//! Whether parity bit j of the index's code is punctured
inline bool vlsnrPunctured(const VLSNRModcod& m, int j) { return (j % m.m_p == 0) && (j / m.m_p < m.m_xp); }

//! The physical layer scrambling sequence (5.5.4) of Gold code n: R_n(i), 0 to 3, the symbol multiplied by j^R
class MODEMDVB_API Scrambler
{
public:
    explicit Scrambler(int n = 0);
    void setCode(int n);
    int code() const { return m_n; }
    //! R_n(i), for i from 0 at the first symbol after the PLHEADER
    const std::vector<uint8_t>& sequence() const { return m_sequence; }

    static constexpr int m_length = 66420;  //!< As the standard defines it

private:
    int m_n;
    std::vector<uint8_t> m_sequence;
};

//! The constellation's points (5.4), indexed by the bits MSB first, at unit average power. APSK's ring ratios depend on
//! the code rate (tables 9 and 10), and for S2X, which constellation it is (EN 302 307-2 5.4), and for 4+12+16rbAPSK
//! 2/3, the frame size
MODEMDVB_API void constellation(Modulation modulation, CodeRate rate, std::vector<Cf>& points, FECType type = FECType::Normal);

//! The bit interleaver (5.3.3) and the mapping's serial to parallel conversion: map[symbol * eta + bit] is the codeword
//! bit that bit (MSB first) of the symbol carries. No interleaving for QPSK; columns read in reverse for 8PSK 3/5, and
//! for S2X, in the order of EN 302 307-2's tables 9a and 9b. 128APSK's padding: mapZero (the 6 zeros after the
//! codeword, interleaved) and mapOne (the 84 ones after the interleaver)
MODEMDVB_API void bitMap(FECType type, Modulation modulation, CodeRate rate, std::vector<int>& map);
static const int mapZero = -1;
static const int mapOne = -2;

//! The nearest of a constellation's points to a symbol, from a grid over the plane: in each cell, the points nearest
//! anywhere in it (those nearest its corners and centre: as the points' regions are far larger than a cell, those
//! are all of them but where three regions meet in it). The nearest of a cell's few (one, but by a boundary), rather
//! than of up to 256
class MODEMDVB_API NearestTable
{
public:
    void build(const std::vector<Cf>& points);
    bool empty() const { return m_table.empty(); }

    int nearest(const Cf& c, const std::vector<Cf>& points) const
    {
        const int gx = std::min(m_grid - 1, std::max(0, (int) ((c.real() + m_range) * m_cellsPerUnit)));
        const int gy = std::min(m_grid - 1, std::max(0, (int) ((c.imag() + m_range) * m_cellsPerUnit)));
        const uint16_t *cell = &m_table[(gy * m_grid + gx) * m_slots];
        int nearest = cell[0];

        if (cell[1] != m_unused)
        {
            float best = std::norm(c - points[nearest]);

            for (int k = 1; (k < m_slots) && (cell[k] != m_unused); k++)
            {
                const float e = std::norm(c - points[cell[k]]);

                if (e < best)
                {
                    best = e;
                    nearest = cell[k];
                }
            }
        }

        return nearest;
    }

    static constexpr int m_grid = 256;              //!< Cells a side
    static constexpr float m_range = 2.0f;      //!< It covers +/- this, at unit energy (beyond, the nearest edge cell's)
    static constexpr float m_cellsPerUnit = m_grid / (2.0f * m_range);
    static constexpr int m_slots = 4;               //!< Points a cell can list
    static constexpr uint16_t m_unused = 0xffff;

private:
    std::vector<uint16_t> m_table;
};

//! For the largest constellations (128 and 256 points), max-log demapping from a few of the points rather than all:
//! the symbol's nearest, and for each bit, those nearest that point with the bit the other way. Near enough what all
//! the points give (where the nearest with a bit the other way is further, the LLR's at its limit anyway), for a tenth
//! of the work
class MODEMDVB_API DemapTable
{
public:
    void build(const std::vector<Cf>& points, int eta);
    bool empty() const { return m_offsets.empty(); }
    const NearestTable& nearestTable() const { return m_nearest; }
    //! The candidates for a symbol whose nearest point is this one
    const uint8_t *candidates(int point, int& count) const
    {
        count = m_offsets[point + 1] - m_offsets[point];
        return &m_candidates[m_offsets[point]];
    }

    static constexpr int m_minPoints = 128;         //!< Constellations this size or more have one
    static constexpr int m_perBit = 4;              //!< Nearest a point with each bit the other way

private:
    NearestTable m_nearest;
    std::vector<int> m_offsets;
    std::vector<uint8_t> m_candidates;
};

//! Max-log demapping of normalised symbols (unit average power, the constellation's points) to int8 LLRs, positive for
//! 0, twice the actual LLR and limited to dvbt2::llrLimit, as dvbt2::demapCells: llrs[map[cell * eta + bit]]. scales:
//! each cell's 4 / noise variance per dimension (times its reliability); if outlierThreshold > 0, a cell whose error
//! power to the nearest point is more than outlierThreshold times the noise per dimension has its scale reduced in
//! proportion. errors: if not null, each cell's error power to the nearest point. Map entries below 0 (padding) are
//! skipped. table: for a constellation of 128 points or more, its candidates (else every point's tried). oneScale:
//! scales[0] is every cell's
MODEMDVB_API void demap(const Cf *cells, const float *scales, int nbCells, const std::vector<Cf>& points, int eta,
    float outlierThreshold, const int *map, int8_t *llrs, float *errors = nullptr, const DemapTable *table = nullptr,
    bool oneScale = false);

struct ModcodEntry;
struct S2XModcodEntry;

//! The tables and helpers of dvbs2.cpp's free functions
class S2Tables
{
private:
    static constexpr double m_pi = 3.141592653589793;
    static const ModcodEntry m_modcods[28];
    static const S2XModcodEntry m_s2xModcods[];
    static const float m_apsk16_18_30[16][2];
    static const float m_apsk16_20_30[16][2];
    static const float m_apsk256_20_30[256][2];
    static const float m_apsk256_22_30[256][2];
    static const VLSNRModcod m_vlsnrModcods[vlsnrIndexes];

    static const S2XModcodEntry *s2xEntry(int modcod);
    static std::vector<uint8_t> vlsnrPilotMask(std::initializer_list<std::array<int, 4>> groups, int last);
    static void normalise(std::vector<Cf>& points);
    static double tableAngle(double a, int p, int q);
    static const char *interleaverPattern(FECType type, Modulation modulation, CodeRate rate);
    static inline int8_t quantise(float v);

    friend MODEMDVB_API bool modcodInfo(int modcod, Modulation& modulation, CodeRate& rate);
    friend MODEMDVB_API int modcod(Modulation modulation, CodeRate rate, FECType type);
    friend MODEMDVB_API const char *modcodString(int modcod);
    friend MODEMDVB_API double idealEsN0(int modcod);
    friend MODEMDVB_API const VLSNRModcod& vlsnrModcod(int index);
    friend MODEMDVB_API const std::vector<uint8_t>& vlsnrPilots(int set);
    friend MODEMDVB_API void constellation(Modulation modulation, CodeRate rate, std::vector<Cf>& points, FECType type);
    friend MODEMDVB_API void bitMap(FECType type, Modulation modulation, CodeRate rate, std::vector<int>& map);
    friend MODEMDVB_API void demap(const Cf *cells, const float *scales, int nbCells, const std::vector<Cf>& points, int eta,
        float outlierThreshold, const int *map, int8_t *llrs, float *errors, const DemapTable *table, bool oneScale);
};

} // namespace dvbs2

#endif // MODEMDVB_DVBS2_H
