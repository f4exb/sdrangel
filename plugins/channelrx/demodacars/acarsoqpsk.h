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

#ifndef INCLUDE_ACARSOQPSK_H
#define INCLUDE_ACARSOQPSK_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "util/crc.h"

// Coherent OQPSK receiver for VHF ACARS.
//
// ACARS audio is MSK about an 1800 Hz subcarrier with 600 Hz deviation at 2400 baud, so
// deviation = 1/(4T): textbook MSK, which has an exact OQPSK representation. Three facts
// from ARINC 618 section 4.4.2 shape the whole receiver:
//
// 1. "The phase is further defined such that the amplitude of each tone is zero at the bit
//    transition. The slope of the waveforms at the end of a bit cell should be positive for
//    a binary one and negative for a binary zero." Over one bit the 2400 Hz tone advances
//    exactly 2 pi and the 1200 Hz tone exactly pi, so the audio crosses zero at every bit
//    boundary. The pre-key is a constant 2400 Hz tone whose rising zero crossings are
//    therefore bit boundaries, and its phase gives symbol timing outright - no search, no
//    half symbol ambiguity.
// 2. ACARS's differential encoding, "1200 Hz indicates a bit change", is exactly MSK's
//    precoding, so the OQPSK arm decisions are the data bits. There is no differential
//    detection penalty, which is where most of the gap to Q(sqrt(2 Eb/N0)) would otherwise
//    go.
// 3. Derotating the matched filter output by 90 degrees per symbol turns MSK into plain
//    BPSK: w_n = beta_n j^n exp(j phi), so u_n = w_n (-j)^n carries the bit directly and
//    everything above it is standard BPSK machinery.
//
// The chain is: blank impulse interference, detect the pre-key on the envelope with a level
// independent statistic, estimate the carrier offset from the mean sample to sample
// product, recover the audio by coherent AM detection (which also supplies the DC reference
// and the AGC), translate the 1800 Hz subcarrier to baseband while matching to the MSK half
// sine pulse, then track timing and phase through the block and slice. Blocks that fail the
// BCS are repaired using the per character parity to locate the damaged byte and the soft
// outputs to rank the bits.
//

#define ACARSOQPSK_CHANNEL_SAMPLE_RATE 48000
#define ACARSOQPSK_BAUD_RATE           2400
#define ACARSOQPSK_SAMPLES_PER_SYMBOL  (ACARSOQPSK_CHANNEL_SAMPLE_RATE/ACARSOQPSK_BAUD_RATE)
#define ACARSOQPSK_SUBCARRIER          1800.0

// SOH mode address(7) ack label(2) block-id STX text(220) ETX BCS(2) DEL and slack
#define ACARSOQPSK_MAX_BYTES  (16+2+2+1+1+7+1+2+1+1+2+220+1+2+1)

// Shortest legal block: SOH mode addr(7) ack label(2) block-id ETX BCS(2) DEL.
// ARINC 618 Appendix B puts ETX directly after the block id when there is no text; STX is
// present only when message text follows.
#define ACARSOQPSK_MIN_BYTES  17

// Pre-key detector window: 16 bits, and a whole number of 2400 Hz cycles so the sliding DFT
// update is exact. ARINC 618 section 4.4.6 only guarantees 27 bits of settled pre-key, so
// this has to stay short.
#define ACARSOQPSK_DETECT_WINDOW (16*ACARSOQPSK_SAMPLES_PER_SYMBOL)

// Longest transmission: the 190 ms maximum pre-key of ARINC 618 section 4.2.1, the four
// synchronisation characters and a maximum length block
#define ACARSOQPSK_MAX_BURST ((int)(0.190f*ACARSOQPSK_CHANNEL_SAMPLE_RATE) \
                             + (32 + ACARSOQPSK_MAX_BYTES*8)*ACARSOQPSK_SAMPLES_PER_SYMBOL)

// Blanker delay line. The lookahead has to exceed the longest run the duration classifier
// will accept plus the guard, or a sample is emitted before it has been classified. It is
// also the latency the blanker adds to the stream, 3 ms.
// Hard ceiling on the experimental 3/4-bit soft list. C(n,4) is the binding term and the
// correction budget trims below this again; this is only here so a nonsense config value
// cannot reach the combination loop at all.
#define ACARSOQPSK_MAX_LIST_BITS 32

// ... and on the combinations it may enumerate. C(24,3) + C(24,4) is 12650, so the
// erasure-widened list fits; C(32,4) alone would be 36k and does not.
#define ACARSOQPSK_MAX_LIST_COMBINATIONS 16384

#define ACARSOQPSK_NB_LOOKAHEAD 144
#define ACARSOQPSK_NB_LINE      512

class AcarsOqpskReceiver
{
    struct SyncCandidate
    {
        double m_score;
        int m_position;
        double m_sign;
    };

    struct RetryCandidate
    {
        int64_t m_startAbs;
        int64_t m_retryAtAbs;
        double m_frequency;
        int64_t m_sohAbs;
    };

    struct RecentFrame
    {
        int64_t m_sohAbs;
        int64_t m_endAbs;
    };

public:
    typedef std::complex<double> Cd;

    // What to put in a blanked sample's place
    enum FillMode
    {
        FillScale = 0,      // Scale the impulse down to the limit
        FillHold,           // The last sample that was not interference
        FillInterpolate,    // A straight line across the gap
        FillCarrier         // The estimated carrier, so no audio is injected. Best measured.
    };

    enum CorrectionReject
    {
        CorrectionValid = 0,
        CorrectionBadLength,
        CorrectionBadFraming,
        CorrectionBadParity
    };

    enum SliceFailure
    {
        SliceComplete = 0,
        SliceNoSoh,
        SliceBufferEnd,
        SliceMaxBytes
    };

    enum SemanticStrength
    {
        SemanticInvalid = 0,
        SemanticWeak,
        SemanticStrong
    };

    enum SemanticField
    {
        SemanticFieldNone = 0,
        SemanticFieldAddress,
        SemanticFieldBlockId,
        SemanticFieldLabel
    };

    struct Config
    {
        // Pre-key detection. The statistic is the depth of 2400 Hz modulation on the
        // envelope, so it is a ratio: it does not move with signal level, which is what lets
        // one threshold work across the 90 dB input range of ARINC 618 section 4.4.7.1.
        double m_detectThreshold = 0.25;
        double m_syncThreshold   = 0.6;     // Normalised soft correlation against + * SYN SYN

        // Impulse blanker. The threshold is a multiple of the running mean envelope: theory
        // says 85 percent AM peaks at 1.85 and 100 percent at 2.0, but real recordings want
        // more headroom - at 3 it costs messages, at 4 it costs nothing anywhere.
        double m_blankThreshold  = 4.0;     // 0 disables the blanker entirely
        FillMode m_fillMode      = FillCarrier;
        int m_blankGuard         = 2;       // Samples blanked either side of a detection
        int m_maxSpike           = 64;      // Longest run still called interference
        int m_minSpike           = 1;       // ... and the shortest
        double m_blankTimeConst  = 0.002;   // Level estimate, seconds

        // Symbol timing and phase
        int m_timingSearch       = 2;       // Offsets tried either side of the predicted one
        double m_timingGain      = 0.004;   // Gardner loop, 0 disables tracking
        double m_phaseGain       = 0.02;    // Decision directed phase loop, 0 disables
        bool m_lsAcquisition     = false;   // Fit residual phase rate over the known pre-key
        bool m_softPhase         = false;   // Reliability-weight the phase detector
        bool m_syncReacquire     = false;   // Re-anchor acquisition on pre-key before sync
        bool m_blockRefine       = false;   // Non-causal phase/timing fit after frame failure
        bool m_parallelCapture   = true;    // Detect a strong pre-key during normal holdoff

        // Error correction
        int m_correctBudget      = 256;     // 0 disables; budget for bounded fallback search
        int m_chaseBits          = 12;      // Weakest bits drawn on beyond the parity flagged
        bool m_requirePlausible  = true;    // Structural check on the ARINC 618 header
        bool m_semanticCorrection = true;   // Stricter validation only after changing bits
        bool m_syndromeCorrection = true;   // Exhaustive parity-constrained 1/2-bit repair
        bool m_deferCorrection   = true;    // Try every timing offset uncorrected first
        bool m_repairSoh         = true;    // Repair one-bit SOH after strong rank-0 sync
        bool m_repairTerminator  = false;   // Search a one-bit damaged ETX/ETB position
        bool m_deferredRetry     = true;    // Retry rank-0 framing after buffering its tail
        int m_listMaxFlips       = 4;       // Soft CRC list order, 2 to 4
        int m_listBits           = 16;      // Weak/parity bits eligible for 3/4-bit lists
        int m_listBitsErased     = 24;      // ... and when the blanker fired in this burst
        double m_carrierRefineHz = 15.0;    // Re-extract if the carrier is still turning
                                            // faster than this. 0 disables
        double m_listMargin      = 0.5;     // Per-bit reliability ceiling for 3/4-bit
                                            // repairs, relative to the block mean. 0 = off
        bool m_impulseReconstruct = false;  // Decision-aided blanked-audio second pass
    };

    struct Stats
    {
        uint64_t m_samples = 0;
        uint64_t m_blanked = 0;             // Samples the blanker replaced
        uint64_t m_triggers = 0;            // Pre-keys detected
        uint64_t m_syncFound = 0;
        uint64_t m_syncMissed = 0;
        uint64_t m_crcValid = 0;
        uint64_t m_crcInvalid = 0;
        uint64_t m_implausible = 0;         // BCS passed, header did not
        uint64_t m_abortNoSoh = 0;
        uint64_t m_abortMaxBytes = 0;
        uint64_t m_abortBufferEnd = 0;
        uint64_t m_parityBytes = 0;
        uint64_t m_timingTries = 0;
        uint64_t m_correctionTries = 0;
        uint64_t m_correctionBcsMatches = 0;
        uint64_t m_correctionCodewordRejected = 0;
        uint64_t m_correctionSemanticRejected = 0;
        uint64_t m_correctionWeakAccepted = 0;
        uint64_t m_correctionAmbiguous = 0;
        uint64_t m_semanticAddressRejected = 0;
        uint64_t m_semanticBlockRejected = 0;
        uint64_t m_semanticLabelRejected = 0;
        uint64_t m_corrected1 = 0;          // Blocks recovered by flipping one bit
        uint64_t m_corrected2 = 0;          // ... and two
        uint64_t m_correctionSecondPass = 0;// Bursts that needed the corrected framing pass
        uint64_t m_correctionPreempted = 0; // Upper bound: a repair was attemptable and a
                                            // later offset then decoded outright
        uint64_t m_corrected3 = 0;          // ... and guarded higher-order list repair
        uint64_t m_corrected4 = 0;
        double m_freqEstSum = 0.0;
        double m_freqEstPeak = 0.0;
        uint64_t m_freqEstCount = 0;
        double m_acquiredPhaseRateSum = 0.0;
        uint64_t m_acquiredPhaseRateCount = 0;
        uint64_t m_syncReacquired = 0;
        uint64_t m_sohRepaired = 0;
        uint64_t m_terminatorRepaired = 0;
        uint64_t m_deferredRetries = 0;
        uint64_t m_deferredRecovered = 0;
        uint64_t m_deferredQueued = 0;
        uint64_t m_deferredDropped = 0;
        uint64_t m_duplicateSyncRejected = 0;
        uint64_t m_refineTries = 0;
        uint64_t m_refineRecovered = 0;
        uint64_t m_list3Tried = 0;
        uint64_t m_list4Tried = 0;
        uint64_t m_list3Recovered = 0;
        uint64_t m_list4Recovered = 0;
        uint64_t m_listAmbiguous = 0;      // Higher-order results dropped for not being unique
        uint64_t m_carrierRefined = 0;    // Bursts re-extracted from a corrected carrier
        uint64_t m_listMarginRejected = 0; // ... and combinations refused for flipping
                                          // bits the demodulator was confident about
        uint64_t m_listTrimmed = 0;        // List length reductions to fit the budget
        uint64_t m_parallelTriggers = 0;
        uint64_t m_impulseReconstructTries = 0;
        uint64_t m_impulseReconstructRecovered = 0;
    };

    struct FrameDiagnostic
    {
        const uint8_t *m_bytes;
        int m_byteCount;
        int m_firstBit;
        bool m_accepted;
        int64_t m_burstStartSample;
        double m_sohSample;
        double m_detectRatio;
        double m_carrierFrequency;
        double m_toneOffset;
        double m_syncScore;
        int m_syncRank;
        int m_syncCandidates;
        int m_timingTry;
        double m_initialTau;
        double m_acquiredPhaseRate;
        double m_finalPhaseRate;
        double m_peakPhaseError;
        double m_rmsPhaseError;
        double m_finalPeriod;
        double m_minPeriod;
        double m_maxPeriod;
        double m_peakTimingError;
        double m_rmsTimingError;
        double m_peakTimingWander;
        double m_signalAmplitude;
        double m_noiseVariance;
        SliceFailure m_sliceFailure;
        int m_partialByteCount;
        uint8_t m_firstByte;
        int m_symbolsAvailableAfterSoh;
    };

    typedef void (*FrameDiagnosticCallback)(void *context, const FrameDiagnostic& diagnostic);

    // Mandatory invariants for a candidate produced by changing received bits. These are
    // separate from plausible(): an unmodified block that passes its BCS keeps the historic
    // acceptance path, while a repaired block must still be a valid character codeword.
    static CorrectionReject correctionCodeword(const uint8_t *bytes, int byteCount)
    {
        if (!bytes || (byteCount < ACARSOQPSK_MIN_BYTES)
                   || (byteCount > ACARSOQPSK_MAX_BYTES)) {
            return CorrectionBadLength;
        }

        const int terminator = byteCount - 4;

        if (((bytes[0] & 0x7f) != 0x01)
            || ((bytes[byteCount-1] & 0x7f) != 0x7f)) {
            return CorrectionBadFraming;
        }

        if (byteCount == ACARSOQPSK_MIN_BYTES)
        {
            if ((bytes[13] & 0x7f) != 0x03) {
                return CorrectionBadFraming;
            }
        }
        else
        {
            if (((bytes[13] & 0x7f) != 0x02)
                || (((bytes[terminator] & 0x7f) != 0x03)
                    && ((bytes[terminator] & 0x7f) != 0x17))) {
                return CorrectionBadFraming;
            }

            // sliceBlock() stopped at the first end marker. A correction must not introduce
            // an earlier one while retaining the old length.
            for (int i = 14; i < terminator; i++)
            {
                const int c = bytes[i] & 0x7f;
                if ((c == 0x03) || (c == 0x17)) {
                    return CorrectionBadFraming;
                }
            }
        }

        // SOH through ETX/ETB carry odd character parity. The BCS bytes and final DEL do
        // not. Rechecking after the proposed flips prevents a BCS collision from accepting
        // a repair that has made another character invalid.
        for (int i = 0; i <= terminator; i++)
        {
            if (!oddParity(bytes[i])) {
                return CorrectionBadParity;
            }
        }

        return CorrectionValid;
    }

