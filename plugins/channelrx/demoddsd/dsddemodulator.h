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

#ifndef INCLUDE_DSDDEMODULATOR_H
#define INCLUDE_DSDDEMODULATOR_H

#include <algorithm>
#include <cstdint>
#include <complex>
#include <memory>
#include <string>
#include <vector>

#include "dsp/gaussian.h"
#include "dsp/rootraisedcosine.h"

#include "dsdfrontend.h"
#include "dsdprotocol.h"
#include "dsdangel61hf.h"
#include "dsdangel61vhf.h"
#include "dsdslicer.h"
#include "dsdcoherent.h"
#include "dsdsymbolclock.h"
#include "dsdsyncsearcher.h"

/**
 * Digital voice demodulator core: 48 kHz IQ in, sliced symbols with soft
 * decisions to a protocol decoder, vocoder frames out.
 *
 *   IQ -> front end (AFC, channel filter, envelope, discriminator in Hz)
 *      -> matched filter (RRC alpha 0.2 or Gaussian, per chain)
 *      -> sync searcher (waveform regression on every sample)
 *      -> symbol clock (Farrow interpolator, Mueller and Muller loop)
 *      -> slicer (level, offset and noise tracking, LLRs weighted by the envelope)
 *      -> protocol decoder
 *
 * The protocols come in three pulse shapes: 4FSK at 4800 symbols/s with root
 * raised cosine shaping (DMR, YSF, P25), the same at 2400 symbols/s (NXDN,
 * dPMR), and GMSK at 4800 bits/s (D-STAR). Each is a chain of matched
 * filter, searcher and clock that runs on the common discriminator output;
 * the chains of the enabled protocols all search, and the one that finds a
 * sync becomes the locked chain.
 *
 * Searching: every sample is tested against the sync patterns of the enabled
 * protocols; a detection with a high correlation and a plausible level locks
 * the receiver and initialises the clock, the slicer and the decoder.
 * Locked: symbols are sliced at the clock strobes; the decoder says where the
 * next sync is expected and the searcher's detections there realign the
 * clock and refresh the levels. A burst without a detection is offered to the
 * decoder for confirmation by other means; unexplained misses unlock.
 */
class DSDDemodulator
{
public:
    //! Angel61 is NB and WB (in Auto too), and HF-NB and HF-WB (OFDM, only when selected)
    enum Protocol { ProtocolAuto, ProtocolDMR, ProtocolDStar, ProtocolYSF, ProtocolNXDN48, ProtocolNXDN96, ProtocolDPMR, ProtocolP25, ProtocolP25P2, ProtocolM17, ProtocolAngel61, ProtocolAngel61WB,
        ProtocolAngel61HFNB, ProtocolAngel61HFWB };

    struct Status
    {
        bool m_locked = false;
        std::string m_protocol;
        std::string m_lastPattern;  //!< Name of the last sync pattern accepted
        bool m_inverted = false;
        float m_outerLevelHz = 0.0f;
        float m_offsetHz = 0.0f;
        float m_correctionHz = 0.0f;
        float m_snrDB = 0.0f;
        float m_clockPpm = 0.0f;
        float m_lastRho = 0.0f;
        int64_t m_firstSyncSample = -1;
        int m_syncCount = 0;
    };

    static constexpr int m_sampleRate = 48000;
    //! A symbol is present when its envelope power exceeds this multiple of the noise floor: at a
    //! post filter C/N of 6 dB the signal plus noise power is 5 times the floor, and the power
    //! over one symbol of noise alone scatters by a third of the floor
    static constexpr float m_presenceFactor = 2.5f;

    DSDDemodulator();
    ~DSDDemodulator();

    void setProtocol(Protocol protocol);
    void setRfBandwidth(float bandwidth);
    //! Narrow the channel filter to the locked signal's occupied bandwidth (default on)
    void setNarrowFilter(bool enabled) { m_narrowFilter = enabled; updateChannelFilter(); }
    //! Coherent detector on the channel filtered IQ (default) or on the unfiltered IQ (about 0.3 dB
    //! less, half the processing)
    void setCoherentFiltered(bool filtered) { m_coherentFiltered = filtered; }
    //! The coherent detector runs on a burst when the first pass is expected to hold more than
    //! this many symbol errors (from its residual); 0 runs it on every burst
    void setCoherentGate(double expectedErrors) { m_coherentGate = expectedErrors; }
    int64_t getCoherentRuns() const { return m_coherentRuns; }
    int64_t getCoherentSkips() const { return m_coherentSkips; }
    //! P25 Phase 2 scrambling: the WACN, system ID and NAC of the system listened to
    void setP25P2System(uint32_t wacn, uint16_t sysid, uint16_t nac);
    //! Experimental: re-detect every refined burst of the 4800 symbols/s 4FSK protocols coherently from the IQ
    void setCoherent(bool enabled) { m_coherent = enabled; }
    bool isCoherent() const { return m_coherent; }
    void reset();

