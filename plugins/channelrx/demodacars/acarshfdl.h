///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// The HFDL constants and conventions - the physical layer parameters and the    //
// MPDU, LPDU and SPDU layouts - were cross-checked against dumphfdl by Tomasz   //
// Lemiech, https://github.com/szpajder/dumphfdl, GPL-3.0, and the ARINC 635     //
// system table of ground station names, positions and frequency assignments     //
// is the one dumphfdl ships.                                                    //
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

#ifndef INCLUDE_ACARSHFDL_H
#define INCLUDE_ACARSHFDL_H

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "util/crc.h"

// HFDL (High Frequency Data Link) receiver, ICAO Annex 10 Volume III Part I Chapter 11
// and ARINC 635. Constants and conventions cross-checked against dumphfdl
// (github.com/szpajder/dumphfdl, GPL-3.0), the reference implementation.
//
// The air interface is M-PSK at 1800 symbols/s on a 1440 Hz subcarrier above the
// published (suppressed) carrier frequency. A burst is:
//
//   prekey (448 unmodulated symbols)
//   A A               two copies of a 127 bit BPSK synchronisation sequence
//   M1 M2             a 127 bit maximal length sequence whose cyclic shift (8 choices)
//                     encodes the bit rate (300/600/1200/1800 bps) and slot length
//                     (single/double), then its first 15 bits again
//   9 x T             a 15 bit BPSK training sequence, nine times
//   [ 30 data + T ]   data frames of 30 symbols, each followed by one training
//                     sequence, 72 frames (single slot) or 168 (double slot)
//
// Data symbols are BPSK/4PSK/8PSK by bit rate, Gray coded onto absolute (coherent)
// phase. Each data symbol's phase is flipped by pi when a scrambler bit is one: an
// additive x^15 + x + 1 LFSR seeded with 0x6959, restarted every 120 symbols (each
// slot's data length is a multiple of 120, so every burst starts the sequence fresh).
// The user bits are convolutionally encoded (K=7, rate 1/2, polynomials 0x6D/0x4F);
// at 300 bps each encoded chip is transmitted twice (rate 1/4). Chips are interleaved
// through a 40 row table filled down columns (advancing 17 or 23 columns per push,
// single/double slot) and read out skipping 9 rows per pop. The decoded octets (bits
// transmitted LSB first within each octet) form MPDUs (bit 0 of the first octet set)
// or SPDUs (squitters). An MPDU carries LPDUs, each with a CRC-16/X-25 FCS; the MPDU
// header has its own FCS. An LPDU of type 0x0D/0x1D holding an HFNPDU of type 0xFF
// ("enveloped data", i.e. FF FF) carries an ACARS message from the mode character
// onwards - which is what the rest of the ACARS demodulator plugin consumes.
//
// The receiver runs at 4 samples per symbol: matched filter, differential correlation
// acquisition on the first A sequence (CFO-immune, amplitude weighted - the same
// approach as the VDL-2 receiver in acarsvdl2.h), then a coherent decision-directed
// pass through the burst with the reference phasor data-aided over every known
// training sequence, fractional (interpolated) Gardner symbol timing, per-bit
// soft decisions and a soft-decision Viterbi decoder.

#define ACARSHFDL_SYMBOL_RATE 1800
#define ACARSHFDL_SAMPLES_PER_SYMBOL 4
#define ACARSHFDL_CHANNEL_SAMPLE_RATE (ACARSHFDL_SYMBOL_RATE * ACARSHFDL_SAMPLES_PER_SYMBOL)
#define ACARSHFDL_SUBCARRIER_HZ 1440
#define ACARSHFDL_RRC_ALPHA 0.4

#define ACARSHFDL_PREKEY_LEN 448
#define ACARSHFDL_A_LEN 127
#define ACARSHFDL_M1_LEN 127
#define ACARSHFDL_M2_LEN 15
#define ACARSHFDL_T_LEN 15
#define ACARSHFDL_EQ_TRAIN_CNT 9
#define ACARSHFDL_DATA_FRAME_LEN 30
#define ACARSHFDL_MODE_CNT 8
#define ACARSHFDL_DEINT_ROWS 40
#define ACARSHFDL_DEINT_POP_ROW_SHIFT 9

class AcarsHfdlReceiver
{
public:
    typedef std::complex<double> Cd;

    struct Config
    {
        // Normalised differential correlation against the A sequence needed to sync,
        // 0 to 1. 126 products, so noise reads around 0.07; a genuine preamble reads
        // near 1 on a clean channel.
        double m_syncThreshold = 0.35;
        // Normalised coherent correlation (|sum y s| / sum |y|) required for the second
        // A sequence and the M1 rate sequence. Coherent matching works several dB below
        // hard bit agreement (dumphfdl's approach): the correlation magnitude is blind
        // to the BPSK pi ambiguity, its argument re-anchors the carrier reference, and
        // the noise-only statistic over 127 samples reads about 0.08 for the single A
        // template but 0.15-0.22 for the max over the 8 M1 shifts, hence the higher M1
        // threshold - a noise M1 accept costs a whole bogus burst of deaf time.
        double m_a2Threshold = 0.25;
        double m_m1Threshold = 0.32;
        // Abandon a burst when this fraction of its training bits disagree (checked
        // once at least 135 training bits have been seen): a genuine burst at the
        // decodable limit reads well under 20 percent, garbage reads 50, and a bogus
        // burst otherwise stays deaf for thousands of symbols
        double m_trainAbortFraction = 0.4;
        // T/2-spaced NLMS equalizer, trained on the known sequences (M2 and every T
        // training sequence - 135 symbols before the data and 15 more every 30 data
        // symbols), frozen over data so decision errors cannot propagate into the
        // taps. Causal (cursor on the newest tap), so it corrects post-cursor ISI -
        // the delayed ray of an HF multipath channel. It runs as a SECOND LANE with
        // its own carrier reference driven by equalized residuals; the raw lane stays
        // byte-identical to the receiver without an equalizer, and the FCS arbitrates
        // (raw first, equalized retry), so the equalizer can only add bursts, never
        // lose them - on a clean channel 15 noisy taps otherwise cost about a dB.
        // m_eqTrackRatio scales the step after the initial training; 1.0 (no gearing)
        // measures best - fast tracking follows fading between training sequences,
        // and the raw lane already protects clean channels from tap noise. 0 taps
        // disables the equalizer entirely. m_eqDelay/m_eqDelay2 are the decision
        // delays in symbols of two independent equalizer lanes: the equalized
        // decision targets the symbol this many symbols behind the newest sample in
        // the line, so the leading taps see the decision instant's FUTURE and can
        // cancel pre-cursor ISI - the early ray of a multipath channel when the
        // timing locked to the later one. No single delay suits every delay spread
        // (measured: 0-1 for 0.5-1 ms paths, 4 for 2 ms), so two lanes run and the
        // FCS arbitration picks whichever decodes. -1 disables the second lane.
        int m_eqTaps = 15;
        double m_eqMu = 0.2;
        double m_eqTrackRatio = 1.0;
        int m_eqDelay = 1;
        int m_eqDelay2 = 4;
        // Channel-interpolation lane: the burst is buffered and the complex channel
        // gain measured over every 15 symbol training sequence (one per 45 symbols =
        // 40 Hz, above Nyquist for the fastest HF fading) is interpolated across the
        // data frames between them - a non-causal estimate that follows fading too
        // fast for the causal tracking loops, such as auroral flutter. Runs as a
        // third decode attempt arbitrated by the FCS, so it can only add bursts.
        bool m_interpEnable = true;
        // Two-tap variant of the interpolation lane, for fading COMBINED with delay
        // spread: each anchor least-squares fits a symbol-spaced [h0 h1] channel,
        // both taps interpolate across the data, and the data demodulates by
        // decision feedback (each data frame starts right after known training, so
        // the feedback seeds from a known symbol). A fourth FCS-arbitrated attempt.
        bool m_interp2Enable = true;
        // Turbo re-estimation: when the ordinary attempts fail, the interpolation
        // lane's Viterbi output is re-encoded into the transmitted chip stream and
        // every symbol - training and re-encoded data alike - becomes virtual
        // training for a dense sliding-window channel estimate; the soft chips are
        // re-derived against it and decoded again. The code cleans up most of the
        // first pass's errors, and the residue averages out inside the window.
        bool m_turboEnable = true;
        bool m_turbo2Enable = true; // Its two-tap variant, for fading + delay spread
        int m_turboWindow = 10;     // Half width of the estimation window, symbols
        // Skip the expensive retry lanes (interpolation, turbo, header repair) when
        // this fraction of the burst's training bits was wrong: measured on air, no
        // burst beyond about 22 percent has ever been recovered - those are slot
        // collisions - and the full retry chain costs 100+ ms of the demodulator
        // thread's time, which matters at accelerated file replay. 0 disables the
        // limit (every failed burst runs the full chain).
        double m_retryLimitFraction = 0.30;
        // Decision directed per-symbol frequency update on the phase residual. Applied
        // scaled by (1 - the reference blend in use), as the loop originally had it in
        // effect - see updateRefLane()
        double m_freqGain = 0.01;
        // Reference phasor blending for decision-directed symbols...
        double m_refGain = 0.15;
        // ... and for known (training) symbols, where the modulation is certain
        double m_trainGain = 0.4;
        // Fractional Gardner timing gain; 0 disables
        double m_timingGain = 0.02;
        // Dump acquisition and symbol decisions for this many bursts (test harness)
        int m_debugBursts = 0;
    };

    struct Stats
    {
        uint64_t m_samples = 0;
        uint64_t m_syncs = 0;           // A1 correlation fires
        uint64_t m_a2Ok = 0;
        uint64_t m_a2Fail = 0;
        uint64_t m_m1Ok = 0;
        uint64_t m_m1Fail = 0;
        uint64_t m_burstsDecoded = 0;   // Data blocks run through the Viterbi decoder
        uint64_t m_mpduFcsOk = 0;
        uint64_t m_mpduFcsBad = 0;
        uint64_t m_lpduFcsOk = 0;
        uint64_t m_lpduFcsBad = 0;
        uint64_t m_spdus = 0;
        uint64_t m_acarsFrames = 0;
        uint64_t m_otherFrames = 0;
        uint64_t m_trainBitsTotal = 0;
        uint64_t m_trainBitsBad = 0;
        uint64_t m_trainAborts = 0;     // Bursts abandoned by the training liveness check
        uint64_t m_eqRetries = 0;       // Bursts retried on the other (raw/equalized) path
        uint64_t m_eqRetryRecovered = 0;
        uint64_t m_interpRetries = 0;   // Bursts retried on the channel-interpolation lane
        uint64_t m_interpRecovered = 0; // ... where that attempt contributed frames
        uint64_t m_interp2Retries = 0;  // ... and on its two-tap (fading + ISI) variant
        uint64_t m_interp2Recovered = 0;
        uint64_t m_headerRepairs = 0;   // MPDU headers recovered by trial bit flips
        uint64_t m_turboRetries = 0;    // Bursts retried with turbo re-estimation
        uint64_t m_turboRecovered = 0;
        uint64_t m_turbo2Retries = 0;   // ... and its two-tap variant
        uint64_t m_turbo2Recovered = 0;
        uint64_t m_preempts = 0;        // Bursts restarted by a stronger detection
        uint64_t m_m1Modes[ACARSHFDL_MODE_CNT] = {0};   // M1 matches by rate/slot mode -
                                        // failed bursts still reveal their bit rate here
    };

    // One decoded LPDU (or SPDU), plus the ACARS payload when it carries one
    struct Frame
    {
        std::vector<uint8_t> m_bytes;   // LPDU (without FCS) or raw SPDU
        std::string m_type;             // "Unnumbered data", "Logon confirm", "SPDU", ...
        uint8_t m_typeId = 0;           // The LPDU type octet m_type names; 0 for a squitter
        bool m_isSquitter = false;      // A squitter has no LPDU, so no type octet
        bool m_uplink = false;          // Ground to air
        uint32_t m_srcId = 0;           // Aircraft ID (downlink) or ground station ID
        uint32_t m_dstId = 0;
        bool m_isAcars = false;
        uint32_t m_acarsOffset = 0;     // Offset of the ACARS mode character in m_bytes
        int m_bitRate = 0;              // 300/600/1200/1800 bps
        bool m_doubleSlot = false;
    };

    AcarsHfdlReceiver()
    {
        designRrc();
        reset();
    }

    void configure(const Config& config)
    {
        m_config = config;
        reset();
    }

    const Config& config() const { return m_config; }
    const Stats& stats() const { return m_stats; }

    void reset()
    {
        m_state = StateSearch;
        std::fill(m_mfDelay, m_mfDelay + MF_TAPS, Cd(0.0, 0.0));
        m_mfIdx = 0;
        std::fill(m_ring, m_ring + RING_LEN, Cd(0.0, 0.0));
        m_ringIdx = 0;
        m_ringFill = 0;
        m_sclk = 0;
        m_mu = 0.0;
        m_statPrev1 = 0.0;
        m_prevAbsD = 0.0;
        m_syncPending = false;
        m_bestAbsD = 0.0;
        m_bestPrevAbsD = 0.0;
        m_bestNextAbsD = 0.0;
        m_bestAgo = 0;
        m_bestD = Cd(0.0, 0.0);
        m_burstAbsD = 0.0;
        m_lastSyncError = 0.0;
        m_ref = Cd(1.0, 0.0);
        m_dphiSym = 0.0;
        m_magMean = 0.0;
        m_eqScale = 1.0;
        m_symbolsSinceSync = 0;
        m_debugRemaining = m_config.m_debugBursts;
        m_debugThisBurst = false;
        m_frames.clear();
        resetBurst();
    }

    bool hasFrame() const { return !m_frames.empty(); }

    Frame popFrame()
    {
        Frame f = m_frames.front();
        m_frames.pop_front();
        return f;
    }

    double lastSyncError() const { return m_lastSyncError; }
    bool synced() const { return m_state == StateBurst; }

    // ---------------------------------------------------------------------------------
    // Constant sequences (from ICAO Annex 10 / dumphfdl)
    // ---------------------------------------------------------------------------------

    // The 127 bit A synchronisation sequence, packed MSB first
    static const uint8_t* aSequenceOctets()
    {
        static const uint8_t octets[16] = {
            0x5B, 0xBC, 0x74, 0x57, 0x03, 0xD9, 0x89, 0x39,
            0xF2, 0x08, 0xD5, 0x36, 0x94, 0x2C, 0x32, 0xFE
        };
        return octets;
    }

    static int aBit(int i)
    {
        return (aSequenceOctets()[i >> 3] >> (7 - (i & 7))) & 1;
    }

    // The 127 bit maximal-length sequence M1 is built from; the cyclic shift applied
    // to it encodes the bit rate and slot length
    static const uint8_t* m1Bits()
    {
        static const uint8_t bits[ACARSHFDL_M1_LEN] = {
            0,1,1,1,0,1,1,0,1,1,1,1,0,1,0,0,0,1,0,1,1,0,0,
            1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,
            0,0,0,1,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,0,0,1,0,0,1,1,
            0,0,0,0,0,1,0,1,0,1,0,1,1,0,1,0,0,1,0,0,1,0,1,0,0,1,
            1,1,1,0,0,1,0,0,0,1,1,0,1,0,1,0,0,0,0,1,1,1,1,1,1,1
        };
        return bits;
    }

