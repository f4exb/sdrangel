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

#ifndef INCLUDE_DSP_DV_DVANGEL61OFDM_H
#define INCLUDE_DSP_DV_DVANGEL61OFDM_H

#include <complex>
#include <cstdint>
#include <vector>

#include "export.h"
#include "dvangel61.h"

/**
 * The Angel61 revision 3 HF modem (ANGEL61.md 2.1, plan item C4): OFDM at 48 kS/s complex
 * baseband, 50 Hz carrier spacing, 20 ms useful symbols with a 4 ms cyclic prefix and a 1 ms WOLA
 * taper, ten symbols a 240 ms frame. Carrier k (0 .. Nc - 1) is at (k - (Nc - 1) / 2) x 50 Hz, so
 * the carriers sit on odd multiples of 25 Hz and the cyclic prefix is the tones' own continuation;
 * carriers -1 and Nc are the reserved edge carriers.
 *
 * The frame's cells come from DVAngel61 (buildFrame, buildGroup): cell l Nc + k.
 */
class SDRBASE_API DVAngel61OFDM
{
public:
    static constexpr int SampleRate = 48000;
    static constexpr int UsefulSamples = 960;        //!< 20 ms
    static constexpr int PrefixSamples = 192;        //!< 4 ms
    static constexpr int SymbolSamples = UsefulSamples + PrefixSamples;
    static constexpr int TaperSamples = 48;          //!< 1 ms WOLA (2.1)
    static constexpr int FrameSymbols = 10;
    static constexpr int FrameSamples = FrameSymbols * SymbolSamples;   //!< 240 ms
    static constexpr double CarrierSpacing = 50.0;

    static int carriers(DVAngel61::Profile profile) { return DVAngel61::layout(profile).m_carriers; }
    static double carrierHz(int nc, int k) { return (k - (nc - 1) / 2.0) * CarrierSpacing; }
    /**
     * The reference transmitter's peak to average at 1e-4 (2.1): 4.1 dB (QPSK frames), 5.4 dB
     * (16-QAM); its mean power, of the frame's nominal (the cells' unit power): the peak reduction
     * takes 0.80 (QPSK) and 0.92 (16-QAM) of it (dvtest); and the gain from the modulator's output
     * that sets that peak at -1 dBFS (2.11): RMS -5.1 and -6.4 dBFS. Taken from the nominal power,
     * it had the peaks 1.0 and 0.35 dB lower (dsemodapitest --spectrum, 30 September 2026)
     */
    static float referencePaprDB(DVAngel61::Constellation c) { return c == DVAngel61::QPSK ? 4.1f : 5.4f; }
    static float referenceMeanPower(DVAngel61::Constellation c) { return c == DVAngel61::QPSK ? 0.80f : 0.92f; }
    static float outputGain(DVAngel61::Constellation c);

    /**
     * The transmitter: a frame's cells to FrameSamples samples, with the reference peak reduction
     * (A8's TRe1HYB, 2.1) when enabled. The output has unit nominal mean power (before peak
     * reduction): a sample is the sum of the cells' tones over sqrt(Nc).
     */
    class SDRBASE_API Modulator
    {
    public:
        explicit Modulator(DVAngel61::Profile profile);
        void setPeakReduction(bool enable) { m_peakReduction = enable; }
        bool getPeakReduction() const { return m_peakReduction; }
        //! Key down: forget the previous symbol's taper
        void reset();
        //! One frame: its cells (layout cells, l Nc + k) and the constellation of its payload cells
        void modulateFrame(const std::complex<float> *cells, DVAngel61::Constellation payload, std::vector<std::complex<float>>& out);
        //! The last symbol's taper, TaperSamples, at the end of a transmission
        void flush(std::vector<std::complex<float>>& out);
        /**
         * The cells the last frame was sent with, after peak reduction: symbol l's Nc + 2 cells from
         * l (Nc + 2), the edge carrier -1 first and Nc last.
         */
        const std::vector<std::complex<float>>& sentCells() const { return m_sent; }

    private:
        DVAngel61::Profile m_profile;
        int m_nc;
        int m_width;                             //!< Nc + 2: the edge carriers included
        bool m_peakReduction;
        std::vector<std::complex<float>> m_tones;//!< (Nc + 2) x (SymbolSamples + TaperSamples): e^{j 2 pi f t} / sqrt(Nc), t from the prefix's start
        std::vector<float> m_window;
        std::vector<std::complex<float>> m_tail; //!< the previous symbol's taper, added to the next
        std::vector<std::complex<float>> m_sent;
        std::vector<std::complex<double>> m_fft;
        int m_fftSize;

        void reduceSymbol(std::complex<float> *cells, const std::complex<float> *nominal, const signed char *kind, bool qam16);
        void synthesizeSymbol(const std::complex<float> *cells, std::complex<float> *out);
    };

    /**
     * The cells of one symbol from its useful part: y holds UsefulSamples samples starting woff
     * samples after the useful start (woff in [-PrefixSamples, 0]), derotated; the Nc cells come
     * out at the transmitted scale.
     */
    static void demodulateSymbol(DVAngel61::Profile profile, const std::complex<float> *y, int woff, std::complex<float> *cells);

