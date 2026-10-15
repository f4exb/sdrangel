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

#ifndef MODEMDVB_DVBS2PL_H
#define MODEMDVB_DVBS2PL_H

#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "dvbs2.h"

namespace dvbs2 {

//! The data symbols of a PLFRAME, ready to demap
struct MODEMDVB_API XFECFrame
{
    int m_modcod = 0;
    int m_type = 0;                 //!< TYPE: short frame in bit 1, pilots in bit 0
    Modulation m_modulation = Modulation::QPSK;
    CodeRate m_rate = CodeRate::R1_2;
    FECType m_fecType = FECType::Normal;
    uint64_t m_sequence = 0;        //!< PLFRAMEs (dummies included) since the decoder's reset
    bool m_gap = false;             //!< Frames were lost before it
    bool m_suspect = false;         //!< Its header or the next one's wasn't found: probably cut by lost samples
    bool m_timeSlicing = false;     //!< Of EN 302 307-1 Annex M's time slicing (its PLHEADER two slots)
    int m_tsn = -1;                 //!< Time slicing, its time slice number: -1 if its header wasn't found
    //! A VL-SNR frame's header's index (m_modulation, m_rate and m_fecType its code's), else -1. Its cells are those
    //! sent: the shortened and punctured bits not, each spread bit's every time; pi/2 BPSK's turned to the real axis
    int m_vlsnr = -1;
    std::vector<Cf> m_cells;        //!< Carrier corrected, normalised to the constellation and descrambled
    float m_noise = 1.0f;           //!< Variance per dimension of the cells' noise
};

//! DVB-S2 physical layer (EN 302 307 5.5): symbols at one a symbol (from the front end) in, XFECFRAMEs out.
//!
//! - Frame sync: the PLHEADER found by differential correlation (each symbol times the last one's conjugate, which a
//!   frequency offset only turns) with the SOF's 25 known differences and the PLS code's 32 pairs (each pair's
//!   difference is known but for b7's sign), against the metric's own statistics; then confirmed by the next header,
//!   where the decoded PLS code says it will be. Locked, every frame's header is checked, and three missing in a row
//!   start the search again.
//! - PLS code: coherently, after the SOF, soft correlated with the (32, 7) code's codewords for both values of b7, and
//!   of b0: S2X's (b0 = 1) are a quarter turn on from the SOF, so its PLS codes are decoded from the quadrature.
//! - Carrier: each frame's known symbols (the header, the pilot blocks, the next header) with the modulation taken off
//!   give the frequency by Luise and Reggiannini's estimator (to a few percent of the symbol rate), then the phase steps
//!   from block to block refine it. Each data symbol's phase is interpolated between the blocks either side. Gain and
//!   noise come from the known symbols, and the APSK data's scale from its power (the standard allows the outer ring at
//!   the pilots' amplitude, as well as unit energy).
//! - Without pilots: a decision directed loop runs from the header. The frequency's tracked frame to frame from the
//!   headers' phase steps, which give it only to within a turn a frame: a new lock's first frames try the rates they
//!   allow, whole turns apart, and take the one the loop leaves the data nearest its points at; short frames' headers'
//!   own estimates, averaged over frames, correct it later (and a lock whose loop keeps ending far from the next
//!   header's phase is dropped, as an alias). The phase noise comes from the headers' steps too, and sets the
//!   loop's bandwidth: narrow in white noise, wide enough to follow an LNB's.
class MODEMDVB_API PLDecoder
{
public:
    enum class State { Searching = 0, Locked = 2 };