    static int m1ShiftForMode(int mode)
    {
        static const int shifts[ACARSHFDL_MODE_CNT] = { 72, 82, 113, 123, 61, 103, 93, 9 };
        return shifts[mode];
    }

    static int m1Bit(int mode, int j)
    {
        return m1Bits()[(m1ShiftForMode(mode) + j) % ACARSHFDL_M1_LEN];
    }

    // The 15 bit training sequence, MSB first from 0x9AF
    static int tBit(int i)
    {
        return (0x9AF >> (14 - i)) & 1;
    }

    struct ModeParams
    {
        int m_arity;            // Bits per symbol (1 = BPSK, 2 = 4PSK, 3 = 8PSK)
        int m_segments;         // Data frames per burst
        int m_codeRate;         // 2 = rate 1/2, 4 = rate 1/4 (chips repeated)
        int m_pushColShift;     // Deinterleaver push column shift
        int m_bitRate;          // Nominal user bit rate, bps
        bool m_doubleSlot;
    };

    static const ModeParams& modeParams(int mode)
    {
        static const ModeParams params[ACARSHFDL_MODE_CNT] = {
            { 1,  72, 4, 17,  300, false },
            { 1,  72, 2, 17,  600, false },
            { 2,  72, 2, 17, 1200, false },
            { 3,  72, 2, 17, 1800, false },
            { 1, 168, 4, 23,  300, true },
            { 1, 168, 2, 23,  600, true },
            { 2, 168, 2, 23, 1200, true },
            { 3, 168, 2, 23, 1800, true },
        };
        return params[mode];
    }

    static int userBitCount(int mode)
    {
        const ModeParams& p = modeParams(mode);
        return p.m_segments * ACARSHFDL_DATA_FRAME_LEN * p.m_arity / p.m_codeRate;
    }

    // The scrambler sequence: additive LFSR x^15 + x + 1 seeded with 0x6959,
    // restarted every 120 bits (one bit per data symbol). Initialised as a magic
    // static so concurrent demodulator instances cannot race the first use.
    static uint8_t scramblerBit(int symbolIndex)
    {
        static const std::array<uint8_t, 120> seq = []()
        {
            std::array<uint8_t, 120> s;
            uint16_t lfsr = 0x6959;
            for (int i = 0; i < 120; i++) {
                s[i] = lfsrNext(lfsr);
            }
            return s;
        }();
        return seq[symbolIndex % 120];
    }

    // ---------------------------------------------------------------------------------
    // Encode side, for the test harness generator. Exact inverses of the decode path.
    // ---------------------------------------------------------------------------------

    // K=7 rate 1/2 convolutional encoder (polynomials per libfec viterbi27)
    static void convEncode(const std::vector<uint8_t>& bits, std::vector<uint8_t>& chips)
    {
        chips.clear();
        chips.reserve(bits.size() * 2);
        uint32_t sr = 0;
        for (uint8_t b : bits)
        {
            sr = ((sr << 1) | (b & 1)) & 0x7F;
            chips.push_back(parity7(sr & 0x6D));
            chips.push_back(parity7(sr & 0x4F));
        }
    }

    // Interleaver permutation for a mode: txPos[k] is where the k'th
    // deinterleaver-output (Viterbi input order) chip goes in the transmitted chip
    // stream. Built by simulating the receiver's push and pop walks. All eight modes
    // are computed together inside a magic static, so concurrent demodulator
    // instances cannot race the construction.
    static const std::vector<int>& interleaverTxPos(int mode)
    {
        static const std::array<std::vector<int>, ACARSHFDL_MODE_CNT> perms = []()
        {
            std::array<std::vector<int>, ACARSHFDL_MODE_CNT> all;
            for (int m = 0; m < ACARSHFDL_MODE_CNT; m++)
            {
                const ModeParams& p = modeParams(m);
                int n = p.m_segments * ACARSHFDL_DATA_FRAME_LEN * p.m_arity;
                int cols = n / ACARSHFDL_DEINT_ROWS;
                // Push walk: transmitted chip i lands in cell pushCell[i]
                std::vector<int> cellToTx(n);
                int row = 0, col = 0;
                for (int i = 0; i < n; i++)
                {
                    cellToTx[row * cols + col] = i;
                    row++;
                    if (row == ACARSHFDL_DEINT_ROWS)
                    {
                        row = 0;
                        col++;
                    }
                    col -= p.m_pushColShift;
                    if (col < 0) {
                        col += cols;
                    }
                }
                // Pop walk: Viterbi-order chip k comes from cell popCell[k]
                std::vector<int>& perm = all[m];
                perm.resize(n);
                row = 0;
                col = 0;
                for (int k = 0; k < n; k++)
                {
                    perm[k] = cellToTx[row * cols + col];
                    row = (row + ACARSHFDL_DEINT_POP_ROW_SHIFT) % ACARSHFDL_DEINT_ROWS;
                    if (row == 0) {
                        col++;
                    }
                }
            }
            return all;
        }();
        return perms[mode];
    }

    // Gray code a phase index (both directions, standard binary-reflected)
    static int grayEncode(int v) { return v ^ (v >> 1); }
    static int grayDecode(int v)
    {
        int r = v;
        for (int s = 1; s < 8; s <<= 1) {
            r ^= r >> s;
        }
        return r;
    }

    // Build the complete symbol sequence of a burst (unit amplitude phasors) carrying
    // the given PDU. Returns false if the PDU does not fit in the mode's user bits.
    static bool encodeBurst(int mode, const std::vector<uint8_t>& pdu, std::vector<Cd>& symbols)
    {
        const ModeParams& p = modeParams(mode);
        int userBits = userBitCount(mode);
        if ((int) pdu.size() * 8 + 6 > userBits) {   // 6 zero bits flush the encoder
            return false;
        }

        // PDU octets, bits LSB first, zero padded to the block size
        std::vector<uint8_t> bits;
        bits.reserve(userBits);
        for (uint8_t oct : pdu)
        {
            for (int b = 0; b < 8; b++) {
                bits.push_back((oct >> b) & 1);
            }
        }
        bits.resize(userBits, 0);

        std::vector<uint8_t> chips;
        convEncode(bits, chips);

        // Rate 1/4 duplicates every chip; the copies are adjacent in Viterbi order
        std::vector<uint8_t> e;
        if (p.m_codeRate == 4)
        {
            e.reserve(chips.size() * 2);
            for (uint8_t c : chips)
            {
                e.push_back(c);
                e.push_back(c);
            }
        }
        else
        {
            e = chips;
        }

        // Interleave into transmit order
        int n = (int) e.size();
        const std::vector<int>& txPos = interleaverTxPos(mode);
        std::vector<uint8_t> tx(n);
        for (int k = 0; k < n; k++) {
            tx[txPos[k]] = e[k];
        }

        // Assemble the burst
        symbols.clear();
        symbols.reserve(ACARSHFDL_PREKEY_LEN + 2 * ACARSHFDL_A_LEN + ACARSHFDL_M1_LEN
            + ACARSHFDL_M2_LEN + (ACARSHFDL_EQ_TRAIN_CNT + p.m_segments) * ACARSHFDL_T_LEN
            + p.m_segments * ACARSHFDL_DATA_FRAME_LEN);
        auto pushBit = [&symbols](int bit) {
            symbols.push_back(bit ? Cd(-1.0, 0.0) : Cd(1.0, 0.0));
        };
        for (int i = 0; i < ACARSHFDL_PREKEY_LEN; i++) {
            symbols.push_back(Cd(1.0, 0.0));
        }
        for (int r = 0; r < 2; r++)
        {
            for (int i = 0; i < ACARSHFDL_A_LEN; i++) {
                pushBit(aBit(i));
            }
        }
        for (int i = 0; i < ACARSHFDL_M1_LEN; i++) {
            pushBit(m1Bit(mode, i));
        }
        for (int i = 0; i < ACARSHFDL_M2_LEN; i++) {
            pushBit(m1Bit(mode, i));
        }
        auto pushTrain = [&pushBit]() {
            for (int i = 0; i < ACARSHFDL_T_LEN; i++) {
                pushBit(tBit(i));
            }
        };
        for (int t = 0; t < ACARSHFDL_EQ_TRAIN_CNT; t++) {
            pushTrain();
        }
        int m = 1 << p.m_arity;
        int chipIdx = 0;
        int dataSymIdx = 0;
        for (int seg = 0; seg < p.m_segments; seg++)
        {
            for (int s = 0; s < ACARSHFDL_DATA_FRAME_LEN; s++)
            {
                // Group arity chips, MSB first, Gray code onto the phase index
                int sym = 0;
                for (int b = 0; b < p.m_arity; b++) {
                    sym = (sym << 1) | tx[chipIdx++];
                }
                int phaseIdx = grayEncode(sym);
                double phi = 2.0 * M_PI * phaseIdx / m;
                if (scramblerBit(dataSymIdx++)) {
                    phi += M_PI;
                }
                symbols.push_back(std::polar(1.0, phi));
            }
            pushTrain();
        }
        return true;
    }

    // FCS helper: CRC-16/X-25 appended little endian, as all HFDL FCSs are
    static uint16_t computeFcs(const uint8_t *buf, int len)
    {
        crc16x25 crc;
        crc.init();
        crc.calculate(const_cast<uint8_t*>(buf), len);
        return (uint16_t) crc.get();
    }

    static void appendFcs(std::vector<uint8_t>& v, size_t from = 0)
    {
        uint16_t fcs = computeFcs(v.data() + from, (int) (v.size() - from));
        v.push_back(fcs & 0xFF);
        v.push_back((fcs >> 8) & 0xFF);
    }

    static std::vector<uint8_t> buildAcarsLpdu(const std::vector<uint8_t>& acarsFromModeChar)
    {
        // Unnumbered data LPDU, "enveloped data" HFNPDU (FF FF), then the ACARS
        // message starting with its SOH - the same 01 that VDL-2 carries after FF FF
        std::vector<uint8_t> lpdu = { 0x0D, 0xFF, 0xFF, 0x01 };
        lpdu.insert(lpdu.end(), acarsFromModeChar.begin(), acarsFromModeChar.end());
        appendFcs(lpdu);
        return lpdu;
    }

