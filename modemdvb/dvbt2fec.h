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

#ifndef MODEMDVB_DVBT2FEC_H
#define MODEMDVB_DVBT2FEC_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "dvbt2.h"

namespace dvbt2 {

// Defined in dvbt2fec.cpp and dvbt2ldpc.h (internal)
struct LDPCKernel;
struct LDPCGraph;
struct PostLayout;

//! BCH outer code (6.1.1), on bytes packed MSB first
class MODEMDVB_API BCH
{
public:
    BCH(FECType type, int t);
    int parityBits() const { return m_parityBits; }
    //! msg: kbch / 8 bytes. parity: parityBits / 8 bytes (rounded up: the medium frame's 180 bits, left aligned)
    void encode(const uint8_t *msg, int msgBytes, uint8_t *parity) const;
    //! The same for a message of any number of bits (the medium frame's VL-SNR codes' aren't whole bytes), MSB first
    void encodeBits(const uint8_t *msg, int msgBits, uint8_t *parity) const;
    //! codeword: (kbch + parityBits) / 8 bytes, corrected in place. Returns the number of bits corrected, -1 if it can't
    int decode(uint8_t *codeword, int codewordBytes) const;

private:
    void remainder(const uint8_t *data, int nbBytes, uint8_t *rem) const;
    int mul(int a, int b) const { return (a && b) ? m_exp[m_log[a] + m_log[b]] : 0; }
    static inline uint64_t loadBigEndian64(const uint8_t *p);

    static const int m_remWords = 3;    //!< The remainder's 64-bit words: up to 192 parity bits (t = 12 of 16 bits)
    static const int m_maxT = 12;
    //! Generator polynomial factors of each frame size (table 7), as the powers of x with non-zero coefficients
    static const std::vector<std::vector<int>> m_bchNormalPolys;
    static const std::vector<std::vector<int>> m_bchMediumPolys;
    static const std::vector<std::vector<int>> m_bchShortPolys;

    int m_m;            //!< GF(2^m)
    int m_t;
    int m_parityBits;
    int m_parityBytes;
    //! For division 8 bytes at a time: [byte k of 8][value][word], the remainder of the value followed by 7 - k zero
    //! bytes, left aligned in the words (the coefficient of x^(P-1) the MSB of the first)
    std::vector<uint64_t> m_slices;
    std::vector<int> m_exp;         //!< Doubled, so the sum of two logs can be looked up
    std::vector<int> m_log;
};

//! Addresses of parity bit accumulators (annexes A and B). rows: for each group of 360 information bits, the
//! number of addresses, then the addresses for the group's first bit
struct LDPCTable
{
    FECType m_type;
    CodeRate m_rate;
    int m_n;
    int m_k;
    const uint16_t *m_rows;
};

//! Null for a combination that doesn't exist in the family
MODEMDVB_API const LDPCTable *ldpcTable(FECType type, CodeRate rate, CodeFamily family = CodeFamily::T2);
//! The S2 codes not shared with T2 (dvbs2ldpctables.cpp), or null
MODEMDVB_API const LDPCTable *ldpcTableS2(FECType type, CodeRate rate);
//! The S2X codes (dvbs2xldpctables.cpp), or null
MODEMDVB_API const LDPCTable *ldpcTableS2X(FECType type, CodeRate rate);

//! LDPC inner code (6.1.2). LLRs are int8, positive for 0, twice the actual LLR. Decoded with layered min-sum
//! (dvbt2ldpckernel.h): AVX2, SSE4.1 or NEON if the CPU has it, else portable C++
class MODEMDVB_API LDPC
{
public:
    //! A rate the family hasn't for the frame size is given its rate 1/2 (std::invalid_argument thrown if it has none)
    LDPC(FECType type, CodeRate rate, CodeFamily family = CodeFamily::T2);
    ~LDPC();
    int n() const { return m_n; }
    int k() const { return m_k; }
    //! Blocks decoded in parallel
    int lanes() const;
    //! About the memory its decoder holds: the code's graph, about 1 MB for a normal frame's, shared with the other
    //! decoders of the code. The messages (a vector of lanes() for each of the graph's edges and variables, a normal
    //! frame's about 10 MB with AVX2) are each decoding thread's, for whatever code it decodes
    size_t memoryBytes() const;
    //! bits: k info bits, one per byte. parity: n - k bits
    void encode(const uint8_t *bits, uint8_t *parity) const;
    //! Decode up to lanes() blocks of n LLRs in place, with the calling thread's buffers (so threads can decode with
    //! the same LDPC at once). Returns the iterations used (maxIterations + 1 if not converged).
    //! unconverged, if not null: the number of blocks that didn't converge. quickBlocks: a bit for each block not worth
    //! more than a few iterations (probably damaged), given up then, so it doesn't hold up the others
    int decode(int8_t *const *blocks, int nbBlocks, int maxIterations = 25, int *unconverged = nullptr,
        uint64_t quickBlocks = 0);

private:
    static const LDPCKernel& chooseLDPCKernel();
    template <class F>
    static void forEachInfoBit(const LDPCTable& table, F f);
    static void buildGraph(const LDPCTable& table, LDPCGraph& graph);
    static std::shared_ptr<const LDPCGraph> sharedGraph(const LDPCTable& table);
    static size_t align64(size_t bytes);