    struct Status
    {
        State m_state = State::Searching;
        int m_modcod = -1;              //!< Of the last frame
        int m_type = 0;
        uint64_t m_frames = 0;          //!< PLFRAMEs decoded (not dummies)
        uint64_t m_dummyFrames = 0;
        uint64_t m_headerMisses = 0;    //!< Headers expected but not found
        uint64_t m_syncLosses = 0;
        uint64_t m_falseLocks = 0;      //!< New locks dropped at once, their known symbols too noisy for any MODCOD
        double m_frequency = 0.0;       //!< Cycles a symbol, of the carrier left after the front end
        double m_esn0 = 0.0;            //!< dB, from the known symbols
        double m_mer = 0.0;             //!< dB, of the data symbols, from the nearest points
        double m_phaseNoise = 0.0;      //!< Radians rms a symbol, of the phase's random walk, from the pilots
        uint64_t m_reservedFrames = 0;  //!< Of S2X's reserved PLS codes (their length known): skipped
        uint64_t m_modcodsSeen[2] = {0, 0}; //!< Bit for each MODCOD received (S2X's from 32)
        //! The spectrum's inverted (I and Q swapped): the symbols are conjugated, m_frequency is of them
        bool m_inverted = false;
        bool m_timeSlicing = false;     //!< The lock's frames are time slicing's (EN 302 307-1 Annex M)
        int m_tsn = -1;                 //!< Of the last frame, time slicing
        int m_vlsnr = -1;               //!< Of the last frame, a VL-SNR frame's header's index
        uint32_t m_vlsnrSeen = 0;       //!< Bit for each VL-SNR header's index received
        uint64_t m_vlsnrFrames = 0;     //!< VL-SNR frames decoded (in m_frames)
        //! VL-SNR's scrambling found to start after the PLHEADER, through the VL-SNR header (as the standard can be read),
        //! rather than after it
        bool m_vlsnrScramblingThroughHeader = false;
    };

    typedef std::function<void(XFECFrame& frame)> FrameCallback;

    PLDecoder();

    void setFrameCallback(FrameCallback callback) { m_callback = callback; }
    //! The Gold code n of the physical layer scrambling, 0 by default
    void setScramblingCode(int n);
    void reset();
    //! Symbols were skipped (searching): the search goes on from the next, with what it's learnt of the noise
    void discontinuity();
    void process(const Cf *symbols, int nbSymbols);
    const Status& getStatus() const { return m_status; }
    //! A large frequency offset (over 5 % of the symbol rate) for the front end's NCO to take out, in cycles a symbol
    //! of its input (so negated if the spectrum's inverted), else 0. Taking it starts the search again, as the symbols
    //! already received are at the old frequency
    double takeFrequencyCorrection();
    //! The symbol clock's error, from VL-SNR frames' headers slipping, for the front end's timing loop: ppm to add to
    //! its clock (positive: the signal's faster), else 0. At VL-SNR's lowest SNRs, the loop's too narrow to pull in a
    //! clock offset itself (and wider, it wanders too far). Taken, no more are measured till clockCorrected()
    double takeClockCorrection();
    //! The correction's made, for the symbols from the next given: slips are measured again from them (those before,
    //! perhaps many queued for a shared front end's DVB-S2 thread, would ask for it again)
    void clockCorrected();
    //! Symbols the search can take to lock, without a header missed: VL-SNR's search starts only after a while, and
    //! time slicing's metric after longer, then a header and the next are found. A restart sooner never locks
    static int64_t acquisitionSymbols();
    //! A header VL-SNR's search found is waiting for its next (a frame later) to confirm it
    bool vlsnrCandidatePending() const;
    //! Up to maxCells of the last frame's data cells, for display
    void getConstellation(std::vector<Cf>& cells, int maxCells) const;

private:
    struct Header
    {
        bool m_valid = false;
        int m_modcod = 0;
        int m_type = 0;
        double m_margin = 0.0;          //!< Of the best codeword's correlation over the next best, 0 to 1
        double m_score = 0.0;           //!< The SOF's and the best codeword's coherent correlations, for comparing frequencies
        double m_frequency = 0.0;       //!< It was decoded at, cycles a symbol
        bool m_timeSlicing = false;     //!< EN 302 307-1 Annex M's: two slots, with the TSN
        int m_tsn = -1;
    };

