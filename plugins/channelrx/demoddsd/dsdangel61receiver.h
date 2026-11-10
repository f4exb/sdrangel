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

#ifndef INCLUDE_DSDANGEL61RECEIVER_H
#define INCLUDE_DSDANGEL61RECEIVER_H

#include <complex>
#include <cstdint>
#include <functional>
#include <vector>

#include "dsp/dv/dvangel61.h"

/**
 * Angel61 revision 3's VHF receiver (ANGEL61.md section 5, plan item C3): revision 2's linear
 * receiver (DSDAngel61Receiver) on revision 3's NB and WB layouts. Fed 48 kS/s complex IQ centred on
 * the channel (the receive root raised cosine is the channel filter), it finds 80 ms frames by their
 * unique word, estimates frequency, timing and the channel gain from the known symbols - the UW, the
 * pilots and, once decoded, the header's 48 symbols - and hands the header and payload to
 * DVAngel61's decoders.
 *
 * What differs from revision 2 (2.2, 2.7): 384 and 768 symbols a frame; 48 and 40 anchor groups;
 * the header's 24 coded symbols sent twice, their copies' LLRs added before the Golay decode (so
 * revision 2's header prediction is not needed, section 5); every block's CRC-12 covers the header;
 * no split types. The signal processing is revision 2's, unchanged.
 *
 * No Qt, no plugin dependency: the harness measures it before it is integrated.
 */
class DSDAngel61Receiver
{
public:
    /**
     * Revision 3's VHF layout (DVAngel61::layout) as the receiver reads it: symbol kinds and
     * indices, the anchor groups (runs of pilots and header symbols between payload parts, 2.2),
     * the known values, the pulse
     */
    struct Layout
    {
        DVAngel61::Profile m_profile;
        int m_symbolRate;                     //!< 4800 or 9600
        int m_samplesPerSymbol;               //!< 10 or 5
        int m_frameSymbols;                   //!< 384 or 768
        int m_payloadSymbols;                 //!< 296 or 664
        int m_pilotSymbols;                   //!< 24 or 40
        int m_headerSymbols;                  //!< 48: the 24 coded header symbols and their copies
        int m_rrcTaps;
        int m_nbGroups;                       //!< anchor groups: 48 (NB) or 40 (WB)
        std::vector<int> m_pilotPositions;    //!< frame symbol index of each pilot
        std::vector<int> m_headerPositions;   //!< frame symbol index of header symbol h (0..47, frame order; it carries coded symbol h mod 24)
        std::vector<int> m_payloadPositions;  //!< frame symbol index of each payload symbol, in payload order
        std::vector<signed char> m_symbolKind;//!< 0 UW, 1 pilot, 2 header, 3 payload
        std::vector<short> m_symbolIndex;     //!< index within its kind
        std::vector<signed char> m_symbolGroup;
        std::complex<float> m_uw[16];
        std::vector<std::complex<float>> m_pilots;

        int frameSamples() const { return m_frameSymbols * m_samplesPerSymbol; }
        std::complex<float> uwSymbol(int i) const { return m_uw[i]; }
        std::complex<float> pilotSymbol(int i) const { return m_pilots[i]; }
        int payloadSymbolIndex(int p) const { return m_payloadPositions[p]; }
        int payloadIndexOfSymbol(int s) const { return m_symbolKind[s] == 3 ? m_symbolIndex[s] : -1; }
        int headerIndexOfSymbol(int s) const { return m_symbolKind[s] == 2 ? m_symbolIndex[s] : -1; }
        int groupOfSymbol(int s) const { return m_symbolGroup[s]; }
        //! True for a UW or pilot symbol, its value in `value`
        bool knownSymbol(int s, std::complex<float>& value) const
        {
            if (m_symbolKind[s] == 0) { value = m_uw[m_symbolIndex[s]]; return true; }
            if (m_symbolKind[s] == 1) { value = m_pilots[m_symbolIndex[s]]; return true; }
            return false;
        }
    };
    static const Layout& layout(DVAngel61::Profile profile);

    static const int UWSymbols = 16;
    static const int MaxFrameSymbols = 768;
    static const int MaxPayloadSymbols = 664;
    static const int MaxHeaderSymbols = 48;
    static const int MaxGroups = 48;
    static const int MaxCodedBits = 4 * MaxPayloadSymbols;
    static const int MaxRrcTaps = DVAngel61::MaxRrcTaps;