    // Conservative semantics for corrected candidates. Invalid is reserved for field
    // values that cannot be an ACARS header. Weak covers unusual but possible proprietary
    // traffic, so it remains eligible when no strong BCS match exists.
    static SemanticStrength correctionSemantics(const uint8_t *bytes, int byteCount,
                                                SemanticField *badField = nullptr)
    {
        if (badField) {
            *badField = SemanticFieldNone;
        }

        if (!bytes || (byteCount < ACARSOQPSK_MIN_BYTES)) {
            return SemanticInvalid;
        }

        SemanticStrength strength = SemanticStrong;
        bool addressStarted = false;
        bool addressContent = false;

        for (int i = 2; i <= 8; i++)
        {
            const int c = bytes[i] & 0x7f;

            if ((c == '.') && !addressStarted) {
                continue;                       // ARINC leading registration padding
            }

            addressStarted = true;

            if (((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '-')) {
                addressContent = true;
            } else if (c == ' ') {
                strength = SemanticWeak;        // Seen in some flight-number addressing
            } else {
                if (badField) {
                    *badField = SemanticFieldAddress;
                }
                return SemanticInvalid;
            }
        }

        if (!addressContent)
        {
            if (badField) {
                *badField = SemanticFieldAddress;
            }
            return SemanticInvalid;
        }

        const int blockId = bytes[12] & 0x7f;
        const bool downlink = (blockId >= '0') && (blockId <= '9');
        const bool uplink = ((blockId >= 'A') && (blockId <= 'Z')) || (blockId == 0x00);

        if (!downlink && !uplink)
        {
            if (badField) {
                *badField = SemanticFieldBlockId;
            }
            return SemanticInvalid;
        }

        const int label0 = bytes[10] & 0x7f;
        const int label1 = bytes[11] & 0x7f;
        const auto normalLabel = [](int c) {
            return ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
        };
        const bool noInformation = (label0 == '_') && (label1 == 0x7f);
        const bool specialLabel = ((label0 == ':') && (label1 == ';')) || noInformation;

        if (!specialLabel && (!normalLabel(label0) || !normalLabel(label1)))
        {
            // Printable punctuation is retained as weak for proprietary labels. A control
            // character other than the defined DEL form cannot be a label.
            if ((label0 < 0x20) || (label0 >= 0x7f)
                || (label1 < 0x20) || (label1 >= 0x7f))
            {
                if (badField) {
                    *badField = SemanticFieldLabel;
                }
                return SemanticInvalid;
            }

            strength = SemanticWeak;
        }

        const int ack = bytes[9] & 0x7f;
        if (!(((ack >= 'A') && (ack <= 'Z')) || ((ack >= '0') && (ack <= '9'))
              || (ack == 0x7f) || (ack == 0x15) || (ack == 0x06))) {
            strength = SemanticWeak;
        }

        if (byteCount == ACARSOQPSK_MIN_BYTES) {
            // ARINC 618 makes STX optional; a no-text block is not restricted to the
            // no-information label. Apply the same label semantics as any other block.
            return strength;
        }

        // Downlink text normally begins with originator, two-digit message number, block
        // sequence and six-character flight. Treat deviations as weak rather than invalid:
        // airline-specific payloads are common and must not become false negatives.
        if (downlink)
        {
            const int textLength = byteCount - 18;
            if (textLength < 10) {
                strength = SemanticWeak;
            } else {
                const auto alphaNum = [](int c) {
                    return ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
                };

                if (!alphaNum(bytes[14] & 0x7f)
                    || ((bytes[15] & 0x7f) < '0') || ((bytes[15] & 0x7f) > '9')
                    || ((bytes[16] & 0x7f) < '0') || ((bytes[16] & 0x7f) > '9')
                    || !alphaNum(bytes[17] & 0x7f)) {
                    strength = SemanticWeak;
                }

                for (int i = 18; i < 24; i++)
                {
                    const int c = bytes[i] & 0x7f;
                    if (!alphaNum(c) && (c != ' ')) {
                        strength = SemanticWeak;
                    }
                }
            }
        }

        return strength;
    }

    AcarsOqpskReceiver()
    {
        configure(Config());
    }

    void configure(const Config& config)
    {
        m_config = config;
        m_sps = ACARSOQPSK_SAMPLES_PER_SYMBOL;

        m_buf.assign(ACARSOQPSK_MAX_BURST + ACARSOQPSK_DETECT_WINDOW
                     + ACARSOQPSK_CHANNEL_SAMPLE_RATE / 10, Cd(0.0, 0.0));
        m_bufLength = (int) m_buf.size();
        m_bufBlank.assign(m_bufLength, 0);

        // Matched filter: the MSK half sine pulse with the subcarrier translation folded
        // into the kernel. Filtering the real audio with h[j] exp(+j wc j) and then rotating
        // the output by exp(-j wc i) is identical to translating first and filtering with
        // the real pulse, but it removes a whole buffer and a sine and cosine per sample.
        {
            const double wc = 2.0 * M_PI * ACARSOQPSK_SUBCARRIER / ACARSOQPSK_CHANNEL_SAMPLE_RATE;

            m_mf.resize(2 * m_sps);

            for (int i = 0; i < 2 * m_sps; i++)
            {
                double p = wc * i;
                m_mf[i] = sin(M_PI * (i + 0.5) / (2.0 * m_sps)) * Cd(cos(p), sin(p));
            }

            m_mfRotStep = Cd(cos(-wc), sin(-wc));
        }

        m_toneTwiddle = std::exp(Cd(0.0, -2.0 * M_PI * ACARSOQPSK_BAUD_RATE
                                         / ACARSOQPSK_CHANNEL_SAMPLE_RATE));
        m_levelAlpha = 1.0 - exp(-1.0 / (m_config.m_blankTimeConst
                                         * ACARSOQPSK_CHANNEL_SAMPLE_RATE));
        m_carrierAlpha = 1.0 - exp(-2.0 * M_PI * 200.0 / ACARSOQPSK_CHANNEL_SAMPLE_RATE);

        // Reserve every working buffer once. These run on a real time thread and the burst
        // path must not reach the allocator.
        const int maxBurst = ACARSOQPSK_MAX_BURST + ACARSOQPSK_DETECT_WINDOW;
        const int maxSyms = maxBurst / m_sps + 8;
        const int maxBits = ACARSOQPSK_MAX_BYTES * 8;

        m_audio.reserve(maxBurst);
        m_w.reserve(maxBurst);
        m_soft.reserve(maxSyms);
        m_softPos.reserve(maxSyms);
        m_symbolRot.reserve(maxSyms);
        m_refinedSoft.reserve(maxSyms);
        m_bestRefinedSoft.reserve(maxSyms);
        m_candidates.reserve(maxSyms);
        m_ranked.reserve(maxBits);
        m_correctBits.reserve(maxBits);
        m_chosen.reserve(maxBits);
        m_badParity.reserve(ACARSOQPSK_MAX_BYTES);
        m_syndromeHead.assign(65536, -1);
        m_syndromeNext.resize(maxBits);
        m_syndromeTouched.reserve(maxBits);
        m_retries.reserve(8);
        m_recentFrames.reserve(16);

        if (m_crcDeltaCache.empty()) {
            m_crcDeltaCache.resize(ACARSOQPSK_MAX_BYTES + 1);
        }

        reset();
    }

    const Config& config() const { return m_config; }
    void setFrameDiagnosticCallback(FrameDiagnosticCallback callback, void *context)
    {
        m_frameDiagnosticCallback = callback;
        m_frameDiagnosticContext = context;
    }

    void reset()
    {
        std::fill(m_buf.begin(), m_buf.end(), Cd(0.0, 0.0));
        std::fill(m_bufBlank.begin(), m_bufBlank.end(), 0);
        m_bufIdx = 0;
        m_bufCnt = 0;
        m_absSample = 0;

        m_toneY = Cd(0.0, 0.0);
        m_prod = Cd(0.0, 0.0);
        m_envSum = 0.0;
        m_detectPrimed = false;
        m_refreshCount = 0;
        m_holdoff = 0;
        m_pending = 0;
        m_bestRatio = -1.0;
        m_bestStart = 0;
        m_toneOffset = 0.0;
        m_ratio = 0.0;
        m_detected = false;
        m_parallelArmed = true;

        std::memset(m_nbBad, 0, sizeof(m_nbBad));
        std::memset(m_nbHi, 0, sizeof(m_nbHi));
        std::memset(m_nbEnv, 0, sizeof(m_nbEnv));
        std::fill(m_nbLine, m_nbLine + ACARSOQPSK_NB_LINE, Cd(0.0, 0.0));
        m_nbWrite = 0;
        m_absIn = 0;
        m_level = 0.0;
        m_levelPrimed = false;
        m_levelCount = 0;
        m_lastGood = Cd(0.0, 0.0);
        m_haveLastGood = false;
        m_inGap = false;
        m_nbRot = Cd(1.0, 0.0);
        m_gapPhasor = Cd(1.0, 0.0);

        m_messageLength = 0;
        m_acceptedSohAbs = 0;
        m_retries.clear();
        m_recentFrames.clear();
        m_retryInProgress = false;
        m_refineInProgress = false;
        m_reconstructInProgress = false;
        m_suppressCorrection = false;
        m_secondPassWanted = false;
        m_repairAvailable = false;
    }

    // Feed one sample at ACARSOQPSK_CHANNEL_SAMPLE_RATE, normalised so that full scale is
    // about unity. Returns true if a block was decoded, in which case message() and
    // messageLength() describe it. At most one block is decoded per sample.
    bool processSample(const Cd& in)
    {
        Cd z = in;

        m_stats.m_samples++;
        m_messageLength = 0;

        blank(z);

        m_buf[m_bufIdx] = z;
        m_bufBlank[m_bufIdx] = m_outputBlanked ? 1 : 0;
        if (++m_bufIdx == m_bufLength) {
            m_bufIdx = 0;
        }
        m_bufCnt = std::min(m_bufCnt + 1, m_bufLength);
        m_absSample++;

        if (m_holdoff > 0) {
            m_holdoff--;
        }

        m_detected = false;

        if (m_bufCnt < m_bufLength) {
            return false;
        }

        int dueRetry = -1;

        for (size_t i = 0; i < m_retries.size(); i++)
        {
            if ((m_retries[i].m_retryAtAbs <= m_absSample)
                && ((dueRetry < 0) || (m_retries[i].m_retryAtAbs
                                      < m_retries[dueRetry].m_retryAtAbs))) {
                dueRetry = (int) i;
            }
        }

        bool retryRecovered = false;

        if (dueRetry >= 0)
        {
            const RetryCandidate retry = m_retries[dueRetry];
            m_retries.erase(m_retries.begin() + dueRetry);
            m_retryInProgress = true;
            m_toneOffset = 0.0;
            m_stats.m_deferredRetries++;
            processBurst(retry.m_startAbs, retry.m_frequency);
            m_retryInProgress = false;

            if (m_messageLength > 0)
            {
                m_stats.m_deferredRecovered++;
                retryRecovered = true;
            }
        }

        // The sliding detector must consume every sample. Skipping its recursion on a
        // recovered retry desynchronises the entering/leaving windows until the next full
        // refresh, creating thousands of false triggers. If a normal pending detection
        // matures on this same sample, defer only its expensive burst processing by one.
        detect(!retryRecovered);

        return m_messageLength > 0;
    }

    const uint8_t *message() const { return m_bytes; }
    int messageLength() const { return m_messageLength; }
    // Absolute input sample of the accepted frame's SOH. Unlike the sample count at the
    // moment of delivery, this does not move when deferred retries or parallel capture
    // change the order bursts are processed in, so it is the key to compare two runs by.
    int64_t messageSohSample() const { return m_acceptedSohAbs; }

    // Diagnostics, for a scope or a test harness
    double detectStatistic() const { return m_ratio; }
    bool detected() const { return m_detected; }
    double level() const { return m_level; }
    const Stats& stats() const { return m_stats; }
    void resetStats() { m_stats = Stats(); }

private:
    // ---------------------------------------------------------------------------
    // Impulse blanker
    // ---------------------------------------------------------------------------

    int nbPosBack(int back) const
    {
        return ((m_nbWrite - 1 - back) % ACARSOQPSK_NB_LINE + ACARSOQPSK_NB_LINE)
               % ACARSOQPSK_NB_LINE;
    }

    // True if anything within +-guard of this position was flagged. A spike reaching the
    // channel rate has already been through the channelizer and the interpolator, so what
    // arrives is not one bad sample but those filters' ringing spread either side of it.
    // Dilating the mask to cover the skirts is where most of the blanker's benefit is.
    bool badNear(int pos) const
    {
        for (int d = -m_config.m_blankGuard; d <= m_config.m_blankGuard; d++)
        {
            int i = ((pos + d) % ACARSOQPSK_NB_LINE + ACARSOQPSK_NB_LINE) % ACARSOQPSK_NB_LINE;

            if (m_nbBad[i]) {
                return true;
            }
        }

        return false;
    }

    // Ignition noise, lightning and co-site switching arrive as spikes of tens of
    // microseconds up to a millisecond, tens of dB above the signal. They do far more damage
    // than their energy suggests: one spike dominates the carrier amplitude estimate for the
    // length of the extraction filter, and drags the pre-key detector's mean envelope with
    // it too.
    //
    // Detection is by duration, not amplitude. Being over the threshold makes a sample a
    // candidate; it is confirmed when the whole run of over-threshold samples turns out
    // shorter than m_maxSpike. That is the thing that actually separates interference from a
    // transmission turning on, and it is what lets the level estimate ignore spikes entirely
    // rather than being ratcheted upward by them - which is how an amplitude-only detector
    // starts missing spikes above about 50 a second.
    void blank(Cd& z)
    {
        m_outputBlanked = false;

        if (m_config.m_blankThreshold <= 0.0) {
            return;
        }

        double env = std::abs(z);

        if (!m_levelPrimed)
        {
            m_level = env;
            m_levelPrimed = true;
        }

        bool hi = false;

        if (m_levelCount < 4096) {
            m_levelCount++;
        } else {
            hi = (env > m_config.m_blankThreshold * m_level);
        }

        m_nbLine[m_nbWrite] = z;
        m_nbEnv[m_nbWrite] = env;
        m_nbHi[m_nbWrite] = hi ? 1 : 0;
        m_nbBad[m_nbWrite] = 0;
        m_nbWrite = (m_nbWrite + 1) % ACARSOQPSK_NB_LINE;
        m_absIn++;

        const int maxSpike = m_config.m_maxSpike;

        if (m_absIn > maxSpike + 1)
        {
            int c = nbPosBack(maxSpike + 1);
            bool spike = false;

            if (m_nbHi[c])
            {
                int len = 1;

                for (int k = 1; k <= maxSpike; k++)
                {
                    if (!m_nbHi[nbPosBack(maxSpike + 1 + k)]) {
                        break;
                    }
                    len++;
                }

                for (int k = 1; k <= maxSpike; k++)
                {
                    if (!m_nbHi[nbPosBack(maxSpike + 1 - k)]) {
                        break;
                    }
                    len++;
                }

                spike = (len >= m_config.m_minSpike) && (len <= maxSpike);
            }

            m_nbBad[c] = spike ? 1 : 0;

            if (spike) {
                m_stats.m_blanked++;
            } else {
                // Sustained changes move the level, spikes do not
                m_level += m_levelAlpha * (m_nbEnv[c] - m_level);
            }
        }

        int e = nbPosBack(ACARSOQPSK_NB_LOOKAHEAD);

        if ((m_absIn - 1 - ACARSOQPSK_NB_LOOKAHEAD) < 0)
        {
            z = Cd(0.0, 0.0);
            return;
        }

        if (!badNear(e))
        {
            z = m_nbLine[e];

            // Track the carrier's rotation on clean samples. For AM the sample to sample
            // product has positive magnitude and its argument is the carrier offset.
            if (m_haveLastGood)
            {
                Cd p = z * std::conj(m_lastGood);
                double n = std::abs(p);

                if (n > 1e-30) {
                    m_nbRot += 0.004 * (p / n - m_nbRot);
                }
            }

            m_lastGood = z;
            m_lastGoodAbs = m_absIn - 1 - ACARSOQPSK_NB_LOOKAHEAD;
            m_haveLastGood = true;
            m_inGap = false;
            return;
        }

        if (!m_haveLastGood)
        {
            m_outputBlanked = true;
            z = Cd(0.0, 0.0);
            return;
        }

        m_outputBlanked = true;

        if (m_config.m_fillMode == FillScale)
        {
            Cd v = m_nbLine[e];
            double a = std::abs(v);
            double limit = m_config.m_blankThreshold * m_level;

            z = ((a > limit) && (a > 0.0)) ? (v * (limit / a)) : v;
            return;
        }

        if (m_config.m_fillMode == FillHold)
        {
            z = m_lastGood;
            return;
        }

        if (m_config.m_fillMode == FillInterpolate)
        {
            Cd nextVal;
            bool found = false;
            int64_t absE = m_absIn - 1 - ACARSOQPSK_NB_LOOKAHEAD;
            int64_t nextAbs = 0;

            for (int k = 1; k <= ACARSOQPSK_NB_LOOKAHEAD - m_config.m_blankGuard; k++)
            {
                int p = (e + k) % ACARSOQPSK_NB_LINE;

                if (!badNear(p))
                {
                    nextVal = m_nbLine[p];
                    nextAbs = absE + k;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                z = m_lastGood;
                return;
            }

            double t = (double)(absE - m_lastGoodAbs) / (double)(nextAbs - m_lastGoodAbs);

            z = m_lastGood + t * (nextVal - m_lastGood);
            return;
        }

        // FillCarrier: the running mean envelope, which for AM is the carrier amplitude
        // because the audio averages to zero, on a phase extrapolated at the tracked carrier
        // rate. Holding the last good sample instead freezes whatever the audio happened to
        // be doing, so a gap opening at an envelope peak or trough injects a spurious
        // excursion of most of the modulation depth. This injects no audio at all, which is
        // the right answer when nothing is known.
        if (!m_inGap)
        {
            double a = std::abs(m_lastGood);
            m_gapPhasor = (a > 1e-30) ? (m_lastGood / a) : Cd(1.0, 0.0);
            m_inGap = true;
        }

        double r = std::abs(m_nbRot);

        if (r > 1e-30) {
            m_gapPhasor *= m_nbRot / r;
        }

        double g = std::abs(m_gapPhasor);

        if (g > 1e-30) {
            m_gapPhasor /= g;
        }

        z = m_level * m_gapPhasor;
    }

    // ---------------------------------------------------------------------------
    // Pre-key detection
    // ---------------------------------------------------------------------------

    Cd bufAt(int64_t abs) const
    {
        int64_t back = m_absSample - 1 - abs;

        if ((back < 0) || (back >= m_bufLength)) {
            return Cd(0.0, 0.0);
        }

        // back is already constrained to one buffer length, so one wrap is sufficient.
        // Avoiding integer division here matters: burst processing calls this for every
        // sample in the maximum-length receive buffer.
        int pos = m_bufIdx - 1 - (int) back;
        if (pos < 0) {
            pos += m_bufLength;
        }

        return m_buf[pos];
    }

    bool blankAt(int64_t abs) const
    {
        const int64_t back = m_absSample - 1 - abs;
        if ((back < 0) || (back >= m_bufLength)) return false;

        int pos = m_bufIdx - 1 - (int) back;
        if (pos < 0) pos += m_bufLength;
        return m_bufBlank[pos] != 0;
    }

    // The pre-key is a constant 2400 Hz tone on the envelope, so the statistic is the depth
    // of 2400 Hz modulation on it: twice the 2400 Hz bin amplitude over the mean envelope,
    // which is the AM modulation index. It reads about 0.85 on a pre-key and near zero on
    // noise, and being a ratio it does not move with signal level.
    //
    // The window sits a whole transmission behind the newest sample, so by the time a
    // pre-key is recognised everything that follows it is already buffered.
    void detect(bool allowBurst = true)
    {
        const int64_t wEnd = m_absSample - 1 - ACARSOQPSK_MAX_BURST;
        const int64_t wStart = wEnd - ACARSOQPSK_DETECT_WINDOW + 1;

        if (!m_detectPrimed)
        {
            // The recursion can only add and remove one sample a step, so the window has to
            // be summed once to start it off, and again periodically because a sliding DFT
            // is only marginally stable
            m_toneY = Cd(0.0, 0.0);
            m_envSum = 0.0;
            m_prod = Cd(0.0, 0.0);

            Cd rot(1.0, 0.0);
            Cd step = std::conj(m_toneTwiddle);

            for (int i = 0; i < ACARSOQPSK_DETECT_WINDOW; i++)
            {
                Cd s = bufAt(wStart + i);
                double e = std::abs(s);

                m_toneY += e * rot;
                rot *= step;
                m_envSum += e;
                m_prod += s * std::conj(bufAt(wStart + i - 1));
            }

            m_detectPrimed = true;
            m_refreshCount = 0;
        }
        else
        {
            Cd entering = bufAt(wEnd);
            Cd enteringPrev = bufAt(wEnd - 1);
            Cd leaving = bufAt(wStart - 1);
            Cd leavingPrev = bufAt(wStart - 2);
            double enteringEnv = std::abs(entering);
            double leavingEnv = std::abs(leaving);

            // The window is a whole number of cycles, so the twiddle on the samples entering
            // and leaving is unity and the update is one complex multiply
            m_toneY = m_toneTwiddle * (m_toneY - leavingEnv + enteringEnv);
            m_envSum += enteringEnv - leavingEnv;

            // For AM the envelope product is always positive, so the argument of the mean is
            // the carrier offset with no modulation bias
            m_prod += entering * std::conj(enteringPrev) - leaving * std::conj(leavingPrev);

            if (++m_refreshCount >= ACARSOQPSK_CHANNEL_SAMPLE_RATE) {
                m_detectPrimed = false;
            }
        }

        const double n = ACARSOQPSK_DETECT_WINDOW;
        double mean = m_envSum / n;

        m_ratio = (mean > 1e-12) ? (2.0 * std::abs(m_toneY) / (n * mean)) : 0.0;

        // Gate on the holdoff alone. Latching until the statistic falls back below the
        // threshold sounds tidier, but one spurious trigger just before a pre-key then
        // blocks the whole transmission, because the statistic never drops through it.
        if (m_ratio < 0.8 * m_config.m_detectThreshold) {
            m_parallelArmed = true;
        }

        // Ordinary detection is held off by an explicit holdoff or by a retry still
        // waiting for its tail. Deriving the second from the queue keeps the hold and the
        // override that relaxes it keyed on the same fact.
        const bool retryCapture = !m_retries.empty();
        const bool held = (m_holdoff > 0) || retryCapture;
        const bool parallelTrigger = (m_config.m_parallelCapture || retryCapture)
            && m_parallelArmed && held
            && (m_ratio >= std::max(0.75, 3.0 * m_config.m_detectThreshold));

        if ((m_ratio >= m_config.m_detectThreshold)
            && (!held || parallelTrigger) && (m_pending == 0))
        {
            m_pending = ACARSOQPSK_DETECT_WINDOW;
            m_bestRatio = -1.0;
            if (m_config.m_parallelCapture || retryCapture) m_parallelArmed = false;
            if (parallelTrigger)
            {
                if (m_config.m_parallelCapture) m_stats.m_parallelTriggers++;
            }
        }

        if (m_pending == 0) {
            return;
        }

        // Keep the best window while the pre-key slides into place. Firing on the first
        // crossing starts the demodulator on a window only partly overlapping the pre-key,
        // and its phase and frequency estimates are then made largely on silence.
        if (m_ratio > m_bestRatio)
        {
            m_bestRatio = m_ratio;
            m_bestStart = wStart;
            m_bestProd = m_prod;
            m_bestToneY = m_toneY;
        }

        if (--m_pending > 0) {
            return;
        }

        if (!allowBurst)
        {
            m_pending = 1;
            return;
        }

        m_detected = true;
        m_stats.m_triggers++;

        double freq = std::arg(m_bestProd) * ACARSOQPSK_CHANNEL_SAMPLE_RATE / (2.0 * M_PI);

        m_stats.m_freqEstSum += freq;
        m_stats.m_freqEstCount++;

        if (fabs(freq) > fabs(m_stats.m_freqEstPeak)) {
            m_stats.m_freqEstPeak = freq;
        }

        // ARINC 618 puts the audio zero at every bit transition with a positive slope for a
        // one, and the pre-key is all ones, so the rising zero crossings of the 2400 Hz tone
        // are the bit boundaries. For an envelope containing m sin(w (i - d)) the sliding
        // DFT comes out as j exp(j w d), so d = (arg(Y) - pi/2) / w.
        double d = (std::arg(m_bestToneY) - M_PI_2) * m_sps / (2.0 * M_PI);

        m_toneOffset = fmod(fmod(d, (double) m_sps) + m_sps, (double) m_sps);
        m_holdoff = std::max(m_holdoff, processBurst(m_bestStart, freq));
    }

    // ---------------------------------------------------------------------------
    // Burst processing. Runs once per detection, not once per sample.
    // ---------------------------------------------------------------------------

    void matchAudio(int available)
    {
        m_w.resize(available);
        const int taps = (int) m_mf.size();
        Cd outRot(1.0, 0.0);

        for (int i = 0; i < available; i++)
        {
            Cd acc(0.0, 0.0);
            const int jmax = std::min(taps, i + 1);

            for (int j = 0; j < jmax; j++) {
                acc += m_audio[i-j] * m_mf[j];
            }

            m_w[i] = acc * outRot;
            outRot *= m_mfRotStep;
        }
    }

    bool reconstructImpulseAudio(int64_t startAbs, int available)
    {
        if (!m_config.m_impulseReconstruct || m_reconstructInProgress
            || m_candidates.empty() || (m_softPos.size() != m_soft.size())
            || (m_soft.size() < 64)) {
            return false;
        }

        int blanked = 0;
        double energy = 0.0;
        int clean = 0;
        for (int i = 0; i < available; i++)
        {
            if (blankAt(startAbs + i)) {
                blanked++;
            } else {
                energy += m_audio[i] * m_audio[i];
                clean++;
            }
        }
        if ((blanked == 0) || (clean < 64)) return false;

        // A matched-filter output at tau represents the two-symbol half-sine ending near
        // tau. Map it back to the corresponding audio cell, then regenerate only erased
        // samples from the nearest hard decisions. The phase at each boundary is fixed by
        // ARINC 618: positive slope for one, negative for zero.
        const double amplitude = std::max(0.1, std::min(1.5,
            sqrt(2.0 * energy / clean)));
        const double filterDelay = (double) m_mf.size() - 1.0;
        const double sign = m_candidates[0].m_sign;
        int reconstructed = 0;

        for (int i = 0; i < available; i++)
        {
            if (!blankAt(startAbs + i)) continue;

            const double target = i + filterDelay;
            const auto it = std::upper_bound(m_softPos.begin(), m_softPos.end(), target);
            const int n = (int)(it - m_softPos.begin()) - 1;
            if ((n < 0) || (n >= (int) m_soft.size())) continue;

            const int bit = (sign * m_soft[n] >= 0.0) ? 1 : 0;
            const int previous = (n > 0)
                ? ((sign * m_soft[n-1] >= 0.0) ? 1 : 0) : 1;
            const double tone = (bit == previous)
                ? ACARSOQPSK_BAUD_RATE : (ACARSOQPSK_BAUD_RATE / 2.0);
            const double boundary = m_softPos[n] - filterDelay;
            const double phase0 = previous ? 0.0 : M_PI;
            const double phase = phase0 + 2.0 * M_PI * tone
                * (i - boundary) / ACARSOQPSK_CHANNEL_SAMPLE_RATE;
            m_audio[i] = amplitude * sin(phase);
            reconstructed++;
        }

        return reconstructed > 0;
    }

    // Returns how many samples to hold the detector off for
    int processBurst(int64_t startAbs, double freqEstimate)
    {
        m_currentBurstStart = startAbs;
        m_currentBurstFrequency = freqEstimate;
        if (m_frameDiagnosticCallback)
        {
            m_diagBurstStart = startAbs;
            m_diagDetectRatio = m_bestRatio;
            m_diagCarrierFrequency = freqEstimate;
            m_diagToneOffset = m_toneOffset;
        }

        int available = (int) std::min<int64_t>(ACARSOQPSK_MAX_BURST + ACARSOQPSK_DETECT_WINDOW,
                                                m_absSample - startAbs);

        if (available < 64 * m_sps) {
            return 0;
        }

        // 1. Coherent AM detection. Derotate by the estimated carrier offset, then extract
        //    the carrier with three cascaded poles at 200 Hz - one pole is nowhere near
        //    enough, because the audio starts at 600 Hz, only 1.6 octaves up, and a quarter
        //    of it would end up in the "carrier". Projecting onto that, removing it and
        //    normalising by it is the phase detector, the DC block and the AGC in one
        //    operation. The quadrature noise is discarded rather than folded in, which is
        //    what an envelope detector cannot do and why it has a threshold effect.
        m_audio.resize(available);

        // The detector fires on a sixteen symbol window, and it can open before the burst
        // does - measured, tens of milliseconds early on a fifth of the bursts that fail.
        // The carrier estimate is then made largely on noise. The extracted carrier itself
        // is a far better instrument than that window: it is a pilot, it is smoothed over
        // the whole burst, and its rotation rate is the residual offset directly. Extract
        // once, measure how fast the result is still turning, and extract again from the
        // corrected frequency. Amplitude weighting makes the sum ignore the part of the
        // window that was noise, which is exactly the case this is here to repair.
        for (int carrierPass = 0; carrierPass < 2; carrierPass++)
        {
            const double dphi = -2.0 * M_PI * freqEstimate / ACARSOQPSK_CHANNEL_SAMPLE_RATE;
            const Cd rotStep(cos(dphi), sin(dphi));
            Cd rot(1.0, 0.0);
            Cd c1, c2, c3;
            Cd carrierTurn(0.0, 0.0);
            Cd previousCarrier(0.0, 0.0);
            int blanked = 0;

            {
                // Start the cascade settled rather than ringing through pre-key the
                // acquisition would rather have
                Cd mean(0.0, 0.0);
                int nAvg = std::min(available, 4 * m_sps);
                Cd r(1.0, 0.0);

                for (int i = 0; i < nAvg; i++)
                {
                    mean += bufAt(startAbs + i) * r;
                    r *= rotStep;
                }

                c1 = c2 = c3 = mean / (double) nAvg;
                previousCarrier = c3;
            }

            for (int i = 0; i < available; i++)
            {
                Cd z = bufAt(startAbs + i) * rot;
                rot *= rotStep;

                c1 += m_carrierAlpha * (z - c1);
                c2 += m_carrierAlpha * (c1 - c2);
                c3 += m_carrierAlpha * (c2 - c3);

                carrierTurn += c3 * std::conj(previousCarrier);
                previousCarrier = c3;

                if (blankAt(startAbs + i)) {
                    blanked++;
                }

                // (projection - carrier) / carrier = dot(z, c3) / |c3|^2 - 1.
                // Working in power avoids a square root and a second division per sample.
                double carrierPower = std::norm(c3);

                if (carrierPower < 1e-24)
                {
                    m_audio[i] = 0.0;
                    continue;
                }

                double dot = z.real() * c3.real() + z.imag() * c3.imag();
                m_audio[i] = dot / carrierPower - 1.0;
            }

            m_burstBlanked = blanked;

            if ((carrierPass > 0) || (m_config.m_carrierRefineHz <= 0.0)
                || (std::norm(carrierTurn) < 1e-30)) {
                break;
            }

            const double residual = std::arg(carrierTurn)
                * ACARSOQPSK_CHANNEL_SAMPLE_RATE / (2.0 * M_PI);

            if (fabs(residual) < m_config.m_carrierRefineHz) {
                break;
            }

            freqEstimate += residual;
            m_currentBurstFrequency = freqEstimate;
            m_stats.m_carrierRefined++;

            if (m_frameDiagnosticCallback) {
                m_diagCarrierFrequency = freqEstimate;
            }
        }

        // 2. Match to the MSK pulse. The subcarrier translation is folded into the kernel
        //    and undone on the output by a recursive rotator, which is identical to
        //    translating the input first but needs neither a second buffer nor a sine and
        //    cosine per sample.
        matchAudio(available);

        // 3. Timing. The pre-key tone phase predicts the bit boundary outright, so this only
        //    has to cover that estimate's own noise. Only the detection window is guaranteed
        //    to be pre-key, so the phase estimate is taken over that and no more.
        int preSymbols = std::min(ACARSOQPSK_DETECT_WINDOW / m_sps, (available / m_sps) - 4);

        if (preSymbols < 8) {
            return 0;
        }

        int span = std::min(m_config.m_timingSearch, m_sps / 2);
        int nOrder = 0;

        for (int d = 0; d <= 2 * span; d++)
        {
            int k = (int) llround(m_toneOffset) + ((d + 1) / 2) * ((d & 1) ? 1 : -1);
            m_order[nOrder++] = ((k % m_sps) + m_sps) % m_sps;
        }

        // 4. Framing, in two passes. Correction used to run inside the innermost loop, so
        //    a repairable frame from an early timing offset pre-empted a clean frame from
        //    a later one: on the +125 kHz recording 777 repairs bought 253 blocks, so
        //    roughly two thirds delivered a one or two bit repair where an untouched
        //    frame was available a few hundred microseconds away. Every offset is now
        //    tried uncorrected first, and a BCS hypothesis is only spent on a repair when
        //    nothing decodes outright.
        //
        //    The second pass repeats the symbol loop, which is the expensive part, but it
        //    only runs when the first pass saw a correctable frame. Noise never gets that
        //    far - it aborts at SOH - so the common case is unchanged, and measured the
        //    second pass runs on 8 percent of triggers.
        for (int pass = 0; pass < 2; pass++)
        {
            m_suppressCorrection = (pass == 0) && m_config.m_deferCorrection
                                && (m_config.m_correctBudget > 0);
            m_secondPassWanted = false;
            m_repairAvailable = false;

            for (int oi = 0; oi < nOrder; oi++)
            {
                double tau0 = 2 * m_sps + m_order[oi];
                if (m_frameDiagnosticCallback) m_diagTimingTry = oi;

                runSymbolLoop(tau0, available, preSymbols);
                m_bitsConsumed = 0;

                if (decodeSoftBits(available, preSymbols, true))
                {
                    m_stats.m_timingTries += oi + 1;
                    if (m_suppressCorrection && m_repairAvailable) {
                        m_stats.m_correctionPreempted++;
                    }
                    m_suppressCorrection = false;
                    return (int)(m_softOrigin + m_bitsConsumed * m_sps);
                }
            }

            m_stats.m_timingTries += nOrder;

            if (!m_suppressCorrection || !m_secondPassWanted) {
                break;
            }

            m_stats.m_correctionSecondPass++;
        }

        m_suppressCorrection = false;

        if (reconstructImpulseAudio(startAbs, available))
        {
            m_stats.m_impulseReconstructTries++;
            m_reconstructInProgress = true;
            matchAudio(available);

            for (int oi = 0; oi < nOrder; oi++)
            {
                const double tau0 = 2 * m_sps + m_order[oi];
                if (m_frameDiagnosticCallback) m_diagTimingTry = oi;
                runSymbolLoop(tau0, available, preSymbols);
                m_bitsConsumed = 0;

                if (decodeSoftBits(available, preSymbols, true))
                {
                    m_reconstructInProgress = false;
                    m_stats.m_impulseReconstructRecovered++;
                    m_stats.m_timingTries += oi + 1;
                    return (int)(m_softOrigin + m_bitsConsumed * m_sps);
                }
            }

            m_stats.m_timingTries += nOrder;
            m_reconstructInProgress = false;
        }

        // Nothing decoded, so this was noise or a mid block false trigger
        return 32 * m_sps;
    }

    // Matched filter output at a fractional position, derotated 90 degrees a symbol so MSK
    // becomes BPSK. Catmull-Rom over a signal band limited to 1.2 kHz and sampled at 48 kHz,
    // so the interpolation error is negligible.
    Cd derotate(double pos, double origin) const
    {
        Cd w = interpolateMf(pos);
        double p = -M_PI * 0.5 * (pos - origin) / (double) m_sps;

        return w * Cd(cos(p), sin(p));
    }

    Cd interpolateMf(double pos) const
    {
        int i = (int) floor(pos);
        double f = pos - i;

        if ((i < 1) || (i + 2 >= (int) m_w.size())) {
            return Cd(0.0, 0.0);
        }

        Cd p0 = m_w[i-1], p1 = m_w[i], p2 = m_w[i+1], p3 = m_w[i+2];

        return p1 + 0.5 * f * (p2 - p0
             + f * (2.0*p0 - 5.0*p1 + 4.0*p2 - p3
             + f * (3.0*(p1 - p2) + p3 - p0)));
    }

    void runSymbolLoop(double tau0, int available, int preSymbols)
    {
        m_soft.clear();
        m_softOrigin = tau0;
        const bool diagnostics = m_frameDiagnosticCallback != nullptr;
        const bool trackPositions = diagnostics || m_config.m_syncReacquire
                                  || m_config.m_deferredRetry || m_config.m_blockRefine
                                  || m_config.m_impulseReconstruct;

        if (trackPositions) {
            m_softPos.clear();
        }
        if (m_config.m_blockRefine) {
            m_symbolRot.clear();
        }

        if (diagnostics)
        {
            m_diagInitialTau = tau0;
            m_diagPeakPhaseError = 0.0;
            m_diagPhaseErrorSq = 0.0;
            m_diagPhaseErrorCount = 0;
            m_diagPeakTimingError = 0.0;
            m_diagTimingErrorSq = 0.0;
            m_diagTimingErrorCount = 0;
            m_diagPeakTimingWander = 0.0;
        }

        // The pre-key is all ones, so it gives a data-aided phase estimate with no sign
        // ambiguity. Optionally fit the residual phase rate as well; otherwise a residual
        // rate smears the vector mean and has to be acquired decision-directed.
        double acquiredRate = 0.0;

        if (m_config.m_lsAcquisition && (preSymbols >= 8))
        {
            // Ignore the matched-filter start-up and the end nearest sync, then unwrap the
            // phase and fit phase = intercept + rate*n with magnitude-squared weights.
            // A coherence gate keeps partial-burst/noise detections from seeding the PLL.
            const int first = 2;
            const int last = preSymbols - 2;
            Cd cross(0.0, 0.0);
            double crossWeight = 0.0;
            Cd previous = derotate(tau0 + first * (double) m_sps, tau0);
            double phase = std::arg(previous);
            double sumW = 0.0, sumN = 0.0, sumP = 0.0;
            double sumNN = 0.0, sumNP = 0.0;

            for (int nSym = first; nSym < last; nSym++)
            {
                Cd current = derotate(tau0 + nSym * (double) m_sps, tau0);

                if (nSym != first)
                {
                    Cd step = current * std::conj(previous);
                    phase += std::arg(step);
                    cross += step;
                    crossWeight += std::abs(current) * std::abs(previous);
                }

                double weight = std::norm(current);
                sumW += weight;
                sumN += weight * nSym;
                sumP += weight * phase;
                sumNN += weight * nSym * nSym;
                sumNP += weight * nSym * phase;
                previous = current;
            }

            const double denominator = sumW * sumNN - sumN * sumN;
            const double coherence = (crossWeight > 1e-12)
                                   ? std::abs(cross) / crossWeight : 0.0;

            if ((denominator > 1e-12) && (coherence >= 0.6))
            {
                const double fittedRate = (sumW * sumNP - sumN * sumP) / denominator;

                // 0.15 rad/symbol is 57 Hz at 2400 baud, already far beyond the residual
                // expected after carrier/tone acquisition. Larger values identify a
                // wrong-sign, partial-burst or noise fit and are safer left to the PLL.
                if (fabs(fittedRate) <= 0.15)
                {
                    acquiredRate = fittedRate;
                    m_stats.m_acquiredPhaseRateSum += acquiredRate;
                    m_stats.m_acquiredPhaseRateCount++;
                }
            }
        }

        Cd acc(0.0, 0.0);
        Cd rateRot(1.0, 0.0);
        const Cd rateStep(cos(-acquiredRate), sin(-acquiredRate));

        for (int nSym = 0; nSym < preSymbols; nSym++)
        {
            acc += derotate(tau0 + nSym * (double) m_sps, tau0) * rateRot;
            rateRot *= rateStep;
        }

        const double theta = std::arg(acc);
        Cd phaseRot(cos(-theta), sin(-theta));
        double tau = tau0;
        double period = m_sps;
        double prevI = 0.0;
        bool havePrev = false;
        double phaseRate = acquiredRate;
        if (diagnostics) m_diagAcquiredPhaseRate = acquiredRate;

        // The quadrature residual over the known pre-key estimates the matched-filter noise
        // variance. It calibrates the reliability weight without changing hard decisions.
        double signalAmplitude = 0.0;
        double noiseVariance = 0.0;
        rateRot = Cd(1.0, 0.0);

        for (int nSym = 0; nSym < preSymbols; nSym++)
        {
            Cd u = derotate(tau0 + nSym * (double) m_sps, tau0)
                 * Cd(cos(-theta), sin(-theta)) * rateRot;
            signalAmplitude += u.real();
            noiseVariance += u.imag() * u.imag();
            rateRot *= rateStep;
        }

        signalAmplitude = fabs(signalAmplitude / preSymbols);
        noiseVariance = std::max(1e-12, noiseVariance / preSymbols);
        if (diagnostics)
        {
            m_diagSignalAmplitude = signalAmplitude;
            m_diagNoiseVariance = noiseVariance;
            m_diagMinPeriod = period;
            m_diagMaxPeriod = period;
        }

        // The timing search lands within half a sample and the half sine matched filter is
        // tolerant, so the Gardner loop only has to hold that across a block. A gain of
        // 0.002 preserves the recording result while retaining margin beyond +-200 ppm.
        const double timingGain = m_config.m_timingGain;
        const double timingFreqGain = m_config.m_timingGain * 0.02;
        const double phaseGain = m_config.m_phaseGain;
        const double phaseFreqGain = m_config.m_phaseGain * 0.04;

        while (tau + 2 * m_sps < available)
        {
            Cd u = derotate(tau, tau0) * phaseRot;
            double soft = u.real();
            const int symbolIndex = (int) m_soft.size();

            if (trackPositions)
            {
                m_softPos.push_back(tau);
            }

            if (m_config.m_blockRefine) {
                m_symbolRot.push_back(phaseRot);
            }

            if (diagnostics)
            {
                m_diagPeakTimingWander = std::max(m_diagPeakTimingWander,
                    fabs(tau - (tau0 + symbolIndex * (double) m_sps)));
            }

            // Decision directed phase loop. Over the pre-key this is a PLL on a constant
            // symbol; through the block it tracks the residual subcarrier error, which at
            // the +-200 ppm ARINC 618 allows on the tones is a third of a cycle across a
            // maximum length block and would otherwise rotate I into Q.
            double err = (soft > 0.0) ? u.imag() : -u.imag();
            double mag = std::abs(u);

            if (m_config.m_softPhase)
            {
                const double llrHalf = std::max(-20.0, std::min(20.0,
                    signalAmplitude * soft / noiseVariance));
                err = tanh(llrHalf) * u.imag();
            }

            if (mag > 1e-12) {
                err /= mag;
            }
            if (diagnostics)
            {
                m_diagPeakPhaseError = std::max(m_diagPeakPhaseError, fabs(err));
                m_diagPhaseErrorSq += err * err;
                m_diagPhaseErrorCount++;
            }

            // Second order only. A third-order acceleration arm was implemented for the
            // 50 Hz/s drift row and measured 191/200 at 1e-6 and 190/200 at 1e-5 against a
            // 191/200 baseline - the drift failures were never phase tracking, they were a
            // carrier estimated on noise, which processBurst() now refines directly. There
            // is no known failure a third arm is the right instrument for, so it is gone.
            phaseRate += phaseFreqGain * err;
            phaseRate = std::max(-0.2, std::min(0.2, phaseRate));
            double phaseStep = phaseGain * err + phaseRate;
            phaseRot *= Cd(cos(-phaseStep), sin(-phaseStep));

            // The Gardner mid point deliberately uses the phase *after* this symbol's
            // update, not before. Sharing one phasor between the two to save a sine and
            // cosine changes the loop dynamics and measurably costs messages.
            if (havePrev)
            {
                Cd mid = derotate(tau - 0.5 * period, tau0) * phaseRot;
                // With tau increasing towards later samples, a positive Gardner error must
                // pull the sampling instant earlier. The previous ordering gave the loop
                // positive feedback and made clock-offset performance worse than disabling
                // timing tracking altogether.
                double e = mid.real() * (prevI - soft);
                double norm = std::max(1e-12, fabs(soft) + fabs(prevI));

                e /= norm;
                e = std::max(-1.0, std::min(1.0, e));
                if (diagnostics)
                {
                    m_diagPeakTimingError = std::max(m_diagPeakTimingError, fabs(e));
                    m_diagTimingErrorSq += e * e;
                    m_diagTimingErrorCount++;
                }

                period += timingFreqGain * e;
                period = std::max(m_sps - 0.1, std::min(m_sps + 0.1, period));
                if (diagnostics)
                {
                    m_diagMinPeriod = std::min(m_diagMinPeriod, period);
                    m_diagMaxPeriod = std::max(m_diagMaxPeriod, period);
                }
                tau += timingGain * e;
            }

            prevI = soft;
            havePrev = true;

            m_soft.push_back(soft);
            tau += period;
        }

        if (diagnostics)
        {
            m_diagFinalPhaseRate = phaseRate;
            m_diagFinalPeriod = period;
        }
    }

    // ---------------------------------------------------------------------------
    // Framing
    // ---------------------------------------------------------------------------

    static bool oddParity(uint8_t byte)
    {
        byte ^= byte >> 4;
        byte ^= byte >> 2;
        byte ^= byte >> 1;

        return byte & 1;
    }

    static int bitCount(uint8_t byte)
    {
        byte = (uint8_t)(byte - ((byte >> 1) & 0x55));
        byte = (uint8_t)((byte & 0x33) + ((byte >> 2) & 0x33));
        return (byte + (byte >> 4)) & 0x0f;
    }

    bool tryRefinedFrame(const SyncCandidate& candidate)
    {
        if (!m_config.m_blockRefine || (candidate.m_score < 0.85)
            || (m_softPos.size() != m_soft.size())
            || (m_symbolRot.size() != m_soft.size())) {
            return false;
        }

        const int firstBit = candidate.m_position + 32;
        const int fitFirst = std::max(0, candidate.m_position);
        const int fitLast = std::min((int) m_soft.size(),
            firstBit + ACARSOQPSK_MAX_BYTES * 8);

        if (fitLast - fitFirst < 64) {
            return false;
        }

        static const double offsets[] = {-0.5, 0.0, 0.5};
        static const double slopes[] = {-0.02, -0.01, 0.0, 0.01, 0.02};
        double bestScore = -1.0;
        m_bestRefinedSoft.clear();

        for (double offset : offsets)
        {
            for (double slope : slopes)
            {
                double sumW = 0.0, sumX = 0.0, sumP = 0.0;
                double sumXX = 0.0, sumXP = 0.0;

                for (int n = fitFirst; n < fitLast; n++)
                {
                    const double x = n - fitFirst;
                    const double pos = m_softPos[n] + offset + slope * x;
                    const Cd u = derotate(pos, m_softOrigin) * m_symbolRot[n];
                    const double decision = (u.real() >= 0.0) ? 1.0 : -1.0;
                    const double phase = std::arg(u * decision);
                    const double reliability = fabs(u.real()) / (std::abs(u) + 1e-12);
                    const double weight = std::norm(u) * reliability * reliability;

                    sumW += weight;
                    sumX += weight * x;
                    sumP += weight * phase;
                    sumXX += weight * x * x;
                    sumXP += weight * x * phase;
                }

                const double denominator = sumW * sumXX - sumX * sumX;
                if ((sumW < 1e-12) || (denominator < 1e-12)) continue;

                const double phaseSlope = (sumW * sumXP - sumX * sumP) / denominator;
                const double phaseIntercept = (sumP - phaseSlope * sumX) / sumW;
                m_refinedSoft = m_soft;
                double margin = 0.0;
                double magnitude = 0.0;

                for (int n = fitFirst; n < fitLast; n++)
                {
                    const double x = n - fitFirst;
                    const double pos = m_softPos[n] + offset + slope * x;
                    Cd u = derotate(pos, m_softOrigin) * m_symbolRot[n];
                    const double phase = phaseIntercept + phaseSlope * x;
                    u *= Cd(cos(-phase), sin(-phase));
                    m_refinedSoft[n] = u.real();
                    margin += fabs(u.real());
                    magnitude += std::abs(u);
                }

                const double score = margin / (magnitude + 1e-12);
                if (score > bestScore)
                {
                    bestScore = score;
                    m_bestRefinedSoft = m_refinedSoft;
                }
            }
        }

        if (m_bestRefinedSoft.empty()) {
            return false;
        }

        m_stats.m_refineTries++;
        m_soft.swap(m_bestRefinedSoft);
        m_refineInProgress = true;
        const bool accepted = tryFrame(firstBit, candidate.m_sign);
        m_refineInProgress = false;
        m_soft.swap(m_bestRefinedSoft);

        if (accepted) m_stats.m_refineRecovered++;
        return accepted;
    }

    bool decodeSoftBits(int available, int preSymbols, bool allowReacquire)
    {
        // Bit sync then character sync: '+' with odd parity, '*', SYN, SYN, each LSB first.
        // ARINC 618 section 4.2.2 says the + and * are there "to enable bit ambiguity
        // resolution", which is what the sign of this correlation provides.
        static const uint8_t syncBytes[4] = { (uint8_t)('+' | 0x80), (uint8_t)'*', 0x16, 0x16 };

        int pat[32];

        for (int b = 0; b < 4; b++)
        {
            for (int i = 0; i < 8; i++) {
                pat[b*8+i] = ((syncBytes[b] >> i) & 1) ? 1 : -1;
            }
        }

        int n = (int) m_soft.size();

        // Every position that clears the threshold is a candidate, not just the best one.
        // With thousands of positions and a 32 bit pattern, noise alone will occasionally
        // out-score the real sync, and taking only the winner then throws the block away.
        m_candidates.clear();
        m_refineEligible = false;

        for (int k = 0; k + 32 <= n; k++)
        {
            double score = 0.0;
            double mag = 0.0;

            for (int i = 0; i < 32; i++)
            {
                score += m_soft[k+i] * pat[i];
                mag += fabs(m_soft[k+i]);
            }

            if (mag < 1e-12) {
                continue;
            }

            double norm = score / mag;

            if (fabs(norm) >= m_config.m_syncThreshold) {
                m_candidates.push_back({fabs(norm), k, (norm < 0.0) ? -1.0 : 1.0});
            }
        }

        if (m_candidates.empty())
        {
            m_stats.m_syncMissed++;
            return false;
        }

        std::sort(m_candidates.begin(), m_candidates.end(),
            [](const SyncCandidate& a, const SyncCandidate& b) {
                return a.m_score > b.m_score;
            });

        // A detector can open a window hundreds of milliseconds before the real burst.
        // In that case the first acquisition pass estimates phase, frequency and noise on
        // silence. Once a high-confidence sync has located the transmission, run the loop
        // again with its known pre-key immediately before sync as the acquisition interval.
        // Keep this a single re-anchor: a false sync must not recursively chase itself.
        if (allowReacquire && m_config.m_syncReacquire
            && (m_candidates[0].m_score >= 0.85)
            && (m_candidates[0].m_position >= preSymbols)
            && (m_candidates[0].m_position < (int) m_softPos.size()))
        {
            const double syncTau = m_softPos[m_candidates[0].m_position];
            const double anchoredTau = syncTau - preSymbols * (double) m_sps;

            if ((anchoredTau >= 2.0)
                && (anchoredTau + (preSymbols + 36) * (double) m_sps < available))
            {
                m_stats.m_syncReacquired++;
                runSymbolLoop(anchoredTau, available, preSymbols);
                m_bitsConsumed = 0;
                return decodeSoftBits(available, preSymbols, false);
            }
        }

        m_stats.m_syncFound++;

        size_t tries = std::min<size_t>(m_candidates.size(), 32);

        for (size_t c = 0; c < tries; c++)
        {
            const SyncCandidate& candidate = m_candidates[c];
            if (m_frameDiagnosticCallback || m_config.m_repairSoh
                || m_config.m_deferredRetry || m_config.m_blockRefine)
            {
                m_diagSyncScore = candidate.m_score;
                m_diagSyncRank = (int) c;
                m_diagSyncCandidates = (int) m_candidates.size();
            }

            if (tryFrame(candidate.m_position + 32, candidate.m_sign)) {
                return true;
            }
        }

        if (m_refineEligible && tryRefinedFrame(m_candidates[0])) {
            return true;
        }

        m_stats.m_crcInvalid++;
        return false;
    }

    // Slice the soft bits into an ARINC 618 block, checking parity and framing on the way.
    // Returns the byte count, or 0. Bytes whose parity fails are recorded so the correction
    // stage knows where to look.
    int sliceBlock(int firstBit, double sign)
    {
        int n = (int) m_soft.size();
        int byteCount = 0;
        bool gotETX = false;
        int etxIndex = 0;
        m_lastSliceFailure = SliceComplete;
        m_partialByteCount = 0;

        m_badParity.clear();
        m_currentSohRepaired = false;

        for (int b = 0; ; b++)
        {
            int base = firstBit + b * 8;

            if (base + 8 > n)
            {
                m_stats.m_abortBufferEnd++;
                m_lastSliceFailure = SliceBufferEnd;
                m_partialByteCount = byteCount;
                return 0;
            }

            uint8_t v = 0;

            for (int i = 0; i < 8; i++)
            {
                if (m_soft[base + i] * sign > 0.0) {
                    v |= (uint8_t)(1 << i);
                }
            }

            // SOH is outside the BCS, so ordinary syndrome correction cannot recover it.
            // Permit only the narrow case measured in diagnostics: one bad hard decision
            // following the highest-ranked, very strong 32-bit character sync.
            if ((byteCount == 0) && ((v & 0x7f) != 0x01)
                && m_config.m_repairSoh
                && (m_diagSyncRank == 0) && (m_diagSyncScore >= 0.9)
                && (bitCount((uint8_t)(v ^ 0x01)) == 1))
            {
                v = 0x01;
                m_currentSohRepaired = true;
            }

            m_bytes[byteCount] = v;

            char c = v & 0x7f;

            // The BCS carries no parity, and neither does anything after ETX
            if (!gotETX && !oddParity(v)) {
                m_badParity.push_back(byteCount);
            }

            if ((byteCount == 0) && (c != 0x01))     // SOH
            {
                m_stats.m_abortNoSoh++;
                m_lastSliceFailure = SliceNoSoh;
                m_partialByteCount = 1;
                return 0;
            }

            if (!gotETX && ((c == 0x03) || (c == 0x17)))    // ETX, ETB
            {
                gotETX = true;
                etxIndex = byteCount;
            }

            byteCount++;

            // ARINC 618 section 4.3: DEL is the BCS suffix, transmitted immediately after
            // the two BCS bytes. It is at a known position, not something to scan for -
            // scanning loses every block whose BCS happens to contain 0x7f or 0xff, which is
            // about 1.5% of all traffic.
            if (gotETX && (byteCount == etxIndex + 4)) {
                return byteCount;
            }

            if (byteCount >= ACARSOQPSK_MAX_BYTES)
            {
                m_stats.m_abortMaxBytes++;
                m_lastSliceFailure = SliceMaxBytes;
                m_partialByteCount = byteCount;
                return 0;
            }
        }
    }

    bool bcsValid(int byteCount)
    {
        m_crc.init();
        m_crc.calculate(&m_bytes[1], byteCount - 2 - 1 - 1);

        uint16_t calc = (uint16_t) m_crc.get();
        uint16_t rx = (m_bytes[byteCount-3] & 0xff) | ((m_bytes[byteCount-2] & 0xff) << 8);

        return calc == rx;
    }

    uint16_t bcsResidual(int byteCount)
    {
        m_crc.init();
        m_crc.calculate(&m_bytes[1], byteCount - 4);

        const uint16_t calc = (uint16_t) m_crc.get();
        const uint16_t rx = (m_bytes[byteCount-3] & 0xff)
                          | ((m_bytes[byteCount-2] & 0xff) << 8);
        return calc ^ rx;
    }

    const std::vector<uint16_t>& crcBitDeltas(int byteCount)
    {
        std::vector<uint16_t>& deltas = m_crcDeltaCache[byteCount];
        if (!deltas.empty()) {
            return deltas;
        }

        const int protectedBytes = byteCount - 4;
        const int bitCount = byteCount * 8;
        const uint8_t zero = 0;

        // The affine constant of the CRC. With a zero-initialised register it does not
        // depend on the message length, which is what lets the running CRCs below start
        // at their own byte instead of at the front of the block.
        m_deltaCrc[0].init();
        const uint16_t baseline = (uint16_t) m_deltaCrc[0].get();

        deltas.assign(bitCount, 0);

        // A CRC is affine, so the delta one bit makes is the CRC of a message that is zero
        // except for that bit. Computing that per bit is a whole CRC each time - 1904 of
        // them over 234 bytes for a maximum length block. Instead note that a bit in byte j
        // is the same pattern as the same bit in byte j+1 with one more trailing zero byte,
        // and that leading zeros leave a zero-initialised register alone. So run eight CRCs,
        // one per bit position, backwards through the block, appending one zero byte a step.
        // O(8n) rather than O(8n^2), and no working buffer.
        for (int bit = 0; bit < 8; bit++)
        {
            const uint8_t seed = (uint8_t)(1 << bit);

            m_deltaCrc[bit].init();
            m_deltaCrc[bit].calculate(&seed, 1);
        }

        for (int byte = protectedBytes; byte >= 1; byte--)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                deltas[byte*8 + bit] = baseline ^ (uint16_t) m_deltaCrc[bit].get();
                m_deltaCrc[bit].calculate(&zero, 1);
            }
        }

        for (int bit = 0; bit < 8; bit++)
        {
            deltas[(byteCount-3)*8 + bit] = (uint16_t)(1 << bit);
            deltas[(byteCount-2)*8 + bit] = (uint16_t)(1 << (bit + 8));
        }

        // SOH and the closing DEL are outside BCS coverage and deliberately retain a zero
        // delta. DEL repair therefore falls back to the bounded soft search below.
        return deltas;
    }