    void append(const Cf *symbols, int nbSymbols);
    void trim(int64_t keepFrom);
    const Cf& at(int64_t index) const { return m_buffer[(size_t) (index - m_base)]; }
    //! length symbols from index turned by a phasor (in double), stepped a symbol at a time, and if known's given, its
    //! symbols' modulation taken off: the phasor after them
    std::complex<double> turnBack(int64_t index, int length, std::complex<double> phasor, const std::complex<double>& step,
        const Cf *known, Cf *out) const;
    //! The differential correlations at a header starting at index: SOF's and the PLS code's (S2's 32 pairs, or 32 of
    //! time slicing's repeated bits')
    void correlate(int64_t index, bool timeSlicing, Cf& sof, Cf& pls) const;
    //! The same at the 4 positions from index, at once (each the same as correlate()'s)
    void correlate4(int64_t index, bool timeSlicing, Cf sof[4], Cf pls[4]) const;
    //! |c| as std::abs's (correctly rounded), without its call to hypot: the search's metric is per symbol
    static float modulus(const Cf& c)
    {
        return (float) std::sqrt((double) c.real() * c.real() + (double) c.imag() * c.imag());
    }
    static double metric(const Cf& sof, const Cf& pls) { return (modulus(sof) + modulus(pls)) / 57.0; }
    //! Of the metric (of the kind) where there's no header, at least m_minMetricSigma
    double metricSigma(bool timeSlicing) const;
    //! The metric's threshold: sigmas over its mean where there's no header
    double metricThreshold(bool timeSlicing, double sigmas) const { return m_metricMean[timeSlicing] + sigmas * metricSigma(timeSlicing); }
    //! The kinds of header a decode tries
    enum class HeaderKind { Any, S2, TimeSlicing };
    //! Decode the PLS code of the header at index, the carrier at frequency (cycles a symbol): S2's (or S2X's), or time
    //! slicing's, or either (S2's, unless it's not clear and time slicing's correlates better). Searching, either;
    //! locked or confirming a candidate, the kind it's of (a weak header of one kind decodes now and then as the other,
    //! whose frame length would lose the lock)
    Header decodeHeader(int64_t index, double frequency, HeaderKind kind = HeaderKind::Any) const;
    static HeaderKind kindOf(const Header& header) { return header.m_timeSlicing ? HeaderKind::TimeSlicing : HeaderKind::S2; }
    //! Time slicing's PLS code from the header's symbols in phase with the SOF after it (154, pi/2 BPSK taken off):
    //! the best 16 bit word and its correlation, and the next best's, by a tail biting list Viterbi decoder
    void decodeTimeSlicing(const float *r, int& word, float& best, float& second) const;
    static int headerLength(const Header& header) { return header.m_timeSlicing ? timeSlicingHeaderSymbols : headerSymbols; }
    //! The PLFRAME's symbols, its header's included: 0 if its length isn't known
    static int frameLength(const Header& header);
    //! The header's symbols (headerLength of them)
    static void knownHeader(const Header& header, Cf *symbols);
    //! The same at frequencies either side of that given, the best: at low SNR a header's differences give the frequency
    //! only to a few percent of the symbol rate, which would turn the phase several radians across the PLS code
    Header searchHeader(int64_t index, double frequency) const;
    //! The differential correlation, the PLS code's sign aligned with the SOF's: its phase is the frequency
    Cf aligned(const Cf& sof, const Cf& pls) const;
    double localFrequency(const Cf& sof, const Cf& pls) const;
    void processFrame(int64_t start, const Header& header, int64_t next, const Header& nextHeader);
    //! A VL-SNR frame (EN 302 307-2 5.5.2): the carrier from the PLHEADER, the pilot blocks and the next header, then
    //! the VL-SNR header's index from its symbols, then the data
    void processVLSNR(int64_t start, const Header& header, int64_t next, const Header& nextHeader);
    //! A VL-SNR frame's headers at index, correlated coherently at the frequency: the PLHEADER of each set, and the
    //! VL-SNR header's rows, each with its sign in the index's sequence. The best's correlation in standard deviations
    //! of the noise's, its set and index, and the set whose PLHEADER alone correlates better
    double vlsnrHeaderScore(int64_t index, double frequency, int& set, int& vlIndex, int& plhSet) const;
    //! The next VL-SNR frame's headers at next, or a symbol or two either side (the symbol timing can slip in a frame
    //! at VL-SNR's lowest SNRs), after a frame of currentSet: their header (next moved to them), else false. A slip in
    //! the headers can make them look like another index's (the rows after it correlating as if inverted): another
    //! set's (another frame length) is taken only if its PLHEADER alone says so too
    bool findVLSNRHeader(int64_t& next, double frequency, int currentSet, Header& header) const;
    //! VL-SNR's search, a block of positions from m_vlsnrScan (see process())
    void searchVLSNR();
    //! A VL-SNR frame's headers at index, at any frequency: each set's and index's correlated coherently at every
    //! frequency (by FFT). The best's header (its frequency), if its score's over m_vlsnrPresentScore
    bool verifyVLSNR(int64_t index, Header& header, double& score);
    //! The phase left after the pilots' interpolation, slot by slot, from the decisions, taken off
    void refineSlotPhases(Cf *cells, int slots, int modcod, Modulation modulation);
    //! The phase left after the pilots' interpolation (phase noise), symbol by symbol, taken off the data cells: from
    //! the frame in m_trackValue (known symbols and data, in order). noise is the known symbols', q the phase noise's
    //! variance a symbol
    void smoothPhase(Cf *cells, int modcod, double noise, double q);
    //! The phase noise from the known blocks' phases (centres in symbols, phases unwrapped), step the frequency left
    //! (radians a symbol)
    void measurePhaseNoise(const double *centres, const double *phases, const int *lengths, int nbBlocks, double step,
        double noise);
    //! The carrier's frequency from a dummy frame, all of whose symbols are known
    void trackDummy(int64_t start, const Header& header, const Header& nextHeader);
    //! A known block's symbols (from start, the frame's first symbol's index) at frequency, their modulation off
    void modulationOff(int64_t frameStart, int64_t blockStart, int length, const Cf *known, double frequency,
        std::vector<Cf>& u) const;
    const NearestTable& nearestPoints(int modcod);
    //! Lock lost (falseLock: dropped as implausible, not counted as a loss)
    void lost(bool falseLock = false);
    //! The spectrum taken the other way up: the symbols held conjugated, and those to come
    void invert();
    //! A dummy frame: false if its known symbols say the lock's false
    bool checkDummy(int64_t start, const Header& header);
    static inline double wrap(double a);
    static double outerRingStep(const std::vector<Cf>& points);
    static inline Cf mul(const Cf& a, const Cf& b);
    static inline Cf nearestQPSK(const Cf& v);
    static inline Cf nearestPSK8(const Cf& v);
    template <bool psk8>
    static Cf pskErrorSum(const Cf *c, int n, const Cf& p);
    static void turnValues(const Cf *in, Cf *out, int n, Cf rot, const Cf& step, const uint8_t *scrambling = nullptr);

