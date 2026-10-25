///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// The demodulator structure and the physical layer constants (preamble phases,  //
// header FEC parity matrix and syndrome table, scrambler seed, Reed-Solomon     //
// parameters, deinterleaver geometry) follow dumpvdl2 by Tomasz Lemiech,        //
// https://github.com/szpajder/dumpvdl2, GPL-3.0.                                //
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

#ifndef INCLUDE_ACARSVDL2_H
#define INCLUDE_ACARSVDL2_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include "util/crc.h"
#include "util/reedsolomon.h"

// VDL Mode 2 receiver (ICAO Annex 10 Volume III Part I Chapter 6).
//
// The air interface is D8PSK at 10500 symbols/s: 3 bits per symbol, Gray coded onto the
// phase change between consecutive symbols, pulse shaped with a root raised cosine of
// roll-off 0.6. A burst is a ramp-up, a 16 symbol synchronisation sequence, a 5 symbol
// header (a reserved symbol, a 17 bit transmission length sent LSB first, and 5 bits of
// single-error-correcting FEC), then the data. The whole bit stream after the sync
// sequence is scrambled with an additive x^15 + x + 1 LFSR seeded with 0x6959. The data
// octets are Reed-Solomon RS(255,249) coded (GF(2^8) with polynomial 0x187, first root
// alpha^120) in blocks of up to 249 octets, block-interleaved octet by octet, with the
// final short block carrying 0/2/4/6 parity octets depending on its length. Inside the
// corrected octet stream sit one or more AVLC frames: HDLC-style 0x7E flags with zero-bit
// stuffing, two 4-octet address fields, a link control octet, an information field and a
// CRC-16/X-25 frame check sequence. An information field starting FF FF 01 carries an
// ACARS message (mode character onwards, ending ETX/ETB BCS DEL), which is what the rest
// of the ACARS demodulator plugin consumes; everything else (XID, X.25, supervisory
// frames) is reported as a frame for the GUI to summarise.
//
// Demodulation: the input at 105 kS/s (10 samples per symbol) is matched filtered with a
// root raised cosine, then the receiver works purely on sample phases. Synchronisation
// fits the 16 known preamble phases to the received phases with a mean (carrier phase)
// and slope (carrier frequency) removed by linear regression; the residual power is the
// sync metric and its minimum, located by parabolic interpolation, is the symbol clock.
// Data symbols are then decisions on the phase change per symbol, with a slow decision
// directed frequency update to ride out clock and carrier drift across long bursts.
//

#define ACARSVDL2_BAUD_RATE           10500
#define ACARSVDL2_SAMPLES_PER_SYMBOL  10
#define ACARSVDL2_CHANNEL_SAMPLE_RATE (ACARSVDL2_BAUD_RATE * ACARSVDL2_SAMPLES_PER_SYMBOL)

#define ACARSVDL2_PREAMBLE_SYMS  16
#define ACARSVDL2_SYNC_BUFLEN    (ACARSVDL2_PREAMBLE_SYMS * ACARSVDL2_SAMPLES_PER_SYMBOL)
#define ACARSVDL2_TRLEN          17    // Transmission length field, bits
#define ACARSVDL2_HDRFECLEN      5     // Header FEC field, bits
#define ACARSVDL2_HEADER_LEN     (3 + ACARSVDL2_TRLEN + ACARSVDL2_HDRFECLEN)
#define ACARSVDL2_RS_N           255
#define ACARSVDL2_RS_K           249
#define ACARSVDL2_BSLEN          32768

// What an AVLC frame type abbreviation means, for the table's Label Decode column. The
// abbreviations are the ones ISO 13239 and the VDL Mode 2 SARPs use, and are what appears
// in the Label column.
inline const char *avlcFrameTypeName(const char *type)
{
    if (!type) {
        return "";
    }
    if (!std::strcmp(type, "I"))    { return "Information"; }
    if (!std::strcmp(type, "UI"))   { return "Unnumbered information"; }
    if (!std::strcmp(type, "XID"))  { return "Exchange identification (link management)"; }
    if (!std::strcmp(type, "TEST")) { return "Test"; }
    if (!std::strcmp(type, "RR"))   { return "Receive ready"; }
    if (!std::strcmp(type, "RNR"))  { return "Receive not ready"; }
    if (!std::strcmp(type, "REJ"))  { return "Reject - retransmit from this sequence"; }
    if (!std::strcmp(type, "SREJ")) { return "Selective reject - retransmit this frame"; }
    if (!std::strcmp(type, "DM"))   { return "Disconnected mode"; }
    if (!std::strcmp(type, "DISC")) { return "Disconnect"; }
    if (!std::strcmp(type, "UA"))   { return "Unnumbered acknowledgement"; }
    if (!std::strcmp(type, "FRMR")) { return "Frame reject"; }
    if (!std::strcmp(type, "U"))    { return "Unnumbered"; }
    return "";
}

class AcarsVdl2Receiver
{
public:
    typedef std::complex<double> Cd;

    struct Frame
    {
        std::vector<uint8_t> m_bytes;   // Whole AVLC frame including the FCS
        uint32_t m_dstAddress;          // 24-bit DLS addresses
        uint32_t m_srcAddress;
        int m_dstType;                  // 3-bit address type (1 aircraft, 4/5 ground, 7 all)
        int m_srcType;
        bool m_fromAircraft;
        bool m_srcStatus;               // Aircraft: airborne(0)/on-ground(1). Ground: command/response
        bool m_dstStatus;
        char m_frameType[8];            // "I", "RR", "XID", ...
        bool m_isAcars;                 // Information field starts FF FF 01
        int m_infoOffset;               // Information field position within m_bytes
        int m_infoLength;               // ... and its length (excludes the FCS)
    };

    struct Config
    {
        // Normalised differential preamble correlation needed to sync, 0 to 1. A genuine
        // preamble reads near 1 (about 0.9 at 9 dB Eb/N0); noise reads around 0.26 (the
        // mean magnitude of a random 15-phasor sum), with a false-fire probability per
        // sample of roughly exp(-15 t^2) - at 0.55 that is ~1% per sample, enough to keep
        // the receiver deaf in noise-triggered bursts most of the time. A ratio, so it
        // does not move with signal level.
        double m_syncThreshold = 0.70;
        // Second detection path for preambles the main threshold misses (nicked by an
        // impulse or a noise excursion): a lower stat suffices when |D| stands well
        // above its ambient average. Stationary noise cannot satisfy both at once -
        // stat and |D| are locked together through the slowly-varying norm, so
        // |D| at m_syncAbsDGate times the ambient mean would imply a stat above 1 -
        // the pair only opens when genuine signal energy arrives. 0 disables.
        double m_syncThresholdLow = 0.50;
        double m_syncAbsDGate = 4.0;
        // Decision directed per-symbol frequency update. Small, because a single bad
        // decision is a 45 degree error; 0 disables. dumpvdl2 free-runs from the preamble
        // estimate, but the estimate has variance and long frames accumulate its error.
        double m_freqGain = 0.005;
        // Gardner symbol timing tracking gain; 0 disables and the clock free-runs from
        // the preamble estimate (as dumpvdl2 does). Without it a clock offset of f ppm
        // drifts the sampling instant by about one sample per 10^6/(10 f) symbols, which
        // costs long bursts. Measured with cliff_vdl2.ps1: 0.03 tracks 800 ppm and costs
        // 0.9 points at the sensitivity cliff (Gardner self-noise); 0.05 costs 1.2
        double m_timingGain = 0.03;
        // Decision-feedback reference blending. Plain differential detection (1.0)
        // compares each symbol against the previous, equally noisy, sample and costs
        // about 2.7 dB against coherent 8PSK. Blending the received samples into a
        // decision-rotated reference phasor averages the reference noise down over about
        // 1/gain symbols, recovering most of that loss while still following phase drift.
        double m_refGain = 0.25;
        // Erasure decoding: when a Reed-Solomon block fails outright (more than 3 octets
        // in error), retry with the least reliable octets - those whose symbols decided
        // closest to a phase boundary - marked as erasures, trading known-bad positions
        // (2 x errors + erasures <= 6) for correction power. Miscorrections this may let
        // through are caught by the AVLC frame check sequence.
        bool m_rsErasures = true;
        // Two-pass detection: at the end of the burst, reconstruct the carrier by
        // removing the first pass's decided modulation, smooth it over +-m_smoothSpan
        // symbols from both sides (self-excluded), and re-decide every symbol against
        // that reference. MEASURED WORSE than the causal reference filter and off by
        // default: at the error rates where it would matter, the modulation removal
        // itself uses enough wrong decisions that the contaminated references create new
        // errors around old ones (50 vs 81 of 200 messages at 10 dB Eb/N0). Kept for
        // future work - a proper per-symbol MAP would need iteration or candidate
        // marginalisation rather than a single hard-decision cleanup pass.
        bool m_twoPass = false;
        int m_smoothSpan = 6;
        // Chase list decoding: a failed Reed-Solomon block (after erasures) is retried
        // with subsets of its least reliable octets flipped to their alternative values
        // (the second-nearest phase decision of their worst symbol), up to this many
        // candidate octets (2^n - 1 subset attempts). 0 disables.
        int m_chaseOctets = 5;
        // Transmission length sanity caps, bits (per dumpvdl2)
        uint32_t m_maxFrameLength = 0x3FFF;
        uint32_t m_maxFrameLengthCorrected = 0x1FFF;
        // A detection this much stronger (in |D|) than the sync of the burst being
        // decoded preempts it: the young margin within the first 32 symbols (premature
        // fires on precursors and sidelobes), the old margin at any age (noise-triggered
        // bursts, whose random header often passes the weak header FEC). 0 disables
        // preemption (A/B testing).
        double m_preemptYoungMargin = 1.2;
        double m_preemptOldMargin = 4.0;
        // Preamble correlation products above this multiple of the median product
        // magnitude are treated as impulses and excluded. 0 disables (A/B testing).
        double m_trimRatio = 4.0;
        // Dump acquisition and symbol decisions for this many bursts (test harness only)
        int m_debugBursts = 0;
    };