    // A structural check on the header, strict enough to stop a 16 bit BCS accepting noise
    // and loose enough not to throw away legal traffic. The exceptions are not decoration -
    // each one is a control character ARINC 618 genuinely puts in a header field, and
    // dropping them costs real messages:
    //
    //  byte 9  technical acknowledgement: DEL when there is nothing to acknowledge, NAK
    //          when the previous block was rejected
    //  10, 11  label: DEL is half of the "_ DEL" no-information label, which on the
    //          2022-07-25 recording is 10 of the 14 messages present
    //  12      block id: 0-9 downlink, A-Z uplink, or NUL
    bool plausible(int byteCount) const
    {
        if (byteCount < ACARSOQPSK_MIN_BYTES) {
            return false;
        }

        for (int i = 1; i <= 12; i++)
        {
            int c = m_bytes[i] & 0x7f;

            if ((i == 9) && ((c == 0x7f) || (c == 0x15))) {
                continue;
            }

            if (((i == 10) || (i == 11)) && (c == 0x7f)) {
                continue;
            }

            if ((i == 12) && (c == 0x00)) {
                continue;
            }

            if ((c < 0x20) || (c >= 0x7f)) {
                return false;
            }
        }

        // ARINC 618 Appendix B: ETX follows the block id when there is no text; otherwise
        // STX introduces the text. Requiring the exact marker for each length keeps the
        // shortened form from weakening the structural filter offered to error correction.
        const int textMarker = m_bytes[13] & 0x7f;
        if (byteCount == ACARSOQPSK_MIN_BYTES)
        {
            if (textMarker != 0x03) {       // ETX: a no-text block
                return false;
            }
        }
        else if (textMarker != 0x02) {      // STX: message text follows
            return false;
        }

        // The BCS suffix. Framing already puts DEL at this position, but its *value* is not
        // covered by the BCS - which is computed over the block up to ETX - so a bit error
        // in it passes the check sequence unnoticed. Insisting on it here is what hands the
        // damaged byte to error correction instead of emitting a block with a corrupt
        // terminator. 0xff is accepted as well as 0x7f: the last symbol of a burst is the
        // one most likely to be clipped by the end of the transmission.
        return (m_bytes[byteCount-1] & 0x7f) == 0x7f;
    }