    /**
     * The HF receiver (section 5), the design studies' (A5, A6) at 16 kS/s: 48 kS/s complex baseband
     * in, decimated by 3; acquisition from the cyclic prefix, then the pilot diagonal (frame phase,
     * whole carriers) and same-carrier pilot pairs (fine frequency), within +-50 Hz, over two frames;
     * frequency and timing tracked on accepted frames; the adaptive delay-domain channel estimator
     * (delay profile learnt by sparse Bayesian learning, two-dimensional above 3 Hz of Doppler, the
     * Doppler spread from the pilots' correlations, re-solved every 4 frames, refusing a degenerate
     * design); noise and interference measured on the decoded header. After 25 frames without an
     * accepted header it acquires again.
     *
     * Every frame period while locked gives a Frame: whether its header was accepted (CRC-8, the
     * profile, a kind this revision acts on) and, when it was, whether its payload decoded and
     * passed its CRC-12 (which covers the header). A 4-frame data group is decoded when its fourth
     * frame's time has passed, from whichever of its headers were accepted; its block comes with
     * the group's last frame.
     */
    class SDRBASE_API Receiver
    {
    public:
        struct Frame
        {
            bool m_headerOk = false;             //!< the header passed its CRC-8, names this profile and a known kind
            DVAngel61::Header m_header;        //!< when m_headerOk
            bool m_payloadOk = false;            //!< the block decoded and its CRC-12 passed: header and block confirmed
            std::vector<uint8_t> m_block;        //!< K bits, when m_payloadOk
            bool m_group = false;                //!< the block is a data group's, confirmed with m_groupHeaders
            DVAngel61::Header m_groupHeaders[DVAngel61::GroupFrames];
            int m_iterations = -1;
            int m_channelErrors = 0;             //!< The payload's channel bit errors the code corrected (not a data group's)
            int m_codedBits = 0;                 //!< Of its coded bits
            float m_snrDb = 0.0f;                //!< SNR3k estimate
            float m_frequencyHz = 0.0f;          //!< the tracked carrier offset
            int64_t m_sample = 0;                //!< input sample (48 kS/s) at the frame's start
        };

        explicit Receiver(DVAngel61::Profile profile);
        ~Receiver();
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;
        //! Forget everything: the next samples are acquired afresh
        void reset();
        //! Samples at 48 kS/s; the frames whose time has passed are appended to `frames`
        void process(const std::complex<float> *samples, int n, std::vector<Frame>& frames);
        bool locked() const;
        int acquisitions() const;
        //! Degenerate channel-estimator designs refused (section 5)
        int designsRefused() const;
        /**
         * The transmitter's peak-reduction error in the receiver's model (study, plan C4). After the
         * reference peak reduction the data cells arrive as alpha x + d beside untouched pilots: alpha
         * 0.86 in QPSK frames and 0.95 in 16-QAM ones, d uncorrelated at 14 to 15 dB and 19.5 to 20 dB
         * below the cell. Flags: DistortionAlpha, the gain measured on the decoded header and divided
         * out; DistortionVariance, a cell's LLR variance N0 / |alpha H|^2 + mse / |H|^2 + kd with N0
         * the channel's noise; DistortionDesignN0, the channel estimator's designs and learner with N0
         * (the pilots carry no distortion); DistortionSnr, SNR3k from N0 and the frame's mean power.
         * The default is all four: the thresholds are unchanged or 0.1 to 0.2 dB better (16-QAM on
         * fading channels), and the SNR3k follows the channel within 0.3 dB on AWGN and 0.7 dB on the
         * poor channel, where it saturated near the payload's MER (results section 32). kd follows the
         * measured gain (0.22 and 0.26 of 1 - alpha on header and payload cells), so a transmitter
         * without peak reduction (alpha 1) is modelled as clean. 0: all of the error is noise.
         */
        enum { DistortionAlpha = 1, DistortionVariance = 2, DistortionDesignN0 = 4, DistortionSnr = 8 };
        void setDistortionModel(int flags);
        /**
         * The channel estimator's learning (plan C4, 10 Hz of Doppler spread): true, the default,
         * learns the delay profile and the Doppler from every known cell of a frame, the pilots and an
         * accepted header's cells (snapshot SBL, a least-squares fit of the Doppler); false, from the
         * pilots alone (two-dimensional SBL above 3 Hz), as before, for comparison.
         */
        void setCellLearning(bool enable);
        /**
         * The constellation display: when enabled, the equalised payload cells of frames whose header
         * was accepted (data cells at unit mean power, the peak reduction's gain divided out), up to
         * ScopeSymbols held until taken
         */
        void setScopeEnabled(bool enable);
        std::vector<std::complex<float>> takeScopeSymbols();
        static constexpr int ScopeSymbols = 4096;
        /**
         * The detection threshold on acquisition: the pilot diagonal's coherence at the best timing,
         * frame phase and carrier hypothesis: the mean of the unit phasors u v* / |u v*| over the
         * pairs of adjacent diagonal pilots, 0 to 1. Each pair weighs the same, so that a steady
         * carrier, which makes the few pairs beside it agree strongly, cannot pass for a signal. Below it the window is not locked to: acquisition moves on half a
         * frame. 0 locks to whatever the window holds (the receiver before the threshold). The default,
         * 0.38 (HF-NB) and 0.25 (HF-WB), is just over what noise and steady carriers reach.
         */
        void setDetectionThreshold(float coherence);
        struct AcquisitionAttempt
        {
            float m_coherence = 0.0f;            //!< the pilot diagonal's coherence
            float m_prefix = 0.0f;               //!< the cyclic prefix's correlation over the samples' power
            bool m_locked = false;               //!< at or above the threshold
            int64_t m_sample = 0;                //!< the window's start (48 kS/s)
        };
        //! The acquisitions attempted since the last call (study, up to 4096 kept)
        std::vector<AcquisitionAttempt> takeAcquisitionAttempts();
        //! Windows not locked to (below the threshold)
        int acquisitionsRefused() const;

    private:
        struct Impl;
        Impl *m_impl;
    };
};

#endif // INCLUDE_DSP_DV_DVANGEL61OFDM_H