    struct Frame
    {
        int64_t m_sample = 0;                //!< Input sample index of the frame's first UW sample
        double m_position = 0.0;             //!< The same, to the fraction of a sample the receiver used
        DVAngel61::Header m_header;
        bool m_payloadOk = false;         //!< CRC-12 of the payload block valid (it covers the header, 2.7)
        bool m_coarseOk = false;          //!< As m_payloadOk (revision 3 has no split types; kept for the session's interface)
        std::vector<uint8_t> m_block;     //!< The k information bits when valid (part 0's when only it is)
        float m_frequencyHz = 0.0f;       //!< Carrier offset estimate used for the frame
        float m_snrDB = 0.0f;             //!< Es/N0 from the known symbols' residuals
        float m_uwCorrelation = 0.0f;     //!< Normalised UW correlation (1 = perfect)
        float m_gainVariation = 0.0f;     //!< Frame length over the gain smoothing window the frame chose (1 = static channel)
        int m_iterations = 0;             //!< LDPC iterations, -1 when the syndrome did not clear
        int m_channelErrors = 0;          //!< The payload's channel bit errors the code corrected (when valid)
        int m_codedBits = 0;              //!< Of the payload's coded bits (the parts that decoded)
    };

    struct Status
    {
        bool m_locked = false;            //!< Frames are expected at the frame period
        bool m_confirmed = false;         //!< A header and its payload decoded since the lock
        int64_t m_firstSyncSample = -1;      //!< First UW detection that led to a decoded header
        int m_uwDetections = 0;           //!< UW candidates examined
        int m_headerOk = 0;
        int m_headerFailed = 0;
        int m_payloadOk = 0;
        int m_payloadCoarse = 0;          //!< Split frames of which only part 0 decoded (not counted as failed)
        int m_payloadFailed = 0;
        int m_misses = 0;                 //!< Consecutive expected frames without a header
        int m_unlocks = 0;
        int m_decodingRounds = 0;         //!< Extra decoding aided estimation rounds run
        int m_headerPredicted = 0;        //!< Headers taken from prediction when the Golay words failed
        int m_headerGated = 0;            //!< Header attempts whose copies disagreed (not decoded)
        float m_frequencyHz = 0.0f;
        float m_snrDB = 0.0f;
        float m_level = 0.0f;             //!< Symbol amplitude at the matched filter output
    };

    explicit DSDAngel61Receiver(DVAngel61::Profile profile = DVAngel61::ProfileNB);
    void reset();
    DVAngel61::Profile getProfile() const { return m_layout->m_profile; }
    const Layout& getLayout() const { return *m_layout; }
    //! Feed IQ samples at 48 kS/s
    void process(const std::complex<float> *iq, int n);
    //! n input samples not processed (another decoder holds the channel): the sample count advances, the filter's history is cleared
    void skip(int n);
    std::vector<Frame> takeFrames();
    bool hasFrames() const { return !m_frames.empty(); }
    //! Equalised payload symbols of the decoded frames (unit power), for a constellation display
    void setScopeEnabled(bool on) { m_scopeEnabled = on; }
    std::vector<std::complex<float>> takeScopeSymbols();
    //! Frame clock offset from the nominal 3840 samples, in ppm (locked)
    float getClockPpm() const { return (float) (1e6 * m_timingDrift / m_frameSamples); }
    const Status& getStatus() const { return m_status; }

    //! UW detection threshold while searching (normalised correlation, default 0.62)
    void setSearchThreshold(float t) { m_searchThreshold = t; }
    //! Frequency hypotheses for the UW search, 500 Hz apart, n each side of zero (default 3: +-1.5 kHz)
    void setFrequencyHypotheses(int n);
    //! Decision feedback within the payload parts (default on)
    void setDecisionFeedback(bool on) { m_decisionFeedback = on; }
    //! Debug prints to stdout
    void setDebug(bool on) { m_debug = on; }
    //! Studies only: the true channel at an input sample (the harness's), and what it is used for:
    //! 1 the payload equalised by it (the channel known, the receiver's own timing and frequency),
    //! 2 the frame's residual frequency from it, the estimator otherwise the receiver's own
    void setGenie(std::function<std::complex<float>(double)> genie, int mode) { m_genie = genie; m_genieMode = mode; }

