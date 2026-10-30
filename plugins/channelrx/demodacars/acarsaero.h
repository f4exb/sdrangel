///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// The Inmarsat Classic Aero protocol parameters - the convolutional code and    //
// its polynomials, the interleaver row permutation, the scrambler polynomial    //
// and seed, the unique words, the frame and signal unit layouts, and the        //
// ISU/SSU reassembly rules - were established by reading JAERO by Jonti Olds,   //
// https://github.com/jontio/JAERO, MIT. The receiver below is an independent    //
// implementation: no code is taken from it.                                     //
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

#ifndef INCLUDE_ACARSAERO_H
#define INCLUDE_ACARSAERO_H

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "util/crc.h"

#include "acarshfdl.h"

// Inmarsat Classic Aero (aviation SATCOM) receiver, ICAO Annex 10 Volume III and
// ARINC 741. 
//
// Aero is the L band satellite protocol for ACARS, the complement to HFDL for oceanic
// aircraft. Three data rates are in scope:
//
//   600 bps    MSK,   600 symbols/s
//   1200 bps   MSK,  1200 symbols/s
//   10500 bps  OQPSK, 5250 symbols/s, 2 bits per symbol
//
// and three channel types:
//
//   P   continuous TDM from the ground station (uplink, GES to aircraft)
//   R   random access bursts from the aircraft (downlink)
//   T   reserved TDMA bursts from the aircraft (downlink)
//
// Unlike HFDL, where the M1 sequence identifies the rate, an Aero channel's rate and
// type are properties of the frequency tuned, so both are settings rather than
// something detected.
//
// P CHANNEL FRAME
//
//   UW (32 bits)          0xE15AE893, the same word at every MSK rate
//   header (16 bits)      uncoded, MSB first:
//                           15-12 format ID (1 on a valid frame)
//                           11-8  super frame marker
//                           7-4   frame counter
//                           3-0   frame counter, repeated
//   data (1152 bits)      coded, or 4992 bits at 10500 bps
//
// which is 1200 channel bits at 600 and 1200 bps - a 2.0 s frame at 600 bps and a
// 1.0 s frame at 1200 - and 5250 bits, a 0.5 s frame, at 10500 bps. Both copies of
// the frame counter are equal in a valid frame, which together with the format ID
// gives two cheap checks that a correlation was not a false alarm.
//
// RECEIVE CHAIN, in order:
//
//   1. matched filter (half sine over two symbol periods for MSK; RRC alpha 1.0 for
//      OQPSK, designed at the symbol rate)
//   2. deinterleave a block of 64 rows x N columns, rows permuted by (i*27) mod 64,
//      reading out[j*64 + i] = in[permute[i]*cols + j]. N is 6, 9 and 78 columns for
//      600, 1200 and 10500 bps, so a 600 bps frame is three blocks, 1200 bps is two
//      and 10500 bps is one
//   3. soft decision Viterbi, K=7 rate 1/2, polynomials 0x6D/0x4F - byte for byte the
//      polynomials HFDL uses, so AcarsHfdlReceiver::viterbiDecode is reused unchanged
//   4. descramble with an additive x^15 + x + 1 LFSR seeded 0x6959, restarted at every
//      frame boundary (and at the start of every burst on R and T)
//   5. pack bits into octets LSB first
//   6. split into signal units and check each CRC
//
// SIGNAL UNITS are 12 octets on P and T (10 of payload then a CRC) and 19 octets on R
// (17 then a CRC). The CRC is CRC-16/X-25 - poly 0x1021, init 0xFFFF, reflected, final
// XOR 0xFFFF - stored little endian in the last two octets. (JAERO's own comment calls
// this GENIBUS; the function it actually calls is the reflected variant, and GENIBUS,
// which it also implements, is never used.)
//
// Octet 0 of a signal unit is its type. Type 0x71 is a user data ISU carrying the AES
// and GES addresses, a queue/reference number pair, the number of SSUs to follow and
// the first two octets of user data; SSUs are typed 0xC0 | SEQNO with SEQNO counting
// down to zero and carry eight more octets each (fewer in the last, per a count in the
// ISU). Reassembled, the user data is FF FF followed by a complete ARINC 618 ACARS
// block - SOH, mode, registration, TAK, label, block id, STX/ETX, text, ETX or ETB,
// a two octet BCS and DEL, with odd parity in bit 7 of every octet except the two
// leading FFs and the BCS. Dropping the two FFs therefore gives exactly the byte
// layout AcarsMessage::decode expects, so nothing has to be synthesised.
//
// MSK DETAIL. Consecutive MSK symbols differ by exactly +-90 degrees, so multiplying
// a symbol by the conjugate of its predecessor and then by -j lands the result on the
// real axis, positive for a one. That is the derotate-to-BPSK view of MSK
// (acarsoqpsk.h:47-50 uses the same identity for VHF ACARS) with the per symbol 90
// degree rotation cancelled in the product, and it means that lane needs no carrier
// recovery at all.
//
// TWO LANES run over every frame and the signal unit CRC arbitrates between them, per
// unit. The differential lane above is the one that always works. The coherent lane
// tracks the carrier with a decision-directed loop seeded from the unique word - a known
// sequence, so a free 33 symbol training burst - and recovers the several dB that
// squaring the noise costs. Neither can lose a unit the other would have got.
//
// Unlike VHF ACARS, whose differential encoding cancels MSK's precoding so its arms
// carry data bits directly, Aero does not pre-encode, so a differential decode still
// follows coherent detection. The two coherent projections are combined with min-sum,
// the standard approximation to the exclusive-or of two log-likelihoods, rather than
// with a raw product that would let one confident symbol inflate an unreliable one.
//
// FRAME TRACKING. A P channel is continuous TDM, so once a frame has been decoded the
// next unique word is due at a known instant. The receiver confirms it inside a half
// symbol window there rather than searching, which both removes almost every false
// correlation and re-locks the symbol timing every frame.
//
// The polarity matters and is easy to get backwards: a one is when the phase does NOT
// reverse. The acquisition correlator cannot catch the mistake, because it correlates
// transitions and takes a magnitude, so an inverted convention correlates just as
// strongly against the unique word and then decodes to nothing at all.
//
// WHAT IS AND IS NOT IMPLEMENTED
//
//   600 and 1200 bps, P channel   implemented and verified against a real recording
//   600 and 1200 bps, T bursts    implemented and verified against a real recording
//   600 and 1200 bps, R bursts    implemented, works end to end, but no recording
//                                 containing R bursts exists to verify it against
//   10500 bps OQPSK               not implemented, rateSupported() returns false
//

#define ACARSAERO_MSK_CHANNEL_SAMPLE_RATE   19200
#define ACARSAERO_OQPSK_CHANNEL_SAMPLE_RATE 105000

#define ACARSAERO_UW        0xE15AE893u
#define ACARSAERO_UW_LEN    32
#define ACARSAERO_HEADER_LEN 16
// How far the per-frame timing refinement may move the OQPSK symbol clock phase, in
// samples, before it is treated as the half-symbol alias rather than as ordinary drift
#define ACARSAERO_TIMING_PULL 3
// Consecutive frames that must decode NOTHING before a symbol clock phase which has already
// been confirmed by a good frame is abandoned for the other half-symbol candidate
#define ACARSAERO_PHASE_FAILURES 3

#define ACARSAERO_INTERLEAVER_ROWS     64
#define ACARSAERO_INTERLEAVER_ROW_STEP 27
#define ACARSAERO_SCRAMBLER_SEED       0x6959

#define ACARSAERO_SU_LEN    12      // P and T channel signal unit, including its CRC
#define ACARSAERO_R_SU_LEN  19      // R channel signal unit, including its CRC
#define ACARSAERO_SU_CRC_LEN 2

// R bursts are five interleaver columns; a T burst is five for its header then three
// per signal unit. 95 columns is the largest JAERO ever allocates.
#define ACARSAERO_BURST_HEAD_COLS 5
#define ACARSAERO_BURST_SU_COLS   3
#define ACARSAERO_BURST_MAX_COLS  95
#define ACARSAERO_T_HEADER_LEN    6 // 4 octets plus a CRC, checked on its own

// Signal units in the largest frame (10500 bps carries 26), for the per-position stats
#define ACARSAERO_MAX_SUS         26

class AcarsAeroReceiver
{
public:
    typedef std::complex<double> Cd;

    enum Rate
    {
        Rate600 = 0,
        Rate1200,
        Rate10500,
        RateCount
    };

    enum ChannelType
    {
        ChannelP = 0,   //!< Continuous TDM from the ground station
        ChannelR,       //!< Random access bursts from the aircraft
        ChannelT        //!< Reserved TDMA bursts from the aircraft
    };

    // The rate and channel combinations that exist on air, as one setting. Only the P
    // channel runs at 10500 bps; R and T are the aircraft's low rate return links.
    // Appending to this list is safe, reordering it is not - the value is serialised.
    enum Submode
    {
        Submode600P = 0,
        Submode1200P,
        Submode10500P,
        Submode600R,
        Submode1200R,
        Submode600T,
        Submode1200T,
        SubmodeCount
    };

    static int submodeRate(int submode)
    {
        switch (submode)
        {
        case Submode1200P:
        case Submode1200R:
        case Submode1200T: return Rate1200;
        case Submode10500P: return Rate10500;
        default: return Rate600;
        }
    }

    static int submodeChannel(int submode)
    {
        switch (submode)
        {
        case Submode600R:
        case Submode1200R: return ChannelR;
        case Submode600T:
        case Submode1200T: return ChannelT;
        default: return ChannelP;
        }
    }

    static const char *submodeName(int submode)
    {
        switch (submode)
        {
        case Submode600P: return "600 bps P";
        case Submode1200P: return "1200 bps P";
        case Submode10500P: return "10500 bps P";
        case Submode600R: return "600 bps R";
        case Submode1200R: return "1200 bps R";
        case Submode600T: return "600 bps T";
        case Submode1200T: return "1200 bps T";
        default: return "?";
        }
    }

    // A sensible channel filter width for a submode, in Hz - what the RF bandwidth
    // control should default to, not the occupied bandwidth.
    //
    // MSK occupies about 1.2 times the bit rate with its main lobe out to 0.75 times
    // it, and OQPSK with roll-off 1.0 occupies twice the symbol rate. The sink sets the
    // interpolator cutoff to bandwidth/2.2, so a default equal to the occupied
    // bandwidth would put the cutoff inside the main lobe; twice the bit rate (and 2.5
    // times the symbol rate for OQPSK) leaves the passband clear with a little margin
    // for tuning error. Being slightly wide costs almost nothing here - the matched
    // filter does the selectivity that matters.
    static int submodeBandwidth(int submode)
    {
        int rate = submodeRate(submode);
        return isOqpsk(rate) ? (5 * symbolRate(rate) / 2) : (2 * bitRate(rate));
    }

    struct Config
    {
        // Normalised differential correlation against the 32 bit unique word needed to
        // declare sync, 0 to 1. 32 products, so noise reads around 0.15; a genuine
        // unique word on a clean satellite downlink reads over 0.9.
        //
        // Measured, and the optimum is much lower than it looks like it should be.
        // Sweeping 0.40 to 0.60 over 5, 6 and 7 dB gives 487, 545, 550, 518, 385, 183
        // of 600 at 0.40/0.44/0.46/0.50/0.55/0.60 - a factor of three between 0.46 and
        // 0.60. Below 0.44 it turns over again, because a false correlation costs a
        // whole frame of deaf time and that starts to outweigh the extra genuine ones.
        // But that sweep was on generated AWGN, and lowering it REGRESSED the real
        // recording - 5 messages against 6, and 163 of 222 signal units against 185 -
        // because off air there is an adjacent channel 5 kHz away and false
        // correlations are commoner than clean noise makes them look. The recording is
        // the ground truth, so this stays where it was and the gain is taken instead by
        // tracking the frame, below, which removes the false syncs rather than trading
        // against them.
        double m_syncThreshold = 0.55;

        // Correlation required to confirm the unique word at the START OF THE NEXT
        // FRAME, once a frame has been decoded. A P channel is continuous TDM, so the
        // next unique word is exactly one frame away - there is no need to search for
        // it, only to confirm it where it must be.
        //
        // That is the whole point: a false correlation can only be accepted if it lands
        // inside a half symbol window at a known instant, which is a vanishingly small
        // target compared with free running search. So this can sit far below
        // m_syncThreshold and hold lock through a fade that free search would lose,
        // without ever letting a data pattern masquerade as a frame boundary.
        double m_trackThreshold = 0.30;
        // Frames whose unique word fails m_trackThreshold before the receiver gives up
        // tracking and searches the whole stream again
        int m_maxMissedFrames = 2;
        // Run a SECOND, coherent lane alongside the differential one and let each
        // signal unit's CRC decide which lane's octets to keep.
        //
        // Differential detection needs no carrier recovery at all, which is why it was
        // built first and why it stays: it is what holds the receiver up when the
        // carrier loop has not converged or has slipped. But multiplying two noisy
        // symbols together squares the noise, and that costs several dB at the error
        // rates that matter. The coherent lane tracks the carrier with a
        // decision-directed loop seeded from the unique word - which is a known
        // sequence, so the reference starts from a 33 symbol data-aided estimate rather
        // than from nothing - and recovers most of it.
        //
        // A 180 degree hangup in that loop is harmless here, because the differential
        // decode that follows cancels any constant sign. Only a slip mid-frame hurts,
        // and that is what the arbitration covers.
        // Carry the symbol timing slip measured at one unique word across the frame that
        // follows it. See measureTimingSlip().
        bool m_timingSlope = true;
        // How much of each slip measurement to believe. The measurement is the position
        // of a correlation peak in noise, so at low SNR most of what it reports is noise
        // and feeding that forward walks the sampling instant off a signal that had no
        // slip to correct. Swept against both: on a live capture that was losing samples
        // 0.05/0.10/0.25/0.50 gave 1402/1425/1452/1432 signal units against 1250 with it
        // off, while on generated signal at 3 dB - where there is nothing to correct -
        // they cost 0/1/5/12 messages of 240. 0.10 takes almost all of the gain for
        // nothing measurable.
        double m_timingSlopeGain = 0.10;
        bool m_coherent = true;
        // Smooth the coherent lane's carrier reference over the frame before detecting
        // again. See refitCoherent.
        bool m_smooth = true;
        // Half width of the centred window, symbols. Measured: 2/4/8/16/24/32 either
        // side give 279/288/291/301/292/292 of 480 against 289 unsmoothed, so the
        // optimum is broad and shallow around 16. Over 1200 trials it is 739 against
        // 705 - real, about 5% relative, but a tenth of a dB rather than the whole
        // remaining gap. Longer windows lose because the loop's reference is genuinely
        // following something, not just wandering.
        int m_smoothHalf = 16;
        // Both measured, and the surface around them is flat - anything in
        // refGain 0.25 to 0.35 with freqGain 0.005 to 0.01 lands within 2% of the best,
        // so neither is delicately tuned. The starting guesses of 0.10 and 0.02 cost
        // about 15%, which is worth knowing: the reference wants to follow the carrier
        // faster than instinct suggests, and the frequency loop slower.
        double m_freqGain = 0.01;       // Decision-directed frequency tracking, rad/symbol
        double m_refGain = 0.30;        // Reference phasor blend per symbol

        // The OQPSK rate wants very different loop gains from the MSK rates, measured
        // rather than assumed: about 0.001 and 0.05 against 0.01 and 0.30. Its symbol
        // rate is nine times higher and its frames are five times longer in symbols, so
        // the carrier moves far less per symbol and there is far more to average over -
        // and without a frequency integrator at all it drifts badly, taking the signal
        // unit CRC rate from 96% over four frames to 19% over a hundred.
        // The OQPSK correlator is a different statistic from the MSK one - 31 products
        // over 32 symbols rather than 32 over 33, on a wider pulse - so it keeps its own
        // threshold even though it currently matches the MSK value.
        //
        // It was 0.45 for a long time, from an early measurement that 0.55 would not sync
        // the recording at all. That stopped being true once the symbol timing was fixed,
        // and 0.45 then measured WORSE than 0.55 in both directions - 27 messages against
        // 30 on the recording, and 52% against 63% at 6 dB Eb/N0 - because a threshold
        // low enough to admit a marginal correlation starts a frame on a bad instant, and
        // with the phase lock a bad acquisition now costs more than a missed one. 0.55 is
        // the knee: everything from 0.55 to 0.70 measures identically, everything below
        // degrades, so this is the lowest value that gives full performance.
        //
        // The stale 0.45 was invisible because the harness was overriding it with the MSK
        // flag's 0.55 - so the plugin shipped worse than anything ever measured it. See
        // test/README.md.
        double m_oqpskSyncThreshold = 0.55;
        // Retuned against the message error curve rather than against a strong recording,
        // which cannot discriminate: at 6 dB Eb/N0 per channel bit these take the rate
        // from 19% of messages to 66%, while leaving the recording at its best (99.2% of
        // signal units either way). The earlier 0.001 / 0.05 were picked on the recording
        // alone and are a little too slow to hold the carrier once noise is in the loop.
        double m_oqpskFreqGain = 0.002;
        double m_oqpskRefGain = 0.10;