    struct Stats
    {
        uint64_t m_samples = 0;
        uint64_t m_syncs = 0;               // Preambles detected
        uint64_t m_headerFecOk = 0;         // Headers with a zero syndrome
        uint64_t m_headerFecCorrected = 0;  // ... with a corrected single-bit error
        uint64_t m_headerRejected = 0;      // Bad reserved bits or implausible length
        uint64_t m_rsBlocksOk = 0;
        uint64_t m_rsBlocksFailed = 0;
        uint64_t m_rsOctetsCorrected = 0;
        uint64_t m_rsErasureRecovered = 0;  // Blocks recovered by erasure decoding
        uint64_t m_rsChaseRecovered = 0;    // Blocks recovered by Chase list decoding
        uint64_t m_twoPassChanged = 0;      // Symbols the two-pass detection re-decided
        uint64_t m_bitstuffErrors = 0;
        uint64_t m_fcsValid = 0;
        uint64_t m_fcsInvalid = 0;
        uint64_t m_shortFrames = 0;         // Under the 11 octet AVLC minimum
        uint64_t m_acarsFrames = 0;
        uint64_t m_otherFrames = 0;
    };

    AcarsVdl2Receiver()
    {
        createMatchedFilter();

        // Known preamble phase increments, for the differential correlator
        const double *pr = preamblePhases();
        m_prDiffRe[0] = 1.0;
        m_prDiffIm[0] = 0.0;
        for (int k = 1; k < ACARSVDL2_PREAMBLE_SYMS; k++)
        {
            m_prDiffRe[k] = std::cos(pr[k] - pr[k-1]);
            m_prDiffIm[k] = std::sin(pr[k] - pr[k-1]);
        }

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
        m_sclk = 0;
        m_mu = 0.0;
        m_ringIdx = 0;
        m_ringFill = 0;
        std::fill(std::begin(m_ring), std::end(m_ring), Cd(0.0, 0.0));
        m_statPrev1 = 0.0;
        m_prevAbsD = 0.0;
        m_absDEma = 0.0;
        m_emaWarmup = 0;
        m_syncPending = false;
        m_bestAbsD = 0.0;
        m_bestPrevAbsD = 0.0;
        m_bestNextAbsD = 0.0;
        m_bestAgo = 0;
        m_bestD = Cd(0.0, 0.0);
        m_burstAbsD = 0.0;
        m_lastSyncError = 0.0;
        m_debugRemaining = m_config.m_debugBursts;
        m_debugThisBurst = false;
        m_frames.clear();
        resetBurst();
    }

    // Process one sample at ACARSVDL2_CHANNEL_SAMPLE_RATE. Returns true when at least one
    // frame was queued by this sample; drain with hasFrame()/popFrame().
    bool processSample(const Cd& in)
    {
        m_stats.m_samples++;
        Cd s = matchedFilter(in);
        size_t framesBefore = m_frames.size();

        // A single ring of raw matched filter outputs serves the preamble correlator, the
        // fractional-delay symbol sampling and the timing detector
        m_ringIdx = (m_ringIdx + 1) % RING_LEN;
        m_ring[m_ringIdx] = s;
        if (m_ringFill < RING_LEN) {
            m_ringFill++;
        }

        // The preamble search runs in both states: during a burst it lets a clearly
        // stronger detection preempt a young burst started by a premature fire
        bool synced = false;
        if (m_ringFill >= ACARSVDL2_SYNC_BUFLEN + 4) {
            synced = searchPreamble();
        }

        if (!synced && (m_state == StateData))
        {
            if (++m_sclk >= ACARSVDL2_SAMPLES_PER_SYMBOL)
            {
                m_sclk = 0;
                // The symbol sample, interpolated at the fractional timing position
                Cd y0 = interpAt(0, m_mu);
                double phi = std::arg(y0);
                double dphiSymUsed = m_dphiSym;
                // Decide the phase change against the reference phasor: with m_refGain of
                // 1 the reference is just the previous sample (plain differential
                // detection); below 1 it is a decision-rotated average with less noise
                double dphi = std::arg(y0 * std::conj(m_ref)) - m_dphiSym;
                while (dphi < 0.0) {
                    dphi += 2.0 * M_PI;
                }
                while (dphi >= 2.0 * M_PI) {
                    dphi -= 2.0 * M_PI;
                }
                int idx = (int) std::lround(dphi / M_PI_4);
                // Decision directed frequency tracking on the quantisation residual
                double residual = dphi - idx * M_PI_4;
                if (m_config.m_freqGain != 0.0) {
                    m_dphiSym += m_config.m_freqGain * residual;
                }
                idx &= 7;

                // Rotate the reference by the decided phase change plus the carrier
                // advance, then blend the received sample in (normalised, so the blend
                // weight does not depend on signal level)
                m_ref *= std::polar(1.0, idx * M_PI_4 + m_dphiSym);
                double sMag = std::abs(y0);
                if (sMag > 1e-12) {
                    m_ref = (1.0 - m_config.m_refGain) * m_ref + m_config.m_refGain * (y0 / sMag);
                }
                double refMag = std::abs(m_ref);
                if (refMag > 1e-12) {
                    m_ref /= refMag;
                }
                m_prevPhi = phi;

                // Gardner timing detector on interpolated samples, derotated relative to
                // each other so a carrier offset does not corrupt the products. The error
                // updates the fractional timing directly.
                if ((m_config.m_timingGain != 0.0) && (++m_symbolsSinceSync > 2))
                {
                    const int sps = ACARSVDL2_SAMPLES_PER_SYMBOL;
                    Cd yHalf = interpAt(sps/2, m_mu) * std::polar(1.0, 0.5 * m_dphiSym);
                    Cd yPrev = interpAt(sps, m_mu) * std::polar(1.0, m_dphiSym);
                    double norm = 0.5 * (std::norm(y0) + std::norm(yPrev)) + 1e-12;
                    double e = ((y0 - yPrev) * std::conj(yHalf)).real() / norm;
                    // An impulse landing on the mid-symbol sample is not covered by the
                    // normalisation and would kick the timing across symbol boundaries
                    e = std::max(-1.0, std::min(1.0, e));

                    // The sign was established against the generator: positive feedback
                    // here diverges, this converges (see test/README.md)
                    m_mu -= m_config.m_timingGain * e;
                    if (m_mu >= 1.0)
                    {
                        m_mu -= 1.0;
                        m_sclk = -1;    // Sample one later
                    }
                    else if (m_mu < 0.0)
                    {
                        m_mu += 1.0;
                        m_sclk = 1;     // Sample one earlier
                    }
                }

                if (m_debugThisBurst && (m_debugSymbol < 60))
                {
                    printf("vdl2 sym %4d: |s|=%6.3f phi=%7.3f dphi=%7.3f idx=%d res=%+6.3f dphiSym=%+8.5f mu=%.3f\n",
                        m_debugSymbol++, std::abs(y0), phi, dphi, idx, residual, m_dphiSym, m_mu);
                }

                static const uint8_t graycode[8] = {0, 1, 3, 2, 6, 7, 5, 4};
                // Reliability from the distance to the decision boundary (pi/8 away),
                // and from the amplitude: an impulse or a fade dominates the sample and
                // decides confidently but wrongly, which the residual alone cannot see,
                // while its magnitude stands well away from the running mean
                double mag = std::abs(y0);
                double relResidual = std::abs(residual) * (255.0 / (M_PI / 8.0));
                double relAmplitude = 0.0;
                if (m_magMean <= 0.0) {
                    m_magMean = mag;    // Seed from the first symbol of the burst
                } else {
                    relAmplitude = std::abs(mag - m_magMean) * (255.0 / m_magMean);
                }
                m_magMean += 0.02 * (mag - m_magMean);
                uint8_t rel = (uint8_t) std::min(255.0, std::max(relResidual, relAmplitude));
                // The second-nearest decision, for Chase list decoding
                int altIdx = (idx + (residual >= 0.0 ? 1 : 7)) & 7;
                m_bs.appendMsbFirst(graycode[idx], 3, rel, graycode[altIdx], true);

                // Record the symbol for the two-pass detection at the end of the burst
                m_syms.push_back({y0, dphiSymUsed, (uint8_t) idx});

                if (m_bs.length() >= m_requestedBits)
                {
                    if (!decodeBurst())
                    {
                        // Burst finished or failed: back to searching, with the detection
                        // history cleared so the burst tail cannot fire a detection
                        m_state = StateSearch;
                        m_sclk = 0;
                        m_statPrev1 = 0.0;
                        m_syncPending = false;
                    }
                }
            }
        }

        return m_frames.size() > framesBefore;
    }