    static const int SampleRate = 48000;
    //! A frame and the next frame's UW, the tail's anchor, in the larger profile: fixed buffers
    static const int MaxExtendedSymbols = MaxFrameSymbols + UWSymbols;

private:
    static const int RingSize = 8192;                                //!< Filtered samples kept (> a frame + margins)
    static const int RingMask = RingSize - 1;
    static const int PeakLookahead = 6;                              //!< Samples after a candidate before it is called a peak
    static const int LockedWindow = 6;                               //!< +- samples searched for the UW when locked
    static const int LockedMetricWindow = LockedWindow + 8;          //!< +- samples around an expected UW whose metrics are worked out when locked
    static const int MaxMisses = 5;
    static constexpr float AliasMargin = 1.2f;                      //!< An acquisition's slope alias taken when its known symbols are this much more coherent
    static const int AliasScanSteps = 64;                            //!< The alias scan's steps a turn
    static constexpr float AliasCoherence = 0.75f;                  //!< ... and when its coherent sum is this much of the known symbols' magnitudes (a steady channel)
    static const int MaxPending = 8;                                 //!< UW candidates awaiting their frame while searching (4 let a first frame's own side peaks, 0.84 to 0.88 at other frequencies, push its UW out: VHF results section 13)
    static constexpr double EstimateRatioKnee = 1.2;                 //!< The fit's error, against the noise, beyond which the decoder is told more of it (trackGains)
    static constexpr double EstimateScaleMax = 4.0;                  //!< ... at most this many times as much
    static constexpr double TimingGain = 0.05;                       //!< Frame clock loop: proportional gain on the UW's position error
    static constexpr double TimingDriftGain = 0.005;                 //!< and integral gain (the sample clock offset)
    static constexpr double TimingCorrectionGain = 0.25;             //!< Of a frame's timing from its known symbols (m_timingCorrection), into the next frame's (twice that for the first 20 frames of a lock)
    static constexpr double TimingCorrectionDriftGain = 0.05;        //!< and into the frame clock's drift (four times for the first 8 frames, twice to the 20th)
    static constexpr double TimingResampleSymbols = 0.1;             //!< A frame whose known symbols put it this fraction of a symbol off is taken again
    static constexpr float TrackerGainMin = 0.05f;                   //!< First order gain trackers of the first decision pass: gain = 1 / (TrackerGainSnr x noise variance) clamped
    static constexpr float TrackerGainMax = 0.5f;
    static constexpr float TrackerGainSnr = 50.0f;
    //! Standard deviations of two anchors' phase difference within which their disagreement is
    //! taken for their own noise rather than for fading (interpolateGain)
    static constexpr float NoiseAngleSigmas = 3.0f;
    //! Bessel J0, for the Jakes correlation of a fading channel
    static double besselJ0(double x);
    /**
     * The minimum mean square error (Wiener) estimate of the channel from the symbols the
     * format makes known - the unique word, the header, the pilot pairs and the next frame's
     * unique word - for a Jakes spectrum of each candidate Doppler frequency.
     *
     * With A = R + noise I over the anchors (R their Jakes correlation) and y their gains, the
     * estimate at symbol s is r_s' A^-1 y, r_s the correlation of s with the anchors, and an
     * anchor's leave one out error is (A^-1 y)_a / (A^-1)_aa (a linear smoother's (y - g) /
     * (1 - S_aa)). A depends on where the anchors are and on the noise, not on what was
     * received, and the format puts them in the same places in every frame: A's Cholesky
     * factor and the diagonal of its inverse are solved when the noise estimate has moved, and
     * a frame is two triangular solves per candidate and a sum per symbol. What the fit does
     * not know at each symbol, 1 - r_s' A^-1 r_s, is worked out for a candidate when it is
     * first chosen.
     *
     * Revision 2's receiver solved every symbol's weights for every candidate: in fading, where
     * the noise estimate moves on most frames, that was 29 ms a frame in NB and 67 ms in WB,
     * nineteen twentieths of the receiver.
     */
    struct ChannelFit
    {
        int m_nbSymbols = 0;                                         //!< Frame length the fit interpolates to (a frame, or a frame and the next UW)
        int m_nbAnchors = 0;
        float m_noise = -1.0f;                                       //!< Noise to signal ratio it was solved for
        std::vector<int> m_anchors;
        std::vector<double> m_correlation;                           //!< [candidate][lag]: J0 at every whole number of symbols
        std::vector<double> m_factor;                                //!< [candidate][anchor][anchor]: A's Cholesky factor (lower)
        std::vector<double> m_factorTransposed;                      //!< [candidate][anchor][anchor]: its transpose, for the back substitution's rows
        std::vector<double> m_inverseDiagonal;                       //!< [candidate][anchor]: (A^-1)_aa
        std::vector<float> m_errorVariance;                          //!< [candidate][symbol]: what the fit itself does not know, in units of the channel's power
        std::vector<char> m_errorValid;                              //!< [candidate]: m_errorVariance worked out
    };
    //! Solve one candidate into a ChannelFit
    void solveFit(ChannelFit& fit, int candidate, float dopplerHz) const;
    //! The fit for these anchors, this length and this noise, solved again only if it has moved
    ChannelFit& channelFit(const int *anchors, int nbAnchors, int nbSymbols, float noise);
    //! A candidate's A^-1 y into alpha; returns its leave one out error over the anchors (the sum)
    static float fitResidual(const ChannelFit& fit, int candidate, const std::complex<float> *y, std::complex<double> *alpha);
    //! A candidate's estimate at every symbol from its A^-1 y
    static void fitGains(const ChannelFit& fit, int candidate, const std::complex<double> *alpha, std::complex<float> *gain);
    //! What a candidate's estimate does not know at every symbol
    static const float *fitErrorVariance(ChannelFit& fit, int candidate);