    // A block is only believed if the BCS passes and the header is structurally sound. The
    // BCS is 16 bits and this receiver offers it a lot of candidates - every timing offset,
    // every sync position above threshold, every correction pattern - so the structural
    // check is not decoration, it is what stops the search manufacturing messages.
    bool accept(int byteCount)
    {
        if (byteCount < ACARSOQPSK_MIN_BYTES) {
            return false;
        }

        if (!bcsValid(byteCount)) {
            return false;
        }

        if (m_config.m_requirePlausible && !plausible(byteCount))
        {
            m_stats.m_implausible++;
            return false;
        }

        m_stats.m_crcValid++;
        m_messageLength = byteCount;

        return true;
    }

    bool validCorrectionCandidate(int byteCount, SemanticStrength& strength)
    {
        if (!bcsValid(byteCount)) {
            return false;
        }

        m_stats.m_correctionBcsMatches++;

        const CorrectionReject codeword = correctionCodeword(m_bytes, byteCount);
        if (codeword != CorrectionValid)
        {
            // parityRepairPossible() makes CorrectionBadParity unreachable here, but the
            // static correctionCodeword() entry point deliberately retains that guard.
            m_stats.m_correctionCodewordRejected++;
            return false;
        }

        if (m_config.m_requirePlausible && !plausible(byteCount))
        {
            m_stats.m_implausible++;
            return false;
        }

        strength = SemanticStrong;

        if (m_config.m_semanticCorrection)
        {
            SemanticField badField;
            strength = correctionSemantics(m_bytes, byteCount, &badField);

            if (strength == SemanticInvalid)
            {
                m_stats.m_correctionSemanticRejected++;
                if (badField == SemanticFieldAddress) {
                    m_stats.m_semanticAddressRejected++;
                } else if (badField == SemanticFieldBlockId) {
                    m_stats.m_semanticBlockRejected++;
                } else if (badField == SemanticFieldLabel) {
                    m_stats.m_semanticLabelRejected++;
                }
                return false;
            }
        }

        return true;
    }