    bool hasFrame() const { return !m_frames.empty(); }

    Frame popFrame()
    {
        Frame f = m_frames.front();
        m_frames.pop_front();
        return f;
    }

    // For the scope: last normalised preamble correlation (0 to 1, high = sync) and
    // whether a burst is being demodulated
    double syncStatistic() const { return m_lastSyncError; }
    bool synced() const { return m_state == StateData; }

    // ---------------------------------------------------------------------------------
    // Encode-side helpers, used by the offline test harness to build loopback bursts.
    // These are the exact inverses of the decode path, so a round-trip through them pins
    // every bit-order and FEC convention.
    // ---------------------------------------------------------------------------------

    // Cumulative preamble phases (radians). The transmitted preamble symbol phases,
    // relative to an arbitrary starting phase.
    static const double *preamblePhases()
    {
        static const double phases[ACARSVDL2_PREAMBLE_SYMS] = {
            0 * M_PI / 4,  3 * M_PI / 4, -3 * M_PI / 4,  1 * M_PI / 4,
            1 * M_PI / 4,  2 * M_PI / 4,  0 * M_PI / 4,  4 * M_PI / 4,
            -3 * M_PI / 4, 4 * M_PI / 4, -2 * M_PI / 4,  3 * M_PI / 4,
            1 * M_PI / 4, -2 * M_PI / 4, -3 * M_PI / 4,  0 * M_PI / 4
        };
        return phases;
    }

    // Build an AVLC frame: address fields, control, information, FCS appended
    static std::vector<uint8_t> buildAvlcFrame(
        uint32_t dstAddress, int dstType, bool dstStatus,
        uint32_t srcAddress, int srcType, bool srcStatus,
        uint8_t control, const std::vector<uint8_t>& info)
    {
        std::vector<uint8_t> frame;
        appendDlcAddress(frame, dstAddress, dstType, dstStatus, false);
        appendDlcAddress(frame, srcAddress, srcType, srcStatus, true);
        frame.push_back(control);
        frame.insert(frame.end(), info.begin(), info.end());
        crc16x25 crc;
        crc.init();
        crc.calculate(frame.data(), (int) frame.size());
        uint16_t fcs = (uint16_t) crc.get();
        frame.push_back(fcs & 0xff);
        frame.push_back((fcs >> 8) & 0xff);
        return frame;
    }

    // Build the complete post-preamble bit sequence for a burst: header (with FEC),
    // then flags/stuffed frames interleaved and RS coded, all scrambled. The result is
    // ready to be D8PSK modulated symbol by symbol (3 bits per symbol, MSB first).
    static std::vector<uint8_t> encodeBurst(const std::vector<std::vector<uint8_t>>& frames)
    {
        // Flag, then each stuffed frame followed by a flag
        std::vector<uint8_t> data;
        appendByteLsbFirst(data, 0x7e);
        for (const auto& frame : frames)
        {
            int ones = 0;
            for (uint8_t byte : frame)
            {
                for (int j = 0; j < 8; j++)
                {
                    int bit = (byte >> j) & 1;
                    data.push_back((uint8_t) bit);
                    if (bit) {
                        if (++ones == 5) {
                            data.push_back(0); // Stuff a zero after five ones
                            ones = 0;
                        }
                    } else {
                        ones = 0;
                    }
                }
            }
            appendByteLsbFirst(data, 0x7e);
        }

        uint32_t datalen = (uint32_t) data.size();
        uint32_t datalenOctets = (datalen + 7) / 8;

        // Pad the bit stream to whole octets (the pad bits are discarded by the decoder,
        // which truncates to the transmission length)
        while (data.size() % 8) {
            data.push_back(0);
        }

        // Pack into octets LSB first, interleave with RS parity
        std::vector<uint8_t> octets(datalenOctets);
        for (uint32_t i = 0; i < datalenOctets; i++)
        {
            uint8_t byte = 0;
            for (int j = 0; j < 8; j++) {
                byte |= data[i*8+j] << j;
            }
            octets[i] = byte;
        }

        uint32_t numBlocks = datalenOctets / ACARSVDL2_RS_K;
        uint32_t lastBlockLen = datalenOctets % ACARSVDL2_RS_K;
        if (lastBlockLen != 0) {
            numBlocks++;
        } else {
            lastBlockLen = ACARSVDL2_RS_K;
        }

        // Each RS block is a sequential chunk of the octet stream (row r of the
        // interleaver matrix), the last block short and zero-padded; the interleaving
        // happens on transmission, which reads the matrix column by column
        std::vector<std::array<uint8_t, ACARSVDL2_RS_N>> rsTab(numBlocks);
        for (auto& row : rsTab) {
            row.fill(0);
        }
        for (uint32_t r = 0; r < numBlocks; r++)
        {
            uint32_t count = (r == numBlocks - 1) ? lastBlockLen : ACARSVDL2_RS_K;
            for (uint32_t i = 0; i < count; i++) {
                rsTab[r][i] = octets[r * ACARSVDL2_RS_K + i];
            }
        }

        // RS parity per block; the last (short) block gets a reduced number of parity octets
        ReedSolomon::reed_solomon<ACARSVDL2_RS_N - ACARSVDL2_RS_K, 120, 1, ReedSolomon::gfpoly<0x187>> rs;
        for (uint32_t r = 0; r < numBlocks; r++)
        {
            uint8_t parity[ACARSVDL2_RS_N - ACARSVDL2_RS_K];
            rs.encode(rsTab[r].data(), ACARSVDL2_RS_K, parity);
            std::memcpy(rsTab[r].data() + ACARSVDL2_RS_K, parity, ACARSVDL2_RS_N - ACARSVDL2_RS_K);
        }

        // Read the matrix back out column by column: data columns then parity columns
        std::vector<uint8_t> bits;
        bits.reserve(ACARSVDL2_HEADER_LEN + 8 * (datalenOctets + numBlocks * 6) + 24);

        // Header: 3 reserved bits, 17 length bits LSB of the length first, 5 FEC bits
        uint32_t word = reverseBits(datalen, ACARSVDL2_TRLEN) << ACARSVDL2_HDRFECLEN;
        for (int i = 0; i < ACARSVDL2_HDRFECLEN; i++)
        {
            if (parity32(word & headerParityMatrix()[i] & ~((1u << ACARSVDL2_HDRFECLEN) - 1))) {
                word |= headerParityMatrix()[i] & ((1u << ACARSVDL2_HDRFECLEN) - 1);
            }
        }
        for (int i = ACARSVDL2_HEADER_LEN - 1; i >= 0; i--) {
            bits.push_back((word >> i) & 1);
        }

        auto appendMatrixColumns = [&bits, &rsTab, numBlocks, lastBlockLen](uint32_t firstCol, uint32_t cols, bool isParity, uint32_t lastBlockFec)
        {
            for (uint32_t col = firstCol; col < firstCol + cols; col++)
            {
                for (uint32_t row = 0; row < numBlocks; row++)
                {
                    bool skip;
                    if (isParity) {
                        // Short last block: only its first lastBlockFec parity octets are sent
                        skip = (row == numBlocks - 1) && (col - firstCol >= lastBlockFec);
                    } else {
                        // Short last block: data columns beyond its length are not sent
                        skip = (row == numBlocks - 1) && (col >= lastBlockLen);
                    }
                    if (skip) {
                        continue;
                    }
                    for (int j = 0; j < 8; j++) {
                        bits.push_back((rsTab[row][col] >> j) & 1);
                    }
                }
            }
        };
        uint32_t lastBlockFec = fecOctetCount(lastBlockLen);
        appendMatrixColumns(0, ACARSVDL2_RS_K, false, 0);
        appendMatrixColumns(ACARSVDL2_RS_K, ACARSVDL2_RS_N - ACARSVDL2_RS_K, true, lastBlockFec);

        // Scramble everything after the sync sequence
        uint16_t lfsr = 0x6959;
        for (auto& bit : bits) {
            bit ^= lfsrNext(lfsr);
        }

        return bits;
    }

private:
    static const int MATCHED_SPAN = 8; // RRC matched filter span, symbols