    void process(const std::complex<float> *iq, int n);
    std::vector<DSDVoiceFrame> takeFrames();
    std::vector<DSDProtocolEvent> takeProtocolEvents();
    int64_t getFramesDiscarded() const { return m_framesDiscarded; }
    //! Display pairs in hertz: consecutive discriminator samples while searching, or symbol-timed
    //! matched-filter samples one symbol apart while locked. Cleared by the call.
    std::vector<std::pair<float, float>> takeScopeSamples();
    //! Discriminator output in hertz for every sample processed since the last call, when enabled
    void setDiscriminatorOutput(bool enabled) { m_discriminatorEnabled = enabled; }
    std::vector<float> takeDiscriminator();
    const Status& getStatus() const { return m_status; }
    bool isLocked() const { return (m_state == StateLocked) || (m_angel61Holding >= 0); }
    //! Points a second the scope is fed: the raw discriminator's while searching, the symbol rate
    //! while locked, an Angel61 frame's payload cells a frame while a session holds the channel
    int getScopeSampleRate() const;
    //! Full scale of the scope samples: the nominal outer level of the FM protocols
    float getScopeFullScale() const;
    /**
     * Angel61 (ANGEL61.md section 5): a session per profile listened for, on the IQ. Selected, its
     * profile's alone, the FM chains idle; in Auto, NB's and WB's beside the FM chains, and the one
     * whose transmission is confirmed (a frame through its CRC-12) holds the channel while it lasts,
     * the FM chains idle
     */
    bool isAngel61Selected() const { return m_angel61Selected; }
    bool hasAngel61Sessions() const { return !m_angel61Sessions.empty(); }
    std::vector<DSDAngel61Session*> getAngel61Sessions();
    //! The session the reports are of: the selected profile's, or in Auto the one holding the channel (else null)
    DSDAngel61Session *getAngel61Holding();
    const DSDAngel61Session *getAngel61Holding() const;
    //! An Angel61 transmission holds the channel
    bool isAngel61() const { return m_angel61Holding >= 0; }
    //! The selected profile's session, or NB's in Auto; null when none listens
    DSDAngel61Session *getAngel61Session() { return m_angel61Sessions.empty() ? nullptr : m_angel61Sessions[std::max(0, m_angel61Holding)].get(); }
    const DSDAngel61Session *getAngel61Session() const { return m_angel61Sessions.empty() ? nullptr : m_angel61Sessions[std::max(0, m_angel61Holding)].get(); }
    bool isAngel61HF() const { return (bool) dynamic_cast<const DSDAngel61HF*>(getAngel61Session()); }
    DSDAngel61HF *getAngel61HF() { return dynamic_cast<DSDAngel61HF*>(getAngel61Session()); }
    static bool isAngel61HF(Protocol protocol) { return (protocol == ProtocolAngel61HFNB) || (protocol == ProtocolAngel61HFWB); }
    static bool isAngel61Vhf(Protocol protocol) { return (protocol == ProtocolAngel61) || (protocol == ProtocolAngel61WB); }
    /**
     * The cutoff of the decimation to 48 kS/s in front of the demodulator (DSDDemodSink, and the
     * harness replaying a recording): the RF bandwidth / 2.2 the FM protocols have always had (their
     * own channel filter follows), but never narrower than the Angel61 receivers enabled need. They
     * take the IQ before any filter of their own and their matched filter is their channel filter,
     * so the decimation has to pass their signal flat: NB's edge is at 2.88 kHz and WB's at 5.76, and
     * the decimator's transition is gentle. Over the air, WB at the default 12.5 kHz (5.7 kHz) lost
     * 12 dB of Es/N0 to it (18.4 dB against 30.6) and at 6.25 kHz 23 dB; NB at 6.25 kHz lost 8
     * (measured with revision 2's receiver, of which revision 3's is a port)
     */
    static float inputCutoff(Protocol protocol, float rfBandwidth)
    {
        // (at least 2 kHz: an RF bandwidth of 0, from the Web API or the slider's end, passed nothing)
        float cutoff = std::max(2000.0f, rfBandwidth / 2.2f);

        if ((protocol == ProtocolAuto) || (protocol == ProtocolAngel61WB)) {
            cutoff = std::max(cutoff, 10000.0f);
        } else if (protocol == ProtocolAngel61) {
            cutoff = std::max(cutoff, 8000.0f);
        } else if (isAngel61HF(protocol)) {
            cutoff = std::max(cutoff, 5000.0f);     // HF-WB's edge at 2.8 kHz; the HF receiver decimates to 16 kS/s itself
        }

        return cutoff;
    }
    //! The protocol decoder in use (null while searching)
    const DSDProtocol *getDecoder() const { return m_decoder; }
    //! The decoder of a protocol, for its last known signalling
    const DSDProtocol *getDecoder(Protocol protocol) const;

private:
    enum State { StateSearching, StateLocked };
    enum Shaping { ShapingRRC, ShapingGaussian };