    struct Private;
    std::unique_ptr<Private> m_private;
    const LDPCTable *m_table;
    int m_n;
    int m_k;
};

//! Bit interleaver (6.1.3) and demultiplexer (6.2.1): codeword bit index for each bit of each cell
//! (cell q, bit e at q * eta + e). Also used for L1-post with no parity interleaving or twist
MODEMDVB_API void cellBitMap(FECType type, CodeRate rate, Constellation constellation, std::vector<int>& map);

//! Constellation mapping (6.2.2) of eta bits, one per byte, to a normalised cell
MODEMDVB_API Cf mapCell(const uint8_t *bits, int eta);
// The channel LLRs (in nats) given to LDPC are limited to this. Its messages are limited to 31 and the variables'
// sums of them to 127, so a variable in 3 checks can still overturn its channel LLR, and its sum doesn't saturate. At
// 127, the cells of the heavy tail of real reception's errors (phase noise, interference) came in confidently wrong,
// beyond what 2 or 3 checks could overturn, and saturated the sums: at 25 to 30 dB, LDPC was at its iteration limit,
// with 10 to 20 % of blocks not converging (B210, HackRF), left for BCH. At 32, a B210's and an RSPduo's recordings
// converge in 2 to 3 iterations, every block, and the thresholds in AWGN (QPSK to 256-QAM) are unchanged
static constexpr float llrLimit = 32.0f;

//! Max-log soft demapping of a normalised cell to eta int8 LLRs (positive for 0, as LDPC wants).
//! llrScale: 2 * 2 / noise variance per dimension of the normalised cell, times its reliability
MODEMDVB_API void demapCell(Cf cell, int eta, float llrScale, int8_t *llrs);
//! Separate demapping of the two axes, for rotated constellations, where they have different reliabilities
MODEMDVB_API void demapCellAxes(Cf cell, int eta, float scaleI, float scaleQ, int8_t *llrs);
//! Demapping of many normalised cells (eta 2 to 8), as demapCell(), with SIMD: llrs[bitMap[cell * eta + bit]].
//! errors: each cell's error power to the nearest constellation point. If outlierThreshold > 0, scales of cells
//! with error power times scale / 4 (relative to the noise per dimension) over it are reduced in proportion
MODEMDVB_API void demapCells(const float *re, const float *im, float *scales, int nbCells, int eta, float outlierThreshold,
    const int *bitMap, int8_t *llrs, float *errors);
//! The same with a scale for each axis, as demapCellAxes(). An outlier's error power is relative to each axis's noise,
//! and both its scales are reduced alike
MODEMDVB_API void demapCellsAxes(const float *re, const float *im, float *scalesI, float *scalesQ, int nbCells, int eta,
    float outlierThreshold, const int *bitMap, int8_t *llrs, float *errors);
//! Constellation rotation angle (table 16), radians
MODEMDVB_API float rotationAngle(Constellation constellation);

//! For cellBitMap(), mapCell(), demapCell() and demapCellAxes(), and L1Codec: the demultiplexer's tables, and the
//! constellations' levels and LLRs
class CellMapper
{
private:
    friend MODEMDVB_API void cellBitMap(FECType type, CodeRate rate, Constellation constellation, std::vector<int>& map);
    friend MODEMDVB_API Cf mapCell(const uint8_t *bits, int eta);
    friend MODEMDVB_API void demapCell(Cf cell, int eta, float llrScale, int8_t *llrs);
    friend MODEMDVB_API void demapCellAxes(Cf cell, int eta, float scaleI, float scaleQ, int8_t *llrs);
    friend class L1Codec;

    static const int *demuxTable(CodeRate rate, Constellation constellation, FECType type);
    static float normalisation(int eta);
    static int axisLevel(const uint8_t *bits, int stride, int m);
    static inline int8_t quantise(float value);
    static inline void demapAxis(float value, int m, float scale, int8_t *llrs, int stride);
};

//! Cell interleaver (6.4): d[L(r, q)] = g[q]
class MODEMDVB_API CellInterleaver
{
public:
    explicit CellInterleaver(int ncells);
    int address(int r, int q) const { return (m_l0[q] + shift(r)) % m_ncells; }
    //! Block r's shift. Doesn't change anything (those of a TI-block's blocks are worked out when it's made), so
    //! threads can ask at once
    int shift(int r) const;
    //! The addresses of block 0: address(r, q) is (l0()[q] + shift(r)) mod ncells, each term less than ncells
    const int *l0() const { return m_l0.data(); }

private:
    int nextShift(int& k) const;