    enum State
    {
        StateSearch,
        StateData
    };

    enum DecoderState
    {
        DecoderHeader,
        DecoderData
    };

    // A bitstream holding one bit per element, with a persistent descrambler position so
    // the header and data are descrambled as one continuous LFSR sequence. Each bit
    // carries a reliability (0 = solid decision, 255 = on a phase boundary), used to
    // choose erasure positions when Reed-Solomon fails, and an alternative value (the
    // same bit from the symbol's second-nearest decision), used by Chase list decoding.
    struct Bitstream
    {
        std::vector<uint8_t> m_bits;
        std::vector<uint8_t> m_rel;
        std::vector<uint8_t> m_alt;
        uint32_t m_start = 0;
        uint32_t m_descramblerPos = 0;

        void reset()
        {
            m_bits.clear();
            m_rel.clear();
            m_alt.clear();
            m_start = 0;
            m_descramblerPos = 0;
        }

        uint32_t length() const { return (uint32_t) m_bits.size(); }
        uint32_t available() const { return (uint32_t) m_bits.size() - m_start; }

        void appendMsbFirst(uint32_t word, int numBits, uint8_t rel = 0, uint32_t altWord = 0, bool haveAlt = false)
        {
            for (int i = numBits - 1; i >= 0; i--)
            {
                m_bits.push_back((word >> i) & 1);
                m_rel.push_back(rel);
                m_alt.push_back(haveAlt ? ((altWord >> i) & 1) : ((word >> i) & 1));
            }
        }

        void descramble(uint16_t& lfsr)
        {
            if (m_descramblerPos < m_start) {
                m_descramblerPos = m_start;
            }
            for (uint32_t i = m_descramblerPos; i < m_bits.size(); i++)
            {
                uint8_t bit = lfsrNext(lfsr);
                m_bits[i] ^= bit;
                m_alt[i] ^= bit;
            }
            m_descramblerPos = (uint32_t) m_bits.size();
        }

        bool readWordMsbFirst(uint32_t& word, int numBits)
        {
            if (m_start + numBits > m_bits.size()) {
                return false;
            }
            word = 0;
            for (int i = 0; i < numBits; i++) {
                word |= ((uint32_t) m_bits[m_start++]) << (numBits - 1 - i);
            }
            return true;
        }

        // Read octets, their reliability (an octet is only as reliable as its worst bit)
        // and their alternative value: the octet with the bits of its least reliable
        // symbol replaced by that symbol's second-nearest decision
        bool readLsbFirst(uint8_t *bytes, uint8_t *rel, uint8_t *alt, uint32_t numBytes)
        {
            if (m_start + 8 * numBytes > m_bits.size()) {
                return false;
            }
            for (uint32_t i = 0; i < numBytes; i++)
            {
                bytes[i] = 0;
                uint8_t worst = 0;
                for (int j = 0; j < 8; j++) {
                    worst = std::max(worst, m_rel[m_start + j]);
                }
                uint8_t altByte = 0;
                for (int j = 0; j < 8; j++)
                {
                    bytes[i] |= m_bits[m_start] << j;
                    uint8_t altBit = (m_rel[m_start] == worst) ? m_alt[m_start] : m_bits[m_start];
                    altByte |= altBit << j;
                    m_start++;
                }
                if (rel) {
                    rel[i] = worst;
                }
                if (alt) {
                    alt[i] = altByte;
                }
            }
            return true;
        }
    };

    Config m_config;
    Stats m_stats;
    State m_state;
    std::deque<Frame> m_frames;

    // Matched filter
    std::vector<double> m_mfTaps;
    std::vector<Cd> m_mfSamples;
    unsigned int m_mfPtr = 0;

    // Synchronisation: differential correlation detection with a peak-hold window
    int m_sclk;
    double m_statPrev1;         // Statistic at the previous sample
    double m_prevAbsD;          // |D| at the previous sample
    double m_absDEma;           // Ambient |D| average, for the low-threshold gate
    int m_emaWarmup;            // Samples the EMA has seen, up to its warmup bound
    bool m_syncPending;         // Above threshold, tracking the peak
    double m_bestAbsD;          // Peak |D| and its neighbours, for interpolation
    double m_bestPrevAbsD;
    double m_bestNextAbsD;
    int m_bestAgo;              // How many samples ago the peak was
    Cd m_bestD;                 // Differential correlation at the peak
    double m_burstAbsD;         // |D| of the sync that started the current burst
    double m_lastSyncError;
    double m_prDiffRe[ACARSVDL2_PREAMBLE_SYMS];    // e^{j(pr[k]-pr[k-1])}, precomputed
    double m_prDiffIm[ACARSVDL2_PREAMBLE_SYMS];
    int m_debugRemaining = 0;
    bool m_debugThisBurst = false;
    int m_debugSymbol = 0;

    // Demodulation and decode of the current burst
    double m_prevPhi = 0.0;
    double m_dphiSym = 0.0;
    double m_mu = 0.0;          // Fractional symbol timing, 0..1 between ring samples
    Cd m_ref = Cd(1.0, 0.0);    // Decision-feedback reference phasor
    double m_magMean = 0.0;     // Running mean symbol magnitude, for the reliability metric

    // Per-symbol record of the burst, for the two-pass detection
    struct SymbolRecord
    {
        Cd m_s;             // The sample the symbol was decided from
        double m_dphiSym;   // The carrier advance estimate used
        uint8_t m_idx;      // The first-pass decision
    };
    std::vector<SymbolRecord> m_syms;

    // Raw matched filter outputs: the preamble correlator needs the whole 16 symbol
    // sync sequence plus a symbol of history for the differential products, the
    // interpolator a few extra taps
    static const int RING_LEN = ACARSVDL2_SYNC_BUFLEN + 2 * ACARSVDL2_SAMPLES_PER_SYMBOL;
    Cd m_ring[RING_LEN];
    int m_ringIdx = 0;
    int m_ringFill = 0;
    int m_symbolsSinceSync = 0;
    Bitstream m_bs;
    DecoderState m_decoderState = DecoderHeader;
    uint32_t m_requestedBits = ACARSVDL2_HEADER_LEN;
    uint16_t m_lfsr = 0x6959;
    uint32_t m_syndrome = 0;
    uint32_t m_datalen = 0;
    uint32_t m_datalenOctets = 0;
    uint32_t m_numBlocks = 0;
    uint32_t m_lastBlockLenOctets = 0;
    uint32_t m_fecOctets = 0;

    ReedSolomon::reed_solomon<ACARSVDL2_RS_N - ACARSVDL2_RS_K, 120, 1, ReedSolomon::gfpoly<0x187>> m_rs;

    void resetBurst()
    {
        m_bs.reset();
        m_syms.clear();
        m_decoderState = DecoderHeader;
        m_requestedBits = ACARSVDL2_HEADER_LEN;
        m_lfsr = 0x6959;
        m_syndrome = 0;
    }

    // ---------------------------------------------------------------------------------
    // Matched filter: root raised cosine, roll-off 0.6, MATCHED_SPAN symbols
    // ---------------------------------------------------------------------------------