    /** A matched filter, searcher and clock for one pulse shape and symbol rate */
    struct Chain
    {
        Shaping m_shaping;
        int m_sps;
        double m_alpha = 0.2;       //!< Roll off of the root raised cosine matched filter (M17: 0.5)
        int m_delay;                //!< Matched filter group delay in samples
        std::vector<float> m_taps;   //!< Matched filter taps (the impulse response of the RootRaisedCosine or Gaussian)
        std::vector<float> m_buffer; //!< The last taps.size() inputs, written twice taps.size() apart so that a window is contiguous
        int m_position;
        std::vector<float> m_kernel; //!< Symbol spaced pulse response (GMSK), empty for RRC
        DSDSyncSearcher m_searcher;
        DSDSymbolClock m_clock;
        std::vector<DSDProtocol*> m_decoders;
        bool m_enabled;
        bool m_idle;                //!< Not run while another chain's decoder holds a confirmed lock
        std::unique_ptr<DSDCoherentDetector> m_coherentDetector; //!< RRC chains only

        //! Matched filter: a dot product over a contiguous window, in eight partial sums (a single
        //! sum is a chain of dependent additions the compiler may not reorder under /fp:precise)
        float filter(float x)
        {
            const int n = (int) m_taps.size();
            m_position = (m_position + 1 == n) ? 0 : m_position + 1;
            m_buffer[m_position] = x;
            m_buffer[m_position + n] = x;
            const float *w = &m_buffer[m_position + 1];  // the last n inputs, oldest first
            const float *t = m_taps.data();               // taps reversed (symmetric, so as they are)
            float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, a4 = 0.0f, a5 = 0.0f, a6 = 0.0f, a7 = 0.0f;
            int i = 0;

            for (; i + 8 <= n; i += 8)
            {
                a0 += w[i] * t[i];
                a1 += w[i + 1] * t[i + 1];
                a2 += w[i + 2] * t[i + 2];
                a3 += w[i + 3] * t[i + 3];
                a4 += w[i + 4] * t[i + 4];
                a5 += w[i + 5] * t[i + 5];
                a6 += w[i + 6] * t[i + 6];
                a7 += w[i + 7] * t[i + 7];
            }

            for (; i < n; i++) {
                a0 += w[i] * t[i];
            }

            return ((a0 + a1) + (a2 + a3)) + ((a4 + a5) + (a6 + a7));
        }
    };

    Protocol m_protocolSelection;
    float m_rfBandwidth;
    bool m_narrowFilter;
    bool m_coherentFiltered;    //!< The coherent detector works on the channel filtered IQ, the filter in its channel model
    double m_coherentGate;      //!< Expected first pass errors in a burst above which the coherent detector runs
    int64_t m_coherentRuns;        //!< Bursts the coherent detector ran on
    int64_t m_coherentSkips;       //!< Bursts it was not needed for
    float m_filterCutoffHz;     //!< Channel filter cutoff in use
    State m_state;
    Status m_status;

    DSDFrontEnd m_frontEnd;
    std::vector<Chain*> m_chains;
    Chain *m_chain;             //!< Locked chain
    DSDSymbolClock *m_clock;    //!< Clock of the locked chain
    int m_sps;
    int m_isiSpan;              //!< Neighbours on each side modelled by the burst refinement
    DSDSlicer m_slicer;
    DSDSymbolHistory m_history;
    std::vector<DSDProtocol*> m_decoders;
    DSDProtocol *m_decoder;
    std::vector<DSDVoiceFrame> m_frames;
    std::vector<DSDProtocolEvent> m_protocolEvents;
    std::vector<DSDProtocolFrame> m_pendingProtocolFrames;
    std::vector<std::pair<float, float>> m_scope;
    float m_lastScopeY;
    //! The GMSK scope's last symbols (the sample and its decision, as sliced), the newest last: the
    //! one in the middle is shown, with its neighbours' intersymbol interference taken out
    static constexpr int GmskScopeSpan = 5;
    float m_gmskScopeY[GmskScopeSpan];
    float m_gmskScopeD[GmskScopeSpan];
    bool m_gmskScopePresent[GmskScopeSpan];
    int m_gmskScopeCount;
    float m_lastScopeFrequency;
    bool m_discriminatorEnabled;
    std::vector<float> m_discriminator;
    bool m_coherent;
    std::vector<std::complex<float>> m_dcFree;   //!< The block's input with the DC blocker's estimate off (process)
    std::vector<std::complex<float>> m_iqRing;   //!< Frequency corrected IQ by input sample index, for the coherent detector