    // ETX/ETB is protected by character parity and the BCS, but sliceBlock() needs it to
    // know where the BCS begins. A one-bit error can therefore hide the very boundary the
    // ordinary correction path needs. Search only one-bit neighbours of the two legal end
    // characters and accept a unique candidate that passes every correction invariant.
    bool repairTerminator(int firstBit, double sign)
    {
        const int availableBytes = std::min(ACARSOQPSK_MAX_BYTES,
            ((int) m_soft.size() - firstBit) / 8);

        if (availableBytes < ACARSOQPSK_MIN_BYTES) {
            return false;
        }

        uint8_t sliced[ACARSOQPSK_MAX_BYTES + 8];

        for (int b = 0; b < availableBytes; b++)
        {
            uint8_t v = 0;
            const int base = firstBit + b * 8;

            for (int i = 0; i < 8; i++) {
                if (m_soft[base + i] * sign > 0.0) v |= (uint8_t)(1 << i);
            }

            sliced[b] = v;
        }

        if ((sliced[0] & 0x7f) != 0x01)
        {
            if (m_config.m_repairSoh && (m_diagSyncRank == 0)
                && (m_diagSyncScore >= 0.9) && (bitCount(sliced[0] ^ 0x01) == 1)) {
                sliced[0] = 0x01;
            } else {
                return false;
            }
        }

        uint8_t best[ACARSOQPSK_MAX_BYTES + 8];
        int bestCount = 0;
        int validCandidates = 0;
        static const uint8_t markers[2] = { 0x83, 0x97 }; // odd-parity ETX and ETB

        for (int terminator = 13; terminator + 3 < availableBytes; terminator++)
        {
            if ((sliced[terminator + 3] & 0x7f) != 0x7f) {
                continue;
            }

            for (uint8_t marker : markers)
            {
                if (bitCount((uint8_t)(sliced[terminator] ^ marker)) != 1) {
                    continue;
                }

                const int candidateCount = terminator + 4;
                std::memcpy(m_bytes, sliced, candidateCount);
                m_bytes[terminator] = marker;
                m_stats.m_correctionTries++;
                SemanticStrength strength;

                if (validCorrectionCandidate(candidateCount, strength))
                {
                    validCandidates++;
                    if (validCandidates == 1)
                    {
                        std::memcpy(best, m_bytes, candidateCount);
                        bestCount = candidateCount;
                    }
                }
            }
        }

        if (validCandidates != 1)
        {
            if (validCandidates > 1) m_stats.m_correctionAmbiguous++;
            return false;
        }

        std::memcpy(m_bytes, best, bestCount);
        m_messageLength = bestCount;
        m_bitsConsumed = firstBit + bestCount * 8;
        m_stats.m_crcValid++;
        m_stats.m_corrected1++;
        m_stats.m_terminatorRepaired++;
        return true;
    }