    static const int m_maxBlocks = 1024;    //!< A TI-block's FEC blocks: PLP_NUM_BLOCKS_MAX's 10 bits
    int m_ncells;
    int m_nd;
    std::vector<int> m_l0;
    std::vector<int> m_shifts;
    int m_nextK;                            //!< The bit reversed counter after the last of m_shifts
};

//! Time interleaver (6.5.3): the order the cells of a TI-block are transmitted in. read[i] is the index into the
//! cells of the FEC blocks (r * ncells + q) of transmitted cell i
MODEMDVB_API void timeInterleaver(int ncells, int nfec, std::vector<int>& read);

//! L1 signalling FEC (7.3): shortened and punctured 16K LDPC with BCH, and modulation
class MODEMDVB_API L1Codec
{
public:
    L1Codec();
    ~L1Codec();

    //! 200 L1-pre bits to 1840 BPSK cells
    void encodePre(const uint8_t *bits, std::vector<Cf>& cells);
    //! Decode 1840 cells, with per-cell LLR scales. Returns false if the FEC or CRC fails
    bool decodePre(const Cf *cells, const float *llrScales, L1Pre& pre);

    //! L1-post bits (including the CRC), encoded and modulated as signalled in pre. Sets L1_POST_SIZE
    void encodePost(const std::vector<uint8_t>& bits, L1Pre& pre, std::vector<Cf>& cells);
    //! Decode the L1_POST_SIZE cells signalled in pre
    bool decodePost(const Cf *cells, const float *llrScales, const L1Pre& pre, L1Post& post);
    //! The number of L1-post cells (L1_POST_SIZE) for this many information bits (L1_POST_INFO_SIZE)
    static int postCells(int infoBits, L1Constellation constellation, int fftSize);

private:
    struct Block
    {
        int m_ksig;
        int m_npunc;
        std::vector<int> m_info;        //!< LDPC codeword positions of the info bits
        std::vector<int> m_transmitted; //!< LDPC codeword positions of the transmitted bits, in order
    };

    static void layout(int ksig, int kbchL1, int kldpcL1, int nparity, int npunc, const int *groupOrder,
        const int *parityOrder, Block& block);
    bool decodeBlock(const Block& block, LDPC& ldpc, const BCH& bch, int kbchL1, const int8_t *llrs, std::vector<uint8_t>& bits);
    static int postTableIndex(L1Constellation constellation);
    static void packBits(const uint8_t *bits, int nbBits, uint8_t *bytes);
    static void encodeL1Block(const uint8_t *info, int ksig, const std::vector<int>& infoPositions, int kbchL1, int kldpcL1,
        const BCH& bch, const LDPC& ldpc, std::vector<uint8_t>& codeword);
    static PostLayout postLayout(int infoBits, L1Constellation constellation, int fftSize);
    static void postCellBitMap(L1Constellation constellation, int npost, std::vector<int>& map);

    // L1 signalling (7.3)
    static const int m_preKbch = 3072;
    static const int m_preKldpc = 3240;
    static const int m_preKsig = 200;
    static const int m_preNpunc = 11488;
    static const int m_postKbch = 7032;
    static const int m_postKldpc = 7200;
    static const int m_l1BchParity = 168;
    static const int m_preGroups[9];
    static const int m_preParity[36];
    static const int m_postGroups[3][20];
    static const int m_postParity[3][25];

    LDPC m_preLDPC;
    LDPC m_postLDPC;
    BCH m_bch;
    Block m_preBlock;               //!< L1-pre's layout, always the same
    // L1-post's layout and bit map, for the parameters they were last made for
    Block m_postBlock;
    std::vector<int> m_postMap;
    int m_postKey[4] = {-1, -1, -1, -1};    //!< KSIG, NPUNC, NPOST, L1 constellation
};

//! BBFRAMEs (5.1, 5.2) from a transport stream: normal or high efficiency mode, optionally with null packet deletion and
//! (normal mode) ISSY bytes. Also DVB-S2's (EN 302 307 5.1.6), the same BBHEADER, with the roll-off and input stream in
//! MATYPE
class MODEMDVB_API BBFrameEncoder
{
public:
    BBFrameEncoder(FECType type, CodeRate rate, bool highEfficiency = false, CodeFamily family = CodeFamily::T2);
    int kbch() const { return m_kbch; }
    //! MATYPE: by default 0xf0 (TS, single input stream, CCM, no ISSY or null packet deletion, roll-off 0.35) and 0
    void setMatype(uint8_t matype1, uint8_t matype2) { m_matype1 = matype1; m_matype2 = matype2; }
    //! Null packet deletion (5.1.5), and ISSY bytes after each packet (0, 2 or 3; normal mode only; a count, not a real
    //! timestamp), signalled in MATYPE. Before the first packet
    void setOptions(bool nullPacketDeletion, int issyBytes);
    //! Add a TS packet. Returns true when a BBFRAME (kbch / 8 bytes, scrambled) is ready
    bool addPacket(const uint8_t *packet, std::vector<uint8_t>& frame);

private:
    void start();