    int64_t m_sampleIndex;         //!< Matched filter output sample count
    int64_t m_symbolIndex;         //!< Sliced symbol count
    int64_t m_lastSymbolIndex;
    double m_lastSymbolCentre;  //!< Sample position of the last sliced symbol
    bool m_inverted;
    int m_misses;
    int m_silentBursts;
    int m_confirmed;            //!< Syncs and bursts confirmed since the lock
    std::vector<DSDVoiceFrame> m_pendingFrames; //!< Voice frames of a lock not yet confirmed, held back
    uint64_t m_nextCallId;
    uint64_t m_callId;
    uint64_t m_protocolSequence;
    bool m_protocolPublished;
    bool m_protocolSuppressed;
    int64_t m_framesDiscarded;     //!< Voice frames of locks that were never confirmed
    DVSync::Pattern m_lockPattern; //!< The pattern that locked, shown once confirmed
    int64_t m_lockIndex;           //!< Symbol index at the lock
    DSDProtocol *m_rejectedDecoder; //!< The decoder whose lock the constellation check last rejected...
    int64_t m_rejectedUntil;       //!< ...ignored until this matched filter sample

    // Envelope
    std::vector<float> m_powerRing;
    float m_blockPower;
    int m_blockCount;
    std::vector<float> m_blockHistory;
    int m_blockIndex;
    int m_blocksFilled;
    float m_noiseFloor;
    float m_signalPower;        //!< Running mean of the symbol power while locked

    // Clock rate from successive syncs
    int64_t m_lastSyncIndex;
    double m_lastSyncCentre;

    // Expected sync
    int64_t m_expectedSyncIndex;
    bool m_expectedPending;
    std::vector<DVSync::Pattern> m_expectedPatterns;

    std::vector<std::unique_ptr<DSDAngel61Session>> m_angel61Sessions;   //!< The profiles listened for
    bool m_angel61Selected = false;     //!< An Angel61 protocol selected: its session alone
    int m_angel61Holding = -1;          //!< In Auto, the session whose transmission holds the channel, -1 for none

    //! In Auto: the sessions' blocks, and the channel taken by a transmission confirmed or given back
    void processAngel61(const std::complex<float> *iq, int n);
    //! The constellation of the Angel61 session that has the channel, into the scope samples
    void scopeAngel61(DSDAngel61Session& session);

    void rebuild();
    void buildChain(Chain& chain);
    bool decoderEnabled(const DSDProtocol *decoder) const;
    void lock(const DSDSyncSearcher::Detection& detection, Chain *chain, DSDProtocol *decoder);
    void unlock();
    //! A sync or burst of the locked decoder checked out: the lock shows and its held voice frames go
    void confirmLock();
    void publishProtocolFrame(const DSDProtocolFrame& frame);
    void handleDetection(const DSDSyncSearcher::Detection& detection, Chain *chain);
    void sliceSymbol();
    bool constellationPlausible(int64_t first, int64_t last) const;
    //! A locked symbol's sample into the scope (a GMSK one two symbols late, without its interference)
    void pushScopeSymbol(float y, const DSDSoftSymbol& symbol);
    void refineBurst(int64_t first, int64_t last);
    //! Channel filter cutoff: the search bandwidth while searching, the locked signal's occupied
    //! bandwidth plus the uncorrected frequency offset once locked
    void updateChannelFilter();
    //! Coherent re-detection of a refined burst (positions: refined instants of first..last)
    void coherentBurst(int64_t first, int64_t last, const std::vector<double>& allPositions);
    DSDSoftSymbol sliceAt(double position, bool track);
    DSDSoftSymbol sliceValue(float y, double position, bool track = false);
    static bool solveNormalEquations(double ata[6][6], const double *aty, int p, double *x);
    float symbolPower(double centre) const;
    float presenceThreshold() const;
    float spanPower(int64_t syncIndex) const;     //!< Mean envelope power over the 24 symbols ending at a sync
    void trackSignalPower(float power);       //!< The presence test's signal power follows the bursts found present
    int64_t symbolIndexNear(double sample) const;
    double sampleOfSymbol(int64_t index) const;
    DSDProtocol *decoderFor(const Chain& chain, DVSync::Pattern pattern) const;
    void updateNoiseFloor(float power);
    static std::vector<float> gmskKernel(int sps, double rxBt, int rxSpan);
};

#endif // INCLUDE_DSDDEMODULATOR_H