    bool plausiblePartialHeader() const
    {
        if (m_partialByteCount < 14) {
            return false;
        }

        for (int i = 1; i <= 12; i++)
        {
            const int c = m_bytes[i] & 0x7f;

            if ((i == 9) && ((c == 0x7f) || (c == 0x15))) continue;
            if (((i == 10) || (i == 11)) && (c == 0x7f)) continue;
            if ((i == 12) && (c == 0x00)) continue;
            if ((c < 0x20) || (c >= 0x7f)) return false;
        }

        // A buffer-ended block cannot be the 17-byte no-text form, so it must have STX.
        return (m_bytes[13] & 0x7f) == 0x02;
    }

    bool sameSoh(int64_t a, int64_t b) const
    {
        return ((a >= b) ? (a - b) : (b - a)) <= 2 * m_sps;
    }

    void expireRecentSoh()
    {
        const int64_t oldest = m_absSample - m_bufLength;
        m_recentFrames.erase(std::remove_if(m_recentFrames.begin(), m_recentFrames.end(),
            [oldest](const RecentFrame& frame) { return frame.m_sohAbs < oldest; }),
            m_recentFrames.end());
    }

    bool recentlyAccepted(int64_t sohAbs, int64_t *endAbs = nullptr)
    {
        expireRecentSoh();

        for (const RecentFrame& accepted : m_recentFrames)
        {
            if (sameSoh(accepted.m_sohAbs, sohAbs))
            {
                if (endAbs) *endAbs = accepted.m_endAbs;
                return true;
            }
        }

        return false;
    }

    void recordAcceptedSoh(int64_t sohAbs, int byteCount)
    {
        expireRecentSoh();

        if (!recentlyAccepted(sohAbs)) {
            m_recentFrames.push_back({sohAbs,
                sohAbs + (int64_t) byteCount * 8 * m_sps});
        }

        m_retries.erase(std::remove_if(m_retries.begin(), m_retries.end(),
            [&](const RetryCandidate& retry) { return sameSoh(retry.m_sohAbs, sohAbs); }),
            m_retries.end());
    }

    void queueRetry(const RetryCandidate& retry)
    {
        if (recentlyAccepted(retry.m_sohAbs)) {
            return;
        }

        for (const RetryCandidate& queued : m_retries) {
            if (sameSoh(queued.m_sohAbs, retry.m_sohAbs)) return;
        }

        static const size_t maxQueuedRetries = 8;

        if (m_retries.size() >= maxQueuedRetries)
        {
            m_stats.m_deferredDropped++;
            return;
        }

        // A pending retry holds ordinary detection off, so that the same incomplete burst
        // is not demodulated again and again while its tail is buffered. That holdoff is
        // *derived* from the queue rather than accumulated into m_holdoff: the two are
        // equivalent while the retry is pending - m_holdoff would have expired exactly as
        // the retry fell due - but only the derived form goes away when a later timing
        // offset decodes the block and recordAcceptedSoh() erases the retry. Stored, it
        // outlived the retry by up to a maximum block time, and because detect() keyed its
        // strong-pre-key override on the queue being non-empty, the override was switched
        // off at the same moment. That left the receiver fully deaf for up to 0.86 s.
        m_retries.push_back(retry);
        m_stats.m_deferredQueued++;
    }