    // The 24 bit ICAO address in logon LPDUs is carried with each octet bit-reversed,
    // most significant octet first (dumphfdl parse_icao_hex)
    static uint8_t reverseByte(uint8_t v)
    {
        v = (uint8_t) (((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
        v = (uint8_t) (((v & 0xCC) >> 2) | ((v & 0x33) << 2));
        v = (uint8_t) (((v & 0xAA) >> 1) | ((v & 0x55) << 1));
        return v;
    }

    static uint32_t parseIcao(const uint8_t *buf)
    {
        return ((uint32_t) reverseByte(buf[0]) << 16)
             | ((uint32_t) reverseByte(buf[1]) << 8)
             | (uint32_t) reverseByte(buf[2]);
    }

    static std::vector<uint8_t> buildLogonRequestLpdu(uint32_t icao)
    {
        std::vector<uint8_t> lpdu = { 0x8F,
            reverseByte((uint8_t) ((icao >> 16) & 0xFF)),
            reverseByte((uint8_t) ((icao >> 8) & 0xFF)),
            reverseByte((uint8_t) (icao & 0xFF)) };
        appendFcs(lpdu);
        return lpdu;
    }

    static std::vector<uint8_t> buildLogonConfirmLpdu(uint32_t icao, uint8_t acId)
    {
        std::vector<uint8_t> lpdu = { 0x9F,
            reverseByte((uint8_t) ((icao >> 16) & 0xFF)),
            reverseByte((uint8_t) ((icao >> 8) & 0xFF)),
            reverseByte((uint8_t) (icao & 0xFF)),
            acId, 0x00, 0x00, 0x00 };
        appendFcs(lpdu);
        return lpdu;
    }

    // Downlink MPDU: aircraft to ground, carrying the given LPDUs (which already
    // include their FCS)
    static std::vector<uint8_t> buildDownlinkMpdu(uint8_t gsId, uint8_t acId,
        const std::vector<std::vector<uint8_t>>& lpdus)
    {
        std::vector<uint8_t> mpdu;
        mpdu.push_back((uint8_t) (0x03 | ((lpdus.size() & 0xF) << 2)));
        mpdu.push_back(gsId & 0x7F);
        mpdu.push_back(acId);
        mpdu.push_back(0x00);   // Reserved/priority octets not parsed by the receiver
        mpdu.push_back(0x00);
        mpdu.push_back(0x00);
        for (const auto& l : lpdus) {
            mpdu.push_back((uint8_t) (l.size() - 1));
        }
        appendFcs(mpdu);
        for (const auto& l : lpdus) {
            mpdu.insert(mpdu.end(), l.begin(), l.end());
        }
        return mpdu;
    }

    // Uplink MPDU: ground to a single aircraft
    static std::vector<uint8_t> buildUplinkMpdu(uint8_t gsId, uint8_t acId,
        const std::vector<std::vector<uint8_t>>& lpdus)
    {
        std::vector<uint8_t> mpdu;
        mpdu.push_back(0x01);   // MPDU, uplink, 1 aircraft
        mpdu.push_back(gsId & 0x7F);
        mpdu.push_back(acId);
        mpdu.push_back((uint8_t) ((lpdus.size() & 0xF) << 4));
        for (const auto& l : lpdus) {
            mpdu.push_back((uint8_t) (l.size() - 1));
        }
        appendFcs(mpdu);
        for (const auto& l : lpdus) {
            mpdu.insert(mpdu.end(), l.begin(), l.end());
        }
        return mpdu;
    }

    // ---------------------------------------------------------------------------------
    // Sample processing
    // ---------------------------------------------------------------------------------

    // Process one sample at ACARSHFDL_CHANNEL_SAMPLE_RATE, centred on the subcarrier
    // (i.e. the caller has already shifted by ACARSHFDL_SUBCARRIER_HZ from the
    // published channel frequency). Returns true when frames may be available.
    bool processSample(const Cd& in)
    {
        m_stats.m_samples++;
        Cd s = matchedFilter(in);
        size_t framesBefore = m_frames.size();

        m_ringIdx = (m_ringIdx + 1) % RING_LEN;
        m_ring[m_ringIdx] = s;
        if (m_ringFill < RING_LEN) {
            m_ringFill++;
        }

        bool haveFill = m_ringFill >= (ACARSHFDL_A_LEN + 1) * ACARSHFDL_SAMPLES_PER_SYMBOL + 4;

        if (m_state == StateSearch)
        {
            if (haveFill) {
                searchPreamble();
            }
        }
        else // StateBurst
        {
            // Until the burst is validated by the M1 match, the search keeps running
            // so a stronger preamble can preempt a bogus window (see searchPreamble)
            bool synced = false;
            if (haveFill && ((m_frState == FrA2) || (m_frState == FrM1))) {
                synced = searchPreamble();
            }
            if (!synced && (++m_sclk >= ACARSHFDL_SAMPLES_PER_SYMBOL))
            {
                m_sclk = 0;
                processSymbol();
            }
        }
        return m_frames.size() != framesBefore;
    }

private:
    static const int MF_SPAN = 8;   // RRC matched filter span, symbols
    static const int MF_TAPS = MF_SPAN * ACARSHFDL_SAMPLES_PER_SYMBOL + 1;
    // The ring must hold the whole A sequence plus interpolation margin
    static const int RING_LEN = (ACARSHFDL_A_LEN + 2) * ACARSHFDL_SAMPLES_PER_SYMBOL + 8;

    enum State
    {
        StateSearch,
        StateBurst
    };

    enum FrameState
    {
        FrA2,           // Second A sequence: verify and refine
        FrM1,           // Rate/slot sequence
        FrM2,           // First 15 bits of M1 again: skip
        FrTrain,        // The 9 initial training sequences
        FrData,         // A 30 symbol data frame
        FrSegTrain      // The training sequence after each data frame
    };

    Config m_config;
    Stats m_stats;

    // Matched filter
    double m_rrc[MF_TAPS];
    Cd m_mfDelay[MF_TAPS];
    int m_mfIdx;

    // Sample ring for correlation and fractional interpolation
    Cd m_ring[RING_LEN];
    int m_ringIdx;
    int m_ringFill;

    // Symbol clock
    int m_sclk;
    double m_mu;

    // Acquisition (same |D| peak-hold approach as the VDL-2 receiver)
    double m_statPrev1;
    double m_prevAbsD;
    bool m_syncPending;
    double m_bestAbsD;
    double m_bestPrevAbsD;
    double m_bestNextAbsD;
    int m_bestAgo;
    Cd m_bestD;
    double m_burstAbsD;         // |D| of the sync that started the current burst
    double m_lastSyncError;

    // Carrier: reference phasor for the 0-phase axis, advanced by m_dphiSym per symbol
    Cd m_ref;
    double m_dphiSym;
    double m_magMean;
    uint64_t m_symbolsSinceSync;

    // Burst state
    State m_state;
    FrameState m_frState;
    int m_symWanted;                // Symbols remaining in the current frame section
    int m_mode;                     // M1 match, -1 until known
    int m_trainSeqLeft;
    int m_segmentsLeft;
    int m_dataSymIdx;               // Index into the scrambler sequence
    std::vector<Cd> m_corrSamples;  // Constellation-frame samples of the A2/M1 windows
    std::vector<uint8_t> m_softChips;   // Soft bits in transmit order, 0..255 (255 = strong 1)
    int m_trainBadThisBurst;
    int m_trainTotalThisBurst;

    // Channel-interpolation lane: every symbol from M2 onward, pre-rotated by the
    // acquisition frequency estimate so anchors only measure the fading channel
    struct InterpSym
    {
        Cd m_y;
        int8_t m_known;     // Training bit, or -1 for data
        bool m_isData;
        bool m_flip;        // Scrambler flip of a data symbol
    };
    std::vector<InterpSym> m_interpBuf;
    Cd m_interpRot;
    double m_interpDphi;

    // Equalizer lane: taps and T/2-spaced delay line (newest first), input scale
    // frozen at sync so the taps train toward a unit constellation, plus the lane's
    // own carrier state (forked from the raw lane when the mode is known)
    // One independent equalizer lane: taps and T/2-spaced delay line (newest first),
    // its own carrier state, training error count and soft chip stream, deciding at
    // its own delay behind the newest sample
    struct EqLane
    {
        std::vector<Cd> m_taps;
        std::vector<Cd> m_line;
        Cd m_ref;
        double m_dphiSym;
        double m_magMean;
        int m_trainBad;
        int m_delay;
        std::vector<uint8_t> m_chips;
    };
    std::vector<EqLane> m_eqLanes;
    double m_eqScale;

    // Per-symbol metadata queued for the lanes' delayed decisions (shared; each lane
    // indexes it at its own delay)
    struct LaneMeta
    {
        int m_knownBit;     // -1 when not a training symbol
        bool m_isData;
        bool m_flip;
    };
    std::deque<LaneMeta> m_laneMeta;

    // Output
    std::deque<Frame> m_frames;

    // Debug
    int m_debugRemaining;
    bool m_debugThisBurst;
    int m_debugSymbol = 0;

    // ---------------------------------------------------------------------------------

    void designRrc()
    {
        const double alpha = ACARSHFDL_RRC_ALPHA;
        const int sps = ACARSHFDL_SAMPLES_PER_SYMBOL;
        double sum = 0.0;
        for (int i = 0; i < MF_TAPS; i++)
        {
            double t = (i - MF_TAPS / 2) / (double) sps;
            double v;
            if (std::abs(t) < 1e-9) {
                v = 1.0 - alpha + 4.0 * alpha / M_PI;
            } else if (std::abs(std::abs(4.0 * alpha * t) - 1.0) < 1e-6) {
                v = (alpha / std::sqrt(2.0))
                    * ((1.0 + 2.0 / M_PI) * std::sin(M_PI / (4.0 * alpha))
                     + (1.0 - 2.0 / M_PI) * std::cos(M_PI / (4.0 * alpha)));
            } else {
                v = (std::sin(M_PI * t * (1.0 - alpha))
                    + 4.0 * alpha * t * std::cos(M_PI * t * (1.0 + alpha)))
                    / (M_PI * t * (1.0 - 16.0 * alpha * alpha * t * t));
            }
            m_rrc[i] = v;
            sum += v;
        }
        for (int i = 0; i < MF_TAPS; i++) {
            m_rrc[i] /= sum;
        }
    }

    Cd matchedFilter(const Cd& in)
    {
        m_mfDelay[m_mfIdx] = in;
        Cd acc(0.0, 0.0);
        int idx = m_mfIdx;
        for (int i = 0; i < MF_TAPS; i++)
        {
            acc += m_mfDelay[idx] * m_rrc[i];
            idx = (idx == 0) ? MF_TAPS - 1 : idx - 1;
        }
        m_mfIdx = (m_mfIdx + 1) % MF_TAPS;
        return acc;
    }

    Cd tap(int back) const
    {
        return m_ring[(m_ringIdx + RING_LEN - back) % RING_LEN];
    }

    // Catmull-Rom interpolation at (now - 2 - d + mu), strictly causal
    Cd interpAt(int d, double mu) const
    {
        Cd ym1 = tap(d + 3);
        Cd y0 = tap(d + 2);
        Cd y1 = tap(d + 1);
        Cd y2 = tap(d);
        Cd a = -0.5 * ym1 + 1.5 * y0 - 1.5 * y1 + 0.5 * y2;
        Cd b = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
        Cd c = 0.5 * (y1 - ym1);
        return ((a * mu + b) * mu + c) * mu + y0;
    }

    // ---------------------------------------------------------------------------------
    // Acquisition: differential correlation against the A sequence. The BPSK products
    // s(k) conj(s(k-1)) read +-e^{j dphi}, the sign following whether consecutive A
    // bits are equal, so correlating with those signs is CFO-immune; arg(D) is the
    // per-symbol carrier advance. Same peak-hold-by-|D| machinery as VDL-2.
    // ---------------------------------------------------------------------------------

    // Returns true when a new burst was started this sample
    bool searchPreamble()
    {
        const int sps = ACARSHFDL_SAMPLES_PER_SYMBOL;

        Cd d(0.0, 0.0);
        double norm = 0.0;
        for (int k = 1; k < ACARSHFDL_A_LEN; k++)
        {
            int back = (ACARSHFDL_A_LEN - 1 - k) * sps;
            Cd prod = tap(back) * std::conj(tap(back + sps));
            double sign = (aBit(k) == aBit(k - 1)) ? 1.0 : -1.0;
            d += prod * sign;
            norm += std::abs(prod);
        }
        double stat = (norm > 1e-12) ? std::abs(d) / norm : 0.0;
        m_lastSyncError = stat;
        double absD = std::abs(d);

        bool detected = stat >= m_config.m_syncThreshold;

        if (!m_syncPending)
        {
            if (detected)
            {
                m_syncPending = true;
                m_bestAbsD = absD;
                m_bestPrevAbsD = m_prevAbsD;
                m_bestNextAbsD = 0.0;
                m_bestAgo = 0;
                m_bestD = d;
            }
            m_prevAbsD = absD;
            m_statPrev1 = stat;
            return false;
        }

        if (absD > m_bestAbsD)
        {
            m_bestAbsD = absD;
            m_bestPrevAbsD = m_prevAbsD;
            m_bestNextAbsD = 0.0;
            m_bestAgo = 0;
            m_bestD = d;
        }
        else
        {
            if (m_bestAgo == 0) {
                m_bestNextAbsD = absD;
            }
            m_bestAgo++;
        }
        m_prevAbsD = absD;
        m_statPrev1 = stat;

        if (detected && (m_bestAgo < sps)) {
            return false;
        }
        m_syncPending = false;

        // The search keeps running through the A2 and M1 stages, so a noise fire that
        // opened a bogus window just before a real preamble cannot hold the receiver
        // deaf through it: a decisively stronger detection preempts. The margin also
        // suppresses the self-refire on the burst's own second A sequence, whose |D|
        // matches the first's.
        if (m_state == StateBurst)
        {
            if (m_bestAbsD <= 1.5 * m_burstAbsD) {
                return false;
            }
            m_stats.m_preempts++;
        }

        // Fractional peak position
        double delta = 0.0;
        if (m_bestNextAbsD > 0.0)
        {
            double denom = m_bestPrevAbsD - 2.0 * m_bestAbsD + m_bestNextAbsD;
            if (denom < -1e-12) {
                delta = 0.5 * (m_bestPrevAbsD - m_bestNextAbsD) / denom;
            }
        }
        delta = std::max(-0.5, std::min(0.5, delta));

        m_dphiSym = refineAcquisitionDphi(std::arg(m_bestD));

        // First following symbol lands one symbol after the peak
        if (delta >= 0.0)
        {
            m_mu = delta;
            m_sclk = m_bestAgo - 2;
        }
        else
        {
            m_mu = 1.0 + delta;
            m_sclk = m_bestAgo - 1;
        }

        // Coherent reference from the whole A sequence, modulation and CFO removed.
        // The sign of the sum resolves the BPSK pi ambiguity (dumphfdl's "bitmask").
        Cd ref(0.0, 0.0);
        for (int k = 0; k < ACARSHFDL_A_LEN; k++)
        {
            int back = m_bestAgo + (ACARSHFDL_A_LEN - 1 - k) * sps;
            double mod = aBit(k) ? -1.0 : 1.0;
            ref += tap(back) * mod * std::polar(1.0, (ACARSHFDL_A_LEN - 1 - k) * m_dphiSym);
        }
        double refMag = std::abs(ref);
        if (refMag > 1e-12) {
            m_ref = ref / refMag;
        } else {
            m_ref = Cd(1.0, 0.0);
        }
        m_magMean = refMag / ACARSHFDL_A_LEN;
        // Freeze the equalizer lane's input scale here so its taps train toward a
        // unit constellation without gain co-adaptation against its amplitude tracker
        m_eqScale = (m_magMean > 1e-12) ? 1.0 / m_magMean : 1.0;

        m_statPrev1 = 0.0;
        m_symbolsSinceSync = 0;

        m_debugThisBurst = m_debugRemaining > 0;
        if (m_debugThisBurst)
        {
            m_debugRemaining--;
            m_debugSymbol = 0;
            printf("hfdl sync: sample=%llu stat=%.3f absD=%.4f ago=%d delta=%.3f mu=%.3f sclk=%d dphiSym=%.5f\n",
                (unsigned long long) m_stats.m_samples, stat, m_bestAbsD, m_bestAgo,
                delta, m_mu, m_sclk, m_dphiSym);
        }

        m_stats.m_syncs++;
        m_burstAbsD = m_bestAbsD;
        m_state = StateBurst;
        resetBurst();
        return true;
    }

    void resetBurst()
    {
        m_frState = FrA2;
        m_symWanted = ACARSHFDL_A_LEN;
        m_mode = -1;
        m_trainSeqLeft = 0;
        m_segmentsLeft = 0;
        m_dataSymIdx = 0;
        m_corrSamples.clear();
        m_softChips.clear();
        m_trainBadThisBurst = 0;
        m_trainTotalThisBurst = 0;
        m_eqLanes.clear();
        if (m_config.m_eqTaps > 0)
        {
            std::vector<int> delays = { std::max(0, m_config.m_eqDelay) };
            if (m_config.m_eqDelay2 >= 0) {
                delays.push_back(m_config.m_eqDelay2);
            }
            for (int d : delays)
            {
                EqLane lane;
                lane.m_taps.assign(m_config.m_eqTaps, Cd(0.0, 0.0));
                lane.m_line.assign(m_config.m_eqTaps, Cd(0.0, 0.0));
                // Spike at the nominal cursor: 2 T/2 taps per symbol of decision delay
                lane.m_taps[std::min(m_config.m_eqTaps - 1, 2 * d)] = Cd(1.0, 0.0);
                lane.m_ref = Cd(1.0, 0.0);
                lane.m_dphiSym = 0.0;
                lane.m_magMean = 1.0;
                lane.m_trainBad = 0;
                lane.m_delay = d;
                m_eqLanes.push_back(lane);
            }
        }
        m_laneMeta.clear();
    }

    // Refine the acquisition's per-symbol rotation by maximising the coherent sum over the
    // whole A sequence, modulation removed - the maximum likelihood estimate given the
    // known symbols. The differential correlation that found the burst multiplies noise
    // by noise and averages only 126 products, so its estimate is far noisier: the
    // coherent sum's error falls as N^-1.5 rather than N^-0.5.
    //
    // It matters because of how the carrier loop uses it. Any error in the rotation
    // leaves the decision-directed reference lagging by error / gain - about seven times
    // the error at the data gain - and the A2 gate, a coherent sum, is where that bites.
    // The loop used to apply only (1 - gain) of the estimate (see updateRefLane()),
    // which by accident shrank a noisy estimate towards zero and so suited the near-zero
    // offsets of real recordings; applying it in full needs it to be accurate instead.
    // Measured: the corrected loop order alone lost 2.5 percent of the LPDUs on the
    // short real recordings, all at the A2 gate; with this as well it gains 5 percent
    // on them, and 4 percent (3249 to 3375) on the overnight Reykjavik capture.
    //
    // A grid of quarter-bin steps around the coarse estimate, then a parabola through
    // the best and its neighbours. Once per sync, so its cost does not matter
    double refineAcquisitionDphi(double coarse) const
    {
        const int sps = ACARSHFDL_SAMPLES_PER_SYMBOL;
        const int n = ACARSHFDL_A_LEN;
        const int span = 8;
        const double step = (2.0 * M_PI / n) / 4.0;

        Cd stripped[ACARSHFDL_A_LEN];
        for (int k = 0; k < n; k++)
        {
            int back = m_bestAgo + (n - 1 - k) * sps;
            stripped[k] = tap(back) * (aBit(k) ? -1.0 : 1.0);
        }

        double power[2 * span + 1];
        int best = span;
        for (int i = 0; i <= 2 * span; i++)
        {
            const double d = coarse + (i - span) * step;
            // Symbol k is (n - 1 - k) symbols before the newest, so rotating it on by
            // that many steps lines every symbol up with the newest one
            const Cd rot = std::polar(1.0, d);
            Cd phasor(1.0, 0.0);
            Cd sum(0.0, 0.0);
            for (int k = n - 1; k >= 0; k--)
            {
                sum += stripped[k] * phasor;
                phasor *= rot;
            }
            power[i] = std::norm(sum);
            if (power[i] > power[best]) {
                best = i;
            }
        }

        double shift = 0.0;
        if ((best > 0) && (best < 2 * span))
        {
            const double denom = power[best - 1] - 2.0 * power[best] + power[best + 1];
            if (denom < -1e-20) {
                shift = std::max(-0.5, std::min(0.5, 0.5 * (power[best - 1] - power[best + 1]) / denom));
            }
        }

        return coarse + ((best - span) + shift) * step;
    }

    void abandonBurst()
    {
        m_state = StateSearch;
        m_sclk = 0;
        m_statPrev1 = 0.0;
        m_syncPending = false;
    }

    // ---------------------------------------------------------------------------------
    // Symbol processing
    // ---------------------------------------------------------------------------------

    // Update a lane's carrier reference with a symbol whose modulation is known or
    // decided: the sample with the modulation removed is blended into the reference.
    //
    // The reference must already have been advanced to this symbol by advanceRef(),
    // BEFORE the symbol was derotated and its residual taken - the order the VDL-2
    // receiver uses. Advancing it here instead, after the residual, meant every residual
    // was measured against last symbol's phase, so the loop carried the per-symbol
    // rotation as a standing lead and the frequency estimate settled at
    // rotation / (1 - gain) rather than at the rotation. That is harmless while the gain
    // never changes, but it switches between the training and the data gains every 15
    // and 30 symbols, so the estimate was always wrong for one of them, and the data
    // symbols carried a phase bias that grew with the tuning error. Measured on
    // generated bursts, 1200 and 1800 bps at 20 dB: nothing at all decoded from 150 Hz
    // of tuning error either way - the 4PSK and 8PSK decisions slipped - and 8PSK was
    // already losing a quarter of its messages at 100 Hz. In this order both decode
    // every message out to 400 Hz.
    //
    // With the gain held constant the two orders are the same second order loop except
    // for the integrator's gain, which the old order scaled by (1 - gain); the frequency
    // updates below keep that scaling, so the loop's noise behaviour is the one the
    // receiver was tuned with. Without it 600 bps lost 6 percent of its messages at the
    // cliff
    static void updateRefLane(Cd& ref, const Cd& yDemod, double gain)
    {
        double mag = std::abs(yDemod);
        if (mag > 1e-12) {
            ref = (1.0 - gain) * ref + gain * (yDemod / mag);
        }
        double refMag = std::abs(ref);
        if (refMag > 1e-12) {
            ref /= refMag;
        }
    }

    // Step a lane's carrier reference on by one symbol's rotation, so it predicts the
    // phase of the symbol about to be derotated. Called once per symbol, first
    static void advanceRef(Cd& ref, double dphiSym)
    {
        ref *= std::polar(1.0, dphiSym);
    }

    void gardner(const Cd& y0)
    {
        if ((m_config.m_timingGain == 0.0) || (m_symbolsSinceSync <= 2)) {
            return;
        }
        const int sps = ACARSHFDL_SAMPLES_PER_SYMBOL;
        Cd yHalf = interpAt(sps / 2, m_mu) * std::polar(1.0, 0.5 * m_dphiSym);
        Cd yPrev = interpAt(sps, m_mu) * std::polar(1.0, m_dphiSym);
        double norm = 0.5 * (std::norm(y0) + std::norm(yPrev)) + 1e-12;
        double e = ((y0 - yPrev) * std::conj(yHalf)).real() / norm;
        e = std::max(-1.0, std::min(1.0, e));
        m_mu -= m_config.m_timingGain * e;
        if (m_mu >= 1.0)
        {
            m_mu -= 1.0;
            m_sclk = -1;
        }
        else if (m_mu < 0.0)
        {
            m_mu += 1.0;
            m_sclk = 1;
        }
    }

    // NLMS equalizer primitives: T/2-spaced delay line, newest sample first
    static void eqPush(EqLane& lane, const Cd& x)
    {
        for (size_t i = lane.m_line.size() - 1; i > 0; i--) {
            lane.m_line[i] = lane.m_line[i - 1];
        }
        lane.m_line[0] = x;
    }

    static Cd eqFilter(const EqLane& lane)
    {
        Cd acc(0.0, 0.0);
        for (size_t i = 0; i < lane.m_taps.size(); i++) {
            acc += lane.m_taps[i] * lane.m_line[i];
        }
        return acc;
    }

    static void eqTrain(EqLane& lane, const Cd& desired, const Cd& yEq, double mu)
    {
        double p = 0.1;
        for (const Cd& v : lane.m_line) {
            p += std::norm(v);
        }
        Cd err = desired - yEq;
        double g = mu / p;
        for (size_t i = 0; i < lane.m_taps.size(); i++) {
            lane.m_taps[i] += g * err * std::conj(lane.m_line[i]);
        }
    }

    // One symbol of one equalizer lane: push the newest T/2 samples (derotated by the
    // lane's own reference), then decide, track and train at the lane's decision delay
    void laneSymbol(EqLane& lane, const Cd& y0)
    {
        const int sps = ACARSHFDL_SAMPLES_PER_SYMBOL;
        // Advanced first, every symbol, warm-up included, so the derotation below and
        // the residual further down are both against this symbol's predicted phase
        advanceRef(lane.m_ref, lane.m_dphiSym);
        Cd xHalf = interpAt(sps / 2, m_mu) * std::conj(lane.m_ref)
            * std::polar(1.0, 0.5 * lane.m_dphiSym) * m_eqScale;
        eqPush(lane, xHalf);
        eqPush(lane, y0 * std::conj(lane.m_ref) * m_eqScale);

        if ((int) m_laneMeta.size() <= lane.m_delay)
        {
            // Warm-up: nothing to decide until the first delayed decision, but the
            // reference has still been advanced above
            return;
        }

        const LaneMeta& meta = m_laneMeta[m_laneMeta.size() - 1 - lane.m_delay];
        int arity = meta.m_isData ? modeParams(m_mode).m_arity : 1;
        int m = 1 << arity;
        double step = 2.0 * M_PI / m;

        Cd yEq = eqFilter(lane);
        Cd ydEq = meta.m_flip ? -yEq : yEq;
        double phi = std::arg(ydEq);
        int phaseIdx = (int) std::lround(phi / step) & (m - 1);
        double residual = phi - phaseIdx * step;
        while (residual > M_PI) { residual -= 2.0 * M_PI; }
        while (residual < -M_PI) { residual += 2.0 * M_PI; }
        lane.m_magMean += 0.02 * (std::abs(yEq) - lane.m_magMean);

        // The raw sample at the delayed decision instant, for the reference blending
        Cd y0d = interpAt(lane.m_delay * sps, m_mu);

        if (meta.m_knownBit >= 0)
        {
            Cd yDemod = meta.m_knownBit ? -y0d : y0d;
            double res = std::arg(yDemod * std::conj(lane.m_ref));
            if (m_config.m_freqGain != 0.0) {
                lane.m_dphiSym += m_config.m_freqGain * (1.0 - m_config.m_trainGain) * res;
            }
            updateRefLane(lane.m_ref, yDemod, m_config.m_trainGain);
            // The known symbols are also the equalizer's training: the taps update
            // here and are frozen over the data frames
            double mu = m_config.m_eqMu;
            if (m_trainTotalThisBurst > ACARSHFDL_EQ_TRAIN_CNT * ACARSHFDL_T_LEN) {
                mu *= m_config.m_eqTrackRatio;
            }
            eqTrain(lane, Cd(meta.m_knownBit ? -1.0 : 1.0, 0.0), yEq, mu);
            if (((std::abs(phi) > M_PI_2) ? 1 : 0) != meta.m_knownBit) {
                lane.m_trainBad++;
            }
        }
        else
        {
            if (m_config.m_freqGain != 0.0) {
                lane.m_dphiSym += m_config.m_freqGain * (1.0 - m_config.m_refGain) * residual;
            }
            double modPhase = phaseIdx * step + (meta.m_flip ? M_PI : 0.0);
            updateRefLane(lane.m_ref, y0d * std::polar(1.0, -modPhase), m_config.m_refGain);
        }

        // Soft chips for the delayed data symbols; they trail the raw lane's by the
        // decision delay but arrive in the same order
        if (meta.m_isData)
        {
            double scale = 48.0 / (lane.m_magMean * lane.m_magMean + 1e-12);
            for (int b = arity - 1; b >= 0; b--)
            {
                double d0 = 1e30, d1 = 1e30;
                for (int cand = 0; cand < m; cand++)
                {
                    Cd pt = std::polar(lane.m_magMean, grayEncode(cand) * step);
                    double dist = std::norm(ydEq - pt);
                    if ((cand >> b) & 1) {
                        d1 = std::min(d1, dist);
                    } else {
                        d0 = std::min(d0, dist);
                    }
                }
                double soft = 128.0 + scale * (d0 - d1);
                lane.m_chips.push_back((uint8_t) std::max(0.0, std::min(255.0, soft)));
            }
        }
    }

    void processSymbol()
    {
        m_symbolsSinceSync++;
        Cd y0 = interpAt(0, m_mu);
        // Raw lane: identical to the receiver without an equalizer, so its soft chips
        // and its carrier trajectory are exactly the proven baseline. The reference is
        // stepped on to this symbol before it derotates anything - see updateRefLane()
        advanceRef(m_ref, m_dphiSym);
        Cd y = y0 * std::conj(m_ref);

        gardner(y0);

        double mag = std::abs(y);
        if (m_magMean <= 0.0) {
            m_magMean = mag;
        } else {
            m_magMean += 0.02 * (mag - m_magMean);
        }

        bool isData = (m_frState == FrData);
        int arity = isData && (m_mode >= 0) ? modeParams(m_mode).m_arity : 1;
        int m = 1 << arity;
        bool flip = isData && scramblerBit(m_dataSymIdx);

        // For data symbols, remove the scrambler flip before deciding
        Cd yd = flip ? -y : y;

        double phi = std::arg(yd);
        double step = 2.0 * M_PI / m;
        int phaseIdx = (int) std::lround(phi / step) & (m - 1);
        double residual = phi - phaseIdx * step;
        while (residual > M_PI) { residual -= 2.0 * M_PI; }
        while (residual < -M_PI) { residual += 2.0 * M_PI; }

        // Known training bits give a data-aided update; everything else is
        // decision-directed. During M2 the mode is already known, so those 15 bits
        // are training too.
        int knownBit = -1;
        switch (m_frState)
        {
        case FrM2:
            knownBit = m1Bit(m_mode, ACARSHFDL_M2_LEN - m_symWanted);
            break;
        case FrTrain:
        case FrSegTrain:
            knownBit = tBit(ACARSHFDL_T_LEN - m_symWanted);
            break;
        default:
            break;
        }

        if (knownBit >= 0)
        {
            // Data-aided: the modulation is certain, so the residual is exact and the
            // reference can be pulled hard
            Cd yDemod = knownBit ? -y0 : y0;
            double res = std::arg(yDemod * std::conj(m_ref));
            if (m_config.m_freqGain != 0.0) {
                m_dphiSym += m_config.m_freqGain * (1.0 - m_config.m_trainGain) * res;
            }
            updateRefLane(m_ref, yDemod, m_config.m_trainGain);
            int rxBit = (std::abs(phi) > M_PI_2) ? 1 : 0;
            m_stats.m_trainBitsTotal++;
            m_trainTotalThisBurst++;
            if (rxBit != knownBit)
            {
                m_stats.m_trainBitsBad++;
                m_trainBadThisBurst++;
            }
        }
        else
        {
            // Decision-directed: remove the decided modulation (constellation phase
            // plus the scrambler flip) from the received sample and blend it in
            if (m_config.m_freqGain != 0.0) {
                m_dphiSym += m_config.m_freqGain * (1.0 - m_config.m_refGain) * residual;
            }
            double modPhase = phaseIdx * step + (flip ? M_PI : 0.0);
            updateRefLane(m_ref, y0 * std::polar(1.0, -modPhase), m_config.m_refGain);
        }

        // Equalizer lanes, active once the mode is known: each has its own carrier
        // reference and frequency estimate driven by residuals of ITS equalized
        // output, so on a multipath channel the whole tracking loop benefits, not
        // just the decisions. Each lane decides at its own delay behind the newest
        // sample in the line, so its leading taps can cancel that much pre-cursor
        // ISI; the shared per-symbol metadata queue lines training bits and scrambler
        // flips up with each delay. The 15 symbol training sequence after the last
        // data frame gives the delayed lanes time to finish their chips before the
        // burst decodes.
        if (!m_eqLanes.empty() && (m_mode >= 0) && (m_frState >= FrM2))
        {
            m_laneMeta.push_back({ knownBit, isData, flip });
            for (EqLane& lane : m_eqLanes) {
                laneSymbol(lane, y0);
            }
        }

        // Channel-interpolation lane: buffer the symbol, pre-rotated by the frozen
        // acquisition frequency estimate, for non-causal channel estimation at
        // decode time
        if (m_config.m_interpEnable && (m_mode >= 0) && (m_frState >= FrM2))
        {
            m_interpBuf.push_back({ y0 * m_interpRot, (int8_t) knownBit, isData, flip });
            m_interpRot *= std::polar(1.0, -m_interpDphi);
        }

        if (m_debugThisBurst && (m_debugSymbol < 40))
        {
            printf("hfdl sym %4d: st=%d |y|=%6.3f phi=%7.3f idx=%d res=%+6.3f dphiSym=%+8.5f mu=%.3f\n",
                m_debugSymbol++, (int) m_frState, mag, phi, phaseIdx, residual, m_dphiSym, m_mu);
        }

        // Collect the constellation-frame samples for coherent A2/M1 matching
        if ((m_frState == FrA2) || (m_frState == FrM1)) {
            m_corrSamples.push_back(y);
        }

        // Collect soft chips for data symbols: per-bit LLR from the squared distance
        // to the nearest constellation point with that bit 0 versus 1. Both lanes
        // collect, so a burst the raw lane cannot decode gets a second chance from
        // the equalized lane, arbitrated by the FCS.
        if (isData)
        {
            auto softPush = [&](std::vector<uint8_t>& chips, const Cd& sample, double radius)
            {
                double scale = 48.0 / (radius * radius + 1e-12);
                for (int b = arity - 1; b >= 0; b--)
                {
                    double d0 = 1e30, d1 = 1e30;
                    for (int cand = 0; cand < m; cand++)
                    {
                        Cd pt = std::polar(radius, grayEncode(cand) * step);
                        double dist = std::norm(sample - pt);
                        if ((cand >> b) & 1) {
                            d1 = std::min(d1, dist);
                        } else {
                            d0 = std::min(d0, dist);
                        }
                    }
                    double soft = 128.0 + scale * (d0 - d1);
                    chips.push_back((uint8_t) std::max(0.0, std::min(255.0, soft)));
                }
            };
            softPush(m_softChips, yd, m_magMean);
            m_dataSymIdx++;
        }

        // Advance the framer
        if (--m_symWanted > 0) {
            return;
        }

        switch (m_frState)
        {
        case FrA2:
        {
            // Coherent correlation against the A sequence, in two halves so the phase
            // difference between them measures any residual carrier frequency error
            Cd c1(0.0, 0.0), c2(0.0, 0.0);
            double norm = 0.0;
            for (int i = 0; i < ACARSHFDL_A_LEN; i++)
            {
                Cd p = m_corrSamples[i] * (aBit(i) ? -1.0 : 1.0);
                if (i < ACARSHFDL_A_LEN / 2) {
                    c1 += p;
                } else {
                    c2 += p;
                }
                norm += std::abs(m_corrSamples[i]);
            }
            Cd c = c1 + c2;
            double stat = (norm > 1e-12) ? std::abs(c) / norm : 0.0;
            if (stat >= m_config.m_a2Threshold)
            {
                m_stats.m_a2Ok++;
                // The correlation's argument is the reference's mean phase error over
                // the window - including pi when the A1 ambiguity resolution picked the
                // wrong sign, which hard bit matching used to reject as corr = -1
                m_ref *= std::polar(1.0, std::arg(c));
                // And the rotation between the halves is the residual CFO
                if ((std::abs(c1) > 1e-12) && (std::abs(c2) > 1e-12)) {
                    m_dphiSym += std::arg(c2 * std::conj(c1)) / (ACARSHFDL_A_LEN / 2.0);
                }
                if (m_debugThisBurst) {
                    printf("hfdl A2 ok: stat=%.3f phase=%.3f dphiSym=%.5f\n",
                        stat, std::arg(c), m_dphiSym);
                }
                m_corrSamples.clear();
                m_frState = FrM1;
                m_symWanted = ACARSHFDL_M1_LEN;
            }
            else
            {
                // Maybe the acquisition fired on the SECOND A sequence, in which case
                // this window is M1: try that before giving up
                int mode; double m1stat; Cd m1c;
                matchM1(m_corrSamples, mode, m1stat, m1c);
                if (m1stat >= m_config.m_m1Threshold)
                {
                    m_stats.m_a2Ok++;
                    m_stats.m_m1Ok++;
                    m_ref *= std::polar(1.0, std::arg(m1c));
                    startMode(mode);
                }
                else
                {
                    m_stats.m_a2Fail++;
                    if (m_debugThisBurst) {
                        printf("hfdl A2 failed: stat=%.3f m1stat=%.3f\n", stat, m1stat);
                    }
                    abandonBurst();
                }
            }
            break;
        }
        case FrM1:
        {
            int mode; double stat; Cd c;
            matchM1(m_corrSamples, mode, stat, c);
            if (stat >= m_config.m_m1Threshold)
            {
                m_stats.m_m1Ok++;
                m_ref *= std::polar(1.0, std::arg(c));  // Data-aided re-anchor
                startMode(mode);
            }
            else
            {
                m_stats.m_m1Fail++;
                if (m_debugThisBurst) {
                    printf("hfdl M1 failed: stat=%.3f\n", stat);
                }
                abandonBurst();
            }
            break;
        }
        case FrM2:
            m_frState = FrTrain;
            m_trainSeqLeft = ACARSHFDL_EQ_TRAIN_CNT;
            m_symWanted = ACARSHFDL_T_LEN;
            break;
        case FrTrain:
            if (trainAbort()) {
                break;
            }
            if (--m_trainSeqLeft > 0)
            {
                m_symWanted = ACARSHFDL_T_LEN;
            }
            else
            {
                m_frState = FrData;
                m_symWanted = ACARSHFDL_DATA_FRAME_LEN;
            }
            break;
        case FrData:
            m_frState = FrSegTrain;
            m_symWanted = ACARSHFDL_T_LEN;
            break;
        case FrSegTrain:
            if (trainAbort()) {
                break;
            }
            if (--m_segmentsLeft > 0)
            {
                m_frState = FrData;
                m_symWanted = ACARSHFDL_DATA_FRAME_LEN;
            }
            else
            {
                decodeBurst();
                abandonBurst();
            }
            break;
        }
    }

    // Liveness check on the known training bits: a burst that is not really there
    // reads ~50 percent errors and would otherwise hold the receiver deaf for
    // thousands of symbols. Checked at every training sequence boundary once enough
    // bits have accumulated.
    bool trainAbort()
    {
        // With equalizer lanes running, a burst is only garbage if EVERY lane reads
        // it as garbage - a severe multipath burst can be decodable by an equalized
        // lane alone
        int bad = m_trainBadThisBurst;
        if (m_mode >= 0)
        {
            for (const EqLane& lane : m_eqLanes) {
                bad = std::min(bad, lane.m_trainBad);
            }
        }
        if ((m_config.m_trainAbortFraction > 0.0)
            && (m_trainTotalThisBurst >= 9 * ACARSHFDL_T_LEN)
            && (bad > m_config.m_trainAbortFraction * m_trainTotalThisBurst))
        {
            m_stats.m_trainAborts++;
            if (m_debugThisBurst) {
                printf("hfdl train abort: best lane %d of %d bad\n", bad, m_trainTotalThisBurst);
            }
            abandonBurst();
            return true;
        }
        return false;
    }

    // Coherent match of a 127 sample window against the 8 cyclic shifts of M1
    void matchM1(const std::vector<Cd>& samples, int& bestMode, double& bestStat, Cd& bestC)
    {
        bestMode = -1;
        bestStat = 0.0;
        bestC = Cd(0.0, 0.0);
        double norm = 0.0;
        for (const Cd& s : samples) {
            norm += std::abs(s);
        }
        if (norm < 1e-12) {
            return;
        }
        for (int mode = 0; mode < ACARSHFDL_MODE_CNT; mode++)
        {
            Cd c(0.0, 0.0);
            for (int i = 0; i < ACARSHFDL_M1_LEN; i++) {
                c += samples[i] * (m1Bit(mode, i) ? -1.0 : 1.0);
            }
            double stat = std::abs(c) / norm;
            if (stat > bestStat)
            {
                bestStat = stat;
                bestMode = mode;
                bestC = c;
            }
        }
    }

    void startMode(int mode)
    {
        m_mode = mode;
        m_stats.m_m1Modes[mode]++;
        m_segmentsLeft = modeParams(mode).m_segments;
        m_corrSamples.clear();
        m_softChips.clear();
        m_interpBuf.clear();
        m_interpRot = Cd(1.0, 0.0);
        m_interpDphi = m_dphiSym;
        m_softChips.reserve(modeParams(mode).m_segments * ACARSHFDL_DATA_FRAME_LEN
            * modeParams(mode).m_arity);
        m_dataSymIdx = 0;
        // Fork the equalizer lanes' carrier state from the raw lane; their taps are
        // still the reset-time spikes and train from M2 onwards
        for (EqLane& lane : m_eqLanes)
        {
            lane.m_ref = m_ref;
            lane.m_dphiSym = m_dphiSym;
            lane.m_magMean = 1.0;
            lane.m_trainBad = 0;
            lane.m_chips.clear();
            lane.m_chips.reserve(modeParams(mode).m_segments * ACARSHFDL_DATA_FRAME_LEN
                * modeParams(mode).m_arity);
        }
        m_laneMeta.clear();
        m_frState = FrM2;
        m_symWanted = ACARSHFDL_M2_LEN;
        if (m_debugThisBurst)
        {
            printf("hfdl M1 match: mode=%d (%d bps, %s slot)\n", mode,
                modeParams(mode).m_bitRate, modeParams(mode).m_doubleSlot ? "double" : "single");
        }
    }

    // ---------------------------------------------------------------------------------
    // Burst decode: deinterleave, Viterbi, PDU parse
    // ---------------------------------------------------------------------------------

    // Result of one decode attempt (raw or an equalizer lane). parsePdu() fills this
    // without touching the receiver state, so decodeBurst() can compare and merge the
    // attempts and commit only the combined best.
    struct ParseResult
    {
        bool m_headerOk = false;    // MPDU header (or SPDU) FCS passed
        bool m_fcsTested = false;   // Got far enough to test that FCS
        bool m_isSpdu = false;
        std::vector<uint8_t> m_pdu; // The decoded octets, kept for header repair
        int m_lpdusOk = 0;          // LPDUs that passed their own FCS
        int m_lpdusBad = 0;
        int m_acarsFrames = 0;
        int m_otherFrames = 0;
        std::vector<uint8_t> m_header;      // Decoded MPDU header octets, for merge matching
        enum LpduState : uint8_t { LpduSkipped, LpduBad, LpduOk };
        std::vector<uint8_t> m_lpduState;   // Per LPDU slot
        std::vector<Frame> m_frames;
        std::vector<int> m_frameSlot;       // LPDU slot of each frame (-1 for an SPDU)
    };

    // A complete attempt has a valid header and no LPDU that failed its FCS - there
    // is nothing left for an equalized retry to add
    static bool attemptComplete(const ParseResult& r)
    {
        return r.m_headerOk && (r.m_lpdusBad == 0);
    }

    // Attempt ranking: a valid header beats none, then more FCS-valid LPDUs win
    static bool attemptBetter(const ParseResult& a, const ParseResult& b)
    {
        if (a.m_headerOk != b.m_headerOk) {
            return a.m_headerOk;
        }
        return a.m_lpdusOk > b.m_lpdusOk;
    }

    // When two attempts decoded the same MPDU header, their LPDU slots correspond,
    // so FCS-valid LPDUs one attempt recovered and the other did not can be adopted
    // slot by slot - raw and equalized lanes each recovering different LPDUs then
    // both contribute. Returns true when anything was adopted.
    static bool attemptMerge(ParseResult& best, const ParseResult& r)
    {
        if (!best.m_headerOk || !r.m_headerOk || best.m_isSpdu || r.m_isSpdu) {
            return false;
        }
        if (best.m_header != r.m_header) {
            return false;
        }
        bool adopted = false;
        // (Not named "slots" - that is a Qt macro)
        size_t slotCnt = std::min(best.m_lpduState.size(), r.m_lpduState.size());
        for (size_t i = 0; i < slotCnt; i++)
        {
            if ((r.m_lpduState[i] != ParseResult::LpduOk) || (best.m_lpduState[i] == ParseResult::LpduOk)) {
                continue;
            }
            for (size_t f = 0; f < r.m_frames.size(); f++)
            {
                if (r.m_frameSlot[f] != (int) i) {
                    continue;
                }
                best.m_frames.push_back(r.m_frames[f]);
                best.m_frameSlot.push_back((int) i);
                if (r.m_frames[f].m_isAcars) {
                    best.m_acarsFrames++;
                } else {
                    best.m_otherFrames++;
                }
                break;
            }
            if (best.m_lpduState[i] == ParseResult::LpduBad) {
                best.m_lpdusBad--;
            }
            best.m_lpduState[i] = ParseResult::LpduOk;
            best.m_lpdusOk++;
            adopted = true;
        }
        return adopted;
    }

    // Non-causal channel estimate for the interpolation lane: the complex channel
    // gain measured over every buffered 15 symbol training sequence, linearly
    // interpolated across the data frames between them. Fading faster than the
    // causal loops can track (auroral flutter spreads the carrier by several Hz to
    // tens of Hz) is still sampled above its Nyquist rate by the 40 Hz anchor
    // spacing. Soft chips are amplitude weighted: a symbol inside a fade carries
    // low confidence into the Viterbi rather than a confident wrong bit.
    bool buildInterpChips(std::vector<uint8_t>& chips) const
    {
        struct Anchor
        {
            double m_pos;
            Cd m_g;
        };
        std::vector<Anchor> anchors;
        size_t n = m_interpBuf.size();
        size_t i = 0;
        while (i < n)
        {
            if (m_interpBuf[i].m_known < 0)
            {
                i++;
                continue;
            }
            size_t start = i;
            while ((i < n) && (m_interpBuf[i].m_known >= 0)) {
                i++;
            }
            // Long known runs (the 9 initial training sequences) become one anchor
            // per 15 symbols
            size_t runLen = i - start;
            for (size_t off = 0; off + ACARSHFDL_T_LEN <= runLen; off += ACARSHFDL_T_LEN)
            {
                Cd g(0.0, 0.0);
                for (int k = 0; k < ACARSHFDL_T_LEN; k++)
                {
                    const InterpSym& s = m_interpBuf[start + off + k];
                    g += s.m_known ? -s.m_y : s.m_y;
                }
                g /= (double) ACARSHFDL_T_LEN;
                anchors.push_back({ (double) (start + off) + (ACARSHFDL_T_LEN - 1) / 2.0, g });
            }
        }
        if (anchors.size() < 2) {
            return false;
        }

        double gMean = 0.0;
        for (const Anchor& a : anchors) {
            gMean += std::abs(a.m_g);
        }
        gMean /= (double) anchors.size();
        if (gMean < 1e-9) {
            return false;
        }

        const ModeParams& p = modeParams(m_mode);
        int arity = p.m_arity;
        int m = 1 << arity;
        double step = 2.0 * M_PI / m;
        double scale = 48.0 / (gMean * gMean);

        chips.clear();
        chips.reserve(p.m_segments * ACARSHFDL_DATA_FRAME_LEN * arity);
        size_t ai = 0;
        for (size_t idx = 0; idx < n; idx++)
        {
            const InterpSym& s = m_interpBuf[idx];
            if (!s.m_isData) {
                continue;
            }
            while ((ai + 1 < anchors.size()) && (anchors[ai + 1].m_pos < (double) idx)) {
                ai++;
            }
            Cd g;
            if ((double) idx <= anchors[0].m_pos) {
                g = anchors[0].m_g;
            } else if (ai + 1 >= anchors.size()) {
                g = anchors.back().m_g;
            } else {
                // Catmull-Rom through the two bracketing anchors and their
                // neighbours: fading is a smooth process, and the spline tracks its
                // curvature between anchors where a straight line cuts the corner
                const Cd& g1 = anchors[ai].m_g;
                const Cd& g2 = anchors[ai + 1].m_g;
                const Cd& g0 = anchors[ai > 0 ? ai - 1 : ai].m_g;
                const Cd& g3 = anchors[(ai + 2 < anchors.size()) ? ai + 2 : ai + 1].m_g;
                double t = ((double) idx - anchors[ai].m_pos) / (anchors[ai + 1].m_pos - anchors[ai].m_pos);
                double t2 = t * t;
                double t3 = t2 * t;
                g = 0.5 * ((2.0 * g1)
                    + (g2 - g0) * t
                    + (2.0 * g0 - 5.0 * g1 + 4.0 * g2 - g3) * t2
                    + (3.0 * g1 - g0 - 3.0 * g2 + g3) * t3);
            }
            double gm = std::abs(g);
            Cd z = (gm > 1e-12) ? s.m_y * std::conj(g) / gm : Cd(0.0, 0.0);
            Cd zd = s.m_flip ? -z : z;
            for (int b = arity - 1; b >= 0; b--)
            {
                double d0 = 1e30, d1 = 1e30;
                for (int cand = 0; cand < m; cand++)
                {
                    Cd pt = std::polar(gm, grayEncode(cand) * step);
                    double dist = std::norm(zd - pt);
                    if ((cand >> b) & 1) {
                        d1 = std::min(d1, dist);
                    } else {
                        d0 = std::min(d0, dist);
                    }
                }
                double soft = 128.0 + scale * (d0 - d1);
                chips.push_back((uint8_t) std::max(0.0, std::min(255.0, soft)));
            }
        }
        return true;
    }

    // Two-tap variant: fading combined with delay spread. Each anchor least-squares
    // fits y_k = h0 s_k + h1 s_(k-1) over its known symbols (the 2x2 normal
    // equations have a constant known correlation term from the training sequence),
    // both taps interpolate across the data, and each data symbol demodulates by
    // decision feedback: subtract the previous symbol's echo through h1, then
    // phase-correct by h0. Every data frame starts right after a training sequence,
    // so the feedback always seeds from a known symbol.
    bool buildInterp2Chips(std::vector<uint8_t>& chips) const
    {
        struct Anchor2
        {
            double m_pos;
            Cd m_h0;
            Cd m_h1;
        };
        std::vector<Anchor2> anchors;
        size_t n = m_interpBuf.size();
        size_t i = 0;
        while (i < n)
        {
            if (m_interpBuf[i].m_known < 0)
            {
                i++;
                continue;
            }
            size_t start = i;
            while ((i < n) && (m_interpBuf[i].m_known >= 0)) {
                i++;
            }
            size_t runLen = i - start;
            for (size_t off = 0; off + ACARSHFDL_T_LEN <= runLen; off += ACARSHFDL_T_LEN)
            {
                // Least squares over k = 1..14 (k = 0's predecessor is unknown data)
                double a00 = 0.0, a01 = 0.0;
                Cd b0(0.0, 0.0), b1(0.0, 0.0);
                for (int k = 1; k < ACARSHFDL_T_LEN; k++)
                {
                    const InterpSym& s = m_interpBuf[start + off + k];
                    const InterpSym& sp = m_interpBuf[start + off + k - 1];
                    double tk = s.m_known ? -1.0 : 1.0;
                    double tk1 = sp.m_known ? -1.0 : 1.0;
                    a00 += 1.0;
                    a01 += tk * tk1;
                    b0 += s.m_y * tk;
                    b1 += s.m_y * tk1;
                }
                double det = a00 * a00 - a01 * a01;
                if (std::abs(det) < 1e-6) {
                    continue;
                }
                Anchor2 a;
                a.m_pos = (double) (start + off) + (ACARSHFDL_T_LEN - 1) / 2.0;
                a.m_h0 = (b0 * a00 - b1 * a01) / det;
                a.m_h1 = (b1 * a00 - b0 * a01) / det;
                anchors.push_back(a);
            }
        }
        if (anchors.size() < 2) {
            return false;
        }

        double gMean = 0.0;
        for (const Anchor2& a : anchors) {
            gMean += std::abs(a.m_h0);
        }
        gMean /= (double) anchors.size();
        if (gMean < 1e-9) {
            return false;
        }

        const ModeParams& p = modeParams(m_mode);
        int arity = p.m_arity;
        int m = 1 << arity;
        double step = 2.0 * M_PI / m;
        double scale = 48.0 / (gMean * gMean);

        // Catmull-Rom on a complex series
        auto splineAt = [&anchors](size_t ai, double idx, Cd Anchor2::*tap)
        {
            if (idx <= anchors[0].m_pos) {
                return anchors[0].*tap;
            }
            if (ai + 1 >= anchors.size()) {
                return anchors.back().*tap;
            }
            const Cd& g1 = anchors[ai].*tap;
            const Cd& g2 = anchors[ai + 1].*tap;
            const Cd& g0 = anchors[ai > 0 ? ai - 1 : ai].*tap;
            const Cd& g3 = anchors[(ai + 2 < anchors.size()) ? ai + 2 : ai + 1].*tap;
            double t = (idx - anchors[ai].m_pos) / (anchors[ai + 1].m_pos - anchors[ai].m_pos);
            double t2 = t * t;
            double t3 = t2 * t;
            return 0.5 * ((2.0 * g1)
                + (g2 - g0) * t
                + (2.0 * g0 - 5.0 * g1 + 4.0 * g2 - g3) * t2
                + (3.0 * g1 - g0 - 3.0 * g2 + g3) * t3);
        };

        chips.clear();
        chips.reserve(p.m_segments * ACARSHFDL_DATA_FRAME_LEN * arity);
        size_t ai = 0;
        Cd dPrev(1.0, 0.0);     // Previous transmitted symbol, decision-fed over data
        for (size_t idx = 0; idx < n; idx++)
        {
            const InterpSym& s = m_interpBuf[idx];
            if (!s.m_isData)
            {
                if (s.m_known >= 0) {
                    dPrev = Cd(s.m_known ? -1.0 : 1.0, 0.0);
                }
                continue;
            }
            while ((ai + 1 < anchors.size()) && (anchors[ai + 1].m_pos < (double) idx)) {
                ai++;
            }
            Cd h0 = splineAt(ai, (double) idx, &Anchor2::m_h0);
            Cd h1 = splineAt(ai, (double) idx, &Anchor2::m_h1);
            double gm = std::abs(h0);
            Cd z = s.m_y - h1 * dPrev;
            Cd zc = (gm > 1e-12) ? z * std::conj(h0) / gm : Cd(0.0, 0.0);
            Cd zd = s.m_flip ? -zc : zc;
            for (int b = arity - 1; b >= 0; b--)
            {
                double d0 = 1e30, d1 = 1e30;
                for (int cand = 0; cand < m; cand++)
                {
                    Cd pt = std::polar(gm, grayEncode(cand) * step);
                    double dist = std::norm(zd - pt);
                    if ((cand >> b) & 1) {
                        d1 = std::min(d1, dist);
                    } else {
                        d0 = std::min(d0, dist);
                    }
                }
                double soft = 128.0 + scale * (d0 - d1);
                chips.push_back((uint8_t) std::max(0.0, std::min(255.0, soft)));
            }
            // Hard decision in the transmitted domain feeds the next symbol's echo
            double phi = std::arg(zd);
            int phaseIdx = (int) std::lround(phi / step) & (m - 1);
            Cd point = std::polar(1.0, phaseIdx * step);
            dPrev = s.m_flip ? -point : point;
        }
        return true;
    }

    // Turbo re-estimation: Viterbi-decode the seed chips, re-encode the decision
    // bits back into the transmitted chip stream, and use every symbol - the real
    // training plus the re-encoded data - as virtual training for a dense sliding
    // window channel estimate. The convolutional code corrects most of the first
    // pass's symbol errors, and what remains averages out inside the window, so the
    // re-derived soft chips see a far finer channel track than 45-symbol anchors.
    // taps = 1 estimates a flat channel; taps = 2 least-squares fits a symbol-spaced
    // [h0 h1] per window and cancels the previous symbol's echo through the known
    // re-encoded reference - no decision feedback, so no error propagation beyond
    // the re-encode errors themselves.
    bool buildTurboChips(const std::vector<uint8_t>& seedChips, std::vector<uint8_t>& chips, int taps) const
    {
        const ModeParams& p = modeParams(m_mode);
        int arity = p.m_arity;
        int m = 1 << arity;
        double step = 2.0 * M_PI / m;
        int n = (int) seedChips.size();

        // First pass: deinterleave, Viterbi, exactly as decodeAttempt
        const std::vector<int>& txPos = interleaverTxPos(m_mode);
        std::vector<uint8_t> e(n);
        for (int k = 0; k < n; k++) {
            e[k] = seedChips[txPos[k]];
        }
        std::vector<uint8_t> vin;
        if (p.m_codeRate == 4)
        {
            vin.resize(n / 2);
            for (int i = 0; i < n / 2; i++) {
                vin[i] = (uint8_t) (((int) e[2 * i] + (int) e[2 * i + 1]) / 2);
            }
        }
        else
        {
            vin = e;
        }
        int userBits = (int) vin.size() / 2;
        std::vector<uint8_t> bits;
        viterbiDecode(vin, userBits, bits);

        // Re-encode the decisions into hard chips in transmit order
        std::vector<uint8_t> txHard(n);
        uint32_t sr = 0;
        for (int i = 0; i < userBits; i++)
        {
            sr = (sr << 1) | bits[i];
            uint8_t ca = parity7(sr & 0x6D);
            uint8_t cb = parity7(sr & 0x4F);
            if (p.m_codeRate == 4)
            {
                txHard[txPos[4 * i]] = ca;
                txHard[txPos[4 * i + 1]] = ca;
                txHard[txPos[4 * i + 2]] = cb;
                txHard[txPos[4 * i + 3]] = cb;
            }
            else
            {
                txHard[txPos[2 * i]] = ca;
                txHard[txPos[2 * i + 1]] = cb;
            }
        }

        // Reference symbol for every buffered position: training bits are certain,
        // data symbols come from the re-encoded decisions (in the transmitted
        // domain, scrambler flip included)
        size_t total = m_interpBuf.size();
        std::vector<Cd> ref(total);
        int dataIdx = 0;
        for (size_t idx = 0; idx < total; idx++)
        {
            const InterpSym& s = m_interpBuf[idx];
            if (s.m_isData)
            {
                int cand = 0;
                for (int b = 0; b < arity; b++) {
                    cand |= txHard[dataIdx * arity + (arity - 1 - b)] << b;
                }
                dataIdx++;
                Cd point = std::polar(1.0, grayEncode(cand) * step);
                ref[idx] = s.m_flip ? -point : point;
            }
            else if (s.m_known >= 0)
            {
                ref[idx] = Cd(s.m_known ? -1.0 : 1.0, 0.0);
            }
            else
            {
                ref[idx] = Cd(0.0, 0.0);
            }
        }
        if (dataIdx * arity != n) {
            return false;
        }

        // Dense channel estimate over sliding windows via prefix sums, then the
        // usual amplitude weighted soft chips against it. For two taps the window's
        // normal equations are solved per symbol and the previous symbol's echo is
        // subtracted through the known reference.
        int w = std::max(1, m_config.m_turboWindow);
        std::vector<Cd> pYd0(total + 1, Cd(0.0, 0.0));  // y * conj(ref)
        std::vector<Cd> pYd1(total + 1, Cd(0.0, 0.0));  // y * conj(ref prev)
        std::vector<Cd> pDd(total + 1, Cd(0.0, 0.0));   // conj(ref) * ref prev
        for (size_t idx = 0; idx < total; idx++)
        {
            Cd refPrev = (idx > 0) ? ref[idx - 1] : Cd(0.0, 0.0);
            pYd0[idx + 1] = pYd0[idx] + m_interpBuf[idx].m_y * std::conj(ref[idx]);
            pYd1[idx + 1] = pYd1[idx] + m_interpBuf[idx].m_y * std::conj(refPrev);
            pDd[idx + 1] = pDd[idx] + std::conj(ref[idx]) * refPrev;
        }
        double gMean = 0.0;
        std::vector<Cd> g(total);       // h0
        std::vector<Cd> g1(total);      // h1 (two taps only)
        for (size_t idx = 0; idx < total; idx++)
        {
            size_t lo = (idx > (size_t) w) ? idx - w : 0;
            size_t hi = std::min(total, idx + (size_t) w + 1);
            double cnt = (double) (hi - lo);
            Cd b0 = pYd0[hi] - pYd0[lo];
            if (taps >= 2)
            {
                Cd b1 = pYd1[hi] - pYd1[lo];
                Cd c = pDd[hi] - pDd[lo];
                double det = cnt * cnt - std::norm(c);
                if (std::abs(det) > 1e-6)
                {
                    g[idx] = (b0 * cnt - c * b1) / det;
                    g1[idx] = (b1 * cnt - std::conj(c) * b0) / det;
                }
                else
                {
                    g[idx] = b0 / cnt;
                    g1[idx] = Cd(0.0, 0.0);
                }
            }
            else
            {
                g[idx] = b0 / cnt;
            }
            gMean += std::abs(g[idx]);
        }
        gMean /= (double) total;
        if (gMean < 1e-9) {
            return false;
        }
        double scale = 48.0 / (gMean * gMean);

        chips.clear();
        chips.reserve(n);
        for (size_t idx = 0; idx < total; idx++)
        {
            const InterpSym& s = m_interpBuf[idx];
            if (!s.m_isData) {
                continue;
            }
            double gm = std::abs(g[idx]);
            Cd y = s.m_y;
            if ((taps >= 2) && (idx > 0)) {
                y -= g1[idx] * ref[idx - 1];
            }
            Cd z = (gm > 1e-12) ? y * std::conj(g[idx]) / gm : Cd(0.0, 0.0);
            Cd zd = s.m_flip ? -z : z;
            for (int b = arity - 1; b >= 0; b--)
            {
                double d0 = 1e30, d1 = 1e30;
                for (int cand = 0; cand < m; cand++)
                {
                    Cd pt = std::polar(gm, grayEncode(cand) * step);
                    double dist = std::norm(zd - pt);
                    if ((cand >> b) & 1) {
                        d1 = std::min(d1, dist);
                    } else {
                        d0 = std::min(d0, dist);
                    }
                }
                double soft = 128.0 + scale * (d0 - d1);
                chips.push_back((uint8_t) std::max(0.0, std::min(255.0, soft)));
            }
        }
        return true;
    }

    void decodeBurst()
    {
        m_stats.m_burstsDecoded++;
        const ModeParams& p = modeParams(m_mode);
        int n = p.m_segments * ACARSHFDL_DATA_FRAME_LEN * p.m_arity;
        if ((int) m_softChips.size() != n) {
            return;
        }

        // The raw lane first; when anything in it fails an FCS - the whole burst or
        // any single LPDU - each equalizer lane gets a try. When the lane decoded the
        // same MPDU header, its FCS-valid LPDUs are merged in slot by slot; otherwise
        // the attempt with the most FCS-valid PDUs wins outright. The FCS stays the
        // arbiter, so the equalizers can only ever add frames, never lose them.
        ParseResult best;
        decodeAttempt(m_softChips, best);
        // Every attempt's decoded octets are kept so a failed burst can try header
        // repair over each of them as the last resort
        std::vector<std::vector<uint8_t>> attemptPdus;
        attemptPdus.push_back(best.m_pdu);
        // Contribution test shared by the alternative attempts: merge FCS-valid
        // LPDUs in when the decoded header matches, otherwise the better whole
        // attempt wins
        auto tryAttempt = [&](const std::vector<uint8_t>& altChips) -> bool
        {
            ParseResult r;
            decodeAttempt(altChips, r);
            attemptPdus.push_back(r.m_pdu);
            if (attemptMerge(best, r)) {
                return true;
            }
            if (attemptBetter(r, best))
            {
                best = std::move(r);
                return true;
            }
            return false;
        };
        // The channel-interpolation lane first (it wins on fast fading), then the
        // equalizer lanes (which win on ISI)
        // Retry-worthiness: bursts with more than the limit fraction of training
        // bits wrong (taking the best lane's count) are collisions or unrecoverable
        // garbage - the expensive retry lanes never save them, so they skip
        // straight to commit. The cheap equalizer-lane attempts still run: their
        // chips were already collected per symbol.
        bool retryWorthwhile = true;
        if ((m_config.m_retryLimitFraction > 0.0) && (m_trainTotalThisBurst > 0))
        {
            int bestBad = m_trainBadThisBurst;
            for (const EqLane& lane : m_eqLanes) {
                bestBad = std::min(bestBad, lane.m_trainBad);
            }
            retryWorthwhile = bestBad <= m_config.m_retryLimitFraction * m_trainTotalThisBurst;
        }

        std::vector<uint8_t> interpChips;   // Kept: also seeds the turbo attempt
        if (!attemptComplete(best) && retryWorthwhile && m_config.m_interpEnable)
        {
            if (buildInterpChips(interpChips) && ((int) interpChips.size() == n))
            {
                m_stats.m_interpRetries++;
                if (tryAttempt(interpChips)) {
                    m_stats.m_interpRecovered++;
                }
            }
        }
        std::vector<uint8_t> interp2Chips;  // Kept: preferred seed for two-tap turbo
        if (!attemptComplete(best) && retryWorthwhile && m_config.m_interp2Enable)
        {
            if (buildInterp2Chips(interp2Chips) && ((int) interp2Chips.size() == n))
            {
                m_stats.m_interp2Retries++;
                if (tryAttempt(interp2Chips)) {
                    m_stats.m_interp2Recovered++;
                }
            }
        }
        bool fromLane = false;
        if (!attemptComplete(best))
        {
            for (const EqLane& lane : m_eqLanes)
            {
                if ((int) lane.m_chips.size() != n) {
                    continue;
                }
                m_stats.m_eqRetries++;
                if (tryAttempt(lane.m_chips)) {
                    fromLane = true;
                }
                if (attemptComplete(best)) {
                    break;
                }
            }
        }
        if (fromLane) {
            m_stats.m_eqRetryRecovered++;
        }

        // Still incomplete: turbo re-estimation seeded by the interpolation lane's
        // chips (or the raw ones), decoding through a dense re-encoded-data channel
        // estimate
        if (!attemptComplete(best) && retryWorthwhile && m_config.m_turboEnable)
        {
            const std::vector<uint8_t>& seed = ((int) interpChips.size() == n) ? interpChips : m_softChips;
            std::vector<uint8_t> turboChips;
            if (buildTurboChips(seed, turboChips, 1) && ((int) turboChips.size() == n))
            {
                m_stats.m_turboRetries++;
                if (tryAttempt(turboChips)) {
                    m_stats.m_turboRecovered++;
                }
            }
        }
        if (!attemptComplete(best) && retryWorthwhile && m_config.m_turbo2Enable)
        {
            const std::vector<uint8_t>& seed = ((int) interp2Chips.size() == n) ? interp2Chips
                : (((int) interpChips.size() == n) ? interpChips : m_softChips);
            std::vector<uint8_t> turbo2Chips;
            if (buildTurboChips(seed, turbo2Chips, 2) && ((int) turbo2Chips.size() == n))
            {
                m_stats.m_turbo2Retries++;
                if (tryAttempt(turbo2Chips)) {
                    m_stats.m_turbo2Recovered++;
                }
            }
        }

        // Nothing produced a valid header: last resort is trial-flip header repair
        // over each attempt's octets. For bursts judged not retry-worthy (likely
        // collisions), repair still runs but only over the raw attempt's octets -
        // header-region hits are cheap to fix even when the payload is beyond help.
        if (!best.m_headerOk)
        {
            if (!retryWorthwhile && (attemptPdus.size() > 1)) {
                attemptPdus.resize(1);
            }
            ParseResult repaired;
            for (const auto& pdu : attemptPdus)
            {
                ParseResult r;
                if (repairHeader(pdu, r)
                    && ((r.m_lpdusOk > repaired.m_lpdusOk) || !repaired.m_headerOk)) {
                    repaired = std::move(r);
                }
            }
            if (repaired.m_headerOk)
            {
                m_stats.m_headerRepairs++;
                best = std::move(repaired);
            }
        }

        // Commit the winning attempt
        if (best.m_headerOk)
        {
            m_stats.m_mpduFcsOk++;
            if (best.m_isSpdu) {
                m_stats.m_spdus++;
            }
        }
        else if (best.m_fcsTested)
        {
            m_stats.m_mpduFcsBad++;
        }
        m_stats.m_lpduFcsOk += best.m_lpdusOk;
        m_stats.m_lpduFcsBad += best.m_lpdusBad;
        m_stats.m_acarsFrames += best.m_acarsFrames;
        m_stats.m_otherFrames += best.m_otherFrames;
        for (Frame& f : best.m_frames) {
            m_frames.push_back(std::move(f));
        }
    }

    void decodeAttempt(const std::vector<uint8_t>& softChips, ParseResult& result)
    {
        const ModeParams& p = modeParams(m_mode);
        int n = p.m_segments * ACARSHFDL_DATA_FRAME_LEN * p.m_arity;

        // Deinterleave into Viterbi order
        const std::vector<int>& txPos = interleaverTxPos(m_mode);
        std::vector<uint8_t> e(n);
        for (int k = 0; k < n; k++) {
            e[k] = softChips[txPos[k]];
        }

        // Rate 1/4: average the adjacent duplicates
        std::vector<uint8_t> vin;
        if (p.m_codeRate == 4)
        {
            vin.resize(n / 2);
            for (int i = 0; i < n / 2; i++) {
                vin[i] = (uint8_t) (((int) e[2 * i] + (int) e[2 * i + 1]) / 2);
            }
        }
        else
        {
            vin = e;
        }

        int userBits = (int) vin.size() / 2;
        std::vector<uint8_t> bits;
        viterbiDecode(vin, userBits, bits);

        // Pack octets, bits LSB first
        int octets = userBits / 8;
        std::vector<uint8_t> pdu(octets, 0);
        for (int i = 0; i < octets * 8; i++)
        {
            if (bits[i]) {
                pdu[i >> 3] |= 1 << (i & 7);
            }
        }

        if (m_debugThisBurst)
        {
            printf("hfdl decode: mode=%d trainBad=%d pdu:", m_mode, m_trainBadThisBurst);
            for (int i = 0; i < std::min(24, octets); i++) {
                printf(" %02x", pdu[i]);
            }
            printf("\n");
        }

        parsePdu(pdu, result);
        result.m_pdu = std::move(pdu);
    }

    // A bit error in the short MPDU header discards a whole burst even when the
    // payload survived (seen on air: pristine LPDUs behind a header one flip away
    // from its FCS). Trial-flip up to two bits across the header region; a repair
    // is only accepted when the header FCS passes AND at least one LPDU (or the
    // SPDU) then passes its own FCS, so a false repair cannot invent frames - the
    // payload CRCs still gate everything. Called as a LAST RESORT, only when no
    // decode attempt produced a valid header, so a repair can never displace a
    // legitimate decode.
    bool repairHeader(const std::vector<uint8_t>& pdu, ParseResult& out) const
    {
        // Largest header region worth trying: downlink 6+15 length octets + FCS
        int repairBytes = std::min((int) pdu.size(), 23);
        int repairBits = repairBytes * 8;
        ParseResult best;
        std::vector<uint8_t> trial = pdu;
        auto tryTrial = [&]()
        {
            ParseResult r;
            parsePdu(trial, r);
            if (r.m_headerOk && (r.m_lpdusOk + (r.m_isSpdu ? 1 : 0) > 0)
                && ((r.m_lpdusOk > best.m_lpdusOk) || !best.m_headerOk)) {
                best = std::move(r);
            }
        };
        for (int b1 = 0; b1 < repairBits; b1++)
        {
            trial[b1 >> 3] ^= 1 << (b1 & 7);
            tryTrial();
            for (int b2 = b1 + 1; b2 < repairBits; b2++)
            {
                trial[b2 >> 3] ^= 1 << (b2 & 7);
                tryTrial();
                trial[b2 >> 3] ^= 1 << (b2 & 7);
            }
            trial[b1 >> 3] ^= 1 << (b1 & 7);
        }
        if (!best.m_headerOk) {
            return false;
        }
        out = std::move(best);
        return true;
    }

public:
    // Soft-decision Viterbi, K=7 rate 1/2, polynomials 0x6D/0x4F.
    // Public for the selftest loopback.
    //
    // flushed: the traceback starts from state 0, which HFDL can rely on because its
    // padding zeros flush the encoder. Aero frames are not flushed, so it passes false
    // and the traceback starts from whichever state ended with the best metric.
    //
    // knownStart: the encoder began in state 0. HFDL restarts its encoder every burst,
    // so forcing state 0 is free information worth having. Aero's P channel encoder runs
    // CONTINUOUSLY across frame boundaries, so a frame decoded on its own begins in an
    // unknown state and must start with every state equally likely - forcing state 0
    // there corrupts the opening bits, and on real signals that cost the first signal
    // unit of every frame.
    static void viterbiDecode(const std::vector<uint8_t>& soft, int nbits, std::vector<uint8_t>& out,
                              bool flushed = true, bool knownStart = true)
    {
        static const int NS = 64;
        // Branch chip values for (state, input): the encoder register is
        // (state << 1) | input with the newest bit in the LSB. Magic static, so
        // concurrent instances cannot race the initialisation.
        struct ChipTables
        {
            uint8_t m_a[128];
            uint8_t m_b[128];
        };
        static const ChipTables chips = []()
        {
            ChipTables t;
            for (int r = 0; r < 128; r++)
            {
                t.m_a[r] = parity7(r & 0x6D);
                t.m_b[r] = parity7(r & 0x4F);
            }
            return t;
        }();
        const uint8_t *chipA = chips.m_a;
        const uint8_t *chipB = chips.m_b;

        std::vector<uint32_t> metric(NS, knownStart ? (1u << 30) : 0u), next(NS);
        metric[0] = 0;
        std::vector<uint64_t> decisions(nbits, 0);

        for (int i = 0; i < nbits; i++)
        {
            int sa = soft[2 * i];       // 255 = strong 1
            int sb = soft[2 * i + 1];
            std::fill(next.begin(), next.end(), 0xFFFFFFFFu);
            uint64_t dec = 0;
            for (int s = 0; s < NS; s++)
            {
                uint32_t pm = metric[s];
                if (pm >= (1u << 30)) {
                    continue;
                }
                for (int b = 0; b < 2; b++)
                {
                    int reg = ((s << 1) | b) & 0x7F;
                    int ns = reg & 0x3F;
                    uint32_t cost = pm
                        + (uint32_t) std::abs(sa - 255 * chipA[reg])
                        + (uint32_t) std::abs(sb - 255 * chipB[reg]);
                    if (cost < next[ns])
                    {
                        next[ns] = cost;
                        if (s & 0x20) {
                            dec |= 1ull << ns;
                        } else {
                            dec &= ~(1ull << ns);
                        }
                    }
                }
            }
            decisions[i] = dec;
            metric.swap(next);
        }

        // Trace back from state 0 (the padding zeros flush the encoder), or from the
        // best surviving state when the burst was not flushed
        out.assign(nbits, 0);
        int state = 0;
        if (!flushed)
        {
            uint32_t best = metric[0];
            for (int s = 1; s < NS; s++)
            {
                if (metric[s] < best)
                {
                    best = metric[s];
                    state = s;
                }
            }
        }

        for (int i = nbits - 1; i >= 0; i--)
        {
            int prevTop = (decisions[i] >> state) & 1;      // Bit 5 of the previous state
            out[i] = state & 1;                             // The input bit is the LSB of the register
            state = (state >> 1) | (prevTop << 5);
        }
    }

private:
    // ---------------------------------------------------------------------------------
    // PDU parsing (MPDU/LPDU per ARINC 635; layout cross-checked against dumphfdl)
    // ---------------------------------------------------------------------------------

    bool fcsCheck(const uint8_t *buf, uint32_t len) const
    {
        // CRC-16/X-25: FCS stored little endian after the covered octets
        uint16_t computed = computeFcs(buf, (int) len);
        uint16_t check = buf[len] | (buf[len + 1] << 8);
        return computed == check;
    }

public:
    // Squitter (SPDU) fields, per ARINC 635 / dumphfdl spdu.c. The 66 octet frame
    // carries the transmitting station's TDMA state plus status for up to three
    // ground stations (its own and two neighbours); the frequency sets are bitmasks
    // into each station's system table frequency list.
    struct SquitterInfo
    {
        int m_version;
        bool m_rls;                 // Reliable link service in use
        bool m_iso8208;
        int m_changeNote;           // 0 none, 1 channel down, 2 frequency change, 3 station down
        uint32_t m_srcId;
        uint32_t m_frameIndex;      // TDMA frame counter and slot offset
        int m_frameOffset;
        int m_minPriority;
        uint32_t m_systableVersion;
        struct GsStatus
        {
            uint32_t m_id;
            bool m_utcSync;
            uint32_t m_freqsInUse;
        } m_gs[3];
    };

    static const char *squitterChangeNote(int note)
    {
        switch (note)
        {
        case 1: return "Channel down";
        case 2: return "Upcoming frequency change";
        case 3: return "Ground station down";
        default: return nullptr;    // No change
        }
    }

    // Performance data HFNPDU (FF D1): the aircraft's periodic link quality report,
    // with its position. Field layout per ARINC 635, cross-checked against dumphfdl
    // hfnpdu.c. Rate-indexed counters are ordered 1800/1200/600/300 bps.
    struct PerfDataInfo
    {
        std::string m_flightId;         // 6 characters, space padded
        double m_latitude;
        double m_longitude;
        int m_utcSeconds;               // Seconds since midnight UTC
        int m_version;
        int m_flightLeg;
        uint32_t m_gsId;
        int m_freqId;                   // Index into the ground station's frequency list
        int m_prevFreqSearches;
        int m_curFreqSearches;
        int m_prevHfDisabledSecs;
        int m_curHfDisabledSecs;
        int m_mpdusRx[4];
        int m_mpdusRxErrs[4];
        int m_spdusRx;
        int m_spdusRxErrs;
        int m_mpdusTx[4];
        int m_mpdusDelivered[4];
        int m_freqChangeCode;
    };

    // Frequency data HFNPDU (FF D5): which ground stations the aircraft hears, and
    // which of each station's frequencies propagate to it / it has tuned - carried
    // inside logon requests and resumes as the propagation report
    struct FreqDataInfo
    {
        std::string m_flightId;
        double m_latitude;
        double m_longitude;
        int m_utcSeconds;
        struct PropFreqs
        {
            uint32_t m_gsId;
            uint32_t m_propagating;     // Bitmask into the station's frequency list
            uint32_t m_tuned;
        };
        std::vector<PropFreqs> m_gs;
    };

    // Offset of an embedded HFNPDU (FF <type>) within an LPDU: after the type octet
    // for unnumbered data, after the ICAO address for logon requests and resumes.
    // -1 when the LPDU carries none.
    static int hfnpduOffset(const std::vector<uint8_t>& lpdu)
    {
        if (lpdu.size() < 2) {
            return -1;
        }
        int offset;
        switch (lpdu[0])
        {
        case 0x0D: case 0x1D: offset = 1; break;
        case 0x4F: case 0x8F: case 0xBF: offset = 4; break;
        default: return -1;
        }
        if (((size_t) (offset + 1) < lpdu.size()) && (lpdu[offset] == 0xFF)) {
            return offset;
        }
        return -1;
    }

    // 20 bit two's complement coordinate, scaled to degrees
    static double hfnpduCoordinate(uint32_t c)
    {
        int32_t r = (int32_t) (c << 12) >> 12;
        return r * 180.0 / (double) 0x7FFFF;
    }

    static bool parsePerfData(const std::vector<uint8_t>& lpdu, PerfDataInfo& out)
    {
        int off = hfnpduOffset(lpdu);
        if ((off < 0) || (lpdu.size() < (size_t) off + 47) || (lpdu[off + 1] != 0xD1)) {
            return false;
        }
        const uint8_t *b = lpdu.data() + off;
        out.m_flightId.assign((const char *) b + 2, 6);
        out.m_latitude = hfnpduCoordinate(b[8] | (b[9] << 8) | ((b[10] & 0xF) << 16));
        out.m_longitude = hfnpduCoordinate((b[10] >> 4) | (b[11] << 4) | (b[12] << 12));
        out.m_utcSeconds = 2 * (b[13] | (b[14] << 8));
        out.m_version = b[15];
        out.m_flightLeg = b[16];
        out.m_gsId = b[17] & 0x7F;
        out.m_freqId = b[18];
        out.m_prevFreqSearches = b[19] | (b[20] << 8);
        out.m_curFreqSearches = b[21] | (b[22] << 8);
        out.m_prevHfDisabledSecs = b[23] | (b[24] << 8);
        out.m_curHfDisabledSecs = b[25] | (b[26] << 8);
        for (int i = 0; i < 4; i++)
        {
            out.m_mpdusRx[i] = b[27 + i];
            out.m_mpdusRxErrs[i] = b[31 + i];
            out.m_mpdusTx[i] = b[38 + i];
            out.m_mpdusDelivered[i] = b[42 + i];
        }
        out.m_spdusRx = b[35] | (b[36] << 8);
        out.m_spdusRxErrs = b[37];
        out.m_freqChangeCode = b[46] & 0xF;
        return true;
    }

    static bool parseFreqData(const std::vector<uint8_t>& lpdu, FreqDataInfo& out)
    {
        int off = hfnpduOffset(lpdu);
        if ((off < 0) || (lpdu.size() < (size_t) off + 15) || (lpdu[off + 1] != 0xD5)) {
            return false;
        }
        const uint8_t *b = lpdu.data() + off;
        size_t len = lpdu.size() - off;
        out.m_flightId.assign((const char *) b + 2, 6);
        out.m_latitude = hfnpduCoordinate(b[8] | (b[9] << 8) | ((b[10] & 0xF) << 16));
        out.m_longitude = hfnpduCoordinate((b[10] >> 4) | (b[11] << 4) | (b[12] << 12));
        out.m_utcSeconds = 2 * (b[13] | (b[14] << 8));
        out.m_gs.clear();
        for (size_t pos = 15; pos + 6 <= len && out.m_gs.size() < 6; pos += 6)
        {
            FreqDataInfo::PropFreqs pf;
            pf.m_gsId = b[pos] & 0x7F;
            pf.m_propagating = b[pos + 1] | (b[pos + 2] << 8) | ((uint32_t) (b[pos + 3] & 0xF) << 16);
            pf.m_tuned = (b[pos + 3] >> 4) | (b[pos + 4] << 4) | ((uint32_t) b[pos + 5] << 12);
            out.m_gs.push_back(pf);
        }
        return true;
    }

    static bool parseSquitter(const std::vector<uint8_t>& b, SquitterInfo& out)
    {
        if (b.size() < 64) {
            return false;
        }
        out.m_rls = (b[0] & 0x02) != 0;
        out.m_version = (b[0] >> 2) & 3;
        out.m_iso8208 = (b[0] & 0x20) != 0;
        out.m_changeNote = (b[0] & 0xC0) >> 6;
        out.m_srcId = b[1] & 0x7F;
        out.m_frameIndex = b[2] | ((b[3] & 0xF) << 8);
        out.m_frameOffset = b[3] >> 4;
        out.m_minPriority = b[52] & 0xF;
        out.m_systableVersion = b[53] | ((b[54] & 0xF) << 8);
        out.m_gs[0].m_id = out.m_srcId;
        out.m_gs[0].m_utcSync = (b[1] & 0x80) != 0;
        out.m_gs[0].m_freqsInUse = (b[54] >> 4) | (b[55] << 4) | (b[56] << 12);
        out.m_gs[1].m_id = b[57] & 0x7F;
        out.m_gs[1].m_utcSync = (b[57] & 0x80) != 0;
        out.m_gs[1].m_freqsInUse = b[58] | (b[59] << 8) | ((b[60] & 0xF) << 16);
        out.m_gs[2].m_id = (b[60] >> 4) | ((b[61] & 0x7) << 4);
        out.m_gs[2].m_utcSync = (b[61] & 0x8) != 0;
        out.m_gs[2].m_freqsInUse = (b[61] >> 4) | (b[62] << 4) | (b[63] << 12);
        return true;
    }

    // Each ground station's frequency list in kHz, in system table order - a
    // squitter's frequencies-in-use bitmask indexes this list, bit i selecting
    // entry i. From the ARINC 635 system table version 52 (as shipped with
    // dumphfdl); the assignments are long-term stable. Returns nullptr for unknown
    // station IDs.
    static const uint16_t *gsFrequencies(uint32_t id, int *count)
    {
        static const uint16_t f1[] = { 21934, 17919, 13276, 11327, 10081, 8927, 6559, 5508 };
        static const uint16_t f2[] = { 21937, 17919, 13324, 13312, 13276, 11348, 11312, 10027, 8936, 8912, 6565, 5514 };
        static const uint16_t f3[] = { 17985, 15025, 11184, 8977, 6712, 5720, 3900 };
        static const uint16_t f4[] = { 21931, 17919, 13276, 11387, 8912, 6661, 5652 };
        static const uint16_t f5[] = { 17916, 13351, 10084, 8921, 6535, 5583 };
        static const uint16_t f6[] = { 21949, 17928, 13270, 10066, 8825, 6535, 5655 };
        static const uint16_t f7[] = { 11384, 10081, 8942, 8843, 6532, 5547, 3455, 2998 };
        static const uint16_t f8[] = { 21949, 17922, 13321, 11321, 8834, 5529, 4681, 3016 };
        static const uint16_t f9[] = { 21937, 21928, 17934, 17919, 11354, 10093, 10027, 8936, 8927, 6646, 5544, 5538, 5529, 4687, 4654, 3497, 3007, 2992, 2944 };
        static const uint16_t f10[] = { 21931, 17958, 13342, 10060, 8939, 6619, 5502, 2941 };
        static const uint16_t f11[] = { 17901, 13264, 10063, 8894, 6589, 5589 };
        static const uint16_t f13[] = { 21997, 17916, 13315, 11318, 8957, 6628, 4660 };
        static const uint16_t f14[] = { 21990, 17912, 13321, 10087, 8886, 6596, 5622 };
        static const uint16_t f15[] = { 21982, 17967, 13312, 10030, 8885, 6646, 5544, 2986 };
        static const uint16_t f16[] = { 21928, 17919, 13312, 11306, 8927, 6652, 5451 };
        static const uint16_t f17[] = { 21955, 17928, 13303, 11348, 8948, 6529 };
        #define ACARSHFDL_GS_FREQS(v) do { *count = (int) (sizeof(v) / sizeof(v[0])); return v; } while (0)
        switch (id)
        {
        case 1: ACARSHFDL_GS_FREQS(f1);
        case 2: ACARSHFDL_GS_FREQS(f2);
        case 3: ACARSHFDL_GS_FREQS(f3);
        case 4: ACARSHFDL_GS_FREQS(f4);
        case 5: ACARSHFDL_GS_FREQS(f5);
        case 6: ACARSHFDL_GS_FREQS(f6);
        case 7: ACARSHFDL_GS_FREQS(f7);
        case 8: ACARSHFDL_GS_FREQS(f8);
        case 9: ACARSHFDL_GS_FREQS(f9);
        case 10: ACARSHFDL_GS_FREQS(f10);
        case 11: ACARSHFDL_GS_FREQS(f11);
        case 13: ACARSHFDL_GS_FREQS(f13);
        case 14: ACARSHFDL_GS_FREQS(f14);
        case 15: ACARSHFDL_GS_FREQS(f15);
        case 16: ACARSHFDL_GS_FREQS(f16);
        case 17: ACARSHFDL_GS_FREQS(f17);
        default: *count = 0; return nullptr;
        }
        #undef ACARSHFDL_GS_FREQS
    }

    // Ground station coordinates from system table version 52. Returns false for
    // unknown station IDs.
    static bool gsPosition(uint32_t id, double& latitude, double& longitude)
    {
        struct Pos { double m_lat; double m_lon; };
        static const Pos positions[18] = {
            { 0.0, 0.0 },                   // 0: unused
            { 38.384587, -121.759647 },     // 1 San Francisco
            { 21.184428, -157.186846 },     // 2 Molokai
            { 63.847168, -22.455754 },      // 3 Reykjavik
            { 40.881922, -72.63762 },       // 4 Riverhead
            { -37.015757, 174.809637 },     // 5 Auckland
            { 6.937536, 100.388451 },       // 6 Hat Yai
            { 52.744089, -8.926752 },       // 7 Shannon
            { -26.129658, 28.206078 },      // 8 Johannesburg
            { 71.25849, -156.577447 },      // 9 Barrow
            { 35.032377, 126.238644 },      // 10 Muan
            { 9.084681, -79.373969 },       // 11 Albrook
            { 0.0, 0.0 },                   // 12: unused
            { -17.671199, -63.157088 },     // 13 Santa Cruz
            { 56.152603, 92.583337 },       // 14 Krasnoyarsk
            { 26.308529, 50.472318 },       // 15 Al Muharraq
            { 13.488833, 144.828233 },      // 16 Agana
            { 27.960945, -15.405608 },      // 17 Canarias
        };
        if ((id < 1) || (id > 17) || (id == 12)) {
            return false;
        }
        latitude = positions[id].m_lat;
        longitude = positions[id].m_lon;
        return true;
    }

    // HFDL ground station names by system table ID (ARINC 635 system table; the
    // assignments are long-term stable and match dumphfdl's shipped table). Returns
    // nullptr for unknown IDs. ID 255 in an MPDU's aircraft field means the aircraft
    // has no local ID assigned yet - it is logging on.
    static const char *gsName(uint32_t id)
    {
        switch (id)
        {
        case 1: return "San Francisco, California";
        case 2: return "Molokai, Hawaii";
        case 3: return "Reykjavik, Iceland";
        case 4: return "Riverhead, New York";
        case 5: return "Auckland, New Zealand";
        case 6: return "Hat Yai, Thailand";
        case 7: return "Shannon, Ireland";
        case 8: return "Johannesburg, South Africa";
        case 9: return "Barrow, Alaska";
        case 10: return "Muan, South Korea";
        case 11: return "Albrook, Panama";
        case 13: return "Santa Cruz, Bolivia";
        case 14: return "Krasnoyarsk, Russia";
        case 15: return "Al Muharraq, Bahrain";
        case 16: return "Agana, Guam";
        case 17: return "Canarias, Spain";
        default: return nullptr;
        }
    }

private:
    static const char *lpduTypeName(uint8_t type)
    {
        switch (type)
        {
        case 0x0D: return "Unnumbered data";
        case 0x1D: return "Unnumbered ack'ed data";
        case 0x2F: return "Logon denied";
        case 0x3F: return "Logoff request";
        case 0x4F: return "Logon resume";
        case 0x5F: return "Logon resume confirm";
        case 0x8F: return "Logon request";
        case 0x9F: return "Logon confirm";
        case 0xBF: return "Logon request (DLS)";
        default: return "Unknown LPDU";
        }
    }

    void parsePdu(const std::vector<uint8_t>& pdu, ParseResult& r) const
    {
        if (pdu.empty()) {
            return;
        }
        if (!(pdu[0] & 1))
        {
            // SPDU (squitter): 66 octets minimum, the FCS at [64..65] covering the
            // first 64. Emit the validated frame for display; detailed field decode
            // (frame index, ground station frequencies) is a GUI concern.
            if (pdu.size() < 66) {
                return;
            }
            r.m_fcsTested = true;
            if (!fcsCheck(pdu.data(), 64)) {
                return;
            }
            r.m_headerOk = true;
            r.m_isSpdu = true;
            Frame f;
            f.m_bytes.assign(pdu.begin(), pdu.begin() + 66);
            f.m_type = "Squitter";
            f.m_isSquitter = true;
            f.m_uplink = true;
            f.m_srcId = pdu[1] & 0x7F;
            f.m_bitRate = modeParams(m_mode).m_bitRate;
            f.m_doubleSlot = modeParams(m_mode).m_doubleSlot;
            r.m_frames.push_back(f);
            r.m_frameSlot.push_back(-1);
            r.m_otherFrames++;
            return;
        }

        const uint8_t *buf = pdu.data();
        uint32_t len = (uint32_t) pdu.size();
        bool downlink = (buf[0] & 0x2) != 0;
        uint32_t srcId;
        // An uplink MPDU can carry LPDUs for several aircraft, so each LPDU keeps
        // its own destination from the per-aircraft header groups
        struct LpduDesc
        {
            uint32_t m_len;
            uint32_t m_dstId;
        };
        std::vector<LpduDesc> lpdus;
        uint32_t hdrLen;

        if (downlink)
        {
            uint32_t lpduCnt = (buf[0] >> 2) & 0xF;
            hdrLen = 6 + lpduCnt;
            if (len < hdrLen + 2) {
                return;
            }
            srcId = buf[2];
            uint32_t dstId = buf[1] & 0x7F;
            for (uint32_t i = 0; i < lpduCnt; i++) {
                lpdus.push_back({ (uint32_t) buf[6 + i] + 1, dstId });
            }
        }
        else
        {
            uint32_t acCnt = ((buf[0] & 0x70) >> 4) + 1;
            srcId = buf[1] & 0x7F;
            hdrLen = 2;
            for (uint32_t a = 0; a < acCnt; a++)
            {
                if (len < hdrLen + 2) {
                    return;
                }
                uint32_t acId = buf[hdrLen];
                uint32_t lpduCnt = buf[hdrLen + 1] >> 4;
                for (uint32_t i = 0; i < lpduCnt; i++)
                {
                    if (hdrLen + 2 + i >= len) {
                        return;
                    }
                    lpdus.push_back({ (uint32_t) buf[hdrLen + 2 + i] + 1, acId });
                }
                hdrLen += 2 + lpduCnt;
            }
        }

        if (len < hdrLen + 2) {
            return;
        }
        r.m_fcsTested = true;
        if (!fcsCheck(buf, hdrLen)) {
            return;
        }
        r.m_headerOk = true;
        r.m_header.assign(buf, buf + hdrLen);
        r.m_lpduState.assign(lpdus.size(), ParseResult::LpduSkipped);

        uint32_t pos = hdrLen + 2;
        for (size_t slot = 0; slot < lpdus.size(); slot++)
        {
            const LpduDesc& d = lpdus[slot];
            if (pos + d.m_len > len) {
                break;
            }
            const uint8_t *l = buf + pos;
            pos += d.m_len;
            if (d.m_len < 3) {
                continue;
            }
            uint32_t payloadLen = d.m_len - 2;
            if (!fcsCheck(l, payloadLen))
            {
                r.m_lpduState[slot] = ParseResult::LpduBad;
                r.m_lpdusBad++;
                continue;
            }
            r.m_lpduState[slot] = ParseResult::LpduOk;
            r.m_lpdusOk++;

            Frame f;
            f.m_bytes.assign(l, l + payloadLen);
            f.m_type = lpduTypeName(l[0]);
            f.m_typeId = l[0];
            f.m_uplink = !downlink;
            f.m_srcId = srcId;
            f.m_dstId = d.m_dstId;
            f.m_bitRate = modeParams(m_mode).m_bitRate;
            f.m_doubleSlot = modeParams(m_mode).m_doubleSlot;
            if (((l[0] == 0x0D) || (l[0] == 0x1D))
                && (payloadLen > 4) && (l[1] == 0xFF) && (l[2] == 0xFF))
            {
                // l[3] is the ACARS SOH; the mode character follows
                f.m_isAcars = true;
                f.m_acarsOffset = 4;
                r.m_acarsFrames++;
            }
            else
            {
                r.m_otherFrames++;
            }
            r.m_frames.push_back(f);
            r.m_frameSlot.push_back((int) slot);
        }
    }

    // ---------------------------------------------------------------------------------

    static uint8_t parity7(uint32_t v)
    {
        v ^= v >> 4;
        v ^= v >> 2;
        v ^= v >> 1;
        return v & 1;
    }

    static uint8_t lfsrNext(uint16_t& lfsr)
    {
        // 15 bit LFSR, feedback polynomial x^15 + x + 1 (same as the VDL-2 scrambler)
        uint8_t bit = ((lfsr >> 0) ^ (lfsr >> 14)) & 1;
        lfsr = (uint16_t) ((lfsr >> 1) | (bit << 14));
        return bit;
    }
};

#endif // INCLUDE_ACARSHFDL_H