    static constexpr double m_pi = 3.141592653589793;
    static constexpr int m_maxMisses = 3;                       //!< Headers missing in a row before searching again
    static constexpr double m_candidateSigmas = 3.5;        //!< Of the metric over its mean, for a header in the search
    static constexpr size_t m_maxCandidates = 64;               //!< Waiting to be confirmed
    static constexpr double m_verifySigmas = 3.0;           //!< At the candidate's next header
    static constexpr double m_confirmSigmas = 2.0;          //!< At a header's expected place, locked
    static constexpr double m_minMetricSigma = 0.02;        //!< The metric's spread is never taken as less (noise's is about 0.06)
    static constexpr double m_minMargin = 0.05;             //!< Of the PLS code's best correlation over the next, locked
    static constexpr double m_searchMargin = 0.15;          //!< The same, searching (noise's is rarely as much)
    static constexpr double m_knownTolerance = 0.005;       //!< Cycles a symbol: a header's own frequency this far from the one known
                                                            //!< is searched around too (the known one may be a false lock's)
    static constexpr double m_strongerCandidate = 1.5;      //!< A candidate waiting with this times the metric goes first: when frames
                                                            //!< repeat (a still picture, padding), data can look like a header in each
    static constexpr double m_minLockEsN0 = -6.5;           //!< dB: a new lock's frames' known symbols below this, it's false (the
                                                            //!< lowest MODCOD's threshold is -2.35, the estimate's spread well within)
    static constexpr uint64_t m_trustedFrames = 4;              //!< Locked this long, the lock's real (a false one doesn't last)
    static constexpr double m_searchStep = 0.0025;          //!< Cycles a symbol, of the frequencies a header's decoded at in the search
    static constexpr int m_searchSteps = 20;                    //!< Either side: +/- 0.05
    static constexpr int m_lrLags = 24;                         //!< Of Luise and Reggiannini's estimator: range +/- 1/25 cycles a symbol
    static constexpr double m_coarseLimit = 2.5e-3;         //!< Cycles a symbol: settled, a frame's estimate used only beyond this (it's
                                                            //!< too noisy at low SNR for the pilots' phase steps, which wrap at 3.4e-4)
    static constexpr double m_pilotPeriod = 1476.0;         //!< Symbols from pilot block to pilot block: their steps alias at 1 / this
    static constexpr double m_vlsnrPilotPeriod = 738.0;     //!< VL-SNR frames' (737 and 739 apart in set 1, 736 and 740 in set 2)
    static constexpr uint64_t m_settledFrames = 1;              //!< Locked this long, the frequency's tracked from the pilots
    static constexpr double m_maxFrequency = 0.05;          //!< Cycles a symbol, beyond which the front end's NCO takes it
    static constexpr double m_invertedFrequency = 0.25;     //!< Cycles a symbol: a valid header this far out's of an inverted spectrum
    static constexpr double m_timeSlicingTest = 0.75;       //!< An S2 PLS code correlating under this times the SOF, time slicing's tried
    static constexpr double m_timeSlicingNoise = 0.55;      //!< Time slicing's code correlating under this times its symbols' rms: noise
    static constexpr double m_timeSlicingPairs = 0.3;       //!< Its repeated bits' agreement under this (of half their power): not tried
    static constexpr int m_sameTsnFrames = 8;                   //!< Frames in a row of one TSN, after which a frame without its header's of it
    static constexpr int64_t m_modeSymbols = 4 * 33372;         //!< Searching this long without a lock, the other kind of header's metric
                                                            //!< (S2's or time slicing's): four of the longest frames (with two, S2's
                                                            //!< longest, near threshold, were cut off before a header was confirmed)
    static constexpr int m_ddBlockSymbols = 8;              //!< Without pilots, the decision directed loop's update interval
    static constexpr double m_ddMinBandwidth = 0.0004;      //!< Of the decision directed loop, without pilots (white noise)
    static constexpr double m_ddMaxBandwidth = 0.02;        //!< (Heavy phase noise)
    static constexpr double m_ddBandwidthGain = 0.25;       //!< Of the Kalman gain, for the loop's bandwidth
    static constexpr double m_ddPhaseNoiseWeight = 0.1;     //!< Of each frame, in the phase noise's average
    static constexpr double m_ddFrequencyGain = 0.5;        //!< Of each frame's header to header frequency, without pilots
    static constexpr double m_ddDriftGain = 0.15;           //!< Of it, for its drift a frame
    static constexpr double m_ddTrustedEnd = 1.0;           //!< Radians: the loop's end from the next header's phase, within which the
                                                            //!< loop's advance is taken (else the headers', within half a turn)
    static constexpr uint64_t m_aliasSearchFrames = 2;          //!< Without pilots, a new lock's frames whose frequency alias is searched
    static constexpr double m_aliasSearchUncertainty = 0.75 * m_searchStep;  //!< Cycles a symbol either side tried at lock
    static constexpr int m_aliasConfirmTurns = 4;               //!< Turns a frame either side tried next
    static constexpr double m_aliasErrorWeight = 0.1;       //!< Of each frame, in the average of the loop's end from the next header's
    static constexpr uint64_t m_aliasErrorFrames = 10;          //!< After the search, before the average's taken as the lock's health
    static constexpr double m_aliasLostError = 1.0;         //!< Radians: the average over which the lock's dropped
    static constexpr uint64_t m_coarseAliasFrames = 16;         //!< Without pilots, frames of the headers' estimates averaged, at least
    static constexpr double m_coarseAliasTurns = 0.9;       //!< Turns a frame their average must be out
    static constexpr double m_coarseAliasSigmas = 5.0;      //!< And standard errors
    static constexpr double m_smoothedPhaseNoise = 0.004;   //!< Radians a symbol: more, the phase is tracked symbol by symbol, less
                                                            //!< (cheaper), slot by slot
    static constexpr double m_vlsnrPresentScore = 6.0;      //!< Standard deviations: a VL-SNR frame's headers there, locked (noise's
                                                            //!< best of the 32 hypotheses is rarely over 3; at -10 dB, a header's ~10)
    static constexpr int m_maxSlip = 2;                         //!< Symbols either side a VL-SNR frame's next header's looked for
    static constexpr int m_slipCorrection = 2;                  //!< Slips one way, for the clock to be corrected
    static constexpr int m_maxSlipFrames = 64;                  //!< Frames, after which slips fewer are forgotten
    static constexpr int64_t m_vlsnrSearchDelay = 2 * 33282;    //!< Searching this long without a lock, VL-SNR's search too: S2's
                                                            //!< finds VL-SNR frames above about -3 dB, and at 27.5 MS/s
                                                            //!< VL-SNR's would take much of the search's CPU
    static constexpr int m_vlsnrSearchFFT = 4096;               //!< VL-SNR's search's FFT: its headers' 990 symbols' differences, and
    static constexpr int m_vlsnrSearchBlock = 3072;             //!< the positions searched with each (at most its size less theirs)
    static constexpr double m_vlsnrSearchSigmas = 3.5;      //!< Of the search's correlation, for a peak to verify: noise's over it
                                                            //!< e^-12 of the time (a few a second at 1 MS/s, each 10 FFTs to verify);
                                                            //!< a header's about 6 at -6 dB, 4 at -8 dB
    static const Cf m_pilotValue;                           //!< A pilot symbol (and a dummy frame's data), unscrambled
    static const Cf m_quarterTurns[4];                      //!< j^R, the scrambling's