    bool tryFrame(int firstBit, double sign)
    {
        // m_softPos is only filled when something needs it, so an index inside it is not
        // on its own evidence that it belongs to this symbol loop - a stale vector from an
        // earlier burst would give a wrong SOH position and let duplicate suppression
        // reject a real frame. Require it to match the soft bits it is supposed to index,
        // which is what tryRefinedFrame() and reconstructImpulseAudio() already do.
        const bool haveSohPosition = (firstBit >= 0)
                                  && (m_softPos.size() == m_soft.size())
                                  && (firstBit < (int) m_softPos.size());
        const int64_t sohAbs = haveSohPosition
            ? (int64_t) llround(m_currentBurstStart + m_softPos[firstBit]) : 0;

        int64_t acceptedEndAbs = 0;
        if (haveSohPosition && recentlyAccepted(sohAbs, &acceptedEndAbs))
        {
            const int consumed = (int) ceil((acceptedEndAbs - m_currentBurstStart)
                / (double) m_sps);
            m_bitsConsumed = std::max(m_bitsConsumed, std::max(0, consumed));
            m_stats.m_duplicateSyncRejected++;
            return false;
        }

        int byteCount = sliceBlock(firstBit, sign);
        uint8_t rawBytes[ACARSOQPSK_MAX_BYTES + 8];
        bool accepted = false;

        if ((byteCount == 0) && (m_lastSliceFailure == SliceBufferEnd)
            && m_config.m_deferredRetry && !m_retryInProgress && !m_refineInProgress
            && !m_reconstructInProgress
            && (m_partialByteCount >= ACARSOQPSK_MIN_BYTES)
            && (m_badParity.size() <= 2) && plausiblePartialHeader()
            && (m_diagSyncRank == 0) && (m_diagSyncScore >= 0.9)
            && (m_softPos.size() == m_soft.size())
            && (firstBit >= 32) && ((firstBit - 32) < (int) m_softPos.size()))
        {
            const double syncAbs = m_currentBurstStart + m_softPos[firstBit - 32];

            // Queued from whichever pass reaches it first; queueRetry() is idempotent by
            // sync position, so the corrected pass adds nothing. Holding it back to the
            // corrected pass was tried and measured two blocks worse on the +125 kHz
            // channel, because a retry queued late is a retry queued after the detector
            // has already moved on.
            RetryCandidate retry;
            retry.m_startAbs = (int64_t) floor(syncAbs - 18.0 * m_sps);
            retry.m_retryAtAbs = (int64_t) ceil(syncAbs
                + (ACARSOQPSK_MAX_BYTES * 8 + 4) * (double) m_sps);
            retry.m_frequency = m_currentBurstFrequency;
            retry.m_sohAbs = (int64_t) llround(syncAbs + 32.0 * m_sps);
            queueRetry(retry);
        }

        if ((byteCount == 0) && (m_partialByteCount > 0)) {
            std::memcpy(rawBytes, m_bytes, m_partialByteCount);
        }

        if (byteCount > 0)
        {
            if (m_config.m_blockRefine && !m_refineInProgress
                && (m_diagSyncRank == 0) && (m_diagSyncScore >= 0.85)
                && (m_badParity.size() <= 4)) {
                m_refineEligible = true;
            }

            std::memcpy(rawBytes, m_bytes, byteCount);

            m_stats.m_parityBytes += (uint64_t) m_badParity.size();
            m_bitsConsumed = firstBit + byteCount * 8;

            accepted = accept(byteCount);

            if (!accepted && (m_config.m_correctBudget > 0))
            {
                if (!m_suppressCorrection) {
                    accepted = correctFrame(firstBit, byteCount);
                } else if (correctionEligible(firstBit, byteCount)) {
                    m_secondPassWanted = true;
                    m_repairAvailable = true;
                }
            }
        }

        if (!accepted && m_config.m_repairTerminator)
        {
            if (!m_suppressCorrection) {
                accepted = repairTerminator(firstBit, sign);
            } else {
                m_secondPassWanted = true;
            }
        }

        if (m_frameDiagnosticCallback)
        {
            FrameDiagnostic diagnostic;
            diagnostic.m_bytes = ((byteCount > 0) || (m_partialByteCount > 0))
                ? rawBytes : nullptr;
            diagnostic.m_byteCount = byteCount;
            diagnostic.m_firstBit = firstBit;
            diagnostic.m_accepted = accepted;
            diagnostic.m_burstStartSample = m_diagBurstStart;
            diagnostic.m_sohSample = (firstBit < (int) m_softPos.size())
                ? m_diagBurstStart + m_softPos[firstBit] : -1.0;
            diagnostic.m_detectRatio = m_diagDetectRatio;
            diagnostic.m_carrierFrequency = m_diagCarrierFrequency;
            diagnostic.m_toneOffset = m_diagToneOffset;
            diagnostic.m_syncScore = m_diagSyncScore;
            diagnostic.m_syncRank = m_diagSyncRank;
            diagnostic.m_syncCandidates = m_diagSyncCandidates;
            diagnostic.m_timingTry = m_diagTimingTry;
            diagnostic.m_initialTau = m_diagInitialTau;
            diagnostic.m_acquiredPhaseRate = m_diagAcquiredPhaseRate;
            diagnostic.m_finalPhaseRate = m_diagFinalPhaseRate;
            diagnostic.m_peakPhaseError = m_diagPeakPhaseError;
            diagnostic.m_rmsPhaseError = m_diagPhaseErrorCount
                ? sqrt(m_diagPhaseErrorSq / m_diagPhaseErrorCount) : 0.0;
            diagnostic.m_finalPeriod = m_diagFinalPeriod;
            diagnostic.m_minPeriod = m_diagMinPeriod;
            diagnostic.m_maxPeriod = m_diagMaxPeriod;
            diagnostic.m_peakTimingError = m_diagPeakTimingError;
            diagnostic.m_rmsTimingError = m_diagTimingErrorCount
                ? sqrt(m_diagTimingErrorSq / m_diagTimingErrorCount) : 0.0;
            diagnostic.m_peakTimingWander = m_diagPeakTimingWander;
            diagnostic.m_signalAmplitude = m_diagSignalAmplitude;
            diagnostic.m_noiseVariance = m_diagNoiseVariance;
            diagnostic.m_sliceFailure = m_lastSliceFailure;
            diagnostic.m_partialByteCount = (byteCount > 0) ? byteCount : m_partialByteCount;
            diagnostic.m_firstByte = diagnostic.m_partialByteCount ? rawBytes[0] : 0;
            diagnostic.m_symbolsAvailableAfterSoh = std::max(0,
                (int) m_soft.size() - firstBit);
            m_frameDiagnosticCallback(m_frameDiagnosticContext, diagnostic);
        }

        // A rank-0 candidate can run out of buffered samples, schedule a retry, and then a
        // later timing/correction candidate in this same burst can decode the block. In
        // that case the deferred work would only emit a duplicate.
        if (accepted && m_currentSohRepaired) {
            m_stats.m_sohRepaired++;
        }
        if (accepted && haveSohPosition)
        {
            m_acceptedSohAbs = sohAbs;
            recordAcceptedSoh(sohAbs, m_messageLength);
        }

        return accepted;
    }

    // Whether correctFrame() would get past its own cheap guards. The first framing pass
    // uses this to decide whether a corrected second pass is worth a symbol loop; without
    // it, every noise burst that happens to slice a byte would run the search twice.
    bool correctionEligible(int firstBit, int byteCount) const
    {
        if ((firstBit + byteCount * 8) > (int) m_soft.size()) {
            return false;
        }

        const int maxFlips = std::max(2, std::min(4, m_config.m_listMaxFlips));

        return m_badParity.size() <= (size_t) maxFlips;
    }