    void createMatchedFilter()
    {
        const double beta = 0.6;
        const int sps = ACARSVDL2_SAMPLES_PER_SYMBOL;
        const int nTaps = MATCHED_SPAN * sps + 1;

        m_mfTaps.resize(nTaps);
        double sum = 0.0;
        for (int i = 0; i < nTaps; i++)
        {
            double t = (i - nTaps / 2) / (double) sps;
            double num = std::sin(M_PI * t * (1.0 - beta)) + 4.0 * beta * t * std::cos(M_PI * t * (1.0 + beta));
            double den = M_PI * t * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));
            double tap;
            if ((num == 0.0) && (den == 0.0)) {
                tap = 1.0 + beta * (4.0 / M_PI - 1.0);
            } else if (den == 0.0) {
                tap = (beta / std::sqrt(2.0)) * ((1.0 + 2.0 / M_PI) * std::sin(M_PI / (4.0 * beta))
                                               + (1.0 - 2.0 / M_PI) * std::cos(M_PI / (4.0 * beta)));
            } else {
                tap = num / den;
            }
            m_mfTaps[i] = tap;
            sum += tap;
        }
        for (auto& tap : m_mfTaps) {  // Unity DC gain
            tap /= sum;
        }
        m_mfSamples.assign(nTaps, Cd(0.0, 0.0));
        m_mfPtr = 0;
    }

    Cd matchedFilter(const Cd& in)
    {
        m_mfSamples[m_mfPtr] = in;
        Cd acc(0.0, 0.0);
        unsigned int idx = m_mfPtr;
        const unsigned int n = (unsigned int) m_mfTaps.size();
        for (unsigned int i = 0; i < n; i++)
        {
            acc += m_mfSamples[idx] * m_mfTaps[i];
            idx = (idx == 0) ? n - 1 : idx - 1;
        }
        m_mfPtr = (m_mfPtr + 1) % n;
        return acc;
    }

    // ---------------------------------------------------------------------------------
    // Synchronisation: two-stage correlation acquisition
    // ---------------------------------------------------------------------------------

    // Sample received `back` samples ago
    Cd tap(int back) const
    {
        return m_ring[(m_ringIdx + RING_LEN - back) % RING_LEN];
    }

    // Cubic (Catmull-Rom) interpolation at the position (now - 2 - d + mu), mu in 0..1.
    // Strictly causal: the newest point used is the current sample.
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

    // Differential correlation of the last 16 symbol-spaced samples against the known
    // preamble phase increments. A carrier offset rotates every product equally, so the
    // normalised magnitude is a CFO-immune detection statistic and the argument of the
    // correlation is the per-symbol carrier advance. Amplitude weighting makes this
    // stronger at low SNR than fitting sample phases. On the peak - located to a
    // fraction of a sample by parabolic interpolation - the 16 preamble symbols are
    // combined coherently into the initial reference phasor. Returns true when a new
    // burst was started this sample.
    bool searchPreamble()
    {
        const int sps = ACARSVDL2_SAMPLES_PER_SYMBOL;

        // A genuine preamble spreads its energy evenly over the 15 products (each about
        // 1/15 of the total); an impulse dominates a few of them, which both fakes
        // detections on non-preamble data (a huge |D| that would preempt a good burst)
        // and blocks genuine ones (an inflated norm deflates the stat, and a corrupted
        // product wrecks the carrier estimate). Products far above the median magnitude
        // are impulses, not preamble, so they are excluded from the correlation.
        Cd prods[ACARSVDL2_PREAMBLE_SYMS];
        double mags[ACARSVDL2_PREAMBLE_SYMS];
        double sorted[ACARSVDL2_PREAMBLE_SYMS];
        for (int k = 1; k < ACARSVDL2_PREAMBLE_SYMS; k++)
        {
            int back = (ACARSVDL2_PREAMBLE_SYMS - 1 - k) * sps;
            prods[k] = tap(back) * std::conj(tap(back + sps));
            mags[k] = std::abs(prods[k]);
            sorted[k - 1] = mags[k];
        }
        double lim = 0.0;
        if (m_config.m_trimRatio > 0.0)
        {
            std::nth_element(sorted, sorted + (ACARSVDL2_PREAMBLE_SYMS - 1) / 2,
                sorted + ACARSVDL2_PREAMBLE_SYMS - 1);
            lim = m_config.m_trimRatio * sorted[(ACARSVDL2_PREAMBLE_SYMS - 1) / 2];
        }

        Cd d(0.0, 0.0);
        double norm = 0.0;
        for (int k = 1; k < ACARSVDL2_PREAMBLE_SYMS; k++)
        {
            if ((lim > 0.0) && (mags[k] > lim)) {
                continue;
            }
            d += prods[k] * Cd(m_prDiffRe[k], -m_prDiffIm[k]);
            norm += mags[k];
        }
        double stat = (norm > 1e-12) ? std::abs(d) / norm : 0.0;
        m_lastSyncError = stat;

        // The normalised stat is scale-invariant, which makes it a good threshold but a
        // bad peak selector: on a clean signal the deterministic RRC precursor tails
        // before the burst correlate perfectly at near-zero amplitude, and preamble
        // sidelobes fire it a few symbols early. So the threshold crossing opens a
        // pending window that lasts while the stat stays above threshold, and within it
        // the peak is picked by |D| - the amplitude-weighted correlation - which is
        // maximal only when all 16 preamble symbols are present at full amplitude.
        double absD = std::abs(d);

        // Detection: the main threshold alone, or the low-threshold path when |D|
        // stands well above its ambient average (see Config). The ambient EMA is
        // updated after the comparison so a peak cannot suppress itself, and the low
        // path stays closed until the EMA has seen enough samples to mean something.
        bool detected = (stat >= m_config.m_syncThreshold)
            || ((m_config.m_syncThresholdLow > 0.0) && (m_emaWarmup >= 1024)
                && (stat >= m_config.m_syncThresholdLow)
                && (absD > m_config.m_syncAbsDGate * m_absDEma));
        m_absDEma += (1.0 / 512.0) * (absD - m_absDEma);
        if (m_emaWarmup < 1024) {
            m_emaWarmup++;
        }

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
                m_bestNextAbsD = absD;  // The sample immediately after the peak
            }
            m_bestAgo++;
        }
        m_prevAbsD = absD;
        m_statPrev1 = stat;

        // The peak must be recent enough for the symbol clock mapping and ring taps
        if (detected && (m_bestAgo < sps)) {
            return false;
        }
        m_syncPending = false;

        // While a burst is being decoded the search keeps running, so a premature fire
        // never goes deaf through a real preamble. A slightly stronger detection
        // preempts a still-young burst (a precursor or sidelobe fire, with the true
        // peak a few symbols later); a decisively stronger one preempts at any age -
        // that is what recovers from noise-triggered bursts, whose random header
        // frequently passes the single-error-correcting FEC and would otherwise leave
        // the receiver deaf for thousands of symbols.
        if (m_state == StateData)
        {
            bool young = m_bs.length() <= 96;   // Within ~32 symbols of the sync
            double margin = young ? m_config.m_preemptYoungMargin : m_config.m_preemptOldMargin;
            // The burst's strength for the comparison is the smaller of its sync |D| and
            // the equivalent |D| of its tracked symbol amplitude: a burst triggered by a
            // gap impulse has an impulse-sized sync |D| that no genuine preamble could
            // ever beat, but its symbol amplitude collapses to noise as soon as the
            // impulse passes, and that is what lets the next real preamble through.
            double strength = m_burstAbsD;
            if (m_magMean > 0.0) {
                strength = std::min(strength, (ACARSVDL2_PREAMBLE_SYMS - 1.0) * m_magMean * m_magMean);
            }
            if ((margin <= 0.0) || (m_bestAbsD <= margin * strength)) {
                return false;
            }
        }

        // Fractional peak position, in (-0.5, 0.5) around the best sample
        double delta = 0.0;
        if (m_bestNextAbsD > 0.0)
        {
            double denom = m_bestPrevAbsD - 2.0 * m_bestAbsD + m_bestNextAbsD;
            if (denom < -1e-12) {
                delta = 0.5 * (m_bestPrevAbsD - m_bestNextAbsD) / denom;
            }
        }
        delta = std::max(-0.5, std::min(0.5, delta));

        // Per-symbol carrier advance from the common rotation of the products
        m_dphiSym = std::arg(m_bestD);

        // The first data symbol instant is one symbol after the peak, which was
        // m_bestAgo samples ago. The decision interpolates at (m - 2 + mu) when the
        // symbol clock fires at sample m.
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

        // Combine the 16 preamble symbols coherently, with their modulation and the
        // carrier offset removed relative to the last preamble symbol, into a far less
        // noisy initial reference than any single sample. Symbols whose magnitude is
        // far above the median are impulses and are excluded, exactly as in the
        // correlation - one impulse-hit symbol would otherwise dominate the sum and
        // start the burst with a rotated reference.
        const double *pr = preamblePhases();
        double refMags[ACARSVDL2_PREAMBLE_SYMS];
        for (int k = 0; k < ACARSVDL2_PREAMBLE_SYMS; k++) {
            refMags[k] = std::abs(tap(m_bestAgo + (ACARSVDL2_PREAMBLE_SYMS - 1 - k) * sps));
        }
        double refLim = 0.0;
        double refMedian = 0.0;
        {
            double refSorted[ACARSVDL2_PREAMBLE_SYMS];
            std::memcpy(refSorted, refMags, sizeof(refSorted));
            std::nth_element(refSorted, refSorted + ACARSVDL2_PREAMBLE_SYMS / 2,
                refSorted + ACARSVDL2_PREAMBLE_SYMS);
            refMedian = refSorted[ACARSVDL2_PREAMBLE_SYMS / 2];
            if (m_config.m_trimRatio > 0.0) {
                refLim = m_config.m_trimRatio * refMedian;
            }
        }
        Cd ref(0.0, 0.0);
        for (int k = 0; k < ACARSVDL2_PREAMBLE_SYMS; k++)
        {
            if ((refLim > 0.0) && (refMags[k] > refLim)) {
                continue;
            }
            int back = m_bestAgo + (ACARSVDL2_PREAMBLE_SYMS - 1 - k) * sps;
            ref += tap(back) * std::polar(1.0, -pr[k] + (ACARSVDL2_PREAMBLE_SYMS - 1 - k) * m_dphiSym);
        }
        double refMag = std::abs(ref);
        if (refMag > 1e-12) {
            m_ref = (ref / refMag) * std::polar(1.0, pr[ACARSVDL2_PREAMBLE_SYMS - 1]);
        } else {
            m_ref = std::polar(1.0, std::arg(tap(m_bestAgo)));
        }
        m_prevPhi = std::arg(tap(m_bestAgo));

        m_statPrev1 = 0.0;
        m_symbolsSinceSync = 0;
        // Seed the amplitude tracker from the median preamble magnitude - impulse-immune
        // both ways: a real burst starts with its true level (sharper reliability from
        // the first symbol), and a burst fired by a gap impulse starts at the noise
        // level, so the preemption gate lets the next genuine preamble straight through
        m_magMean = refMedian;
        m_burstAbsD = m_bestAbsD;

        m_debugThisBurst = m_debugRemaining > 0;
        if (m_debugThisBurst)
        {
            m_debugRemaining--;
            m_debugSymbol = 0;
            printf("vdl2 sync: sample=%llu absD=%.4f ago=%d delta=%.3f mu=%.3f sclk=%d dphiSym=%.5f\n",
                (unsigned long long) m_stats.m_samples, m_bestAbsD, m_bestAgo, delta, m_mu, m_sclk, m_dphiSym);
        }

        m_stats.m_syncs++;
        m_state = StateData;
        resetBurst();
        return true;
    }

    // ---------------------------------------------------------------------------------
    // Burst decode (after dumpvdl2 decode.c decode_vdl2_burst)
    // ---------------------------------------------------------------------------------

    // Returns true if the burst is still in progress (more bits wanted), false when done
    // or failed
    bool decodeBurst()
    {
        if (m_decoderState == DecoderHeader)
        {
            m_bs.descramble(m_lfsr);
            uint32_t header;
            if (!m_bs.readWordMsbFirst(header, ACARSVDL2_HEADER_LEN)) {
                return false;
            }
            // Force the reserved symbol bits to zero to improve the decode chances
            header &= (1u << (ACARSVDL2_TRLEN + ACARSVDL2_HDRFECLEN)) - 1;
            m_syndrome = decodeHeader(header);
            if (m_syndrome == 0) {
                m_stats.m_headerFecOk++;
            } else {
                m_stats.m_headerFecCorrected++;
            }
            // The corrected word must still have zero reserved bits
            if ((header & ((1u << (ACARSVDL2_TRLEN + ACARSVDL2_HDRFECLEN)) - 1)) != header)
            {
                m_stats.m_headerRejected++;
                return false;
            }
            header >>= ACARSVDL2_HDRFECLEN;
            m_datalen = reverseBits(header & ((1u << ACARSVDL2_TRLEN) - 1), ACARSVDL2_TRLEN);
            if (((m_syndrome != 0) && (m_datalen > m_config.m_maxFrameLengthCorrected))
                || (m_datalen > m_config.m_maxFrameLength))
            {
                m_stats.m_headerRejected++;
                return false;
            }
            m_datalenOctets = (m_datalen + 7) / 8;
            m_numBlocks = m_datalenOctets / ACARSVDL2_RS_K;
            m_fecOctets = m_numBlocks * (ACARSVDL2_RS_N - ACARSVDL2_RS_K);
            m_lastBlockLenOctets = m_datalenOctets % ACARSVDL2_RS_K;
            if (m_lastBlockLenOctets != 0) {
                m_numBlocks++;
            }
            m_fecOctets += fecOctetCount(m_lastBlockLenOctets);
            if (m_lastBlockLenOctets == 0) {
                m_lastBlockLenOctets = ACARSVDL2_RS_K;
            }
            if (m_fecOctets == 0)
            {
                m_stats.m_headerRejected++;
                return false;
            }
            m_requestedBits = m_bs.m_start + 8 * (m_datalenOctets + m_fecOctets);
            m_decoderState = DecoderData;
            return true;
        }

        // DecoderData
        if (m_config.m_twoPass) {
            twoPassDetect();
        }
        m_bs.descramble(m_lfsr);
        std::vector<uint8_t> data(m_datalenOctets);
        std::vector<uint8_t> fec(m_fecOctets);
        std::vector<uint8_t> dataRel(m_datalenOctets);
        std::vector<uint8_t> fecRel(m_fecOctets);
        std::vector<uint8_t> dataAlt(m_datalenOctets);
        std::vector<uint8_t> fecAlt(m_fecOctets);
        if (!m_bs.readLsbFirst(data.data(), dataRel.data(), dataAlt.data(), m_datalenOctets)
            || !m_bs.readLsbFirst(fec.data(), fecRel.data(), fecAlt.data(), m_fecOctets)) {
            return false;
        }

        // Deinterleave into one RS codeword per row, with the octet reliabilities and
        // alternatives deinterleaved through the same geometry (the padding positions
        // stay at 0, fully reliable, as they are known zeros)
        std::vector<std::array<uint8_t, ACARSVDL2_RS_N>> rsTab(m_numBlocks);
        std::vector<std::array<uint8_t, ACARSVDL2_RS_N>> relTab(m_numBlocks);
        std::vector<std::array<uint8_t, ACARSVDL2_RS_N>> altTab(m_numBlocks);
        for (auto& row : rsTab) {
            row.fill(0);
        }
        for (auto& row : relTab) {
            row.fill(0);
        }
        for (auto& row : altTab) {
            row.fill(0);
        }
        if (!deinterleave(data.data(), m_datalenOctets, m_numBlocks, rsTab, ACARSVDL2_RS_K, 0)) {
            return false;
        }
        deinterleave(dataRel.data(), m_datalenOctets, m_numBlocks, relTab, ACARSVDL2_RS_K, 0);
        deinterleave(dataAlt.data(), m_datalenOctets, m_numBlocks, altTab, ACARSVDL2_RS_K, 0);
        // A last block under 3 octets carries no FEC at all, so the last row gets no
        // parity octets from the stream
        uint32_t fecRows = m_numBlocks;
        if (fecOctetCount(m_lastBlockLenOctets) == 0) {
            fecRows--;
        }
        if (!deinterleave(fec.data(), m_fecOctets, fecRows, rsTab, ACARSVDL2_RS_N - ACARSVDL2_RS_K, ACARSVDL2_RS_K)) {
            return false;
        }
        deinterleave(fecRel.data(), m_fecOctets, fecRows, relTab, ACARSVDL2_RS_N - ACARSVDL2_RS_K, ACARSVDL2_RS_K);
        deinterleave(fecAlt.data(), m_fecOctets, fecRows, altTab, ACARSVDL2_RS_N - ACARSVDL2_RS_K, ACARSVDL2_RS_K);

        // Reed-Solomon correct each block and rebuild the corrected bit stream
        Bitstream frameBits;
        for (uint32_t r = 0; r < m_numBlocks; r++)
        {
            int numFecOctets = ACARSVDL2_RS_N - ACARSVDL2_RS_K;
            if (r == m_numBlocks - 1) {
                numFecOctets = fecOctetCount(m_lastBlockLenOctets);
            }
            int corrections = rsVerify(rsTab[r].data(), relTab[r].data(), altTab[r].data(), numFecOctets);
            if (corrections < 0)
            {
                m_stats.m_rsBlocksFailed++;
                return false;
            }
            m_stats.m_rsBlocksOk++;
            m_stats.m_rsOctetsCorrected += std::max(0, corrections - (ACARSVDL2_RS_N - ACARSVDL2_RS_K - numFecOctets));
            uint32_t octets = (r != m_numBlocks - 1) ? ACARSVDL2_RS_K : m_lastBlockLenOctets;
            for (uint32_t i = 0; i < octets; i++)
            {
                for (int j = 0; j < 8; j++) {
                    frameBits.m_bits.push_back((rsTab[r][i] >> j) & 1);
                }
            }
        }

        // The transmission length is in bits; the octet padding is not part of the stream
        if (m_datalen < frameBits.m_bits.size()) {
            frameBits.m_bits.resize(m_datalen);
        }

        // Split into AVLC frames: flags and zero-bit unstuffing
        std::vector<uint8_t> frameBytes;
        int ret;
        while ((ret = nextUnstuffedFrame(frameBits, frameBytes)) >= 0)
        {
            if (!frameBytes.empty()) {
                parseAvlcFrame(frameBytes);
            }
            if (ret == 0) {
                break;
            }
        }
        if (ret < 0) {
            m_stats.m_bitstuffErrors++;
        }
        return false; // Burst complete
    }

    // Second detection pass over the whole burst, once all its symbols are in. The first
    // pass's decisions remove the modulation from the received samples, leaving the
    // carrier; smoothing that over +-m_smoothSpan symbols (self-excluded) gives each
    // symbol a nearly noiseless two-sided phase reference to be re-decided against. A
    // first-pass decision error rotates the cumulative phase and the carrier estimate
    // equally, so it cancels in the re-decision away from the error itself. Only bits
    // after the header (already parsed and descrambled) are rewritten.
    void twoPassDetect()
    {
        const int n = (int) m_syms.size();
        const int span = m_config.m_smoothSpan;
        if (n < 2 * span) {
            return;
        }

        // Cumulative decided phase, and the modulation-removed carrier
        std::vector<double> psi(n);
        std::vector<Cd> carrier(n);
        double acc = 0.0;
        double magSum = 0.0;
        for (int i = 0; i < n; i++)
        {
            acc += m_syms[i].m_idx * M_PI_4 + m_syms[i].m_dphiSym;
            psi[i] = acc;
            carrier[i] = m_syms[i].m_s * std::polar(1.0, -psi[i]);
            magSum += std::abs(m_syms[i].m_s);
        }
        double magMean = magSum / n;

        // Sliding sum over +-span
        std::vector<Cd> prefix(n + 1, Cd(0.0, 0.0));
        for (int i = 0; i < n; i++) {
            prefix[i+1] = prefix[i] + carrier[i];
        }

        static const uint8_t graycode[8] = {0, 1, 3, 2, 6, 7, 5, 4};

        for (int i = 1; i < n; i++)
        {
            int lo = std::max(0, i - span);
            int hi = std::min(n - 1, i + span);
            Cd ref = prefix[hi+1] - prefix[lo] - carrier[i];
            if (std::norm(ref) < 1e-20) {
                continue;
            }

            double dphi = std::arg(m_syms[i].m_s * std::conj(ref)) - psi[i-1] - m_syms[i].m_dphiSym;
            while (dphi < 0.0) {
                dphi += 2.0 * M_PI;
            }
            while (dphi >= 2.0 * M_PI) {
                dphi -= 2.0 * M_PI;
            }
            int idx = (int) std::lround(dphi / M_PI_4);
            double residual = dphi - idx * M_PI_4;
            idx &= 7;

            if (idx != m_syms[i].m_idx) {
                m_stats.m_twoPassChanged++;
            }

            double relResidual = std::abs(residual) * (255.0 / (M_PI / 8.0));
            double relAmplitude = 0.0;
            if (magMean > 1e-12) {
                relAmplitude = std::abs(std::abs(m_syms[i].m_s) - magMean) * (255.0 / magMean);
            }
            uint8_t rel = (uint8_t) std::min(255.0, std::max(relResidual, relAmplitude));
            int altIdx = (idx + (residual >= 0.0 ? 1 : 7)) & 7;

            // Rewrite this symbol's bits, but only past the header, which has been
            // parsed and descrambled already
            for (int b = 0; b < 3; b++)
            {
                uint32_t pos = 3 * i + b;
                if ((pos < ACARSVDL2_HEADER_LEN) || (pos >= m_bs.m_bits.size())) {
                    continue;
                }
                m_bs.m_bits[pos] = (graycode[idx] >> (2 - b)) & 1;
                m_bs.m_alt[pos] = (graycode[altIdx] >> (2 - b)) & 1;
                m_bs.m_rel[pos] = rel;
            }
        }
    }

    static uint32_t fecOctetCount(uint32_t lastBlockLenOctets)
    {
        if (lastBlockLenOctets < 3) {
            return 0;
        } else if (lastBlockLenOctets < 31) {
            return 2;
        } else if (lastBlockLenOctets < 68) {
            return 4;
        }
        return 6;
    }

    static const uint32_t *headerParityMatrix()
    {
        static const uint32_t H[ACARSVDL2_HDRFECLEN] = {
            0b0000000011111111111110000,
            0b0011111100001111111101000,
            0b1100011100110000111100100,
            0b1101101101010011001100010,
            0b0110100111100101010100001
        };
        return H;
    }

    static uint32_t decodeHeader(uint32_t& word)
    {
        static const uint32_t syndtable[1 << ACARSVDL2_HDRFECLEN] = {
            0b0000000000000000000000000,
            0b0000000000000000000000001,
            0b0000000000000000000000010,
            0b0100000000000000000000100,
            0b0000000000000000000000100,
            0b0100000000000000000000010,
            0b1000000000000000000000000,
            0b0100000000000000000000000,
            0b0000000000000000000001000,
            0b0010000000000000000000000,
            0b0001000000000000000000000,
            0b0000100000000000000000000,
            0b0000010000000000000000000,
            0b1000100000000000000000000,
            0b0000001000000000000000000,
            0b0000000100000000000000000,
            0b0000000000000000000010000,
            0b0000000010000000000000000,
            0b0100000000100000000000000,
            0b0000000001000000000000000,
            0b0100000001000000000000000,
            0b0000000000100000000000000,
            0b0000000000010000000000000,
            0b1000000010000000000000000,
            0b0000000000001000000000000,
            0b0000000000000100000000000,
            0b0000000000000010000000000,
            0b0000000000000001000000000,
            0b0000000000000000100000000,
            0b0000000000000000010000000,
            0b0000000000000000001000000,
            0b0000000000000000000100000,
        };
        uint32_t syndrome = 0;
        for (int i = 0; i < ACARSVDL2_HDRFECLEN; i++) {
            syndrome |= parity32(word & headerParityMatrix()[i]) << (ACARSVDL2_HDRFECLEN - 1 - i);
        }
        word ^= syndtable[syndrome];
        return syndrome;
    }

    static uint32_t parity32(uint32_t v)
    {
        uint32_t p = 0;
        while (v)
        {
            p = !p;
            v = v & (v - 1);
        }
        return p;
    }

    static uint32_t reverseBits(uint32_t v, int numBits)
    {
        uint32_t r = 0;
        for (int i = 0; i < numBits; i++)
        {
            r = (r << 1) | (v & 1);
            v >>= 1;
        }
        return r;
    }

    static uint8_t lfsrNext(uint16_t& lfsr)
    {
        // 15 bit LFSR, feedback polynomial x^15 + x + 1
        uint8_t bit = ((lfsr >> 0) ^ (lfsr >> 14)) & 1;
        lfsr = (uint16_t) ((lfsr >> 1) | (bit << 14));
        return bit;
    }

    // Deinterleave: input octets were transmitted column by column across the RS blocks
    // (rows), with the short last row zero padded in the data columns and truncated in
    // the parity columns
    static bool deinterleave(const uint8_t *in, uint32_t len, uint32_t rows,
                             std::vector<std::array<uint8_t, ACARSVDL2_RS_N>>& out,
                             uint32_t fillWidth, uint32_t offset)
    {
        if ((rows == 0) || (fillWidth == 0)) {
            return false;
        }
        uint32_t lastRowLen = len % fillWidth;
        if (lastRowLen == 0) {
            lastRowLen = fillWidth;
        }
        if (fillWidth + offset > ACARSVDL2_RS_N) {
            return false;
        }
        if (len > rows * fillWidth) {
            return false;
        }
        if ((rows > 1) && (len - lastRowLen < (rows - 1) * fillWidth)) {
            return false;
        }
        uint32_t row = 0, col = offset;
        lastRowLen += offset;
        for (uint32_t i = 0; i < len; i++)
        {
            if ((row == rows - 1) && (col >= lastRowLen))
            {
                out[row][col] = 0x00;
                row = 0;
                col++;
            }
            out[row++][col] = in[i];
            if (row == rows)
            {
                row = 0;
                col++;
            }
        }
        return true;
    }

    // Reed-Solomon check/correct one 255 octet block. Parity octets that were not
    // transmitted (short last block) are marked as erasures. When a block fails and
    // erasure decoding is enabled, retry with the least reliable transmitted octets
    // erased (2 x errors + erasures <= 6, so 6 erasures beat 3 errors when the
    // reliability ranking finds the damage). Returns the correction count, or negative
    // on failure.
    int rsVerify(uint8_t *block, const uint8_t *rel, const uint8_t *alt, int numFecOctets)
    {
        if (numFecOctets == 0) {
            return 0;
        }
        const int nroots = ACARSVDL2_RS_N - ACARSVDL2_RS_K;
        int missingParity = nroots - numFecOctets;
        int erasPos[ACARSVDL2_RS_N - ACARSVDL2_RS_K];
        for (int i = 0; i < missingParity; i++) {
            erasPos[i] = ACARSVDL2_RS_K + numFecOctets + i;
        }

        uint8_t pristine[ACARSVDL2_RS_N];
        std::memcpy(pristine, block, ACARSVDL2_RS_N);

        int ret;
        if (missingParity > 0) {
            ret = m_rs.decode(block, ACARSVDL2_RS_K, block + ACARSVDL2_RS_K, erasPos, missingParity);
        } else {
            ret = m_rs.decode(block, ACARSVDL2_RS_K, block + ACARSVDL2_RS_K);
        }
        if ((ret >= 0) || !m_config.m_rsErasures) {
            return ret;
        }

        // Rank the transmitted positions, least reliable first
        const int txCount = ACARSVDL2_RS_K + numFecOctets;
        std::vector<int> order(txCount);
        for (int i = 0; i < txCount; i++) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(),
                         [rel](int a, int b) { return rel[a] > rel[b]; });

        // Chase list decoding first: flip subsets of the least reliable octets that have
        // a distinct second-best value to that alternative. Unlike erasures the flips
        // consume no parity budget, so the full correction capability still has to agree
        // with the result - a much stronger validity check than the all-erasures decode
        // below, which always yields a consistent codeword.
        if (m_config.m_chaseOctets > 0)
        {
            std::vector<int> candidates;
            for (int i : order)
            {
                if ((alt[i] != pristine[i]) && (rel[i] > 0))
                {
                    candidates.push_back(i);
                    if ((int) candidates.size() >= m_config.m_chaseOctets) {
                        break;
                    }
                }
            }

            for (uint32_t mask = 1; mask < (1u << candidates.size()); mask++)
            {
                uint8_t attempt[ACARSVDL2_RS_N];
                std::memcpy(attempt, pristine, ACARSVDL2_RS_N);
                for (size_t i = 0; i < candidates.size(); i++)
                {
                    if (mask & (1u << i)) {
                        attempt[candidates[i]] = alt[candidates[i]];
                    }
                }
                if (missingParity > 0) {
                    ret = m_rs.decode(attempt, ACARSVDL2_RS_K, attempt + ACARSVDL2_RS_K, erasPos, missingParity);
                } else {
                    ret = m_rs.decode(attempt, ACARSVDL2_RS_K, attempt + ACARSVDL2_RS_K);
                }
                if (ret >= 0)
                {
                    std::memcpy(block, attempt, ACARSVDL2_RS_N);
                    m_stats.m_rsChaseRecovered++;
                    return ret;
                }
            }
        }

        for (int extra = 2; extra + missingParity <= nroots; extra += 2)
        {
            uint8_t attempt[ACARSVDL2_RS_N];
            std::memcpy(attempt, pristine, ACARSVDL2_RS_N);
            for (int i = 0; i < extra; i++) {
                erasPos[missingParity + i] = order[i];
            }
            ret = m_rs.decode(attempt, ACARSVDL2_RS_K, attempt + ACARSVDL2_RS_K,
                              erasPos, missingParity + extra);
            if (ret >= 0)
            {
                std::memcpy(block, attempt, ACARSVDL2_RS_N);
                m_stats.m_rsErasureRecovered++;
                return ret;
            }
        }

        std::memcpy(block, pristine, ACARSVDL2_RS_N);
        return -1;
    }

    // Copy the next 0x7E delimited frame out of the bit stream, removing stuffed zero
    // bits. Returns 1 if more frames may follow, 0 at the end of the stream, negative on
    // an invalid bit sequence. (After dumpvdl2 bitstream_copy_next_frame.)
    static int nextUnstuffedFrame(Bitstream& src, std::vector<uint8_t>& frameBytes)
    {
        frameBytes.clear();
        std::vector<uint8_t> bits;
    restart:
        int ones = 0;
        bits.clear();
        for (uint32_t i = src.m_start; i < src.m_bits.size(); i++, src.m_start++)
        {
            if ((src.m_bits[i] == 0) && (ones == 5))
            {
                ones = 0; // Stuffed zero, skip it
                continue;
            }
            else if (src.m_bits[i] == 1)
            {
                ones++;
                if (ones > 6) {
                    return -1; // Seven ones is invalid
                }
            }
            bits.push_back(src.m_bits[i]);
            if (src.m_bits[i] == 0)
            {
                if (ones == 6)
                {
                    // Frame boundary flag 0x7E
                    if (bits.size() == 8)
                    {
                        src.m_start++;
                        goto restart; // Initial flag
                    }
                    else if (bits.size() < 8)
                    {
                        return -1;
                    }
                    bits.resize(bits.size() - 8); // Remove the trailing flag
                    src.m_start++;
                    break;
                }
                ones = 0;
            }
        }
        if (bits.size() % 8) {
            return -1;
        }
        frameBytes.resize(bits.size() / 8);
        for (size_t i = 0; i < frameBytes.size(); i++)
        {
            uint8_t byte = 0;
            for (int j = 0; j < 8; j++) {
                byte |= bits[i*8+j] << j;
            }
            frameBytes[i] = byte;
        }
        return (src.m_start < src.m_bits.size()) ? 1 : 0;
    }

    // ---------------------------------------------------------------------------------
    // AVLC frame parsing (after dumpvdl2 avlc.c)
    // ---------------------------------------------------------------------------------

    void parseAvlcFrame(const std::vector<uint8_t>& bytes)
    {
        const int MIN_AVLC_LEN = 11; // 2 addresses, control, FCS
        if ((int) bytes.size() < MIN_AVLC_LEN)
        {
            m_stats.m_shortFrames++;
            return;
        }

        crc16x25 crc;
        crc.init();
        crc.calculate(bytes.data(), (int) bytes.size() - 2);
        uint16_t fcs = (uint16_t) crc.get();
        uint16_t rxFcs = bytes[bytes.size() - 2] | (bytes[bytes.size() - 1] << 8);
        if (fcs != rxFcs)
        {
            m_stats.m_fcsInvalid++;
            return;
        }
        m_stats.m_fcsValid++;

        Frame frame;
        frame.m_bytes = bytes;

        uint32_t dst = parseDlcAddress(bytes.data());
        uint32_t src = parseDlcAddress(bytes.data() + 4);
        frame.m_dstAddress = dst & 0xffffff;
        frame.m_dstType = (dst >> 24) & 0x7;
        frame.m_dstStatus = (dst >> 27) & 0x1;
        frame.m_srcAddress = src & 0xffffff;
        frame.m_srcType = (src >> 24) & 0x7;
        frame.m_srcStatus = (src >> 27) & 0x1;
        frame.m_fromAircraft = frame.m_srcType == 1;

        uint8_t lcf = bytes[8];
        frame.m_infoOffset = 9;
        frame.m_infoLength = (int) bytes.size() - 9 - 2;
        frame.m_isAcars = false;

        if ((lcf & 0x1) == 0)
        {
            // Information frame
            std::strcpy(frame.m_frameType, "I");
            const uint8_t *info = bytes.data() + frame.m_infoOffset;
            frame.m_isAcars = (frame.m_infoLength > 3) && (info[0] == 0xff) && (info[1] == 0xff) && (info[2] == 0x01);
        }
        else if ((lcf & 0x3) == 0x1)
        {
            // Supervisory frame
            static const char *sCmd[4] = {"RR", "RNR", "REJ", "SREJ"};
            std::strcpy(frame.m_frameType, sCmd[(lcf >> 2) & 0x3]);
        }
        else
        {
            // Unnumbered frame
            uint8_t mfunc = ((lcf >> 2) & 0x3f) & 0x3b;
            switch (mfunc)
            {
            case 0x00: std::strcpy(frame.m_frameType, "UI"); break;
            case 0x03: std::strcpy(frame.m_frameType, "DM"); break;
            case 0x10: std::strcpy(frame.m_frameType, "DISC"); break;
            case 0x18: std::strcpy(frame.m_frameType, "UA"); break;
            case 0x21: std::strcpy(frame.m_frameType, "FRMR"); break;
            case 0x2b: std::strcpy(frame.m_frameType, "XID"); break;
            case 0x38: std::strcpy(frame.m_frameType, "TEST"); break;
            default: std::strcpy(frame.m_frameType, "U"); break;
            }
        }

        if (frame.m_isAcars) {
            m_stats.m_acarsFrames++;
        } else {
            m_stats.m_otherFrames++;
        }
        m_frames.push_back(frame);
    }

    // The 28 significant bits of the two 4 octet DLS address fields: bits 1-7 of each
    // octet, LSB (bit 0) of each octet is the HDLC address extension bit. Bit-reversed so
    // the result reads address(24) | type(3) << 24 | status(1) << 27.
    static uint32_t parseDlcAddress(const uint8_t *buf)
    {
        uint32_t v = (buf[0] >> 1) | (buf[1] << 6) | (buf[2] << 13) | ((uint32_t) (buf[3] & 0xfe) << 20);
        return reverseBits(v, 28);
    }

    // Inverse of parseDlcAddress: bits 1-7 of each octet carry 7 address bits, bit 0 is
    // the HDLC extension bit, set only on the final octet of the second address field
    static void appendDlcAddress(std::vector<uint8_t>& frame, uint32_t address, int type, bool status, bool last)
    {
        uint32_t v = reverseBits((address & 0xffffff) | ((uint32_t) (type & 0x7) << 24) | ((uint32_t) status << 27), 28);
        frame.push_back((uint8_t) ((v & 0x7f) << 1));
        frame.push_back((uint8_t) (((v >> 7) & 0x7f) << 1));
        frame.push_back((uint8_t) (((v >> 14) & 0x7f) << 1));
        frame.push_back((uint8_t) ((((v >> 21) & 0x7f) << 1) | (last ? 1 : 0)));
    }

    static void appendByteLsbFirst(std::vector<uint8_t>& bits, uint8_t byte)
    {
        for (int j = 0; j < 8; j++) {
            bits.push_back((byte >> j) & 1);
        }
    }
};

#endif // INCLUDE_ACARSVDL2_H