        // A second pass that re-estimates the carrier over the whole frame was built and
        // removed. It is recorded in test/README.md rather than here, because the
        // negative result is worth more than the code was: forced to the correct carrier
        // frequency it beat the tracking loop outright (359 messages against 289), but
        // nothing available estimates that frequency well enough to extrapolate over a
        // frame, and every principled way of avoiding the need to - squaring, decision
        // stripping, the loop's own integrator, block correction, non-causal smoothing -
        // measured worse than simply tracking. The reason is that after derotation the
        // imaginary axis of an MSK symbol carries the orthogonal arm's crosstalk, which
        // is large and data dependent, so any estimator touching the full complex value
        // is corrupted by it.
        // How far the 16 bit frame header may be from a legal one before the frame is
        // abandoned, in bits. 8 accepts anything; 0 demands an exact match.
        //
        // This is the single most valuable number in the receiver and it was measured,
        // not guessed. The header is UNCODED and hard decided, while the data field
        // behind it is rate 1/2 convolutionally coded and interleaved - so the header is
        // by far the weakest thing in the frame. Demanding an exact match threw away
        // 17 of 65 frames at 8 dB whose every signal unit then decoded with a perfect
        // CRC, costing 3 to 4 dB. Accepting anything is worse again in a different way:
        // a false correlation then burns a whole frame before the receiver looks again,
        // and the real frame boundary behind it is missed.
        //
        // Three is where the two costs cross, measured both ways. Going 0, 1, 2, 3, 4, 8
        // at 6 and 7 dB the yield runs 72, 187, 232, 240, 238, 236 of 320 - rising
        // steeply then flat. On a clean signal, where false correlations on data
        // patterns are what bite, 1, 2 and 3 all give a perfect 240 of 240 while 4 and 8
        // lose six. So 3 takes the whole of the low SNR gain and stops exactly before
        // the false syncs start. Confirmed on a wider run: 626 against 603 of 960 for 2.
        //
        // The header was never the real gate in any case. Every signal unit carries its
        // own CRC-16, which discriminates far harder than 8 bits of header redundancy;
        // this check exists to stop a false sync wasting a whole frame of deaf time, not
        // to keep garbage out of the output.
        int m_headerMaxErrors = 3;
        //!< How strongly the unique word has to correlate before the frame header is
        //!< trusted rather than checked, and what is allowed then. The header is 16 bits,
        //!< so 16 is no check at all - see the note in consumeSoftBit().
        //!< Carry the converged rotation from one frame into the next. Added when the
        //!< unique word seed was poor; kept configurable so it can be measured against
        //!< the seed now that the seed is good - see seedOqpsk().
        bool m_carryDphi = true;
        //!< Rotate the seeded carrier reference by this many quarter turns. For OQPSK a
        //!< 90 degree error swaps the arms, which is indistinguishable from starting on
        //!< the wrong symbol clock phase - both give a frame that decodes nothing at all.
        //!< Diagnostic: if the frames that die are dying of a quadrant error, forcing the
        //!< quadrant swaps which offsets work.
        int m_seedQuadrant = 0;
        //!< Print the carrier loop's state through the first few frames, to stderr.
        //!< Diagnostic only - see the note on the tuning cliff in test/README.md.
        bool m_traceLoop = false;
        double m_headerTrustCorrelation = 0.85;
        int m_headerMaxErrorsTrusted = 16;
        // Accept signal units that are entirely zero with a zero CRC as fill. JAERO
        // does this, and real P channels do transmit them.
        bool m_acceptZeroFill = true;
        // Emit a Frame for signal units that are not ACARS (log-on, channel control,
        // acknowledgements ...) as well as for reassembled ACARS blocks
        bool m_emitNonAcars = true;
        // Seconds of receiver time after which a partially reassembled ISU/SSU chain
        // is abandoned. A 22 SSU chain at 600 bps spans several frames, so this has to
        // be generous, but it must be bounded or a lost final SSU leaks the chain.
        double m_reassemblyTimeout = 120.0;
        // Hard cap on partial reassemblies held at once, per channel
        int m_maxReassemblies = 64;
    };

    struct Stats
    {
        uint64_t m_samples = 0;
        uint64_t m_syncs = 0;           //!< Unique word correlations that started a frame
        double m_uwPeak = 0.0;          //!< Largest unique word correlation seen
        uint64_t m_framesDecoded = 0;
        uint64_t m_headerBad = 0;       //!< Headers not exactly legal, whether or not accepted
        //!< OQPSK arm selection: how often a frame confirmed the symbol clock phase by
        //!< decoding something, and how often one decoded nothing and the phase was
        //!< flipped to the other candidate half a symbol away
        uint64_t m_phaseConfirms = 0;
        uint64_t m_phaseFlips = 0;
        uint64_t m_phaseDeadFrames = 0;
        //!< How good the carrier seed from the unique word is: the seed the frame started
        //!< with against the rotation the loop had converged on by the end of it, summed
        //!< over frames that decoded. Radians per symbol; the harness converts to Hz.
        double m_seedErrorSum = 0.0;
        double m_seedErrorMax = 0.0;
        double m_convergedSum = 0.0;
        uint64_t m_seedCount = 0;
        //!< Frames where the seed came from the unique word rather than the carry, split
        //!< by whether they went on to decode anything
        uint64_t m_seededFromUw = 0;
        uint64_t m_seededFromUwOk = 0;
        //!< Frames by how many of their 26 signal units passed: none, some, most, all.
        //!< Whether a bad frame is bad outright or merely noisy says whether the loss is
        //!< a lock going wrong or ordinary noise.
        uint64_t m_frameYield[4] = {0, 0, 0, 0};
        //!< Which symbol clock phase the frames that decoded everything started on, and
        //!< which the frames that decoded nothing did. If the dead frames sit half a
        //!< symbol away from the live ones then the arm really is alternating; if they
        //!< sit on the same phase then whatever kills them is not the timing.
        uint64_t m_phaseHistOk[32] = {0};
        uint64_t m_phaseHistDead[32] = {0};
        uint64_t m_headerRejected = 0;  //!< ... and those too far off to keep the frame
        uint64_t m_uwTracked = 0;       //!< Frames whose unique word was confirmed where predicted
        uint64_t m_uwLost = 0;          //!< Tracking gave up and returned to search
        uint64_t m_suCrcOk = 0;
        uint64_t m_suCrcBad = 0;
        uint64_t m_suFromDifferential = 0;  //!< Signal units the differential lane won
        uint64_t m_suFromCoherent = 0;      //!< ... and the coherent lane
        // Signal unit CRC results by position within the frame. The convolutional code
        // is not flushed at the end of a frame, so the last unit is expected to be the
        // weakest; this is what says by how much.
        uint64_t m_suOkByPos[ACARSAERO_MAX_SUS] = {0};
        uint64_t m_suBadByPos[ACARSAERO_MAX_SUS] = {0};
        uint64_t m_suFill = 0;          //!< Fill-in signal units (type 0x01 or all zero)
        uint64_t m_isus = 0;            //!< User data ISUs seen
        uint64_t m_ssus = 0;
        uint64_t m_ssuOrphan = 0;       //!< SSU with no matching ISU
        uint64_t m_reassembled = 0;     //!< Complete user data blocks
        uint64_t m_acarsFrames = 0;
        uint64_t m_otherFrames = 0;
        uint64_t m_bursts = 0;          //!< R/T bursts that passed their CRCs
        uint64_t m_burstsBad = 0;
        uint64_t m_reassemblyTimeouts = 0;
    };

    // One decoded item: either a reassembled ACARS block or a single non-ACARS signal unit
    struct Frame
    {
        std::vector<uint8_t> m_bytes;   //!< ACARS block from its SOH, or the raw signal unit
        std::string m_type;             //!< "User data ISU", "Log-on request", ...
        bool m_uplink = false;          //!< True on the P channel (ground to air)
        uint32_t m_aesId = 0;           //!< Aircraft Earth Station id, the ICAO 24 bit address
        uint8_t m_gesId = 0;            //!< Ground Earth Station id
        bool m_isAcars = false;
        int m_bitRate = 0;              //!< 600, 1200 or 10500
        int m_channel = ChannelP;
    };

    AcarsAeroReceiver()
    {
        setMode(Rate600, ChannelP);
    }

    // Every rate in scope is implemented. Kept as a hook because the R and T burst
    // paths still do not decode real bursts, and because a rate could be disabled here
    // rather than silently receiving nothing.
    static bool rateSupported(int rate) { (void) rate; return true; }

    // Rate and channel type are settings, not detected. Reconfigures and resets.
    void setMode(int rate, int channel)
    {
        m_rate = (rate >= 0) && (rate < RateCount) ? rate : (int) Rate600;
        m_channel = (channel >= ChannelP) && (channel <= ChannelT) ? channel : (int) ChannelP;
        m_sps = samplesPerSymbol(m_rate);
        designFilter();
        reset();
    }

    void configure(const Config& config)
    {
        m_config = config;
        reset();
    }

    const Config& config() const { return m_config; }
    const Stats& stats() const { return m_stats; }

    //! RMS error vector magnitude as a fraction, or a negative value before the coherent
    //! lane has seen anything. Only meaningful on a continuous P channel
    double evm() const { return (m_evmMeanSq > 0.0) ? std::sqrt(m_evmMeanSq) : -1.0; }
    //! Fraction of recent signal units whose CRC passed, negative before the first one
    double suRate() const { return m_suRate; }
    int rate() const { return m_rate; }
    int channelType() const { return m_channel; }

    void reset()
    {
        m_lockedPhase = -1;
        m_phaseConfirmed = false;
        m_phaseFailures = 0;
        m_haveCarriedDphi = false;
        m_state = StateSearch;
        m_ring.assign(ringLength(), Cd(0.0, 0.0));
        m_ringIdx = 0;
        m_ringFill = 0;
        m_sclk = 0;
        m_symPeriod = m_sps;
        m_timingAcc = 0.0;
        m_timingRate = 0.0;
        m_symbolCount = 0;
        m_prevSym = Cd(0.0, 0.0);
        m_dphi = 0.0;
        m_lastSyncError = 0.0;
        m_magMean = 0.0;
        m_trackWait = 0;
        m_missedFrames = 0;
        m_magMeanCoh = 0.0;
        m_symBuf.clear();
        m_refPhase.clear();
        m_refPhaseAcc = 0.0;
        m_projPrevFirst = 0.0;
        m_oqpskInit = false;
        m_softBitsCoh.clear();
        m_ref = Cd(1.0, 0.0);
        m_evmMeanSq = 0.0;
        m_suRate = -1.0;
        m_dphiSym = 0.0;
        m_projPrev = 0.0;
        m_cohValid = false;
        m_syncPending = false;
        m_bestStat = 0.0;
        m_bestAgo = 0;
        m_bestSum = Cd(0.0, 0.0);
        m_softBits.clear();
        m_headerBits.clear();
        m_burstBits.clear();
        m_burstCols = 0;
        m_frames.clear();
        m_reassembly.clear();
        m_time = 0.0;
    }

    //! Nothing is held back, so this is a no-op. Kept because the harness and the sink
    //! call it at the end of a stream and a future receiver may need it again.
    void flush() {}

    bool hasFrame() const { return !m_frames.empty(); }

    Frame popFrame()
    {
        Frame f = m_frames.front();
        m_frames.pop_front();
        return f;
    }

    double lastSyncError() const { return m_lastSyncError; }
    bool synced() const { return m_state != StateSearch; }

    // The rate the demodulator for a given Aero rate runs at
    static int channelSampleRate(int rate)
    {
        return (rate == Rate10500) ? ACARSAERO_OQPSK_CHANNEL_SAMPLE_RATE
                                   : ACARSAERO_MSK_CHANNEL_SAMPLE_RATE;
    }

    static int bitRate(int rate)
    {
        switch (rate)
        {
        case Rate1200: return 1200;
        case Rate10500: return 10500;
        default: return 600;
        }
    }

    // Symbol rate. MSK carries one bit per symbol so it equals the bit rate; OQPSK
    // carries two, so it is half.
    static int symbolRate(int rate)
    {
        return (rate == Rate10500) ? 5250 : bitRate(rate);
    }

    static int samplesPerSymbol(int rate)
    {
        return channelSampleRate(rate) / symbolRate(rate);
    }

    static bool isOqpsk(int rate) { return rate == Rate10500; }

    // ---------------------------------------------------------------------------------
    // Frame geometry
    // ---------------------------------------------------------------------------------

    // Interleaver columns per block. A frame is one or more whole blocks.
    static int interleaverCols(int rate)
    {
        switch (rate)
        {
        case Rate1200: return 9;
        case Rate10500: return 78;
        default: return 6;
        }
    }

    // Coded bits in a frame's data field
    static int codedBitsPerFrame(int rate)
    {
        return (rate == Rate10500) ? 4992 : 1152;
    }

    // Dummy bits between the header and the data field. Only 10500 bps has any; JAERO
    // names them but does not explain them, so they are simply skipped.
    static int dummyBits(int rate)
    {
        return (rate == Rate10500) ? 178 : 0;
    }

    static int dataBitsPerFrame(int rate) { return codedBitsPerFrame(rate) / 2; }
    static int susPerFrame(int rate) { return dataBitsPerFrame(rate) / 8 / ACARSAERO_SU_LEN; }
    static int blocksPerFrame(int rate)
    {
        return codedBitsPerFrame(rate) / (ACARSAERO_INTERLEAVER_ROWS * interleaverCols(rate));
    }

    // ---------------------------------------------------------------------------------
    // Protocol primitives. Public so the harness can test each in isolation.
    // ---------------------------------------------------------------------------------

    static uint32_t uniqueWord() { return ACARSAERO_UW; }

    static int uwBit(int i) { return (ACARSAERO_UW >> (ACARSAERO_UW_LEN - 1 - i)) & 1; }

    // Scrambler: additive LFSR x^15 + x + 1 seeded 0x6959, restarted at every frame
    // boundary. HFDL uses the same polynomial and seed (its lfsrNext shifts the other
    // way with the seed bits reversed, which is the same sequence), so this is the
    // identical generator applied over a longer run.
    static uint8_t scramblerBit(int index)
    {
        static const std::vector<uint8_t> seq = []()
        {
            std::vector<uint8_t> s(MAX_SCRAMBLER);
            uint16_t lfsr = ACARSAERO_SCRAMBLER_SEED;
            for (int i = 0; i < MAX_SCRAMBLER; i++)
            {
                uint8_t bit = (uint8_t) (((lfsr >> 0) ^ (lfsr >> 14)) & 1);
                lfsr = (uint16_t) ((lfsr >> 1) | (bit << 14));
                s[i] = bit;
            }
            return s;
        }();
        return seq[index % MAX_SCRAMBLER];
    }

    // Deinterleaver permutation for a block of 64 rows by cols columns: srcPos[k] is
    // where the k'th output (Viterbi input order) bit sits in the received block.
    // Computed once per column count and cached.
    static const std::vector<int>& deinterleavePos(int cols)
    {
        static std::map<int, std::vector<int>> cache;
        static const std::array<int, ACARSAERO_INTERLEAVER_ROWS> permute = []()
        {
            std::array<int, ACARSAERO_INTERLEAVER_ROWS> p;
            for (int i = 0; i < ACARSAERO_INTERLEAVER_ROWS; i++) {
                p[i] = (i * ACARSAERO_INTERLEAVER_ROW_STEP) % ACARSAERO_INTERLEAVER_ROWS;
            }
            return p;
        }();

        auto it = cache.find(cols);
        if (it != cache.end()) {
            return it->second;
        }

        std::vector<int> pos((size_t) ACARSAERO_INTERLEAVER_ROWS * cols);
        int k = 0;
        for (int j = 0; j < cols; j++)
        {
            for (int i = 0; i < ACARSAERO_INTERLEAVER_ROWS; i++) {
                pos[k++] = permute[i] * cols + j;
            }
        }
        return cache.emplace(cols, std::move(pos)).first->second;
    }

    template<typename T>
    static void deinterleave(const T *in, int cols, T *out)
    {
        const std::vector<int>& pos = deinterleavePos(cols);
        for (size_t k = 0; k < pos.size(); k++) {
            out[k] = in[pos[k]];
        }
    }

    // MSK bursts are deinterleaved in GROUPS, not as one matrix: the first five
    // columns are a 64x5 block of their own, then every following group of three
    // columns is its own 64x3 block (JAERO AeroLInterleaver::deinterleaveMSK_ba).
    // The OQPSK burst path treats the whole burst as a single 64xN matrix instead, so
    // the two must not be conflated.
    template<typename T>
    static void deinterleaveBurstMsk(const T *in, int cols, T *out)
    {
        const int rows = ACARSAERO_INTERLEAVER_ROWS;
        deinterleave(in, ACARSAERO_BURST_HEAD_COLS, out);
        int done = ACARSAERO_BURST_HEAD_COLS;
        while (done + ACARSAERO_BURST_SU_COLS <= cols)
        {
            deinterleave(in + (size_t) done * rows, ACARSAERO_BURST_SU_COLS,
                         out + (size_t) done * rows);
            done += ACARSAERO_BURST_SU_COLS;
        }
    }

    // Interleave a burst the way deinterleaveBurstMsk() undoes it: the first five
    // columns as one block, then groups of three
    template<typename T>
    static void interleaveBurstMsk(const T *in, int cols, T *out)
    {
        const int rows = ACARSAERO_INTERLEAVER_ROWS;
        interleave(in, ACARSAERO_BURST_HEAD_COLS, out);
        int done = ACARSAERO_BURST_HEAD_COLS;
        while (done + ACARSAERO_BURST_SU_COLS <= cols)
        {
            interleave(in + (size_t) done * rows, ACARSAERO_BURST_SU_COLS,
                       out + (size_t) done * rows);
            done += ACARSAERO_BURST_SU_COLS;
        }
    }