    FrameCallback m_callback;
    Scrambler m_scrambler;
    Status m_status;

    std::vector<Cf> m_buffer;       //!< Symbols from m_base
    std::vector<Cf> m_diff;         //!< Each times the last's conjugate
    int64_t m_base;
    int64_t m_end;                  //!< Index after the last symbol

    // Known patterns: the SOF's 25 differences, the PLS code's 32 pairs' differences (b7 = 0), time slicing's 77 pairs',
    // the header symbols for 0
    Cf m_sofDiff[sofSymbols];
    Cf m_plsDiff[32];
    Cf m_timeSlicingDiff[timeSlicingCodeBits];
    Cf m_zero[timeSlicingHeaderSymbols];
    Cf m_vlsnrKnown[2][vlsnrHeaderLength];          //!< Each VL-SNR set's PLHEADER, then the VL-SNR header of index 0
    float m_vlsnrSigns[vlsnrIndexes][16];           //!< Each index's sequence: each row's sign against index 0's
    //! VL-SNR's search: the next position, the conjugate spectrum of its headers' known differences, how many there are,
    //! and each assigned index's (and the dummy's) headers, with its set's PLHEADER
    int64_t m_vlsnrScan;
    std::vector<std::complex<double>> m_vlsnrDiffSpectrum;
    int m_vlsnrPairs;
    struct VLSNRHypothesis
    {
        int m_set;
        int m_index;
        std::vector<Cf> m_known;
    };
    std::vector<VLSNRHypothesis> m_vlsnrHypotheses;
    std::vector<std::complex<double>> m_vlsnrFFT;      //!< The search's block's correlation
    std::vector<std::complex<double>> m_vlsnrVerify;   //!< A verify's (in the search's block: not the same)
    std::vector<double> m_vlsnrPower;
    std::vector<std::vector<float>> m_codewords;    //!< The (32, 6) code's (b1..b6), as +/-1
    float m_b0Signs[32];                            //!< S2X's row (b0), as +/-1: added to a codeword, its signs multiply
    // Each codeword as a Walsh function and a sign (the code is first order Reed-Muller), so a 32-point fast
    // Walsh-Hadamard transform correlates with them all; -1 if one isn't (when they're correlated directly)
    int m_walsh[64];
    float m_walshSign[64];