    static constexpr int MaxFitCandidates = 9;                       //!< Doppler hypotheses of the channel fit, at most
    //! The profile's Doppler hypotheses (Hz) and their number
    static const float *fitGrid(DVAngel61::Profile profile, int& count);
    static constexpr float FitNoiseHysteresis = 3.0f;                //!< Factor the noise estimate must move by before the fit is solved again (4.8 dB)
    static constexpr float FitMargin = 0.7f;                         //!< The fit must beat the smoothing window by this factor to be used: on a static channel the window averages the whole frame and the fit only its anchors, which the comparison over the anchors cannot see
    static constexpr float WindowPenalty = 3.0f;                     //!< A smoothing window of w symbols must beat a longer one by the factor 1 + this / w
    static constexpr float HypothesisStepHz = 500.0f;                //!< Spacing of the UW search frequency hypotheses
    static constexpr float RecentreHz = 250.0f;                      //!< Offset from the filter centre beyond which a decoded frame re-centres it
    static constexpr float TrackedSlopeLimitHz = 40.0f;              //!< Largest frequency correction the pilots may apply to a tracked frame
    static constexpr float FadingFrequencyGain = 0.25f;              //!< In fading, the tracked frequency's pull per frame towards the frame's known symbols' estimate
    static constexpr float FitFadingHz = 100.0f;                     //!< A fit that chose this Doppler or more is used whatever the window's residual, and the frame is fading (no phase slope removed)
    static constexpr float FadingSlopeHz = 100.0f;                   //!< A frame whose window is short and whose fit chose this Doppler or more is fading: no phase slope is removed from it
    static const int DecodingRounds = 6;                             //!< Further channel estimate + decode rounds after a failed payload
    static constexpr float HeaderCopyGate = 0.2f;                    //!< Least correlation of the header copies' LLRs for the Golay words to be decoded
    static const int LocalAnchors = 24;                              //!< Anchors of the local fit of a decoding round (localFit)
    static constexpr float ReferenceConfidence = 2.0f;               //!< Smallest posterior |LLR| of a re-encoded symbol's bits for it to serve as a reference

    // The profile (2.1): its layout and what follows from it
    const Layout *m_layout;
    int m_sps;                                                       //!< Samples per symbol: 10 in NB, 5 in WB
    int m_frameSymbols;
    int m_frameSamples;                                              //!< 3840 in both profiles
    int m_uwSpan;                                                    //!< First to last UW symbol sample: 150 in NB, 75 in WB
    int m_extendedSymbols;                                           //!< A frame and the next frame's UW, its tail's anchor
    int m_decodeLookahead;                                           //!< Samples after a frame's start before it is decoded (the next UW included)
    const float *m_fitDopplers;                                      //!< The channel fit's Doppler hypotheses (Hz): fitGrid
    int m_nbFitCandidates;