    template<typename T>
    static void interleave(const T *in, int cols, T *out)
    {
        const std::vector<int>& pos = deinterleavePos(cols);
        for (size_t k = 0; k < pos.size(); k++) {
            out[pos[k]] = in[k];
        }
    }

    // CRC-16/X-25 over the payload octets of a signal unit
    static uint16_t computeCrc(const uint8_t *buf, int len)
    {
        crc16x25 crc;
        crc.init();
        crc.calculate(const_cast<uint8_t *>(buf), len);
        return (uint16_t) crc.get();
    }

    // A signal unit's CRC sits in its last two octets, little endian
    static bool suCrcOk(const uint8_t *su, int len)
    {
        uint16_t received = (uint16_t) (su[len - 1] << 8) | su[len - 2];
        return computeCrc(su, len - ACARSAERO_SU_CRC_LEN) == received;
    }

    static void appendCrc(std::vector<uint8_t>& su)
    {
        uint16_t crc = computeCrc(su.data(), (int) su.size());
        su.push_back((uint8_t) (crc & 0xFF));
        su.push_back((uint8_t) ((crc >> 8) & 0xFF));
    }

    static bool allZero(const uint8_t *buf, int len)
    {
        for (int i = 0; i < len; i++)
        {
            if (buf[i]) {
                return false;
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------------
    // Signal unit types (JAERO aerol.h, namespaces AEROTypeP/R/T)
    // ---------------------------------------------------------------------------------

    enum SuType
    {
        SuFill                      = 0x01,
        SuSystemTableSmc            = 0x05,
        SuSystemTableBeam           = 0x07,
        SuSystemTableIndex          = 0x0A,
        SuSystemTableSatellite      = 0x0C,
        SuLogOnRequest              = 0x10,
        SuLogOnConfirm              = 0x11,
        SuLogOffRequest             = 0x12,
        SuLogOnReject               = 0x13,
        SuLogOnInterrogation        = 0x14,
        SuLogOnOffAcknowledge       = 0x15,
        SuLogOnPrompt               = 0x16,
        SuDataChannelReassignment   = 0x17,
        // Reserved in the SDM rather than unrecognised by us. They do appear on air -
        // JAERO names them the same way (aerol.h, Reserved_18/19/26)
        SuReserved18                = 0x18,
        SuReserved19                = 0x19,
        SuReserved26                = 0x26,
        SuCallAnnouncement          = 0x21,
        SuAccessRequestData         = 0x22,
        SuEirpTable                 = 0x28,
        SuCallProgress              = 0x30,
        SuCChannelDistress          = 0x31,
        SuCChannelFlightSafety      = 0x32,
        SuCChannelOtherSafety       = 0x33,
        SuCChannelNonSafety         = 0x34,
        SuPRChannelControl          = 0x40,
        SuTChannelControl           = 0x41,
        SuTChannelAssignment        = 0x51,
        SuTelephonyAck              = 0x60,
        SuRequestAcknowledgement    = 0x61,
        SuAcknowledge               = 0x62,
        SuUserDataIsu               = 0x71,
        SuUserData3Octet            = 0x74,
        SuUserData4Octet            = 0x76
    };

    static bool isSsu(uint8_t type) { return (type & 0xC0) == 0xC0; }

    // Which Inmarsat satellite and ocean region a Ground Earth Station id belongs to.
    // https://acars-vdl2.groups.io/g/main/topic/frequencies_in_use_inmarsat/112254777
    // Appears valid for AOR-E
    static const char *gesName(uint8_t id)
    {
        switch (id)
        {
        case 0x43: case 0x44:
            return "AOR-E";
        case 0x90: case 0xC1: case 0xC5:
            return "IOR";
        case 0x02: case 0x05: case 0xD0:
            return "AOR-W";
        case 0x50: case 0x82: case 0x85:
            return "POR";
        default:
            return nullptr;
        }
    }

    static bool suIsNoInfo(uint8_t type)
    {
        switch (type)
        {
        case SuFill:
        case SuSystemTableSmc:
        case SuSystemTableBeam:
        case SuSystemTableIndex:
        case SuSystemTableSatellite:
        case SuEirpTable:
        case SuReserved18:
        case SuReserved19:
        case SuReserved26:
            return true;
        default:
            return false;
        }
    }

    static bool suHasAesId(uint8_t type)
    {
        if (isSsu(type)) {
            return true;
        }
        switch (type)
        {
        case SuLogOnRequest:
        case SuLogOnConfirm:
        case SuLogOffRequest:
        case SuLogOnReject:
        case SuLogOnInterrogation:
        case SuLogOnOffAcknowledge:
        case SuLogOnPrompt:
        case SuDataChannelReassignment:
        case SuCallAnnouncement:
        case SuAccessRequestData:
        case SuCallProgress:
        case SuCChannelDistress:
        case SuCChannelFlightSafety:
        case SuCChannelOtherSafety:
        case SuCChannelNonSafety:
        case SuPRChannelControl:
        case SuTChannelControl:
        case SuTChannelAssignment:
        case SuTelephonyAck:
        case SuRequestAcknowledgement:
        case SuAcknowledge:
        case SuUserDataIsu:
        case SuUserData3Octet:
        case SuUserData4Octet:
            return true;
        default:
            return false;
        }
    }

    static const char *suTypeName(uint8_t type)
    {
        if (isSsu(type)) {
            return "User data SSU";
        }

        switch (type)
        {
        case SuFill: return "Fill-in signal unit";
        case SuSystemTableSmc: return "System table, Psmc/Rsmc channels";
        case SuSystemTableBeam: return "System table, GES beam support";
        case SuSystemTableIndex: return "System table index";
        case SuSystemTableSatellite: return "System table, satellite identification";
        case SuLogOnRequest: return "Log-on request";
        case SuLogOnConfirm: return "Log-on confirm";
        case SuLogOffRequest: return "Log-off request";
        case SuLogOnReject: return "Log-on reject";
        case SuLogOnInterrogation: return "Log-on interrogation";
        case SuLogOnOffAcknowledge: return "Log-on/off acknowledge";
        case SuLogOnPrompt: return "Log-on prompt";
        case SuDataChannelReassignment: return "Data channel reassignment";
        case SuReserved18:
        case SuReserved19:
        case SuReserved26: return "Reserved";
        case SuCallAnnouncement: return "Call announcement";
        case SuAccessRequestData: return "Access request, data";
        case SuEirpTable: return "Data EIRP table broadcast";
        case SuCallProgress: return "Call progress";
        case SuCChannelDistress: return "C channel assignment, distress";
        case SuCChannelFlightSafety: return "C channel assignment, flight safety";
        case SuCChannelOtherSafety: return "C channel assignment, other safety";
        case SuCChannelNonSafety: return "C channel assignment, non-safety";
        case SuPRChannelControl: return "P/R channel control";
        case SuTChannelControl: return "T channel control";
        case SuTChannelAssignment: return "T channel assignment";
        case SuTelephonyAck: return "Telephony acknowledge";
        case SuRequestAcknowledgement: return "Request for acknowledgement";
        case SuAcknowledge: return "Acknowledge";
        case SuUserDataIsu: return "User data ISU";
        case SuUserData3Octet: return "User data, 3 octet LSDU";
        case SuUserData4Octet: return "User data, 4 octet LSDU";
        default: return "Unknown";
        }
    }

    // C channel assignment carries the assigned voice frequencies as 15 bit channel
    // numbers with bit 15 flagging a spot beam (JAERO aerol.cpp SendCAssignment)
    static double cChannelReceiveMHz(int channel) { return channel * 0.0025 + 1510.0; }
    static double cChannelTransmitMHz(int channel) { return channel * 0.0025 + 1611.5; }

    // ---------------------------------------------------------------------------------
    // ISU/SSU reassembly. This is link layer fragmentation of ONE ACARS block, which is
    // a different thing from ACARS multipart (ETB) - that is left to
    // AcarsMultipartAssembler downstream, exactly as the other protocols do it.
    // ---------------------------------------------------------------------------------

    struct IsuKey
    {
        uint32_t m_aesId;
        uint8_t m_gesId;
        uint8_t m_qNo;
        uint8_t m_refNo;

        bool operator<(const IsuKey& o) const
        {
            if (m_aesId != o.m_aesId) return m_aesId < o.m_aesId;
            if (m_gesId != o.m_gesId) return m_gesId < o.m_gesId;
            if (m_qNo != o.m_qNo) return m_qNo < o.m_qNo;
            return m_refNo < o.m_refNo;
        }
    };

    struct Reassembly
    {
        int m_remaining = 0;            //!< SSUs still expected, counts down
        int m_lastOctets = 0;           //!< Octets used in the final SSU
        double m_started = 0.0;
        std::vector<uint8_t> m_data;
    };

    // ---------------------------------------------------------------------------------
    // ACARS extraction. The reassembled user data is FF FF then a complete ARINC 618
    // block; strip the FFs and the odd parity and the rest of the plugin takes it.
    // ---------------------------------------------------------------------------------

    static bool looksLikeAcars(const std::vector<uint8_t>& userData)
    {
        // JAERO's own sniff test, aerol.cpp ParserISU::parse
        return (userData.size() > 16)
            && (userData[0] == 0xFF)
            && (userData[1] == 0xFF)
            && ((userData[15] == 0x83) || (userData[15] == 0x02));
    }

    // True when every octet that should carry odd parity does
    static bool acarsParityOk(const std::vector<uint8_t>& userData)
    {
        // The two leading FFs and the two BCS octets before the DEL are the exceptions
        if (userData.size() < 7) {
            return false;
        }
        size_t bcs = userData.size() - 3;
        for (size_t i = 2; i < userData.size(); i++)
        {
            if ((i == bcs) || (i == bcs + 1)) {
                continue;
            }
            if (!oddParity(userData[i])) {
                return false;
            }
        }
        return true;
    }

    // Strip the two FF octets, giving the ARINC 618 block from its SOH.
    //
    // The parity bits are deliberately LEFT ON, which is what VDL-2 and HFDL deliver
    // too: AcarsMessage::decode masks them itself, and libacars wants the block as
    // transmitted. SOH and DEL both happen to have odd parity already, so the raw
    // first and last octets still compare equal to 0x01 and 0x7F, which is what
    // AcarsMessage::decode tests without masking.
    static std::vector<uint8_t> acarsBlock(const std::vector<uint8_t>& userData)
    {
        if (userData.size() <= 2) {
            return std::vector<uint8_t>();
        }
        return std::vector<uint8_t>(userData.begin() + 2, userData.end());
    }

    static bool oddParity(uint8_t v)
    {
        v = (uint8_t) (v ^ (v >> 4));
        v = (uint8_t) (v ^ (v >> 2));
        v = (uint8_t) (v ^ (v >> 1));
        return (v & 1) != 0;
    }

    static uint8_t withOddParity(uint8_t v)
    {
        v = (uint8_t) (v & 0x7F);
        return oddParity(v) ? v : (uint8_t) (v | 0x80);
    }

    // ---------------------------------------------------------------------------------
    // Encode side, for the test harness generator. Exact inverses of the decode path.
    // ---------------------------------------------------------------------------------

    // Wrap an ARINC 618 ACARS block (from its SOH, with its BCS and DEL) as Aero user
    // data: two FF octets then the block with odd parity added
    static std::vector<uint8_t> buildUserData(const std::vector<uint8_t>& acarsBlock)
    {
        std::vector<uint8_t> ud = { 0xFF, 0xFF };
        if (acarsBlock.size() < 3) {
            return ud;
        }
        size_t bcs = acarsBlock.size() - 3;
        for (size_t i = 0; i < acarsBlock.size(); i++)
        {
            if ((i == bcs) || (i == bcs + 1)) {
                ud.push_back(acarsBlock[i]);
            } else {
                ud.push_back(withOddParity(acarsBlock[i]));
            }
        }
        return ud;
    }

    // Split user data into an ISU and its SSUs, each a complete signal unit with CRC
    static std::vector<std::vector<uint8_t>> buildIsuChain(uint32_t aesId, uint8_t gesId,
        uint8_t qNo, uint8_t refNo, const std::vector<uint8_t>& userData)
    {
        std::vector<std::vector<uint8_t>> sus;
        // Two octets ride in the ISU, eight in each SSU
        int remaining = (int) userData.size() - 2;
        if (remaining < 0) {
            remaining = 0;
        }
        int ssus = (remaining + 7) / 8;
        int lastOctets = remaining - (ssus - 1) * 8;
        if (ssus == 0) {
            lastOctets = 0;
        }

        std::vector<uint8_t> isu;
        isu.push_back(SuUserDataIsu);
        isu.push_back((uint8_t) ((aesId >> 16) & 0xFF));
        isu.push_back((uint8_t) ((aesId >> 8) & 0xFF));
        isu.push_back((uint8_t) (aesId & 0xFF));
        isu.push_back(gesId);
        isu.push_back((uint8_t) (((qNo & 0x0F) << 4) | (refNo & 0x0F)));
        isu.push_back((uint8_t) (ssus & 0x3F));
        isu.push_back((uint8_t) ((lastOctets & 0x0F) << 4));
        isu.push_back(userData.size() > 0 ? userData[0] : 0);
        isu.push_back(userData.size() > 1 ? userData[1] : 0);
        appendCrc(isu);
        sus.push_back(isu);

        for (int s = 0; s < ssus; s++)
        {
            std::vector<uint8_t> ssu;
            // SEQNO counts down and the FIRST SSU is one less than the count in the
            // ISU, so the last one carries zero (JAERO ISUData::findisuitemC0 matches
            // on SEQNO + 1 against the stored value)
            ssu.push_back((uint8_t) (0xC0 | ((ssus - 1 - s) & 0x3F)));
            ssu.push_back((uint8_t) (((qNo & 0x0F) << 4) | (refNo & 0x0F)));
            int octets = (s == ssus - 1) ? lastOctets : 8;
            for (int i = 0; i < 8; i++)
            {
                size_t src = (size_t) (2 + s * 8 + i);
                ssu.push_back((i < octets) && (src < userData.size()) ? userData[src] : 0);
            }
            appendCrc(ssu);
            sus.push_back(ssu);
        }
        return sus;
    }

    static std::vector<uint8_t> buildFillSu()
    {
        std::vector<uint8_t> su(ACARSAERO_SU_LEN - ACARSAERO_SU_CRC_LEN, 0);
        su[0] = SuFill;
        appendCrc(su);
        return su;
    }

    // K=7 rate 1/2 convolutional encoder that CARRIES ITS STATE, which is what the real
    // P channel encoder does - it runs continuously across frame boundaries rather than
    // restarting each frame. Measured, not assumed: decoding a real recording with the
    // Viterbi forced to start in state 0 lost the first signal unit of three frames in
    // four, and letting it start from any state took that unit's success rate from 24%
    // to the 95% every other position already got.
    //
    // Same polynomials as AcarsHfdlReceiver::convEncode; that one always starts from
    // zero, which is right for HFDL's per-burst encoder and wrong here.
    static void convEncodeContinuous(const std::vector<uint8_t>& bits,
                                     std::vector<uint8_t>& chips, uint32_t& sr)
    {
        chips.clear();
        chips.reserve(bits.size() * 2);
        for (uint8_t b : bits)
        {
            sr = ((sr << 1) | (b & 1)) & 0x7F;
            chips.push_back(parity7(sr & 0x6D));
            chips.push_back(parity7(sr & 0x4F));
        }
    }

    static uint8_t parity7(uint32_t v)
    {
        v ^= v >> 4;
        v ^= v >> 2;
        v ^= v >> 1;
        return (uint8_t) (v & 1);
    }

    // Build one P channel frame's worth of channel bits: unique word, header, then the
    // data field encoded, interleaved and scrambled. sus are padded with fill units.
    // sr carries the convolutional encoder state from the previous frame.
    static std::vector<uint8_t> encodeFrameBits(int rate, uint16_t header,
        const std::vector<std::vector<uint8_t>>& sus, uint32_t& sr)
    {
        const int cols = interleaverCols(rate);
        const int blockLen = ACARSAERO_INTERLEAVER_ROWS * cols;
        const int coded = codedBitsPerFrame(rate);
        const int dataBits = coded / 2;

        // Octets of the data field, padded with fill signal units
        std::vector<uint8_t> octets;
        octets.reserve(dataBits / 8);
        for (const auto& su : sus)
        {
            if ((int) octets.size() + ACARSAERO_SU_LEN > dataBits / 8) {
                break;
            }
            octets.insert(octets.end(), su.begin(), su.end());
        }
        std::vector<uint8_t> fill = buildFillSu();
        while ((int) octets.size() + ACARSAERO_SU_LEN <= dataBits / 8) {
            octets.insert(octets.end(), fill.begin(), fill.end());
        }
        octets.resize(dataBits / 8, 0);

        // Unpack LSB first, then scramble
        std::vector<uint8_t> bits(dataBits);
        for (int i = 0; i < dataBits; i++) {
            bits[i] = (uint8_t) (((octets[i / 8] >> (i % 8)) & 1) ^ scramblerBit(i));
        }

        // Convolutionally encode, then interleave block by block
        std::vector<uint8_t> chips;
        convEncodeContinuous(bits, chips, sr);
        chips.resize(coded, 0);

        std::vector<uint8_t> tx(coded);
        for (int b = 0; b < coded / blockLen; b++) {
            interleave(chips.data() + b * blockLen, cols, tx.data() + b * blockLen);
        }

        std::vector<uint8_t> out;
        out.reserve(uwBits(rate) + ACARSAERO_HEADER_LEN + dummyBits(rate) + coded);
        appendUw(rate, out);
        for (int i = 0; i < ACARSAERO_HEADER_LEN; i++) {
            out.push_back((uint8_t) ((header >> (ACARSAERO_HEADER_LEN - 1 - i)) & 1));
        }
        for (int i = 0; i < dummyBits(rate); i++) {
            out.push_back(0);
        }
        out.insert(out.end(), tx.begin(), tx.end());
        return out;
    }

    // Encode one R or T burst, the counterpart of tryBurstDecode().
    //
    // A burst is NOT a frame: there is no 16 bit header and no dummy field, the
    // convolutional encoder starts from state zero for every burst instead of running
    // continuously, the scrambler restarts with it, and the interleaver is segmented.
    // The generator had none of that - it built P channel frames whatever the submode was
    // - so the R and T paths were never exercised end to end at all, and the "passes
    // loopback" claim in the plan was simply untrue for them.
    //
    // An R burst is five columns holding one 19 octet signal unit. A T burst is five
    // columns of header then three columns per 12 octet signal unit. Either way the
    // block is 64 x cols channel bits, so it carries 32 x cols information bits, and
    // whatever the signal units do not fill is left as zeros - which also flushes the
    // encoder, since the tail of every burst is zero.
    static std::vector<uint8_t> encodeBurstBits(int rate, int channel,
        const std::vector<std::vector<uint8_t>>& sus)
    {
        const int rows = ACARSAERO_INTERLEAVER_ROWS;
        const int suLen = (channel == ChannelR) ? ACARSAERO_R_SU_LEN : ACARSAERO_SU_LEN;

        std::vector<uint8_t> octets;
        int cols;

        if (channel == ChannelR)
        {
            cols = ACARSAERO_BURST_HEAD_COLS;
            if (!sus.empty()) {
                octets.insert(octets.end(), sus[0].begin(), sus[0].end());
            }
        }
        else
        {
            cols = ACARSAERO_BURST_HEAD_COLS + (int) sus.size() * ACARSAERO_BURST_SU_COLS;
            // The T header is four octets and its own CRC. The receiver takes the signal
            // unit count from the burst length rather than from the header, so only the
            // CRC has to be right.
            std::vector<uint8_t> hdr(ACARSAERO_T_HEADER_LEN, 0);
            hdr[0] = 0xC0;
            hdr[1] = 0x40;
            hdr[2] = 0x00;
            hdr[3] = 0x01;
            uint16_t crc = computeCrc(hdr.data(), ACARSAERO_T_HEADER_LEN - ACARSAERO_SU_CRC_LEN);
            hdr[ACARSAERO_T_HEADER_LEN - 2] = (uint8_t) (crc & 0xFF);
            hdr[ACARSAERO_T_HEADER_LEN - 1] = (uint8_t) (crc >> 8);
            octets = hdr;
            for (const auto& su : sus) {
                octets.insert(octets.end(), su.begin(), su.end());
            }
        }
        (void) suLen;

        const int coded = rows * cols;
        const int dataBits = coded / 2;
        octets.resize((size_t) dataBits / 8, 0);

        std::vector<uint8_t> bits(dataBits);
        for (int i = 0; i < dataBits; i++) {
            bits[i] = (uint8_t) (((octets[i / 8] >> (i % 8)) & 1) ^ scramblerBit(i));
        }

        // From state zero, per burst - not the continuous encoder the P channel uses
        uint32_t sr = 0;
        std::vector<uint8_t> chips;
        convEncodeContinuous(bits, chips, sr);
        chips.resize(coded, 0);

        std::vector<uint8_t> tx(coded);
        interleaveBurstMsk(chips.data(), cols, tx.data());

        std::vector<uint8_t> out;
        out.reserve(uwBits(rate) + coded);
        appendUw(rate, out);
        out.insert(out.end(), tx.begin(), tx.end());
        return out;
    }

    // Bits the unique word field occupies. OQPSK carries the same 32 bit word on BOTH
    // arms, so it takes 64 bits of the stream - which is exactly the 64 that the frame
    // arithmetic needs (16 + 178 + 4992 + 64 = 5250) and 32 symbols either way.
    static int uwBits(int rate)
    {
        return isOqpsk(rate) ? (2 * ACARSAERO_UW_LEN) : ACARSAERO_UW_LEN;
    }

    static void appendUw(int rate, std::vector<uint8_t>& out)
    {
        for (int i = 0; i < ACARSAERO_UW_LEN; i++)
        {
            uint8_t b = (uint8_t) uwBit(i);
            if (isOqpsk(rate)) {
                out.push_back(b);       // Q arm
            }
            out.push_back(b);           // I arm, or the MSK symbol
        }
    }

    // OQPSK modulate a bit stream. The bits arrive Q first then I for each symbol, the
    // Q arm is inverted with respect to I, and the Q pulse is staggered HALF A SYMBOL
    // EARLY - the three conventions the receiver expects, none of them guessable and
    // all three established against a real recording.
    static void modulateOqpsk(const std::vector<uint8_t>& bits, int sps, std::vector<Cd>& out)
    {
        const size_t symbols = bits.size() / 2;
        const int span = 6;
        const int taps = span * sps + 1;

        // Root raised cosine, roll-off 1.0, the same pulse the receiver matches to
        std::vector<double> h(taps);
        double sum = 0.0;
        for (int i = 0; i < taps; i++)
        {
            double t = (i - taps / 2) / (double) sps;
            // Root raised cosine with roll-off exactly 1, where the general formula
            // collapses to 4t cos(2 pi t) / (pi t (1 - 16 t^2))
            double v;
            if (std::abs(t) < 1e-9) {
                v = 4.0 / M_PI;
            } else if (std::abs(std::abs(4.0 * t) - 1.0) < 1e-6) {
                v = (1.0 / std::sqrt(2.0))
                    * ((1.0 + 2.0 / M_PI) * std::sin(M_PI / 4.0)
                     + (1.0 - 2.0 / M_PI) * std::cos(M_PI / 4.0));
            } else {
                v = (4.0 * t * std::cos(2.0 * M_PI * t))
                    / (M_PI * t * (1.0 - 16.0 * t * t));
            }
            h[i] = v;
            sum += v;
        }
        for (int i = 0; i < taps; i++) {
            h[i] /= sum;
        }

        const int lead = taps / 2 + sps;
        out.assign(symbols * sps + 2 * lead, Cd(0.0, 0.0));

        for (size_t n = 0; n < symbols; n++)
        {
            double q = bits[2 * n] ? -1.0 : 1.0;    // Q is inverted with respect to I
            double a = bits[2 * n + 1] ? 1.0 : -1.0;

            int iCentre = (int) (lead + n * sps);
            int qCentre = iCentre - sps / 2;        // Q leads I by half a symbol

            for (int k = 0; k < taps; k++)
            {
                int pi = iCentre + k - taps / 2;
                int pq = qCentre + k - taps / 2;
                if ((pi >= 0) && (pi < (int) out.size())) {
                    out[pi] += Cd(a * h[k], 0.0);
                }
                if ((pq >= 0) && (pq < (int) out.size())) {
                    out[pq] += Cd(0.0, q * h[k]);
                }
            }
        }
    }

    // MSK modulate a bit stream. The bits are differentially encoded onto the derotated
    // BPSK stream, which is the same as saying the MSK phase advances +90 degrees for a
    // one and -90 for a zero - continuous phase, constant envelope.
    static void modulateMsk(const std::vector<uint8_t>& bits, int sps, std::vector<Cd>& out)
    {
        out.clear();
        out.reserve(bits.size() * sps);
        double phase = 0.0;
        for (uint8_t b : bits)
        {
            double step = (b ? M_PI_2 : -M_PI_2) / sps;
            for (int s = 0; s < sps; s++)
            {
                phase += step;
                out.push_back(std::polar(1.0, phase));
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // Receiver
    // ---------------------------------------------------------------------------------

    bool processSample(const Cd& in)
    {
        m_stats.m_samples++;
        m_time += 1.0 / channelSampleRate(m_rate);
        size_t before = m_frames.size();

        Cd s = matchedFilter(in);
        m_ringIdx = (m_ringIdx + 1) % (int) m_ring.size();
        m_ring[m_ringIdx] = s;
        if (m_ringFill < (int) m_ring.size()) {
            m_ringFill++;
        }

        if (!rateSupported(m_rate)) {
            return false;
        }

        bool haveFill = m_ringFill >= (ACARSAERO_UW_LEN + 1) * m_sps + 4;

        if (m_state == StateSearch)
        {
            if (haveFill) {
                searchUniqueWord();
            }
        }
        else if (m_state == StateTrack)
        {
            trackUniqueWord();
        }
        else
        {
            if (++m_sclk >= m_symPeriod)
            {
                m_sclk = 0;
                processSymbol();
                nextSymbolPeriod();
            }
        }

        return m_frames.size() != before;
    }

private:
    static const int MAX_SCRAMBLER = 8192;  // Longest frame's data field, with margin

    enum State
    {
        StateSearch = 0,
        StateHeader,        //!< Collecting the 16 uncoded header bits
        StateData,          //!< Collecting the coded data field
        StateBurst,         //!< R/T: collecting soft bits, trying a decode each block
        StateTrack          //!< P: waiting for the next frame's unique word where predicted
    };

    Config m_config;
    Stats m_stats;
    int m_rate = Rate600;
    int m_channel = ChannelP;
    int m_sps = 32;
    double m_time = 0.0;

    std::vector<double> m_filter;   //!< Matched filter taps
    std::vector<Cd> m_filterDelay;
    int m_filterIdx = 0;

    std::vector<Cd> m_ring;
    int m_ringIdx = 0;
    int m_ringFill = 0;

    State m_state = StateSearch;
    //!< How well the unique word correlated for the frame being decoded now, which is
    //!< how sure we are that this IS a frame - see the header check in consumeSoftBit()
    double m_syncCorrelation = 0.0;
    int m_lockedPhase = -1;         //!< OQPSK symbol clock phase held across frames, -1 until locked
    int m_framePhase = -1;          //!< Phase this frame was started on, locked if it decodes
    bool m_phaseConfirmed = false;  //!< A frame has actually decoded on m_lockedPhase
    int m_phaseFailures = 0;        //!< Consecutive dead frames since it was confirmed
    int m_sclk = 0;
    int m_symPeriod = 0;            //!< Samples in the symbol being collected
    double m_timingAcc = 0.0;       //!< Fractional part of the timing correction
    double m_timingRate = 0.0;      //!< Samples of slip per symbol - measureTimingSlip()
    int m_symbolCount = 0;
    Cd m_prevSym;
    double m_dphi = 0.0;            //!< Carrier offset, radians per symbol, from acquisition
    double m_lastSyncError = 0.0;
    double m_magMean = 0.0;         //!< Running mean differential magnitude, for soft scaling

    int m_trackWait = 0;            //!< Samples until the predicted unique word window opens
    int m_missedFrames = 0;

    bool m_syncPending = false;     //!< Holding the correlation peak before committing
    double m_bestStat = 0.0;
    int m_bestAgo = 0;
    Cd m_bestSum;

    std::vector<uint8_t> m_headerBits;
    std::vector<uint8_t> m_softBits;    //!< Data field soft bits, 255 = strong 1
    std::vector<uint8_t> m_softBitsCoh;  //!< ... from the coherent lane

    std::vector<Cd> m_symBuf;       //!< Derotated symbols of the frame, for the smoother
    std::vector<double> m_refPhase; //!< ... and the unwrapped reference phase at each
    double m_refPhaseAcc = 0.0;     //!< Running unwrapped phase of m_ref
    double m_projPrevFirst = 0.0;   //!< Projection before the first data symbol
    bool m_oqpskInit = false;       //!< OQPSK carrier seeded for this frame
    Cd m_ref;                       //!< Coherent lane carrier reference, unit magnitude
    double m_dphiSym = 0.0;         //!< ... and its per symbol advance, tracked
    double m_projPrev = 0.0;        //!< Previous coherent projection, for the differential decode
    double m_evmMeanSq = 0.0;       //!< Running mean square in-phase error, see detectCoherent
    double m_suRate = -1.0;         //!< Running fraction of signal units passing CRC
    double m_magMeanCoh = 0.0;
    bool m_cohValid = false;
    std::vector<uint8_t> m_burstBits;
    int m_burstTargetSus = 0;       //!< T burst signal unit count, from unit one's SEQNO
    double m_carriedDphi = 0.0;     //!< Per-symbol rotation carried from the last good frame
    double m_frameSeedDphi = 0.0;   //!< What the unique word alone said, for the statistics
    int m_traceSymbol = 0;
    int m_traceFrame = 0;
    int m_frameHeaderDistance = 0;
    bool m_frameSeededFromUw = false;
    bool m_haveCarriedDphi = false; //!< ... and whether there is one
    int m_burstCols = 0;

    std::deque<Frame> m_frames;
    std::map<IsuKey, Reassembly> m_reassembly;

    int ringLength() const { return (ACARSAERO_UW_LEN + 2) * m_sps + 8; }

    void designFilter()
    {
        if (isOqpsk(m_rate)) {
            designRrc();
        } else {
            designHalfSine();
        }
        m_filterDelay.assign(m_filter.size(), Cd(0.0, 0.0));
        m_filterIdx = 0;
    }

    // MSK's matched filter is a half sine spanning two symbol periods
    void designHalfSine()
    {
        int len = 2 * m_sps;
        m_filter.assign(len, 0.0);
        double sum = 0.0;
        for (int i = 0; i < len; i++)
        {
            m_filter[i] = std::sin(M_PI * i / (double) len);
            sum += m_filter[i];
        }
        for (int i = 0; i < len; i++) {
            m_filter[i] /= sum;
        }
    }

    // OQPSK uses a root raised cosine with roll-off 1.0, designed at the symbol rate
    void designRrc()
    {
        const double alpha = 1.0;
        const int span = 6;
        int len = span * m_sps + 1;
        m_filter.assign(len, 0.0);
        double sum = 0.0;
        for (int i = 0; i < len; i++)
        {
            double t = (i - len / 2) / (double) m_sps;
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
            m_filter[i] = v;
            sum += v;
        }
        for (int i = 0; i < len; i++) {
            m_filter[i] /= sum;
        }
    }

    Cd matchedFilter(const Cd& in)
    {
        m_filterDelay[m_filterIdx] = in;
        Cd acc(0.0, 0.0);
        int idx = m_filterIdx;
        for (size_t i = 0; i < m_filter.size(); i++)
        {
            acc += m_filterDelay[idx] * m_filter[i];
            idx = (idx == 0) ? (int) m_filter.size() - 1 : idx - 1;
        }
        m_filterIdx = (m_filterIdx + 1) % (int) m_filter.size();
        return acc;
    }

    Cd tap(int back) const
    {
        int n = (int) m_ring.size();
        return m_ring[(m_ringIdx + n - back) % n];
    }

    // Differential detection of MSK. Consecutive symbols differ by exactly +-90
    // degrees - s[n]/s[n-1] is +j for a one and -j for a zero - so multiplying the
    // product of a symbol and the conjugate of its predecessor by -j lands it on the
    // real axis, positive for a one. This is the derotate-to-BPSK view of MSK with the
    // per symbol 90 degree rotation cancelled out: it appears in the product as a
    // constant, so there is no need to track a symbol index at all.
    static Cd differential(const Cd& sym, const Cd& prev)
    {
        return sym * std::conj(prev) * Cd(0.0, -1.0);
    }

    // The other view of the same identity, the one the coherent lane needs: rotating
    // the matched filter output by -90 degrees per symbol turns MSK into plain BPSK,
    // u[n] = b[n] exp(j phi), where b[n] are the PRECODED symbols. Their running
    // product is the data, which is why a differential decode still follows.
    //
    // The symbol index origin is arbitrary - shifting it rotates every sample by the
    // same multiple of 90 degrees, which the carrier reference absorbs - so long as it
    // is used consistently.
    static Cd derotate(const Cd& v, int n)
    {
        switch (((n % 4) + 4) % 4)
        {
        case 0: return v;
        case 1: return Cd(v.imag(), -v.real());     // * -j
        case 2: return -v;
        default: return Cd(-v.imag(), v.real());    // * +j
        }
    }

    // ---------------------------------------------------------------------------------
    // Acquisition. Differential correlation against the unique word: the products
    // u[k] conj(u[k-1]) read +-e^{j dphi}, the sign following whether consecutive
    // differentially encoded unique word bits are equal, so the statistic is immune to
    // carrier offset and arg(D) gives the per symbol carrier advance. Same machinery as
    // the HFDL and VDL-2 receivers.
    // ---------------------------------------------------------------------------------

    // The 32 unique word bits are carried by the transitions between 33 symbols, the
    // last of which is the newest sample. Bit k is the transition from symbol k to
    // symbol k+1, so it reads back (32 - k - 1) and (32 - k) symbols ago.
    double uwCorrelation(Cd& sum) const
    {
        sum = Cd(0.0, 0.0);
        double mag = 0.0;

        if (isOqpsk(m_rate))
        {
            // OQPSK is not precoded, so the unique word sits directly on the I arm's 32
            // symbols rather than on the transitions between 33 of them. That makes 31
            // products, and the sign template is whether consecutive unique word BITS
            // agree rather than whether consecutive data bits do.
            for (int k = 0; k + 1 < ACARSAERO_UW_LEN; k++)
            {
                Cd a = tap((ACARSAERO_UW_LEN - k - 2) * m_sps);
                Cd b = tap((ACARSAERO_UW_LEN - k - 1) * m_sps);
                Cd d = a * std::conj(b);
                sum += (uwBit(k) == uwBit(k + 1)) ? d : -d;
                mag += std::abs(d);
            }
            return (mag > 0.0) ? std::abs(sum) / mag : 0.0;
        }

        for (int k = 0; k < ACARSAERO_UW_LEN; k++)
        {
            Cd d = differential(tap((ACARSAERO_UW_LEN - k - 1) * m_sps),
                                tap((ACARSAERO_UW_LEN - k) * m_sps));
            sum += uwBit(k) ? d : -d;
            mag += std::abs(d);
        }
        return (mag > 0.0) ? std::abs(sum) / mag : 0.0;
    }

    bool searchUniqueWord()
    {
        Cd sum;
        double stat = uwCorrelation(sum);
        m_lastSyncError = stat;
        if (stat > m_stats.m_uwPeak) {
            m_stats.m_uwPeak = stat;
        }
        const double syncThreshold = isOqpsk(m_rate) ? m_config.m_oqpskSyncThreshold
                                                     : m_config.m_syncThreshold;

        // Hold the peak rather than firing on the first sample over the threshold. The
        // correlation rises over roughly a symbol either side of perfect alignment, so
        // firing on the crossing samples the eye off centre - worth a dB or more at
        // 32 samples per symbol, and it costs only half a symbol of latency.
        if (stat >= syncThreshold)
        {
            if (!m_syncPending || (stat > m_bestStat))
            {
                m_syncPending = true;
                m_bestStat = stat;
                m_bestAgo = 0;
                m_bestSum = sum;
            }
        }

        if (!m_syncPending) {
            return false;
        }

        m_bestAgo++;
        if (m_bestAgo <= m_sps / 2) {
            return false;
        }

        m_stats.m_syncs++;
        m_syncPending = false;
        m_dphi = std::arg(m_bestSum);
        m_lastSyncError = m_bestStat;
        m_syncCorrelation = m_bestStat;
        startFrame(refineTiming(m_bestAgo));
        return true;
    }

    // A P channel frame is a fixed number of symbols, so once one has been decoded the
    // next unique word is due at a known instant. Rather than free running search - which
    // is what lets a data pattern or an adjacent channel steal a frame - open a window of
    // half a symbol either side of the predicted instant, take the correlation peak
    // inside it, and accept it against a much lower threshold than acquisition uses.
    //
    // The window also re-locks the symbol timing every frame, so clock error cannot
    // accumulate across a long lock.
    //
    // Returns true when a frame was started.
    bool trackUniqueWord()
    {
        if (--m_trackWait > 0) {
            return false;
        }

        Cd sum;
        double stat = uwCorrelation(sum);

        if (!m_syncPending || (stat > m_bestStat))
        {
            m_syncPending = true;
            m_bestStat = stat;
            m_bestAgo = 0;
            m_bestSum = sum;
        }
        m_bestAgo++;

        // The window is one symbol wide, centred on the prediction
        if (m_trackWait > -m_sps) {
            return false;
        }

        m_lastSyncError = m_bestStat;
        m_syncPending = false;

        if (m_bestStat < m_config.m_trackThreshold)
        {
            m_missedFrames++;
            if (m_missedFrames > m_config.m_maxMissedFrames)
            {
                m_stats.m_uwLost++;
                m_state = StateSearch;
                m_lockedPhase = -1;     // a fresh acquisition may legitimately pick either phase
                m_phaseConfirmed = false;
                m_phaseFailures = 0;
                m_haveCarriedDphi = false;
                        return false;
            }
            // Predict the frame after next and keep tracking through the gap
            armTracking();
            return false;
        }

        m_missedFrames = 0;
        m_stats.m_uwTracked++;
        m_stats.m_syncs++;
        m_dphi = std::arg(m_bestSum);
        // This frame's own correlation, not the one the acquisition had - a tracked
        // frame is accepted against a much lower threshold, so inheriting the previous
        // frame's confidence would trust a header that has not earned it
        m_syncCorrelation = m_bestStat;
        measureTimingSlip(m_bestAgo);
        startFrame(refineTiming(m_bestAgo));
        return true;
    }

    // Open the next prediction window half a symbol before the unique word is due
    void armTracking()
    {
        m_state = StateTrack;
        m_syncPending = false;
        m_bestStat = 0.0;
        // From the frame's last data symbol, the next unique word ends 32 symbols later
        m_trackWait = ACARSAERO_UW_LEN * m_sps - m_sps / 2;
    }

    // The sample phase, modulo one symbol, of an instant "ago" samples back from now
    int instantPhase(int ago) const
    {
        int64_t at = (int64_t) m_stats.m_samples - (int64_t) ago;
        int ph = (int) (at % (int64_t) m_sps);
        return (ph < 0) ? (ph + m_sps) : ph;
    }

    // Signed distance from a candidate instant's phase to the locked one, in samples,
    // taken the short way round the symbol
    int phaseError(int ago) const
    {
        int d = m_lockedPhase - instantPhase(ago);
        if (d > m_sps / 2) {
            d -= m_sps;
        } else if (d < -m_sps / 2) {
            d += m_sps;
        }
        return d;
    }

    // Refine the symbol timing against the unique word, which is a known sequence.
    // OQPSK ONLY - see below.
    //
    // The acquisition correlator cannot do this itself. It is deliberately differential,
    // so that it works before the carrier is known, and a differential product is a very
    // blunt timing discriminator: it multiplies two neighbouring samples and so stays
    // large anywhere the eye is open at all. On the 10500 bps recording it reads between
    // 0.944 and 0.965 across the WHOLE symbol, which is no discrimination worth the
    // name, so peak-holding it lands the sampling instant essentially at random within
    // the symbol. That cost the OQPSK rate almost everything: 5% of signal units passing
    // at the correlator's own choice of instant against 80% six samples away.
    //
    // Stripping the modulation with the known unique word bits and summing coherently
    // gives the matched filter output instead, which is exactly what peaks at the symbol
    // centre. One pass, no loop to lose lock, nothing to tune.
    //
    // It is NOT applied to the MSK rates, and that is measured rather than assumed.
    // Their pulse is a half sine at 32 or 16 samples per symbol, so the differential
    // correlator's timing is already good, and every variant of this tried there made it
    // worse - 210 signal units of 222 without it, against 195, 175 and 81 for the three
    // ways of aligning it to MSK's 33 symbol differentially carried word. Refining
    // something that is already right can only add noise.
    // Kay's estimator over the unique word at a given sampling instant. The same
    // estimator seedOqpsk() uses, factored out because the timing refinement needs it
    // too - and needs it BEFORE the seed runs.
    double uwKayDphi(int at) const
    {
        const int n = ACARSAERO_UW_LEN;
        double sum = 0.0;
        double weight = 0.0;
        Cd previous(0.0, 0.0);

        for (int k = 0; k < n; k++)
        {
            Cd v = tap(at + (n - 1 - k) * m_sps) * (uwBit(k) ? 1.0 : -1.0);
            if (k)
            {
                const Cd step = v * std::conj(previous);
                if (std::abs(step) > 1e-12)
                {
                    const double x = ((double) k - n / 2.0) / (n / 2.0);
                    const double w = 1.0 - x * x;
                    sum += w * std::arg(step);
                    weight += w;
                }
            }
            previous = v;
        }
        return (weight > 0.0) ? (sum / weight) : 0.0;
    }

    // Two passes, because the timing needs the frequency and the frequency needs the
    // timing.
    //
    // The rotation the scoring needs cannot come from the acquisition correlator, which
    // is where it was taken from first. That correlator multiplies adjacent I arm
    // samples, and the unique word sits on the Q arm as well, so the crosstalk between
    // the arms is coherent across the word rather than averaging away and its argument
    // carries a systematic bias - measured at +30 Hz on a noiseless channel with no
    // offset at all, and +50 Hz at 1200 Hz. Thirty Hz sounds negligible for a frequency
    // and is not: across the 32 symbols of the word it is 1.15 rad of de-rotation that
    // should not be there, which moved the chosen instant and cost half the frames at
    // offsets that had been working perfectly.
    //
    // So: refine once with the correlator's estimate, which is biased but close enough
    // to sample the word near its centre; re-estimate the frequency there with Kay's
    // estimator, which is good to a few Hz; then refine again with that. The improved
    // estimate is kept, since it is strictly better than the one acquisition produced.
    int refineTiming(int ago)
    {
        if (!isOqpsk(m_rate)) {
            return ago;
        }

        const int first = refineTimingAt(ago, m_dphi);
        const double kay = uwKayDphi(first);
        m_dphi = kay;
        return refineTimingAt(ago, kay);
    }

    int refineTimingAt(int ago, double rot)
    {
        const int span = m_sps / 2;
        int bestOffset = 0;
        double best = -1.0;

        for (int d = -span; d <= span; d++)
        {
            int at = ago + d;
            // tap() only looks backwards, and the ring must hold the word plus the Q arm
            if ((at < 0) || (at + ACARSAERO_UW_LEN * m_sps + span >= (int) m_ring.size())) {
                continue;
            }
            // Refuse candidates that would move the symbol clock phase off the one already
            // locked. Applied HERE rather than by adjusting the winner afterwards, so the
            // result always stays inside the range the bounds check above has validated -
            // adjusting it after the fact could push it past the start of the ring, which
            // cost 15 of 118 frames on the recording.
            if ((m_lockedPhase >= 0)
                && (std::abs(phaseError(at - m_sps / 2)) > ACARSAERO_TIMING_PULL)) {
                continue;
            }

            // A reference direction from the I arm with the modulation stripped, and
            // with the carrier rotation taken out.
            //
            // The de-rotation is not a refinement, it is what makes this work off
            // frequency at all. The sum runs over 32 symbols, so at 300 Hz the vectors
            // being added swing through 3.8 rad and the "reference direction" becomes
            // the average of an arc rather than a direction. The score below then
            // projects every symbol onto that one direction, so the symbols at the ends
            // of the word contribute nothing, and the instant this picks is wrong by up
            // to half a symbol. Everything downstream is then built on samples taken off
            // the eye: the frequency estimate seeded from those taps came out at -657 Hz
            // where the truth was +300, the carrier reference spun at 45 degrees a
            // symbol, and the frame decoded nothing at all.
            //
            // rot is the per symbol rotation, supplied by refineTiming() above.
            Cd acc(0.0, 0.0);
            for (int k = 0; k < ACARSAERO_UW_LEN; k++)
            {
                Cd v = tap(at + (ACARSAERO_UW_LEN - 1 - k) * m_sps);
                acc += (uwBit(k) ? v : -v) * std::polar(1.0, -(double) k * rot);
            }
            double amag = std::abs(acc);
            if (amag < 1e-12) {
                continue;
            }
            const Cd u = acc / amag;

            // Score by the EYE OPENING the detector actually uses - the real axis of the
            // I arm and the imaginary axis of the Q arm - rather than by the magnitude of
            // that vector sum.
            //
            // Magnitude is the obvious criterion and it is measurably wrong here. The
            // unique word sits on BOTH arms, so its Q contribution to an I sample adds
            // coherently rather than averaging away, and the magnitude peak lands a few
            // samples off the instant where the arms actually separate best. On the
            // generated signal that difference was everything: 26 of 208 signal units at
            // the magnitude peak against 208 of 208 a few samples away.
            double score = 0.0;
            for (int k = 0; k < ACARSAERO_UW_LEN; k++)
            {
                int off = at + (ACARSAERO_UW_LEN - 1 - k) * m_sps;
                // The reference carried forward to this symbol, and back half a symbol
                // again for the Q arm, which is sampled that much earlier
                const Cd uk = u * std::polar(1.0, (double) k * rot);
                const Cd uq = uk * std::polar(1.0, -0.5 * rot);
                score += std::abs((tap(off) * std::conj(uk)).real());
                score += std::abs((tap(off + m_sps / 2) * std::conj(uq)).imag());
            }

            if (score > best)
            {
                best = score;
                bestOffset = d;
            }
        }

        // HALF A SYMBOL BACK, and this is the single largest thing in the 10500 bps path.
        //
        // The score above cannot tell the two arms apart. OQPSK is symmetric under "shift
        // half a symbol and swap the arms", and the reference direction u is estimated
        // from whatever sits at the candidate instant - so at the swapped instant u simply
        // rotates 90 degrees with the arms and the score comes out the same. The criterion
        // has two near-equal maxima half a symbol apart and takes whichever the noise
        // favours; on the recording it took the wrong one.
        //
        // Measured over the whole of samples/10.5k_sample.ogg, sweeping the instant across
        // the full symbol: a good region from -13 to -7 samples peaking at -10, and a
        // second, worse one around 0 where the search actually lands, with a hole between
        // them. -10 is exactly m_sps / 2. That is a structural quantity, not a fitted
        // constant, and the plateau is six samples wide, so this is not knife edge.
        //
        //     offset    0   ->  119 frames, 1274/3094 signal units (41%), 12 messages
        //     offset  -10   ->  118 frames, 1820/3068 signal units (59%), 16 messages
        //
        // Fixing u across the candidates instead was tried, to break the symmetry
        // honestly rather than by construction, and is worse than either - it scores zero
        // signal units, because the u taken at the search centre is itself on the wrong
        // arm, so the criterion is circular. A Gardner loop over the data does not help
        // either: it is symmetric under the same shift, so it cannot choose between the
        // two maxima, and as an error detector here it measures biased and walks off.
        if (best < 0.0) {
            // Every candidate was refused by the phase constraint, so the clock has moved
            // further than drift explains. Take the lock off and let the next frame
            // re-establish it rather than dropping the frame.
            m_lockedPhase = -1;
            m_framePhase = -1;
            return ago - m_sps / 2;
        }

        int chosen = ago + bestOffset - m_sps / 2;

        // HOLD THE PHASE ONCE IT IS KNOWN. The two maxima above are half a symbol apart
        // and the search is re-run for every frame, so without this it is free to hop
        // between them frame by frame - and a frame started on the wrong one decodes to
        // NOTHING. On a noiseless generated channel that showed up as every frame being
        // either 26 of 26 signal units or 0 of 26 and never anything between, with the
        // dead frames exactly accounted for by the header check rejecting them.
        //
        // A symbol clock cannot physically move half a symbol between frames - that would
        // be a 5% clock error - so a jump that large is always the alias and never the
        // signal. Anything further than ACARSAERO_TIMING_PULL samples from the phase
        // already locked is pulled back to it, which leaves ordinary drift free to track
        // and puts the alias out of reach.
        //
        // The phase is locked ON EVIDENCE, not on the first frame: the search picks the
        // wrong maximum often enough at acquisition that locking whatever it chose first
        // is WORSE than re-searching every frame (78 of 208 signal units against 104).
        // It is committed only once a frame has actually decoded - see decodeFrame() -
        // and released again as soon as one does not, so a wrong choice costs one frame
        // rather than the whole lock.
        m_framePhase = instantPhase(chosen);
        return chosen;
    }

    // ago is how many samples back the correlation peak sits, so the last unique word
    // symbol is at tap(ago) and the next symbol arrives m_sps - ago samples from now
    // How long the next symbol should be, in samples.
    //
    // The unique word re-locks the symbol timing at the start of every frame, and nothing
    // tracked it in between - so any steady slip walked the sampling instant further off
    // the further in to the frame it got, and the frame degraded from its first signal
    // unit to its last. On a live capture that was dropping samples this cost the last
    // unit of each frame five times the error rate of the first, inside two seconds.
    //
    // The slip is already measured: trackUniqueWord() finds the word in a one symbol
    // window centred on where it predicted, so the offset of the peak from the centre IS
    // the accumulated slip over the frame just gone. Spreading that over the next frame -
    // Bresenham fashion, lengthening or shortening the odd symbol by one sample - holds
    // the instant where it belongs without a timing loop, an interpolator, or any change
    // to how a symbol is taken.
    void nextSymbolPeriod()
    {
        m_symPeriod = m_sps;
        if (!m_config.m_timingSlope) {
            return;
        }
        m_timingAcc += m_timingRate;
        if (m_timingAcc >= 0.5)
        {
            m_symPeriod++;
            m_timingAcc -= 1.0;
        }
        else if (m_timingAcc <= -0.5)
        {
            m_symPeriod--;
            m_timingAcc += 1.0;
        }
    }

    //! Symbols from the end of one unique word to the end of the next
    int symbolsPerCycle() const
    {
        if (m_channel != ChannelP) {
            return 0;               // Bursts re-acquire every time; there is no cycle
        }
        return ACARSAERO_UW_LEN + ACARSAERO_HEADER_LEN
             + codedBitsPerFrame(m_rate) + dummyBits(m_rate);
    }

    // Turn the offset of the tracked unique word from its predicted position in to a
    // per symbol correction for the frame that follows
    void measureTimingSlip(int bestAgo)
    {
        const int cycle = symbolsPerCycle();
        if (!m_config.m_timingSlope || (cycle <= 0)) {
            return;
        }
        // The window is one symbol wide and centred on the prediction, so a word found
        // at the centre means the timing held. Early means the frame arrived in fewer
        // samples than expected, which is a clock running slow or samples going missing.
        const double slip = (m_sps / 2.0) - (double) bestAgo;
        const double rate = slip / (double) cycle;
        m_timingRate = (1.0 - m_config.m_timingSlopeGain) * m_timingRate
                     + m_config.m_timingSlopeGain * rate;
    }

    void startFrame(int ago)
    {
        m_symPeriod = m_sps;
        m_timingAcc = 0.0;
        m_sclk = ago;
        m_symbolCount = 0;
        m_magMean = 0.0;
        m_headerBits.clear();
        m_softBits.clear();
        m_burstBits.clear();
        m_burstCols = 0;
        // The symbol the last unique word bit ended on is the reference the first data
        // bit's differential detection works against
        m_prevSym = tap(ago);
        if (isOqpsk(m_rate)) {
            seedOqpsk(ago);
        } else {
            seedCoherent(ago);
        }
        m_state = (m_channel == ChannelP) ? StateHeader : StateBurst;
        if (m_state == StateBurst)
        {
            // R and T bursts have no header; the data field starts immediately
            m_burstBits.reserve((size_t) ACARSAERO_INTERLEAVER_ROWS * ACARSAERO_BURST_MAX_COLS);
            m_burstTargetSus = 0;
        }
    }


    // Seed the OQPSK carrier from the unique word on the I arm, which carries its 32
    // bits directly. Both the phase and the per symbol rotation come out of the same
    // pass: the vector sum with the modulation stripped gives the phase, and the mean
    // symbol to symbol product of those stripped symbols gives the rotation.
    void seedOqpsk(int ago)
    {
        Cd acc(0.0, 0.0);
        Cd rot(0.0, 0.0);
        Cd prev(0.0, 0.0);

        for (int k = 0; k < ACARSAERO_UW_LEN; k++)
        {
            Cd v = tap(ago + (ACARSAERO_UW_LEN - 1 - k) * m_sps) * (uwBit(k) ? 1.0 : -1.0);
            acc += v;
            if (k) {
                rot += v * std::conj(prev);
            }
            prev = v;
        }

        // The rotation again, this time from Kay's estimator rather than the plain mean
        // of the symbol to symbol phase steps.
        //
        // Why it is worth the extra loop: the reference below is referenced to the END of
        // the word, which means extrapolating the rotation across 31 symbols, so an error
        // in it is multiplied by 31. The plain mean is good to about 30 Hz at this SNR,
        // which sounds ample for a frequency and is not: 30 Hz over 31 symbols is
        // 31 * 2*pi*30/5250 = 1.1 rad, most of the way to the quadrant boundary, and the
        // detector is decision directed so it then locks to the wrong thing and the whole
        // frame decodes nothing. Measured on generated signal at 20 dB, the receiver was
        // losing 23 frames in 27 at a 180 Hz offset for exactly this reason.
        //
        // Kay weights each phase step by how much of the word it sits inside - a
        // parabolic window, heaviest in the middle - which is the minimum variance
        // combination for a constant frequency in white noise, and costs one more pass
        // over 32 symbols.
        double kaySum = 0.0;
        double kayWeight = 0.0;
        {
            const int n = ACARSAERO_UW_LEN;
            Cd previous(0.0, 0.0);
            for (int k = 0; k < n; k++)
            {
                Cd v = tap(ago + (n - 1 - k) * m_sps) * (uwBit(k) ? 1.0 : -1.0);
                if (k)
                {
                    const Cd step = v * std::conj(previous);
                    if (std::abs(step) > 1e-12)
                    {
                        // 1 - ((k - n/2)/(n/2))^2, zero at the ends and one in the middle
                        const double x = ((double) k - n / 2.0) / (n / 2.0);
                        const double w = 1.0 - x * x;
                        kaySum += w * std::arg(step);
                        kayWeight += w;
                    }
                }
                previous = v;
            }
        }

        // Sum again with the rotation taken out, referenced to the LAST symbol of the
        // word. Two things were wrong with the plain sum above once the tuning is not
        // exact, and both of them only bite off frequency:
        //
        //   - it smears. At 100 Hz the symbol to symbol advance is 0.12 rad, so across
        //     32 symbols the vectors being added swing through 3.8 rad and largely
        //     cancel, which is a poor reference however it is pointed.
        //   - it points at the MIDDLE of the word. The data starts after the word, so by
        //     the first data symbol the reference is already half the word's rotation
        //     behind - 1.9 rad at 100 Hz, most of a quadrant.
        //
        // The detector is decision directed, so a reference that far out makes the first
        // decisions wrong and the loop then locks to the wrong thing and stays there.
        // That is why the receiver was bistable in tuning: at the exact centre the seed
        // is good and every frame decodes, and 100 Hz away NO frame ever decodes, so the
        // frame to frame carry below - which is what usually rescues the seed - never
        // gets its first good frame to start from.
        const double seedDphi = (kayWeight > 0.0) ? (kaySum / kayWeight)
                              : ((std::abs(rot) > 1e-12) ? std::arg(rot) : 0.0);
        // Referenced to the FIRST symbol of the word, not the last. Aligning it to the
        // last means extrapolating the rotation across 31 symbols, which multiplies the
        // frequency estimate's error by 31: on the first frame of a run, before the
        // frame to frame carry has anything to offer, that error is 10 to 12 Hz and
        // becomes 25 degrees of phase. The loop then spends the header walking it off,
        // the header decodes wrong on a channel with no noise in it, and the interleaver
        // spreads the damage over all 26 signal units so the whole frame dies.
        Cd aligned(0.0, 0.0);
        for (int k = 0; k < ACARSAERO_UW_LEN; k++)
        {
            Cd v = tap(ago + (ACARSAERO_UW_LEN - 1 - k) * m_sps) * (uwBit(k) ? 1.0 : -1.0);
            aligned += v * std::polar(1.0, -(double) k * seedDphi);
        }
        // Always, never "whichever sum is bigger". That test looked like prudence and
        // was the bug: the two sums are referenced to DIFFERENT points in the word - the
        // plain one to its centre, the de-rotated one to its end - and those differ by
        // half the word's rotation, 15.5 symbols' worth. At 180 Hz that is 3.3 rad, so
        // letting noise pick between them threw the phase reference across the quadrant
        // boundary at random, frame by frame, and a decision directed detector cannot
        // survive that. It is why the receiver looked chaotic in tuning rather than
        // merely worse off frequency.
        acc = aligned;

        double mag = std::abs(acc);
        if (mag > 1e-12)
        {
            m_ref = acc / mag;
            m_cohValid = true;
            m_oqpskInit = true;
        }
        else
        {
            m_ref = Cd(1.0, 0.0);
            m_cohValid = false;
            m_oqpskInit = false;
        }
        m_dphiSym = seedDphi;

        // Now walk the carrier loop across the unique word itself, using its KNOWN bits
        // where the detector would use decisions. The reference then arrives at the end
        // of the word by tracking rather than by extrapolation, so it is as good as the
        // loop can make it rather than as good as a single algebraic estimate - and the
        // 31x amplification above is gone entirely. It is the same loop the data uses,
        // so it also arrives already settled instead of converging through the header.
        if (m_cohValid)
        {
            for (int k = 1; k < ACARSAERO_UW_LEN; k++)
            {
                m_ref *= std::polar(1.0, m_dphiSym);
                Cd d = tap(ago + (ACARSAERO_UW_LEN - 1 - k) * m_sps) * (uwBit(k) ? 1.0 : -1.0);
                const double mag = std::abs(d);
                if (mag < 1e-12) {
                    continue;
                }
                d /= mag;
                m_dphiSym += m_config.m_oqpskFreqGain * std::arg(d * std::conj(m_ref));
                m_ref = (1.0 - m_config.m_oqpskRefGain) * m_ref + m_config.m_oqpskRefGain * d;
                const double refMag = std::abs(m_ref);
                if (refMag > 1e-12) {
                    m_ref /= refMag;
                }
            }
        }

        // CARRY THE FREQUENCY ACROSS FRAMES. The unique word is only 32 symbols, so the
        // rotation estimated from it is coarse - and a 10500 bps frame is 2625 symbols
        // long, over which even a tiny error winds up into a phase the loop then has to
        // chase from behind. With the loop frozen (gains at zero) a NOISELESS channel
        // decodes nothing at all, which is the proof: what the loop mostly does is not
        // track a real carrier offset, it repairs this seed.
        //
        // A P channel is continuous and the offset is a property of the receiver and the
        // satellite, not of the frame, so the value the loop has converged on by the END
        // of a good frame is a far better seed for the next one than the unique word can
        // ever be. Only carried from a frame that actually decoded, so a bad frame cannot
        // poison the next - the same evidence gate the symbol clock phase uses.
        if (m_config.m_seedQuadrant && m_cohValid) {
            m_ref *= std::polar(1.0, m_config.m_seedQuadrant * M_PI / 2.0);
        }
        m_traceSymbol = 0;
        m_traceFrame++;
        if (m_config.m_traceLoop && (m_traceFrame <= 3))
        {
            fprintf(stderr, "FRAME %d seed %+8.2f Hz  acqDphi %+8.2f Hz  (carried %d)\n",
                    m_traceFrame, m_dphiSym * 5250.0 / (2.0 * M_PI),
                    m_dphi * 5250.0 / (2.0 * M_PI), (int) m_haveCarriedDphi);
        }
        m_frameSeedDphi = m_dphiSym;
        m_frameSeededFromUw = !m_haveCarriedDphi;
        if (m_haveCarriedDphi && m_config.m_carryDphi) {
            m_dphiSym = m_carriedDphi;
        }
        m_magMeanCoh = 0.0;
    }

    // One OQPSK symbol: two bits. The Q arm is sampled HALF A SYMBOL BEFORE the I
    // instant, and the stream carries the Q bit first and then the I bit - which is
    // JAERO's order, pushing imag() then real() of each constellation point. The Q arm
    // also comes out inverted relative to I. All three were established against the
    // recording; none of them is guessable.
    void oqpskSymbol(uint8_t& softQ, uint8_t& softI)
    {
        if (!m_oqpskInit)
        {
            softQ = softI = 128;
            return;
        }

        m_ref *= std::polar(1.0, m_dphiSym);

        const Cd yI = tap(0);
        const Cd yQ = tap(m_sps / 2);

        // The Q arm is sampled HALF A SYMBOL BEFORE the I instant, so the carrier has
        // rotated half a symbol's worth less by the time it was taken - m_dphiSym/2.
        // Derotating both arms by the same reference leaves that as a systematic error
        // on Q which grows with the frequency offset: 6 degrees at 180 Hz, 24 at 700,
        // a quadrant at 2.6 kHz. And because the reference is decision directed, the
        // error does not merely sit on the Q bit, it is fed back into the reference and
        // the frequency, so both arms then drift.
        const Cd refQ = m_ref * std::polar(1.0, -0.5 * m_dphiSym);

        double sI = (yI * std::conj(m_ref)).real();
        double sQ = (yQ * std::conj(refQ)).imag();

        // Decision-directed update from both arms. The Q arm is rotated onto the real
        // axis before it is blended in, so the two contribute symmetrically - and
        // forward by the same half symbol, so that both are on the I arm's timebase
        // when they reach the reference.
        const double signI = (sI >= 0.0) ? 1.0 : -1.0;
        const double signQ = (sQ >= 0.0) ? 1.0 : -1.0;

        Cd dI = yI * signI;
        Cd dQ = yQ * Cd(0.0, -1.0) * signQ * std::polar(1.0, 0.5 * m_dphiSym);

        for (int i = 0; i < 2; i++)
        {
            Cd d = i ? dQ : dI;
            double mag = std::abs(d);
            if (mag < 1e-12) {
                continue;
            }
            d /= mag;
            m_dphiSym += m_config.m_oqpskFreqGain * std::arg(d * std::conj(m_ref));
            m_ref = (1.0 - m_config.m_oqpskRefGain) * m_ref + m_config.m_oqpskRefGain * d;
        }
        double refMag = std::abs(m_ref);
        if (refMag > 1e-12) {
            m_ref /= refMag;
        }

        if (m_config.m_traceLoop && (m_traceFrame <= 3)
            && ((m_traceSymbol % 250) == 0) && (m_traceSymbol <= 2625))
        {
            // The rotation the loop is holding, and how far the reference is from where
            // the decisions say it should be - the residual the loop has not removed
            const double errI = std::arg(Cd(std::abs(sI), 0.0) * std::conj(Cd(1.0, 0.0)));
            (void) errI;
            fprintf(stderr, "  sym %5d  dphi %+8.2f Hz  |sI| %7.4f  |sQ| %7.4f\n",
                    m_traceSymbol, m_dphiSym * 5250.0 / (2.0 * M_PI),
                    std::abs(sI), std::abs(sQ));
        }
        m_traceSymbol++;

        double mag = 0.5 * (std::abs(sI) + std::abs(sQ));
        m_magMeanCoh = (m_magMeanCoh > 0.0) ? (0.995 * m_magMeanCoh + 0.005 * mag) : mag;
        double scale = (m_magMeanCoh > 1e-12) ? m_magMeanCoh : 1.0;

        // Error vector magnitude, exactly as detectCoherent() measures it and for the
        // same reason: the amplitude error about the decision, with the quadrature error
        // left out. The arms are staggered by half a symbol and the pulse is a raised
        // cosine, which is Nyquist at multiples of T - so at one arm's sampling instant
        // the other is nowhere near zero, and its crosstalk would swamp the figure.
        //
        // Both arms, averaged, since OQPSK carries a decision on each and either can be
        // the one degrading. Only counted for this rate here: the MSK lane has its own,
        // and this detector is the ONLY one 10500 bps runs, which is why the rate showed
        // no quality figure at all.
        const double errI = std::abs(sI) / scale - 1.0;
        const double errQ = std::abs(sQ) / scale - 1.0;
        const double meanSq = 0.5 * (errI * errI + errQ * errQ);
        m_evmMeanSq = (m_evmMeanSq > 0.0)
                    ? (0.999 * m_evmMeanSq + 0.001 * meanSq) : meanSq;

        auto quantise = [scale](double v)
        {
            return (uint8_t) std::max(0.0, std::min(255.0, 128.0 + (v / scale) * 110.0));
        };
        softQ = quantise(-sQ);      // The Q arm is inverted with respect to I
        softI = quantise(sI);
    }

    // Seed the coherent lane from the unique word, which is a known sequence and so a
    // free training burst: reconstruct the precoded symbols it must have produced, strip
    // them off, de-rotate by the frequency estimate the correlator already gave us, and
    // average. 33 symbols of data-aided estimate is a far better starting phase than
    // anything a decision-directed loop reaches on its own within a frame.
    //
    // The overall sign of the reconstruction is arbitrary (b[0] is taken as +1), which
    // does not matter: the differential decode downstream cancels any constant sign.
    void seedCoherent(int ago)
    {
        m_dphiSym = m_dphi;

        Cd acc(0.0, 0.0);
        double b = 1.0;

        for (int i = 0; i <= ACARSAERO_UW_LEN; i++)
        {
            // Symbol i of the unique word span sits (UW_LEN - i) symbols before the
            // reference symbol, which is itself at tap(ago)
            Cd y = tap(ago + (ACARSAERO_UW_LEN - i) * m_sps);
            int n = i - ACARSAERO_UW_LEN;               // Index relative to the reference
            acc += derotate(y, n) * b * std::polar(1.0, -n * m_dphiSym);

            if (i < ACARSAERO_UW_LEN) {
                b *= uwBit(i) ? 1.0 : -1.0;
            }
        }

        double mag = std::abs(acc);
        if (mag > 1e-12)
        {
            m_ref = acc / mag;
            m_cohValid = true;
        }
        else
        {
            m_ref = Cd(1.0, 0.0);
            m_cohValid = false;
        }
        // The reference symbol's own projection starts the differential decode
        m_projPrev = (derotate(tap(ago), 0) * std::conj(m_ref)).real();
    }

    // One coherent symbol: project onto the tracked reference, decide, feed the
    // frequency and reference loops, then combine with the previous projection to undo
    // MSK's precoding.
    //
    // The combination is min-sum, not the product the differential lane uses. Two soft
    // values feeding an exclusive-or have log-likelihood
    // 2 atanh(tanh(a/2) tanh(b/2)), and sign(ab) min(|a|,|b|) is its standard
    // approximation - the same one every LDPC and turbo decoder uses. A raw product
    // would let one confident symbol inflate an unreliable one.
    uint8_t softBitCoherent(const Cd& sym, int index)
    {
        if (!m_cohValid) {
            return 128;
        }

        Cd u = derotate(sym, index);
        const Cd refUsed = m_ref * std::polar(1.0, m_dphiSym);

        // Keep the reference phase UNWRAPPED. Smoothing has to happen in the phase
        // domain, not by averaging the phasor: a reference that is rotating - which it
        // is whenever there is any carrier offset - averages towards zero magnitude and
        // a biased angle, whereas a linear phase ramp averages to its own centre value
        // exactly.
        {
            double p = std::arg(refUsed);
            double d = p - m_refPhaseAcc;
            while (d > M_PI) { d -= 2.0 * M_PI; }
            while (d < -M_PI) { d += 2.0 * M_PI; }
            m_refPhaseAcc += d;
        }

        // Buffered for smoothCoherent() only, which runs on a P channel data field and
        // nowhere else. R and T bursts never reach the header, so nothing clears these
        // for them: buffering in every state let them grow without bound on an R/T
        // channel. The loop itself still runs on everything, so the EVM and the carrier
        // tracking are unchanged; and a P header's symbols were cleared out when it was
        // accepted anyway, so the data field's buffer is exactly what it was
        if (m_state == StateData)
        {
            m_symBuf.push_back(u);
            m_refPhase.push_back(m_refPhaseAcc);
        }

        Cd proj = u * std::conj(refUsed);
        double s = proj.real();
        double sign = (s >= 0.0) ? 1.0 : -1.0;

        // Phase error with the decision removed, for the frequency loop
        double residual = std::arg(proj * sign);
        m_dphiSym += m_config.m_freqGain * residual;

        // Reference blend, decision-directed. updateRef does the one symbol prediction.
        double mag = std::abs(u);
        Cd yDemod = (mag > 1e-12) ? (u * sign / mag) : Cd(1.0, 0.0);
        m_ref *= std::polar(1.0, m_dphiSym);
        m_ref = (1.0 - m_config.m_refGain) * m_ref + m_config.m_refGain * yDemod;
        double refMag = std::abs(m_ref);
        if (refMag > 1e-12) {
            m_ref /= refMag;
        }

        m_magMeanCoh = (m_magMeanCoh > 0.0)
                     ? (0.99 * m_magMeanCoh + 0.01 * std::abs(s)) : std::abs(s);
        double scale = (m_magMeanCoh > 1e-12) ? m_magMeanCoh : 1.0;

        // Error vector magnitude, from the IN PHASE component only.
        //
        // The textbook EVM would include the quadrature error too, but for MSK the
        // imaginary axis of a derotated symbol carries the ORTHOGONAL arm's crosstalk -
        // large, data dependent and nothing to do with signal quality. Including it
        // would report a terrible EVM on a perfect signal. What is left is the amplitude
        // error about the decision, which for BPSK in noise is sigma/A and so still
        // tracks SNR: about 30% at 10 dB Es/N0, 10% at 20 dB.
        //
        // Exponentially averaged rather than accumulated, so it describes the signal now
        // rather than the whole session.
        const double evmErr = std::abs(s) / scale - 1.0;
        m_evmMeanSq = (m_evmMeanSq > 0.0)
                    ? (0.999 * m_evmMeanSq + 0.001 * evmErr * evmErr)
                    : evmErr * evmErr;

        double a = s / scale;
        double p = m_projPrev / scale;
        m_projPrev = s;

        double v = ((a * p) >= 0.0 ? 1.0 : -1.0) * std::min(std::abs(a), std::abs(p));
        return (uint8_t) std::max(0.0, std::min(255.0, 128.0 + v * 110.0));
    }

    // Second pass over the frame: SMOOTH the tracking loop's carrier reference, using
    // symbols from both sides of each instant, and detect against that instead.
    //
    // The loop is causal and first order - at a blend of 0.30 its memory is about three
    // symbols, so its phase is nearly as noisy as one symbol, and it necessarily lags
    // whatever it follows. A receiver that buffers a whole frame before decoding is
    // under no such constraint: it can look forwards as well as backwards. That is the
    // one advantage this pass takes, and it is the only smoother in the receiver.
    //
    // Phase accuracy earns more here than it would on plain BPSK. In the derotated view
    // of MSK the orthogonal arm sits entirely in quadrature, so the real part rejects it
    // exactly - but only while the reference is right. An error of theta both attenuates
    // the wanted arm by cos(theta) and leaks the other one in at sin(theta), and that
    // second term is interference, not noise.
    //
    // Smoothing the reference is deliberately modest. Fitting a carrier MODEL to the
    // frame - a constant phase and a constant frequency - measures better still when it
    // is handed the right frequency (359 messages against 289), but nothing available
    // estimates that frequency well enough to extrapolate over 1152 symbols: squaring,
    // the textbook decision-free method for BPSK, is destroyed by the crosstalk on the
    // imaginary axis and returned half a radian per symbol on a signal with no offset at
    // all; stripping the modulation with the decisions is bistable, because one wrong
    // decision flips a term rather than shrinking it; and the loop's own integrator is a
    // control signal, not an estimate. Smoothing needs no frequency estimate at all.
    void smoothCoherent()
    {
        const size_t n = m_symBuf.size();
        if (!m_cohValid || (n < 64) || (m_softBitsCoh.size() != n) || (m_refPhase.size() != n)) {
            return;
        }

        const int half = std::max(1, m_config.m_smoothHalf);

        // Running sum, so the window costs one add and one subtract per symbol
        std::vector<double> prefix(n + 1, 0.0);
        for (size_t i = 0; i < n; i++) {
            prefix[i + 1] = prefix[i] + m_refPhase[i];
        }

        double magMean = 0.0;
        std::vector<double> proj(n);

        for (size_t i = 0; i < n; i++)
        {
            size_t from = (i > (size_t) half) ? (i - half) : 0;
            size_t to = std::min(n, i + half + 1);
            double mean = (prefix[to] - prefix[from]) / (double) (to - from);

            // A centred average of a phase ramp gives the ramp's value at the window
            // centre, which is only this symbol when the window is not clipped by an
            // end of the frame. Correct for that with the local slope.
            double centre = 0.5 * (double) (from + to - 1);
            double slope = (n > 1) ? ((m_refPhase[n - 1] - m_refPhase[0]) / (double) (n - 1)) : 0.0;
            double phase = mean + slope * ((double) i - centre);

            proj[i] = (m_symBuf[i] * std::polar(1.0, -phase)).real();
            magMean += std::abs(proj[i]);
        }

        magMean = (n > 0) ? (magMean / n) : 1.0;
        if (magMean < 1e-12) {
            return;
        }

        double prev = m_projPrevFirst;
        for (size_t i = 0; i < n; i++)
        {
            double a = proj[i] / magMean;
            double p = prev / magMean;
            prev = proj[i];
            double v = ((a * p) >= 0.0 ? 1.0 : -1.0) * std::min(std::abs(a), std::abs(p));
            m_softBitsCoh[i] = (uint8_t) std::max(0.0, std::min(255.0, 128.0 + v * 110.0));
        }
    }

    // Soft bit from differential detection, 255 = strong 1.
    //
    // Scaled by a running mean of the product magnitude rather than per symbol.
    // Normalising each symbol by its own magnitude throws away exactly the information
    // the Viterbi decoder wants: a symbol landing near the origin is the one that
    // should be marked uncertain, and dividing by its own small magnitude instead
    // promotes it to full confidence.
    uint8_t softBit(const Cd& sym)
    {
        // Remove the carrier offset estimated at acquisition so the product stays on
        // the real axis over the whole frame
        Cd d = differential(sym, m_prevSym) * std::polar(1.0, -m_dphi);
        m_prevSym = sym;

        double mag = std::abs(d);
        m_magMean = (m_magMean > 0.0) ? (0.99 * m_magMean + 0.01 * mag) : mag;
        double ref = (m_magMean > 1e-12) ? m_magMean : 1.0;
        // The scale factor here is not a tuning knob and was measured not to be one.
        // viterbiDecode's branch metric is |s - 255c|, so the difference between the
        // two branches is exactly 2s - 255: affine in s, which makes the survivor
        // decisions invariant to any positive scaling of the soft values. Sweeping this
        // from 1 to 1000 changed nothing at all, at any Eb/N0. 110 simply uses most of
        // the range without clipping typical symbols.
        double scaled = 128.0 + (d.real() / ref) * 110.0;
        return (uint8_t) std::max(0.0, std::min(255.0, scaled));
    }

    void processSymbol()
    {
        m_symbolCount++;    // Symbols since the unique word

        if (isOqpsk(m_rate))
        {
            // Two bits per symbol, and no differential lane: OQPSK is not precoded, so
            // there is nothing for a differential detector to recover
            uint8_t softQ, softI;
            oqpskSymbol(softQ, softI);
            consumeSoftBit(softQ, 128);
            if (m_state != StateSearch) {
                consumeSoftBit(softI, 128);
            }
            return;
        }

        // The coherent lane has to see tap(0) BEFORE the differential lane consumes it,
        // because softBit advances m_prevSym
        uint8_t softCoh = m_config.m_coherent ? softBitCoherent(tap(0), m_symbolCount) : 128;
        uint8_t soft = softBit(tap(0));
        consumeSoftBit(soft, softCoh);
    }

    void consumeSoftBit(uint8_t soft, uint8_t softCoh)
    {
        switch (m_state)
        {
        case StateHeader:
            m_headerBits.push_back((uint8_t) (soft >= 128 ? 1 : 0));
            if ((int) m_headerBits.size() == ACARSAERO_HEADER_LEN)
            {
                uint16_t header = 0;
                for (uint8_t b : m_headerBits) {
                    header = (uint16_t) ((header << 1) | b);
                }
                int distance = headerDistance(header);
                m_frameHeaderDistance = distance;
                if (distance > 0) {
                    m_stats.m_headerBad++;
                }

                // The header is the weakest thing in the frame: 16 bits, uncoded and
                // hard decided, where everything behind it is rate 1/2 coded and
                // interleaved and carries its own CRC per signal unit. It is not there
                // to check the data - it is there to stop a false sync burning a whole
                // frame, and it earns its place at correlations near the sync threshold,
                // where noise does reach.
                //
                // But a unique word that correlated at 0.96 IS a frame, and rejecting it
                // for header bit errors throws away 26 signal units that would have
                // decoded. Measured on a real off air recording: a 100 Hz tuning error
                // left the correlation at 0.96 to 0.97 and unchanged, while header
                // rejections went from 4 frames in 193 to 272 in 273 - the whole channel
                // lost, with the data behind it perfectly good. So the tolerance follows
                // the correlation: strict where the sync was marginal, and off where it
                // was emphatic.
                // OQPSK only. The MSK correlator is a different statistic - differential,
                // over the transitions between 33 symbols rather than 32 symbols - and it
                // sits near 0.9 on a clean channel, so the same threshold would disable
                // the check for those rates. Measured: it cost 600 bps P a message and
                // six signal unit CRCs on a channel with no noise in it at all.
                const int maxErrors =
                    (isOqpsk(m_rate) && (m_syncCorrelation >= m_config.m_headerTrustCorrelation))
                        ? m_config.m_headerMaxErrorsTrusted
                        : m_config.m_headerMaxErrors;
                if (distance > maxErrors)
                {
                    m_stats.m_headerRejected++;
                    m_state = StateSearch;
                                m_syncPending = false;
                    m_missedFrames = 0;
                }
                else
                {
                    m_header = header;
                    m_softBits.clear();
                    m_softBitsCoh.clear();
                    // The buffers must start where the soft bits do
                    m_symBuf.clear();
                    m_refPhase.clear();
                    m_projPrevFirst = m_projPrev;
                    // The symbol buffer has to start where the soft bits do, or the
                    // refit's own length check quietly disables it
                                m_softBits.reserve(codedBitsPerFrame(m_rate) + dummyBits(m_rate));
                    m_softBitsCoh.reserve(codedBitsPerFrame(m_rate) + dummyBits(m_rate));
                    m_state = StateData;
                }
            }
            break;

        case StateData:
            m_softBits.push_back(soft);
            m_softBitsCoh.push_back(softCoh);
            if ((int) m_softBits.size() == codedBitsPerFrame(m_rate) + dummyBits(m_rate))
            {
                decodeFrame();
                armTracking();
            }
            break;

        case StateBurst:
            m_burstBits.push_back(soft);
            tryBurstDecode();
            break;

        default:
            break;
        }
    }

    // Bits that would have to flip to make this a legal header: the format ID nibble
    // must read 1, and the two copies of the frame counter must agree. The super frame
    // marker is unconstrained, so it never contributes. 0 means already legal, 8 is the
    // worst case.
    static int headerDistance(uint16_t header)
    {
        auto bits = [](unsigned v)
        {
            int n = 0;
            while (v) { n += (int) (v & 1); v >>= 1; }
            return n;
        };
        return bits((unsigned) (((header >> 12) & 0x0F) ^ 0x01))
             + bits((unsigned) (((header >> 4) & 0x0F) ^ (header & 0x0F)));
    }

    static bool headerValid(uint16_t header) { return headerDistance(header) == 0; }

    // ---------------------------------------------------------------------------------
    // Frame decode: deinterleave block by block, Viterbi, descramble, split into
    // signal units
    // ---------------------------------------------------------------------------------

    // Deinterleave, Viterbi, descramble and pack one lane's soft bits into octets.
    //
    // The frame is decoded with BOTH ends of the encoder unknown. The start is unknown
    // because the P channel encoder runs continuously across frame boundaries, and the
    // end because the frame is exactly full of data with no room for a flush.
    //
    // Handing the previous frame's final state over as this frame's start looks free -
    // the encoder is continuous, so it is even exact in principle - and measures worse:
    // the first signal unit fell from 95% to 81%. The reason is that the state handed
    // over comes from the traceback's END, which is precisely the unflushed part of the
    // previous frame and so its least reliable bits. Being confidently wrong about the
    // start state costs more than admitting it is unknown.
    void decodeLane(const std::vector<uint8_t>& soft, std::vector<uint8_t>& octets) const
    {
        const int cols = interleaverCols(m_rate);
        const int blockLen = ACARSAERO_INTERLEAVER_ROWS * cols;
        const int coded = codedBitsPerFrame(m_rate);
        const int dummies = dummyBits(m_rate);

        // Decoding THROUGH the frame boundary was tried and made no difference at all.
        // The last signal unit sits at the traceback's starting edge, so extending the
        // block with the next frame's first interleaver block - a genuine continuation,
        // because the P channel encoder runs continuously - should have given the
        // traceback a future to converge on. On a live capture whose signal units fell
        // 87/87/82/77/74/74 percent across the frame it moved the total from 1250 to
        // 1251 of 1560, and left the profile unchanged. So the traceback converges long
        // before the frame ends, and whatever costs the later units is not this.
        std::vector<uint8_t> de(coded);
        for (int b = 0; b < coded / blockLen; b++)
        {
            deinterleave(soft.data() + dummies + b * blockLen, cols,
                         de.data() + b * blockLen);
        }

        // Neither end of the frame is a known encoder state: the P channel encoder runs
        // continuously across frame boundaries (measured - forcing state 0 at the start
        // cost the first signal unit of three frames in four on a real recording), and
        // there is no flush at the end because the frame is exactly full of data.
        std::vector<uint8_t> bits;
        AcarsHfdlReceiver::viterbiDecode(de, coded / 2, bits, false, false);

        octets.assign(bits.size() / 8, 0);
        for (size_t i = 0; i < bits.size(); i++)
        {
            uint8_t b = (uint8_t) (bits[i] ^ scramblerBit((int) i));
            octets[i / 8] = (uint8_t) (octets[i / 8] | (b << (i % 8)));
        }
    }

    // Both lanes are decoded and the CRC arbitrates PER SIGNAL UNIT, not per frame: a
    // carrier slip part way through a frame costs the coherent lane only the units after
    // it, and the differential lane covers those. Neither lane can lose a unit the other
    // would have got, which is the same guarantee HFDL's equalizer lane gives.
    void decodeFrame()
    {
        // OQPSK has no differential lane to arbitrate against - it is not precoded, so
        // there is nothing for a differential detector to recover - and its coherent
        // output goes straight into the primary soft bits
        if (m_config.m_coherent && m_config.m_smooth && !isOqpsk(m_rate)) {
            smoothCoherent();
        }

        std::vector<uint8_t> octets;
        decodeLane(m_softBits, octets);

        std::vector<uint8_t> octetsCoh;
        bool haveCoh = m_config.m_coherent && m_cohValid && !isOqpsk(m_rate)
                    && (m_softBitsCoh.size() == m_softBits.size());
        if (haveCoh) {
            decodeLane(m_softBitsCoh, octetsCoh);
        }

        m_stats.m_framesDecoded++;
        int suOkThisFrame = 0;

        // How much of the frame came back, which is the statistic that mattered while
        // the 10500 bps frame losses were being chased: a frame is 26 signal units
        // sharing one interleaver and one coded block, so it comes back whole or not at
        // all, and "0 of 26 with a perfect header" is a very different fault from a few
        // units failing.
        if (m_config.m_traceLoop && isOqpsk(m_rate))
        {
            int here = 0;
            for (size_t i = 0; i + ACARSAERO_SU_LEN <= octets.size(); i += ACARSAERO_SU_LEN)
            {
                if (suCrcOk(octets.data() + i, ACARSAERO_SU_LEN)) {
                    here++;
                }
            }
            fprintf(stderr, "  FRAME yield %2d/26  header distance %2d of 16\n",
                    here, m_frameHeaderDistance);
        }

        for (size_t i = 0; i + ACARSAERO_SU_LEN <= octets.size(); i += ACARSAERO_SU_LEN)
        {
            const uint8_t *diff = octets.data() + i;
            bool diffOk = suCrcOk(diff, ACARSAERO_SU_LEN);
            size_t pos = i / ACARSAERO_SU_LEN;
            bool anyOk = diffOk;

            if (!anyOk && haveCoh && (i + ACARSAERO_SU_LEN <= octetsCoh.size())) {
                anyOk = suCrcOk(octetsCoh.data() + i, ACARSAERO_SU_LEN);
            }
            if (pos < ACARSAERO_MAX_SUS)
            {
                if (anyOk) {
                    m_stats.m_suOkByPos[pos]++;
                } else {
                    m_stats.m_suBadByPos[pos]++;
                }
            }
            if (anyOk) {
                suOkThisFrame++;
            }

            if (diffOk)
            {
                m_stats.m_suFromDifferential++;
                processSignalUnit(diff, ACARSAERO_SU_LEN);
                continue;
            }

            if (haveCoh && (i + ACARSAERO_SU_LEN <= octetsCoh.size()))
            {
                const uint8_t *coh = octetsCoh.data() + i;
                if (suCrcOk(coh, ACARSAERO_SU_LEN))
                {
                    m_stats.m_suFromCoherent++;
                    processSignalUnit(coh, ACARSAERO_SU_LEN);
                    continue;
                }
            }

            // Neither lane passed. Hand the differential one over anyway so the fill and
            // bad-CRC counters stay comparable with the single lane build.
            processSignalUnit(diff, ACARSAERO_SU_LEN);
        }

        // Commit the symbol clock phase this frame ran on if it decoded, and FLIP TO THE
        // ALIAS if it did not.
        //
        // Nothing in the unique word can choose between the two phases - OQPSK carries
        // the same 32 bit word on BOTH arms (see uwBits()), so the two are genuinely
        // indistinguishable there, which is why every criterion tried on it landed on the
        // wrong one about half the time. But there are only ever TWO candidates, half a
        // symbol apart, so a frame that decodes nothing has already identified the other
        // one as correct. That turns a coin flip repeated every frame into at most one
        // wasted frame per acquisition.
        //
        // A frame started on the wrong phase yields ZERO signal units, never a few, so
        // this is a clean signal rather than a threshold.
        // A frame that decoded NOTHING says the carried rotation no longer describes the
        // signal, so it is dropped and the next frame seeds itself from the unique word
        // again. Without this the carry is a trap rather than a shortcut: it is only ever
        // refreshed by a frame that decodes, so once it stops matching, nothing decodes,
        // and because nothing decodes it is never refreshed. The receiver stays locked to
        // the unique word the whole time - the correlator is differential and finds the
        // word whatever the offset - so it never gives up and re-acquires either.
        //
        // That is what retuning the channel did to it. Moving the frequency offset moves
        // the residual by the same amount, the carried figure is wrong by that much from
        // the next frame onwards, and the demodulator went silent until the mode or rate
        // was changed - which is the one path that resets the receiver outright.
        //
        // Seeding from the word costs nothing now: Kay's estimator over the 32 symbols is
        // good to a few Hz, which is what the carry was introduced to improve on when the
        // seed was a plain mean and 30 Hz out.
        if (isOqpsk(m_rate) && (suOkThisFrame == 0)) {
            m_haveCarriedDphi = false;
        }

        if (isOqpsk(m_rate) && (suOkThisFrame > 0))
        {
            // Taken outright rather than averaged over frames: blending the previous
            // value in at 0.5, 0.3 and 0.15 measured identical to the message, because
            // the loop converges to the same figure inside every frame and there is
            // almost no frame to frame variance left to average away.
            const double seedError = std::abs(m_dphiSym - m_frameSeedDphi);
            m_stats.m_seedErrorSum += seedError;
            m_stats.m_convergedSum += m_dphiSym;
            m_stats.m_seedCount++;
            if (seedError > m_stats.m_seedErrorMax) {
                m_stats.m_seedErrorMax = seedError;
            }
            m_carriedDphi = m_dphiSym;
            m_haveCarriedDphi = true;
        }

        if (isOqpsk(m_rate) && (m_framePhase >= 0) && (suOkThisFrame == 0)) {
            m_stats.m_phaseDeadFrames++;
        }
        if (isOqpsk(m_rate) && (m_framePhase >= 0))
        {
            const int bucket = (suOkThisFrame == 0) ? 0
                             : (suOkThisFrame < ACARSAERO_MAX_SUS / 2) ? 1
                             : (suOkThisFrame < ACARSAERO_MAX_SUS) ? 2 : 3;
            m_stats.m_frameYield[bucket]++;
            const int ph = m_framePhase % 32;
            if (suOkThisFrame == 0) {
                m_stats.m_phaseHistDead[ph]++;
            } else if (suOkThisFrame >= ACARSAERO_MAX_SUS) {
                m_stats.m_phaseHistOk[ph]++;
            }
        }
        if (isOqpsk(m_rate) && m_frameSeededFromUw)
        {
            m_stats.m_seededFromUw++;
            if (suOkThisFrame > 0) {
                m_stats.m_seededFromUwOk++;
            }
        }
        if (isOqpsk(m_rate) && (m_framePhase >= 0))
        {
            if (suOkThisFrame > 0)
            {
                m_lockedPhase = m_framePhase;
                m_phaseConfirmed = true;
                m_phaseFailures = 0;
                m_stats.m_phaseConfirms++;
            }
            else if (!m_phaseConfirmed
                     || (++m_phaseFailures >= ACARSAERO_PHASE_FAILURES))
            {
                // Flip to the other candidate. Only ever on an UNCONFIRMED phase, or
                // after several consecutive dead frames on a confirmed one: a frame that
                // decodes nothing means the wrong arm at high SNR, but near the cliff it
                // usually just means noise, and flipping then throws away a phase that was
                // right. Doing it unconditionally cost most of the sensitivity - header
                // rejections ran to 18 of 24 frames at 5 dB because each noise-killed
                // frame flipped the lock and broke the next one too.
                m_lockedPhase = (m_framePhase + m_sps / 2) % m_sps;
                m_phaseConfirmed = false;
                m_phaseFailures = 0;
                m_stats.m_phaseFlips++;
            }
        }
        expireReassemblies();
    }

    // ---------------------------------------------------------------------------------
    // R/T bursts. An R burst is five interleaver columns holding one 19 octet signal
    // unit. A T burst is five columns of header - a 6 octet header with its own CRC -
    // then three columns per 12 octet signal unit, so every three column boundary is a
    // candidate end of burst and is tried until the CRCs pass.
    // ---------------------------------------------------------------------------------

    void tryBurstDecode()
    {
        const int rows = ACARSAERO_INTERLEAVER_ROWS;
        int filled = (int) m_burstBits.size();

        if ((filled % rows) != 0) {
            return;
        }
        int cols = filled / rows;
        if (cols < ACARSAERO_BURST_HEAD_COLS) {
            return;
        }
        if (((cols - ACARSAERO_BURST_HEAD_COLS) % ACARSAERO_BURST_SU_COLS) != 0) {
            return;
        }
        if (cols > ACARSAERO_BURST_MAX_COLS)
        {
            abandonBurst();
            return;
        }

        std::vector<uint8_t> de(filled);
        if (isOqpsk(m_rate)) {
            deinterleave(m_burstBits.data(), cols, de.data());
        } else {
            deinterleaveBurstMsk(m_burstBits.data(), cols, de.data());
        }

        std::vector<uint8_t> bits;
        AcarsHfdlReceiver::viterbiDecode(de, filled / 2, bits, false);

        std::vector<uint8_t> octets(bits.size() / 8, 0);
        for (size_t i = 0; i < bits.size(); i++)
        {
            uint8_t b = (uint8_t) (bits[i] ^ scramblerBit((int) i));
            octets[i / 8] = (uint8_t) (octets[i / 8] | (b << (i % 8)));
        }

        if (cols == ACARSAERO_BURST_HEAD_COLS)
        {
            // An R burst is one 19 octet signal unit
            if ((m_channel == ChannelR) && (octets.size() >= ACARSAERO_R_SU_LEN)
                && suCrcOk(octets.data(), ACARSAERO_R_SU_LEN))
            {
                m_stats.m_bursts++;
                processSignalUnit(octets.data(), ACARSAERO_R_SU_LEN);
                endBurst();
                return;
            }
            // Otherwise it may be a T burst header, which has to pass its own CRC before
            // any signal unit behind it is worth looking at
            if ((m_channel == ChannelT) && (octets.size() >= ACARSAERO_T_HEADER_LEN)
                && suCrcOk(octets.data(), ACARSAERO_T_HEADER_LEN)) {
                return;     // Keep accumulating signal units
            }
            // Neither an R signal unit nor a T header, so this was a false correlation.
            // Give up now: holding on to the maximum burst length instead would leave
            // the receiver deaf for seconds, which at 1200 symbols/s is most of a
            // recording. Costing a false sync five columns bounds it to a quarter
            // second.
            abandonBurst();
            return;
        }

        // T burst: the header, then one signal unit per three columns.
        //
        // How many signal units there are cannot be taken from the burst length, because
        // the length is what we are trying to find. Accepting the first column count at
        // which everything passes does not work either - the ISU alone passes its own CRC,
        // so a ten unit burst was being accepted as a one unit burst and truncated, which
        // is why generated T bursts yielded signal units but never a whole message.
        //
        // JAERO takes the count from SIX BITS at the start of signal unit ONE, read once
        // eleven columns are in, as count = 2 + those bits. That works because the SSU
        // sequence numbers count DOWN: unit one of an N unit chain carries SEQNO N-2, so
        // 2 + SEQNO is N. The burst then ends at 5 + 3N columns, and no other length is
        // worth testing.
        //
        // Like JAERO, this cannot decode a single unit T burst - the count lives in a
        // unit that such a burst does not have.
        if (!suCrcOk(octets.data(), ACARSAERO_T_HEADER_LEN)) {
            return;
        }

        if (m_burstTargetSus == 0)
        {
            const int probeCols = ACARSAERO_BURST_HEAD_COLS + 2 * ACARSAERO_BURST_SU_COLS;
            if (cols < probeCols) {
                return;
            }
            size_t at = (size_t) ACARSAERO_T_HEADER_LEN + ACARSAERO_SU_LEN;
            if (octets.size() <= at) {
                return;
            }
            int count = 2 + (octets[at] & 0x3F);
            if (count >= 16) {
                count = count / 2 + 1;
            }
            if ((count < 2) || (ACARSAERO_BURST_HEAD_COLS + count * ACARSAERO_BURST_SU_COLS
                                > ACARSAERO_BURST_MAX_COLS))
            {
                abandonBurst();
                return;
            }
            m_burstTargetSus = count;
        }

        if (cols != ACARSAERO_BURST_HEAD_COLS + m_burstTargetSus * ACARSAERO_BURST_SU_COLS) {
            return;
        }

        int sus = m_burstTargetSus;
        if ((int) octets.size() < ACARSAERO_T_HEADER_LEN + sus * ACARSAERO_SU_LEN) {
            return;
        }
        for (int i = 0; i < sus; i++)
        {
            if (!suCrcOk(octets.data() + ACARSAERO_T_HEADER_LEN + i * ACARSAERO_SU_LEN,
                         ACARSAERO_SU_LEN)) {
                return;     // Not the end of the burst yet
            }
        }

        m_stats.m_bursts++;
        for (int i = 0; i < sus; i++) {
            processSignalUnit(octets.data() + ACARSAERO_T_HEADER_LEN + i * ACARSAERO_SU_LEN,
                              ACARSAERO_SU_LEN);
        }
        endBurst();
    }

    void endBurst()
    {
        m_state = StateSearch;
        m_syncPending = false;
        m_burstBits.clear();
        m_burstTargetSus = 0;
        expireReassemblies();
    }

    // ---------------------------------------------------------------------------------
    // Signal units
    // ---------------------------------------------------------------------------------

    void processSignalUnit(const uint8_t *su, int len)
    {
        if (!suCrcOk(su, len))
        {
            // Real P channels transmit all zero signal units as fill, and their CRC
            // field is zero rather than a real CRC over zeros
            if (m_config.m_acceptZeroFill && allZero(su, len))
            {
                m_stats.m_suFill++;
                return;
            }
            m_stats.m_suCrcBad++;
        m_suRate = (m_suRate < 0.0) ? 0.0 : (0.95 * m_suRate + 0.05 * 0.0);
            return;
        }

        m_stats.m_suCrcOk++;
        m_suRate = (m_suRate < 0.0) ? 1.0 : (0.95 * m_suRate + 0.05 * 1.0);
        uint8_t type = su[0];

        if (type == SuFill)
        {
            m_stats.m_suFill++;
            return;
        }

        if (type == SuUserDataIsu)
        {
            handleIsu(su, len);
            return;
        }
        if (isSsu(type))
        {
            handleSsu(su, len);
            return;
        }

        if (m_config.m_emitNonAcars) {
            emitNonAcars(su, len);
        }
    }

    void handleIsu(const uint8_t *su, int len)
    {
        (void) len;
        m_stats.m_isus++;

        IsuKey key;
        key.m_aesId = ((uint32_t) su[1] << 16) | ((uint32_t) su[2] << 8) | su[3];
        key.m_gesId = su[4];
        key.m_qNo = (uint8_t) ((su[5] >> 4) & 0x0F);
        key.m_refNo = (uint8_t) (su[5] & 0x0F);

        // Only evict for a genuinely NEW key. An ISU for a chain already in progress -
        // a retransmission, or a replacement after the ground station starts the chain
        // again - does not grow the map, so evicting to make room for it would destroy
        // an unrelated live chain for nothing.
        if ((m_reassembly.find(key) == m_reassembly.end())
            && ((int) m_reassembly.size() >= m_config.m_maxReassemblies)) {
            dropOldestReassembly();
        }

        Reassembly& r = m_reassembly[key];
        r.m_remaining = su[6] & 0x3F;
        r.m_lastOctets = (su[7] >> 4) & 0x0F;
        r.m_started = m_time;
        r.m_data.assign(su + 8, su + 10);

        if (r.m_remaining == 0)
        {
            completeReassembly(key, r);
            m_reassembly.erase(key);
        }
    }

    void handleSsu(const uint8_t *su, int len)
    {
        (void) len;
        m_stats.m_ssus++;

        int seqNo = su[0] & 0x3F;
        uint8_t qNo = (uint8_t) ((su[1] >> 4) & 0x0F);
        uint8_t refNo = (uint8_t) (su[1] & 0x0F);

        // The queue and reference numbers plus the expected sequence number identify
        // which chain this belongs to; the aircraft address appears only in the ISU.
        // SEQNO counts down and is one BELOW the count still outstanding, so the first
        // SSU of a chain of N carries N-1 and the last carries zero.
        //
        // (JAERO compares the aircraft address too, but an SSU does not carry one - it
        // reuses whatever the last ISU parsed left in a shared scratch item. Matching
        // on the queue and reference numbers alone, across all chains in flight, is
        // what the fields actually support.)
        for (auto it = m_reassembly.begin(); it != m_reassembly.end(); ++it)
        {
            if ((it->first.m_qNo != qNo) || (it->first.m_refNo != refNo)) {
                continue;
            }
            if (it->second.m_remaining != seqNo + 1) {
                continue;
            }

            Reassembly& r = it->second;
            r.m_remaining--;
            if (r.m_remaining == 0)
            {
                int octets = r.m_lastOctets;
                if ((octets < 1) || (octets > 8)) {
                    octets = 8;
                }
                r.m_data.insert(r.m_data.end(), su + 2, su + 2 + octets);
                IsuKey key = it->first;
                completeReassembly(key, r);
                m_reassembly.erase(it);
            }
            else
            {
                r.m_data.insert(r.m_data.end(), su + 2, su + 10);
            }
            return;
        }

        m_stats.m_ssuOrphan++;
    }

    void completeReassembly(const IsuKey& key, const Reassembly& r)
    {
        m_stats.m_reassembled++;

        Frame f;
        f.m_uplink = (m_channel == ChannelP);
        f.m_aesId = key.m_aesId;
        f.m_gesId = key.m_gesId;
        f.m_bitRate = bitRate(m_rate);
        f.m_channel = m_channel;

        if (looksLikeAcars(r.m_data))
        {
            f.m_isAcars = true;
            f.m_type = "ACARS";
            f.m_bytes = acarsBlock(r.m_data);
            m_stats.m_acarsFrames++;
        }
        else
        {
            f.m_type = "User data";
            f.m_bytes = r.m_data;
            m_stats.m_otherFrames++;
        }
        m_frames.push_back(f);
    }

    void emitNonAcars(const uint8_t *su, int len)
    {
        Frame f;
        f.m_uplink = (m_channel == ChannelP);
        f.m_type = suTypeName(su[0]);
        f.m_bitRate = bitRate(m_rate);
        f.m_channel = m_channel;
        f.m_bytes.assign(su, su + len - ACARSAERO_SU_CRC_LEN);

        // Signal units addressed to or from an aircraft carry the AES address in octets
        // 1 to 3 and the GES in octet 4, the same place the user data ISU has them.
        // Broadcasts do not - see suHasAesId()
        if ((len >= 6) && suHasAesId(su[0]))
        {
            f.m_aesId = ((uint32_t) su[1] << 16) | ((uint32_t) su[2] << 8) | su[3];
            f.m_gesId = su[4];
        }
        m_stats.m_otherFrames++;
        m_frames.push_back(f);
    }

    void abandonBurst()
    {
        m_stats.m_burstsBad++;
        m_state = StateSearch;
        m_syncPending = false;
        m_burstBits.clear();
        m_burstTargetSus = 0;
    }

    // Called once per decoded frame or finished burst, not per sample: an ISU/SSU chain
    // spans several frames at 600 bps, so frame cadence is fine resolution for a
    // timeout measured in a minute or two, and per-sample would be 19200 map walks a
    // second for nothing.
    void expireReassemblies()
    {
        if (m_reassembly.empty()) {
            return;
        }
        for (auto it = m_reassembly.begin(); it != m_reassembly.end(); )
        {
            if (m_time - it->second.m_started > m_config.m_reassemblyTimeout)
            {
                m_stats.m_reassemblyTimeouts++;
                it = m_reassembly.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void dropOldestReassembly()
    {
        auto oldest = m_reassembly.begin();
        for (auto it = m_reassembly.begin(); it != m_reassembly.end(); ++it)
        {
            if (it->second.m_started < oldest->second.m_started) {
                oldest = it;
            }
        }
        if (oldest != m_reassembly.end())
        {
            m_stats.m_reassemblyTimeouts++;
            m_reassembly.erase(oldest);
        }
    }

    uint16_t m_header = 0;
};

#endif // INCLUDE_ACARSAERO_H