    // Search
    int64_t m_scan;                 //!< Next header position to try
    // Of the metric where there's no header, for the thresholds: S2's, then time slicing's
    double m_metricMean[2];
    double m_metricVar[2];
    uint64_t m_metricCount[2];
    //! A header found in the search, waiting for its next to confirm it (the search goes on meanwhile, so a false one
    //! doesn't hold it up for a frame)
    struct Candidate
    {
        int64_t m_index;
        int64_t m_next;
        Header m_header;
        double m_frequency;
        double m_metric;                //!< The search's, at the header
        bool m_timeSlicing;             //!< The search's metric's kind, for the next header's
        bool m_vlsnr;                   //!< Found by VL-SNR's search: its metric's verifyVLSNR's score
    };
    std::deque<Candidate> m_candidates;

    // Locked
    int64_t m_frameStart;
    int64_t m_lockStart;            //!< The first frame's, of the lock
    bool m_implausible;             //!< The last frame's known symbols said the lock's false
    Header m_frameHeader;
    int m_misses;
    bool m_suspect;                 //!< The frame being processed: its header or the next one's missing
    bool m_frequencyKnown;
    double m_frequency;             //!< Cycles a symbol
    double m_frequencyBeforeLock;   //!< Restored if the lock turns out false (lost within a few frames), with these
    bool m_frequencyKnownBeforeLock;
    double m_phaseNoiseBeforeLock;
    uint64_t m_phaseNoiseFramesBeforeLock;
    double m_coarseAverage;         //!< Of the frames' Luise and Reggiannini estimates, settled, for aliases
    double m_vlsnrCoarseAverage;    //!< The same of VL-SNR frames', whose pilot blocks are half as far apart
    uint64_t m_vlsnrCoarseFrames;
    double m_ddEndError;            //!< Without pilots, average of the loop's phase at a frame's end from the next header's
    double m_ddPhaseNoise;          //!< Without pilots, the phase noise's variance a symbol, from the headers
    double m_ddPhaseNoiseBeforeLock;
    double m_ddDrift;               //!< Without pilots, the frequency's drift a frame, cycles a symbol
    double m_ddCoarseSum;           //!< Without pilots, of the headers' Luise and Reggiannini estimates since the search
    double m_ddCoarseSquares;
    uint64_t m_ddCoarseFrames;
    double m_ddResidual;            //!< The last frame's header to header phase step, less the frequency's
    bool m_ddResidualValid;
    uint64_t m_lockedFrames;        //!< Since lock
    uint64_t m_sequence;
    bool m_gap;
    bool m_inverted;                //!< The symbols are conjugated as they come, the spectrum being inverted
    //! The last lock was time slicing's, or searching long without a lock, the search tries it: the metric's then of its
    //! header's pairs (a header's decoded either way)
    bool m_timeSlicing;
    int64_t m_modeStart;            //!< Searching, where the metric last changed kind (S2's or time slicing's)
    int64_t m_searchStart;          //!< Where the search started (VL-SNR's runs only after a while)
    //! Locked to VL-SNR frames, their headers' slips (symbols) since the last clock correction, over these symbols and
    //! frames; the correction waiting
    int m_slipSum;
    int64_t m_slipSymbols;
    int m_slipFrames;
    double m_clockCorrection;
    bool m_clockHold;               //!< A correction taken, not yet made
    int64_t m_slipFrom;             //!< Slips are measured in frames from here (after the last correction)
    int m_tsnRepeats;               //!< Time slicing, frames in a row of the same TSN: a frame's whose header's missed, then
    double m_esn0Average;
    std::vector<double> m_dataPower;    //!< Per MODCOD, of the APSK data relative to the pilots', 0 until measured
    std::vector<Cf> m_points[nbModcods];        //!< Constellation of each MODCOD
    double m_slipAngle[nbModcods];              //!< Each's outer ring's angle between neighbouring points: a decision's slip
    NearestTable m_nearestTables[nbModcods];    //!< For the decisions, made when wanted
    //! The MODCOD whose table each uses (one made for the same points: S2's QPSK and 8PSK at every rate), -1 till wanted
    int m_nearestOf[nbModcods];
    std::vector<Cf> m_dummy;                    //!< A dummy frame's symbols, its known values taken off
    double m_phaseNoise;                        //!< Variance a symbol of the phase's random walk, from the pilots
    uint64_t m_phaseNoiseFrames;
    std::vector<double> m_slotUnwrapped;        //!< Each slot's phase from its decisions, unwrapped
    std::vector<double> m_slotPhases;
    std::vector<Cf> m_blockSymbols;             //!< A known block's, modulation off
    //! A frame's known blocks (the header, pilot blocks and next header): where, their symbols, phasor and phase
    struct KnownBlock
    {
        int64_t m_start;
        int m_length;
        Cf m_known[timeSlicingHeaderSymbols];
        Cf m_phasor;
        double m_phase;
    };
    std::vector<KnownBlock> m_knownBlocks;      //!< processFrame's, kept (made each frame, they were 35 KB to allocate and zero)
    //! A VL-SNR frame's, its symbols elsewhere (its VL-SNR header's 900)
    struct VLSNRBlock
    {
        int64_t m_start;
        int m_length;
        const Cf *m_known;
        Cf m_phasor;
        double m_phase;
    };
    std::vector<VLSNRBlock> m_vlsnrBlocks;      //!< processVLSNR's, and the trials of its scrambling's start
    std::vector<VLSNRBlock> m_vlsnrTrial;
    std::vector<Cf> m_vlsnrPilotKnown[2];       //!< The pilot blocks' symbols, in order, for each start
    Cf m_vlsnrFirstKnown[timeSlicingHeaderSymbols];     //!< Its header's, the next's, and its VL-SNR header's
    Cf m_vlsnrLastKnown[timeSlicingHeaderSymbols];
    Cf m_vlsnrHeaderKnown[vlsnrHeaderSymbols];
    std::vector<Cf> m_vlsnrSmoothed;            //!< The blocks' phasors, smoothed
    std::vector<double> m_blockCentres;         //!< The known blocks', for the phase noise
    std::vector<double> m_blockPhases;
    std::vector<int> m_blockLengths;
    std::vector<Cf> m_trackValue;               //!< The frame for the smoother: known symbols (modulation off) and data
    std::vector<uint8_t> m_trackKnown;
    std::vector<float> m_trackForward;          //!< The smoother's estimates each way, and their variances
    std::vector<float> m_trackForwardVariance;
    std::vector<Cf> m_trackForwardRot;          //!< e^-j the forward's phase
    std::vector<float> m_trackBackward;
    std::vector<float> m_trackBackwardVariance;
    std::vector<float> m_pointInvPower;         //!< The smoother's: each point's inverse power, and decision variance
    std::vector<float> m_pointNoise;
    std::vector<std::pair<int, int>> m_segmentSlips;    //!< Each run of data's forward and backward slips, in points
    std::vector<Cf> m_display;
    XFECFrame m_frame;
};

} // namespace dvbs2

#endif // MODEMDVB_DVBS2PL_H