    // Matched filter
    float m_rrc[MaxRrcTaps];                              //!< The root raised cosine
    float m_reversedRrc[MaxRrcTaps];                      //!< The same, oldest sample's tap first
    float m_filterCentreHz;                                          //!< Where the matched filter's passband is centred
    std::vector<std::complex<float>> m_delay;                        //!< Circular delay line of input samples (to mix again when the centre moves)
    std::vector<float> m_mixedRe, m_mixedIm;                         //!< The input mixed down by the filter centre, each sample twice (a taps long window ends at every index)
    int m_delayIndex;
    std::complex<double> m_mixer;                                    //!< exp(-j w n) for the next sample
    std::complex<double> m_mixerStep;                                //!< exp(-j w)
    int m_mixerCount;                                                //!< Samples since the mixer was renormalised

    // Filtered history and UW metric history, indexed by input sample number
    std::vector<std::complex<float>> m_ring;
    std::vector<float> m_metric;                                     //!< UW correlation metric per candidate start
    int64_t m_sampleIndex;                                              //!< Input samples consumed
    int64_t m_filteredCount;                                            //!< Filtered samples produced (= m_sampleIndex, the filter delay is absorbed)

    struct KnownGroup
    {
        float m_centre;               //!< Symbol position of the group's centre
        std::complex<float> m_gain;   //!< Channel gain estimate from the group
        float m_weight;               //!< Number of symbols
    };

    // Frame search and tracking
    enum State { StateSearch, StateLocked };
    State m_state;
    struct Candidate { int64_t m_start; float m_metric; };
    std::vector<Candidate> m_pending;                                //!< UW peaks whose frames are awaited, in order
    double m_expectedStart;                                          //!< Locked: where the next UW starts (fractional)
    double m_trackPosition;                                          //!< Locked: the position of the frame being awaited (-1 none)
    int64_t m_lockStart = -1;                                        //!< The UW start the search locked on
    float m_trackMetric;
    double m_timingDrift;                                            //!< Samples per frame the frame clock gains on the nominal 2880
    double m_timingCorrection;
    int m_framesLocked;                                              //!< Frames tracked since the lock (the timing loop's gear)                                       //!< The last frame's timing from its known symbols less its UW's (samples; 0 when its header failed)
    bool m_lastFrameSeen;                                            //!< The last frame decoded, its payload with it
    bool m_lastFrameClaimed;                                         //!< A header said last frame, its payload did not decode
    bool m_haveLastHeader;                                           //!< Confirmed and a header decoded since
    DVAngel61::Header m_lastHeader;                                //!< The last header decoded
    float m_frequencyHz;                                             //!< Tracked carrier offset
    float m_noiseVariance;                                           //!< Of the equalised symbols, last frame
    float m_gainVariation;                                           //!< Frame length over the gain smoothing window chosen, last frame (1 = static)
    float m_dopplerHz;                                               //!< Doppler the channel estimate settled on, last frame
    float m_fitDopplerHz;                                            //!< Doppler the fit chose (used or not), last pass: the known symbols' measure of fading
    float m_fitError[MaxExtendedSymbols];                               //!< Variance of the channel estimate itself per symbol, last frame (0 where the smoothing window was used)
    bool m_windowOnly;                                               //!< DSD_ANGEL61_WINDOW: the smoothing window only, no MMSE interpolation
    std::function<std::complex<float>(double)> m_genie;              //!< Studies: the true channel (setGenie)
    int m_genieMode = 0;
    ChannelFit m_channelFit[4];                                      //!< The fit's weights: the anchors with and without the header, each with and without the next frame's UW
    int m_channelFitNext = 0;                                        //!< Slot the next set of anchors takes if none of them holds it
    float m_searchThreshold;
    float m_lockedThreshold;
    int m_hypotheses;
    std::vector<std::complex<float>> m_hypothesisRotation;             //!< UW de-rotation per hypothesis
    bool m_decisionFeedback;
    bool m_debug;
    bool m_scopeEnabled;
    std::vector<std::complex<float>> m_scope;
    Status m_status;
    std::vector<Frame> m_frames;