    // Error correction.
    //
    // ACARS is unusually well suited to it. Every character carries odd parity over its
    // eight bits (ARINC 618 section 4.4.2.1), so a byte hit by an odd number of errors is
    // not just detected but located, and the BCS then confirms the repair. Combining that
    // with the soft outputs - flip the least reliable bits first - is stronger than either
    // alone. Only the BCS bytes and the DEL suffix carry no parity, and errors there are
    // covered by the plain soft ranking.
    //
    // The fallback Chase budget is deliberately small. Syndrome mode instead enumerates
    // the complete parity-compatible one/two-bit set whose deltas satisfy the BCS, then
    // relies on codeword and semantic validation rather than blind CRC trials.
    bool correctFrame(int firstBit, int byteCount)
    {
        const int nBits = byteCount * 8;

        if ((firstBit + nBits) > (int) m_soft.size()) {
            return false;
        }

        // A repair can toggle odd parity in at most one byte per flipped bit. Reject an
        // impossible frame before offering patterns to the BCS. Orders above two use the
        // separately bounded, semantically-strong list search below.
        const int maxFlips = std::max(2, std::min(4, m_config.m_listMaxFlips));
        if (m_badParity.size() > (size_t) maxFlips) {
            return false;
        }

        std::memcpy(m_base, m_bytes, byteCount);

        m_correctBits.clear();
        m_chosen.assign(nBits, false);

        for (int b : m_badParity)
        {
            for (int i = 0; i < 8; i++)
            {
                int bit = b * 8 + i;

                if ((bit < nBits) && !m_chosen[bit])
                {
                    m_chosen[bit] = true;
                    m_correctBits.push_back(bit);
                }
            }
        }

        // A blanked sample is an erasure at a position the receiver knows, which is a far
        // stronger reason to widen the candidate set than a merely weak soft value. Widen
        // it only for bursts the blanker actually touched: measured, a wider list is worth
        // 11 blocks in 200 under impulse noise, and costs false decodes at the thermal
        // cliff where the extra candidates are just noise.
        const int listBits = (m_burstBlanked > 0)
            ? std::max(m_config.m_listBits, m_config.m_listBitsErased)
            : m_config.m_listBits;

        const int targetBits = (maxFlips > 2)
            ? std::max(m_config.m_chaseBits, listBits)
            : m_config.m_chaseBits;

        if ((int) m_correctBits.size() < targetBits)
        {
            m_ranked.clear();

            for (int i = 0; i < nBits; i++) {
                m_ranked.push_back(std::make_pair(fabs(m_soft[firstBit + i]), i));
            }

            // Only the weakest unchosen bits are used. Partial sorting the number that can
            // be consumed gives the same prefix without sorting a maximum-length block.
            const size_t rankedCount = std::min(m_ranked.size(),
                (size_t) targetBits + m_correctBits.size());
            std::partial_sort(m_ranked.begin(), m_ranked.begin() + rankedCount, m_ranked.end(),
                [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
                    return a.first < b.first;
                });

            for (size_t i = 0; (i < rankedCount)
                            && ((int) m_correctBits.size() < targetBits); i++)
            {
                int bit = m_ranked[i].second;

                if (!m_chosen[bit])
                {
                    m_chosen[bit] = true;
                    m_correctBits.push_back(bit);
                }
            }
        }

        std::sort(m_correctBits.begin(), m_correctBits.end(), [&](int a, int b) {
            return fabs(m_soft[firstBit + a]) < fabs(m_soft[firstBit + b]);
        });

        int budget = m_config.m_correctBudget;
        int nc = (int) m_correctBits.size();
        uint8_t bestBytes[ACARSOQPSK_MAX_BYTES + 8];
        bool haveBest = false;
        SemanticStrength bestStrength = SemanticInvalid;
        int bestFlips = 0;
        double bestCost = 0.0;
        int bestHeaderChanges = 0;
        int validCandidates = 0;

        auto parityRepairPossible = [&](int bitA, int bitB, int flips)
        {
            // Bytes up to and including ETX/ETB carry odd parity. The two BCS bytes and DEL
            // that follow it do not. A candidate is possible exactly when the set of parity
            // bytes toggled an odd number of times equals the set that initially failed.
            const int lastParityByte = byteCount - 4;
            int toggled[2];
            int toggledCount = 0;
            const int byteA = bitA / 8;
            const int byteB = bitB / 8;

            if (byteA <= lastParityByte) {
                toggled[toggledCount++] = byteA;
            }
            if ((flips == 2) && (byteB <= lastParityByte))
            {
                if ((toggledCount > 0) && (toggled[0] == byteB)) {
                    toggledCount = 0; // Two flips in one byte preserve its parity.
                } else {
                    toggled[toggledCount++] = byteB;
                }
            }

            if (toggledCount != (int) m_badParity.size()) {
                return false;
            }

            for (int i = 0; i < toggledCount; i++)
            {
                if (std::find(m_badParity.begin(), m_badParity.end(), toggled[i])
                    == m_badParity.end())
                {
                    return false;
                }
            }

            return true;
        };

        auto consider = [&](int bitA, int bitB, int flips)
        {
            // Keep consuming the same correction budget and visiting candidates in the same
            // order, but do not calculate a BCS for a parity state that cannot be valid.
            if (!parityRepairPossible(bitA, bitB, flips)) {
                return;
            }

            std::memcpy(m_bytes, m_base, byteCount);
            m_bytes[bitA / 8] ^= (uint8_t)(1 << (bitA % 8));
            if (flips == 2) {
                m_bytes[bitB / 8] ^= (uint8_t)(1 << (bitB % 8));
            }

            m_stats.m_correctionTries++;

            SemanticStrength strength;
            if (!validCorrectionCandidate(byteCount, strength)) {
                return;
            }

            // Exhaustive syndrome enumeration deliberately widens coverage beyond the
            // parity bytes and least-reliable Chase set. Apply that wider search only to a
            // semantically strong header/payload. Proprietary weak candidates remain
            // eligible, but only when the former bounded search would also have offered
            // their bits; otherwise one observed BCS collision becomes an invented frame.
            if (m_config.m_syndromeCorrection && (strength == SemanticWeak)
                && (!m_chosen[bitA] || ((flips == 2) && !m_chosen[bitB]))) {
                return;
            }

            validCandidates++;

            double cost = fabs(m_soft[firstBit + bitA]);
            int headerChanges = (bitA / 8 <= 13) ? 1 : 0;

            if (flips == 2)
            {
                cost += fabs(m_soft[firstBit + bitB]);
                headerChanges += (bitB / 8 <= 13) ? 1 : 0;
            }

            const bool better = !haveBest
                || (strength > bestStrength)
                || ((strength == bestStrength) && (flips < bestFlips))
                || ((strength == bestStrength) && (flips == bestFlips) && (cost < bestCost))
                || ((strength == bestStrength) && (flips == bestFlips)
                    && (cost == bestCost) && (headerChanges < bestHeaderChanges));

            if (better)
            {
                std::memcpy(bestBytes, m_bytes, byteCount);
                haveBest = true;
                bestStrength = strength;
                bestFlips = flips;
                bestCost = cost;
                bestHeaderChanges = headerChanges;
            }
        };

        const uint16_t residual = bcsResidual(byteCount);
        const std::vector<uint16_t>& deltas = crcBitDeltas(byteCount);

        if (m_config.m_syndromeCorrection && (residual != 0))
        {
            // Intrusive buckets avoid allocating and hashing a multimap for every failed
            // frame. Only the syndromes touched by the preceding frame are cleared.
            for (uint16_t syndrome : m_syndromeTouched) {
                m_syndromeHead[syndrome] = -1;
            }
            m_syndromeTouched.clear();

            for (int bit = 0; bit < nBits; bit++)
            {
                const uint16_t syndrome = deltas[bit];

                if (m_syndromeHead[syndrome] < 0) {
                    m_syndromeTouched.push_back(syndrome);
                }

                m_syndromeNext[bit] = m_syndromeHead[syndrome];
                m_syndromeHead[syndrome] = bit;
            }

            for (int bit = m_syndromeHead[residual]; bit >= 0; bit = m_syndromeNext[bit]) {
                consider(bit, 0, 1);
            }

            if (!(haveBest && (bestStrength == SemanticStrong) && (bestFlips == 1)))
            {
                for (int bitA = 0; bitA < nBits; bitA++)
                {
                    const uint16_t wanted = residual ^ deltas[bitA];

                    for (int bitB = m_syndromeHead[wanted]; bitB >= 0;
                         bitB = m_syndromeNext[bitB])
                    {
                        if (bitB > bitA) {
                            consider(bitA, bitB, 2);
                        }
                    }
                }
            }
        }

        // The complete 3/4-bit codeword search is far too large for a real-time decoder.
        // Instead enumerate combinations of a small soft/parity candidate list, reject
        // them algebraically by BCS syndrome and byte parity, and require strong ACARS
        // semantics before accepting, and every accepted repair must be both unique and
        // cheap against the block's mean bit reliability. Set m_listMaxFlips to 2 to
        // confine correction to the syndrome-enumerated one and two bit repairs.
        if (!haveBest && (residual != 0) && (maxFlips > 2))
        {
            // The list is bounded twice over. C(n,4) grows fast enough that a config or
            // command line value of a few tens would occupy the sample thread for seconds,
            // and unlike the one and two bit searches this one is not bounded by the BCS
            // syndrome. Cap the list length, then shorten it until the combination count
            // fits the correction budget - trimming up front rather than abandoning the
            // enumeration part way keeps the result independent of visiting order. At the
            // default 16 bits this changes nothing: 560 + 1820 combinations against 4096.
            int listCount = std::min(nc, std::max(0, listBits));
            listCount = std::min(listCount, ACARSOQPSK_MAX_LIST_BITS);

            {
                const int64_t allowed = ACARSOQPSK_MAX_LIST_COMBINATIONS;
                const int requested = listCount;

                while (listCount > 4)
                {
                    const int64_t n = listCount;
                    int64_t combinations = (n * (n-1) * (n-2)) / 6;

                    if (maxFlips >= 4) {
                        combinations += (n * (n-1) * (n-2) * (n-3)) / 24;
                    }
                    if (combinations <= allowed) {
                        break;
                    }

                    listCount--;
                }

                if (listCount < requested) {
                    m_stats.m_listTrimmed++;
                }
            }

            // A higher-order repair has to be cheap in absolute terms, not merely the
            // cheapest of the candidates that happened to satisfy the syndrome. A three or
            // four bit pattern drawn from soft rankings can always be found; what separates
            // a repair from an invention is whether the bits it flips were ones the
            // demodulator was unsure of. Referring the flip cost to the mean confidence over
            // the block makes that test independent of level and of block length.
            double meanAbsSoft = 0.0;

            if (m_config.m_listMargin > 0.0)
            {
                for (int i = 0; i < nBits; i++) {
                    meanAbsSoft += fabs(m_soft[firstBit + i]);
                }

                meanAbsSoft /= nBits;
            }

            // Every accepted higher-order repair has to be the only one. The one and two
            // bit searches are constrained by the syndrome and by character parity and have
            // measured no false decodes on 4459 recorded blocks; a three or four bit list
            // drawn from soft rankings is a much weaker hypothesis, and did invent a frame
            // at 8 dB. repairTerminator() already takes the same position on its own search.
            int higherCandidates = 0;

            auto considerHigher = [&](const int *bits, int flips)
            {
                if (flips == 3) {
                    m_stats.m_list3Tried++;
                } else {
                    m_stats.m_list4Tried++;
                }

                // At most four bits move, so the parity state follows from the flipped
                // bytes alone. Zeroing and then rescanning a block-sized array cost several
                // hundred operations per combination for a handful of bits, and this runs
                // millions of times in a list stress run.
                uint16_t syndrome = 0;
                int toggled[4];
                int toggledCount = 0;
                const int lastParityByte = byteCount - 4;

                for (int i = 0; i < flips; i++)
                {
                    syndrome ^= deltas[bits[i]];

                    const int byte = bits[i] / 8;

                    if (byte > lastParityByte) {
                        continue;
                    }

                    int at = -1;

                    for (int j = 0; j < toggledCount; j++)
                    {
                        if (toggled[j] == byte) { at = j; break; }
                    }

                    if (at >= 0) {
                        toggled[at] = toggled[--toggledCount];  // Two flips restore parity
                    } else {
                        toggled[toggledCount++] = byte;
                    }
                }

                if (syndrome != residual) return;
                if (toggledCount != (int) m_badParity.size()) return;

                for (int byte : m_badParity)
                {
                    bool found = false;

                    for (int j = 0; j < toggledCount; j++)
                    {
                        if (toggled[j] == byte) { found = true; break; }
                    }

                    if (!found) return;
                }

                double cost = 0.0;

                for (int i = 0; i < flips; i++) {
                    cost += fabs(m_soft[firstBit + bits[i]]);
                }

                // Cheaper than the BCS it would otherwise be offered to, so it goes first.
                if ((m_config.m_listMargin > 0.0)
                    && (cost > m_config.m_listMargin * flips * meanAbsSoft))
                {
                    m_stats.m_listMarginRejected++;
                    return;
                }

                std::memcpy(m_bytes, m_base, byteCount);
                int headerChanges = 0;

                for (int i = 0; i < flips; i++)
                {
                    const int bit = bits[i];
                    m_bytes[bit / 8] ^= (uint8_t)(1 << (bit % 8));
                    headerChanges += (bit / 8 <= 13) ? 1 : 0;
                }

                m_stats.m_correctionTries++;
                SemanticStrength strength;
                if (!validCorrectionCandidate(byteCount, strength)
                    || (strength != SemanticStrong)) {
                    return;
                }

                validCandidates++;
                higherCandidates++;
                const bool better = !haveBest
                    || (strength > bestStrength)
                    || ((strength == bestStrength) && (flips < bestFlips))
                    || ((strength == bestStrength) && (flips == bestFlips)
                        && (cost < bestCost))
                    || ((strength == bestStrength) && (flips == bestFlips)
                        && (cost == bestCost) && (headerChanges < bestHeaderChanges));

                if (better)
                {
                    std::memcpy(bestBytes, m_bytes, byteCount);
                    haveBest = true;
                    bestStrength = strength;
                    bestFlips = flips;
                    bestCost = cost;
                    bestHeaderChanges = headerChanges;
                }
            };

            int bits[4];
            for (int a = 0; a < listCount; a++)
            {
                bits[0] = m_correctBits[a];
                for (int b = a + 1; b < listCount; b++)
                {
                    bits[1] = m_correctBits[b];
                    for (int c = b + 1; c < listCount; c++)
                    {
                        bits[2] = m_correctBits[c];
                        considerHigher(bits, 3);

                        if (maxFlips >= 4)
                        {
                            for (int d = c + 1; d < listCount; d++)
                            {
                                bits[3] = m_correctBits[d];
                                considerHigher(bits, 4);
                            }
                        }
                    }
                }
            }

            if (higherCandidates > 1)
            {
                // m_correctionAmbiguous counts competition behind a frame that was accepted;
                // this one is not accepted at all, so it gets its own counter.
                m_stats.m_listAmbiguous++;
                haveBest = false;
            }
        }

        if (!m_config.m_syndromeCorrection || (residual == 0))
        {
            for (int a = 0; (a < nc) && (budget > 0); a++, budget--)
            {
                consider(m_correctBits[a], 0, 1);

                // One-bit candidates are visited in increasing soft cost. A strong match
                // cannot be beaten by a later one- or two-bit candidate under the ranking.
                if (haveBest && (bestStrength == SemanticStrong) && (bestFlips == 1)) {
                    break;
                }
            }

            if (!(haveBest && (bestStrength == SemanticStrong) && (bestFlips == 1)))
            {
                for (int a = 0; (a < nc) && (budget > 0); a++)
                {
                    for (int b = a + 1; (b < nc) && (budget > 0); b++, budget--) {
                        consider(m_correctBits[a], m_correctBits[b], 2);
                    }
                }
            }
        }

        if (haveBest)
        {
            std::memcpy(m_bytes, bestBytes, byteCount);
            m_stats.m_crcValid++;
            m_messageLength = byteCount;

            if (bestFlips == 1) {
                m_stats.m_corrected1++;
            } else if (bestFlips == 2) {
                m_stats.m_corrected2++;
            } else if (bestFlips == 3) {
                m_stats.m_corrected3++;
                m_stats.m_list3Recovered++;
            } else {
                m_stats.m_corrected4++;
                m_stats.m_list4Recovered++;
            }

            if (bestStrength == SemanticWeak) {
                m_stats.m_correctionWeakAccepted++;
            }
            if (validCandidates > 1) {
                m_stats.m_correctionAmbiguous++;
            }

            return true;
        }

        std::memcpy(m_bytes, m_base, byteCount);
        return false;
    }

    Config m_config;
    Stats m_stats;

    int m_sps = ACARSOQPSK_SAMPLES_PER_SYMBOL;

    // Channel baseband, long enough that a whole transmission is captured by the time its
    // pre-key reaches the detector
    std::vector<Cd> m_buf;
    std::vector<char> m_bufBlank;
    int m_bufLength = 0;
    int m_bufIdx = 0;
    int m_bufCnt = 0;
    int64_t m_absSample = 0;

    // Pre-key detector, all updated recursively
    Cd m_toneY, m_toneTwiddle, m_prod, m_bestProd, m_bestToneY;
    double m_envSum = 0.0;
    bool m_detectPrimed = false;
    int m_refreshCount = 0;
    int m_holdoff = 0;
    int m_pending = 0;
    double m_bestRatio = -1.0;
    int64_t m_bestStart = 0;
    double m_toneOffset = 0.0;
    double m_ratio = 0.0;
    bool m_detected = false;
    bool m_parallelArmed = true;

    // Impulse blanker
    double m_level = 0.0;
    double m_levelAlpha = 0.0;
    double m_carrierAlpha = 0.0;
    Cd m_lastGood, m_nbRot, m_gapPhasor;
    int64_t m_lastGoodAbs = 0;
    bool m_haveLastGood = false;
    bool m_inGap = false;
    bool m_levelPrimed = false;
    int m_levelCount = 0;
    Cd m_nbLine[ACARSOQPSK_NB_LINE];
    double m_nbEnv[ACARSOQPSK_NB_LINE];
    char m_nbHi[ACARSOQPSK_NB_LINE];
    char m_nbBad[ACARSOQPSK_NB_LINE];
    int m_nbWrite = 0;
    int64_t m_absIn = 0;
    bool m_outputBlanked = false;

    // Matched filter, with the subcarrier translation folded in
    std::vector<Cd> m_mf;
    Cd m_mfRotStep;

    // Burst working buffers, all reserved once in configure()
    std::vector<double> m_audio;
    std::vector<Cd> m_w;
    std::vector<double> m_soft;
    std::vector<double> m_softPos;
    std::vector<Cd> m_symbolRot;
    std::vector<double> m_refinedSoft;
    std::vector<double> m_bestRefinedSoft;
    std::vector<SyncCandidate> m_candidates;
    std::vector<std::pair<double, int>> m_ranked;
    std::vector<int> m_correctBits;
    std::vector<bool> m_chosen;
    std::vector<int> m_badParity;
    std::vector<std::vector<uint16_t>> m_crcDeltaCache;
    std::vector<int> m_syndromeHead;
    std::vector<int> m_syndromeNext;
    std::vector<uint16_t> m_syndromeTouched;
    int m_order[ACARSOQPSK_SAMPLES_PER_SYMBOL];
    int m_bitsConsumed = 0;
    double m_softOrigin = 0.0;
    SliceFailure m_lastSliceFailure = SliceComplete;
    int m_partialByteCount = 0;
    bool m_currentSohRepaired = false;
    int64_t m_currentBurstStart = 0;
    double m_currentBurstFrequency = 0.0;
    bool m_retryInProgress = false;
    bool m_refineInProgress = false;
    bool m_refineEligible = false;
    bool m_reconstructInProgress = false;
    bool m_suppressCorrection = false;  // First pass of the two-pass framing search
    bool m_secondPassWanted = false;    // ... and whether it passed up a repair worth redoing
    int m_burstBlanked = 0;             // Samples the blanker replaced inside this burst
    int64_t m_acceptedSohAbs = 0;       // ... and where the accepted frame's SOH was
    bool m_repairAvailable = false;     // ... specifically a repair, for the statistic
    std::vector<RetryCandidate> m_retries;
    std::vector<RecentFrame> m_recentFrames;

    int64_t m_diagBurstStart = 0;
    double m_diagDetectRatio = 0.0;
    double m_diagCarrierFrequency = 0.0;
    double m_diagToneOffset = 0.0;
    double m_diagSyncScore = 0.0;
    int m_diagSyncRank = 0;
    int m_diagSyncCandidates = 0;
    int m_diagTimingTry = 0;
    double m_diagInitialTau = 0.0;
    double m_diagAcquiredPhaseRate = 0.0;
    double m_diagFinalPhaseRate = 0.0;
    double m_diagPeakPhaseError = 0.0;
    double m_diagPhaseErrorSq = 0.0;
    uint64_t m_diagPhaseErrorCount = 0;
    double m_diagFinalPeriod = ACARSOQPSK_SAMPLES_PER_SYMBOL;
    double m_diagMinPeriod = ACARSOQPSK_SAMPLES_PER_SYMBOL;
    double m_diagMaxPeriod = ACARSOQPSK_SAMPLES_PER_SYMBOL;
    double m_diagPeakTimingError = 0.0;
    double m_diagTimingErrorSq = 0.0;
    uint64_t m_diagTimingErrorCount = 0;
    double m_diagPeakTimingWander = 0.0;
    double m_diagSignalAmplitude = 0.0;
    double m_diagNoiseVariance = 0.0;

    uint8_t m_bytes[ACARSOQPSK_MAX_BYTES + 8];
    uint8_t m_base[ACARSOQPSK_MAX_BYTES + 8];
    int m_messageLength = 0;
    FrameDiagnosticCallback m_frameDiagnosticCallback = nullptr;
    void *m_frameDiagnosticContext = nullptr;
    crc16itut m_crc;
    // One running CRC per bit position, built once so the delta table costs no LUT setup
    // and no allocation. Only valid because crc16itut initialises its register to zero.
    crc16itut m_deltaCrc[8];
};

#endif // INCLUDE_ACARSOQPSK_H