    int m_kbch;
    bool m_hem;
    uint8_t m_matype1 = 0xf0;
    uint8_t m_matype2 = 0;
    std::vector<uint8_t> m_data;    //!< Data field
    int m_syncd;
    uint8_t m_crc;                  //!< CRC-8 of the previous packet
    bool m_npd = false;
    int m_issyBytes = 0;
    int m_dnp = 0;                  //!< Null packets deleted since the last packet sent
    uint32_t m_issyCount = 0;
};

//! Transport stream from BBFRAMEs (5.1): normal and high efficiency modes, null packet deletion, ISSY
class MODEMDVB_API BBFrameDecoder
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback;

    //! A BBHEADER's fields (5.1.6), from a descrambled BBFRAME
    struct Header
    {
        uint8_t m_matype1 = 0;
        uint8_t m_matype2 = 0;      //!< The input stream identifier, in a multiple input stream
        int m_upl = 0;
        int m_dfl = 0;
        uint8_t m_sync = 0;
        int m_syncd = 0;
        int m_mode = 0;             //!< CRC-8 xor MODE: 0 normal, 1 high efficiency (T2), else the CRC failed

        bool transportStream() const { return (m_matype1 >> 6) == 3; }
        bool singleStream() const { return (m_matype1 & 0x20) != 0; }
        int rollOff() const { return m_matype1 & 3; }   //!< 0 0.35, 1 0.25, 2 0.20 (S2)
    };

    //! The header of a descrambled BBFRAME. Returns false if its CRC fails (mode neither 0 nor 1)
    static bool parseHeader(const uint8_t *frame, Header& header);

    struct Stats
    {
        uint64_t m_frames = 0;
        uint64_t m_headerErrors = 0;    //!< BBHEADER CRC failures
        uint64_t m_packets = 0;
        uint64_t m_nullPackets = 0;     //!< Reinserted
        uint64_t m_crcErrors = 0;       //!< Packet CRC-8 failures, in normal mode (those packets are flagged)
        uint64_t m_genericFrames = 0;   //!< Of a generic stream (headers good): not decoded
        int m_streamType = -1;          //!< MATYPE-1's TS/GS field of the last good header, -1 none yet
    };

    //! MATYPE-1's TS/GS field: 0 generic packetized, 1 generic continuous (in DVB-S2, as GSE is carried), 2 GSE (T2,
    //! and S2X's GSE-HEM), 3 transport stream
    static const char *streamTypeName(int type);

    void setPacketCallback(PacketCallback callback) { m_callback = callback; }
    //! DVB-S2 has no high efficiency mode: a CRC-8 that's 1 out is an error
    void setHighEfficiencyAllowed(bool allowed) { m_hemAllowed = allowed; }
    //! A descrambled BBFRAME of kbch / 8 bytes. bad: BCH failed, so the packets are flagged as errored
    void process(const uint8_t *frame, int kbch, bool bad);
    //! BBFRAMEs were lost before the next: the packet being assembled can't be completed from it
    void discontinuity();
    void reset();
    //! The end of the stream: the packet held for the next's CRC-8 is output as it is
    void flush();
    const Stats& stats() const { return m_stats; }

private:
    void outputPacket(bool bad);
    void releaseHeld();

    PacketCallback m_callback;
    bool m_hemAllowed = true;
    std::vector<uint8_t> m_packet;  //!< Current user packet being assembled
    int m_packetBits = 0;           //!< Bits of it received
    bool m_synced = false;
    bool m_packetBad = false;
    int m_upl = 0;
    bool m_hem = false;
    bool m_npd = false;
    int m_issyBytes = 0;
    uint8_t m_lastCrc = 0;
    bool m_lastCrcValid = false;    //!< m_lastCrc is of the packet before the one being assembled
    bool m_firstByteBad = false;    //!< The packet being assembled began in a BBFRAME that BCH didn't correct
    //! In normal mode, the last packet, until the next's CRC-8 of it says whether it's errored
    uint8_t m_held[188];
    bool m_heldValid = false;
    Stats m_stats;
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2FEC_H