    // Working buffers for one frame
    std::complex<float> m_symbols[MaxExtendedSymbols];
    std::complex<float> m_gain[MaxExtendedSymbols];
    bool m_nextUwValid;                                              //!< The next frame's UW correlates: its symbols anchor the tail of this frame's estimate
    std::complex<float> m_headerSymbols[MaxHeaderSymbols];           //!< The decoded header re-encoded and mapped, per header symbol (copies included)
    float m_noisePerSymbol[MaxFrameSymbols];                 //!< Noise variance of each equalised symbol (the absolute noise over its gain squared)
    float m_thermalPerSymbol[MaxFrameSymbols];               //!< ... of it, the channel's noise
    float m_estimatePerSymbol[MaxFrameSymbols];              //!< ... and the estimate's error, which goes with the symbol's power (amplitude-aware 16-QAM)
    bool m_amplitudeAware;                                   //!< DSD_VHF_AMPLITUDE=1: 16-QAM's likelihoods amplitude-aware (measured and not adopted: VHF results section 9)
    std::vector<float> m_llr;

    inline const std::complex<float>& filtered(int64_t index) const { return m_ring[index & RingMask]; }
    //! Centre the matched filter's passband on a carrier offset (the taps become complex)
    void centreFilter(float hz);
    std::complex<float> interpolate(double position) const;
    //! UW correlation metric at a candidate start sample and the frequency (Hz) the segment phases give
    float uwMetric(int64_t start, float *frequency) const;
    void search();
    void track();
    //! Demodulate and decode the frame starting at position (fractional sample); returns true when the header decoded
    bool decodeFrame(double position, float uwMetric);
    //! Decision directed per symbol gain estimate: fills m_gain and equalised, returns the residual phase slope (rad/symbol)
    float trackGains(DVAngel61::Constellation constellation, std::complex<float> *equalised, const std::complex<float> *payloadReference = nullptr, const float *confidence = nullptr);
    //! The channel at every symbol by a Wiener fit to its nearest `LocalAnchors` anchors (the
    //! format's known symbols and a decoding round's references), for a Jakes spectrum at
    //! `dopplerHz`: into m_gain and m_fitError
    void localFit(const bool *known, const std::complex<float> *ref, const std::complex<float> *inst, int nbSymbols, float dopplerHz, float noise);
    static std::complex<float> hardDecision(DVAngel61::Constellation constellation, const std::complex<float>& z);
    /**
     * The residual phase slope over the known groups, unwrapped from one to the next. The
     * unwrapping is only unambiguous while the phase turns by less than half a turn across the
     * largest gap between groups, so `reference` - the unique word's own slope - is removed
     * first and added back after: what is unwrapped is then the small difference rather than
     * the whole residual.
     */
    static float phaseSlope(const std::vector<KnownGroup>& groups, float reference = 0.0f);
    /**
     * The channel gain over `n` symbols from the known groups. Between two anchors that agree
     * to within their own noise the angle is interpolated and the magnitude kept, since the
     * channel did not move; between two that disagree by more than that the complex value is
     * interpolated and allowed to fade, since what the channel did between them is unknown.
     * `noiseVariance` is the per symbol noise the anchors were measured in.
     */
    static void interpolateGain(const std::vector<KnownGroup>& groups, std::complex<float> *gain, int n,
        float noiseVariance = 0.0f);
    void unlock();
    //! An unconfirmed lock's miss: the UW candidates since the lock away from its frames, queued for the
    //! search. True when there are any (the lock is then over)
    bool rescanUnconfirmed();
    //! The payload's LLRs in position order: each symbol with its own noise, or the frame's (m_noiseVariance) when noisePerSymbol is null
    void payloadLLR(const std::complex<float> *equalised, int type, const float *noisePerSymbol, float *llr) const;
    //! Decode the payload from m_llr: the block, iterations and channel errors into the frame; true when its CRC-12 passes
    bool decodePayload(const DVAngel61::Header& header, Frame& frame, std::vector<uint8_t>& codeword, std::vector<float>& posterior) const;
};

#endif // INCLUDE_DSDANGEL61RECEIVER_H
