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

#ifndef INCLUDE_IEEE_802_15_4_DEMODRECEIVER_H
#define INCLUDE_IEEE_802_15_4_DEMODRECEIVER_H

#include <complex>
#include <deque>
#include <vector>
#include <stdint.h>

#include "util/ieee_802_15_4.h"

// Receiver for the IEEE 802.15.4 O-QPSK and BPSK DSSS PHYs.
//
// O-QPSK with half-sine pulses is MSK, and could be received non-coherently with an FM
// discriminator, but the chip tables are defined on the O-QPSK chips, so a discriminator
// effectively detects each chip XORed with its neighbour - a product of two noisy samples -
// and does so at a chip SNR of Eb/N0 - 9 dB (2.4 GHz), well below its FM threshold.
// Instead this receiver correlates against the transmitted waveform: a matched filter for
// the chip pulse (half-sine or RC), sampled at each chip time, then despread against the
// 16 (or 2 for BPSK) chip sequences. That correlation is exactly the inner product of the
// received signal with the reference waveform of each symbol, so it is the matched filter
// for the whole symbol, ISI between chips included.
//
// Acquisition:
//  1. The preamble is a symbol (O-QPSK) or bit (BPSK) repeated, so it is detected with a
//     delay-and-correlate (Schmidl-Cox) metric over one symbol period, which is insensitive
//     to carrier frequency offset, and whose phase gives the frequency offset modulo the
//     symbol rate.
//  2. The ambiguity is resolved, and symbol timing found, by correlating against the
//     preamble symbol for each candidate frequency within the configured maximum offset and
//     each sample offset within a symbol, combining symbols non-coherently.
//
// Tracking: the input is de-rotated by the estimated offset, and a second order
// decision-directed loop tracks the carrier phase using the decided symbol's correlation,
// which benefits from the full processing gain of the spreading so is far more reliable
// than chip decisions. Symbol timing is tracked with an early-late correlator.
//
// Detection is done in two lanes. The coherent lane picks the symbol whose correlation has
// the largest real part after phase correction. The non-coherent lane picks the largest
// magnitude (O-QPSK) or uses differential detection (BPSK), so needs no phase lock, and still
// works if the phase loop slips. The frame from the first lane to pass the FCS is used.
class IEEE_802_15_4_DemodReceiver
{
public:
    typedef std::complex<float> Cf;
    typedef std::complex<double> Cd;

    struct Frame
    {
        std::vector<uint8_t> m_psdu;    //!< PSDU (MAC frame including FCS)
        bool m_fcsOK;
        bool m_coherent;                //!< Frame from coherent lane (true) or non-coherent lane (false)
        float m_frequencyOffset;        //!< Estimated carrier frequency offset in Hz
        float m_lqi;                    //!< Mean normalised symbol correlation, 0-1
        float m_power;                  //!< Mean power during frame (linear)
        float m_snr;                    //!< Estimated SNR in dB (Es/N0 per symbol, after despreading)
        int64_t m_startSample;          //!< Index of first sample of acquisition window
        int64_t m_endSample;            //!< Index of last sample of frame
    };

    struct Scope
    {
        float m_detectMetric;           //!< Preamble detection metric
        float m_correlation;            //!< Normalised correlation of last symbol
        float m_phase;                  //!< Tracked carrier phase / pi
        float m_state;                  //!< 0 = searching, 1 = acquiring, 2 = receiving frame
        float m_crc;                    //!< 1 = FCS OK, -1 = FCS bad, briefly at end of frame
        float m_chip;                   //!< Phase corrected soft value of last chip (I/Q)
    };

    struct Statistics
    {
        uint64_t m_detections;          //!< Preamble detections
        uint64_t m_acquisitions;        //!< Successful acquisitions
        uint64_t m_framesOK;            //!< Frames with valid FCS
        uint64_t m_framesBad;           //!< Frames with invalid FCS
        uint64_t m_framesCoherent;      //!< Frames with valid FCS from coherent lane
        uint64_t m_framesNonCoherent;   //!< Frames with valid FCS only from non-coherent lane
    };

    IEEE_802_15_4_DemodReceiver();

    //! Configure for a PHY. Sample rate must be samplesPerChip * PHY chip rate.
    //! maxFrequencyOffset in Hz. threshold is the preamble detection threshold (0-1).
    void configure(const IEEE_802_15_4_PHY& phy, int samplesPerChip, float maxFrequencyOffset, float threshold);
    void setThreshold(float threshold) { m_threshold = threshold; }
    void setMaxFrequencyOffset(float maxFrequencyOffset);
    void reset();

    int getSampleRate() const { return m_spc * m_phy.getChipRate(); }

    //! Process one sample
    void process(const Cf& x);

    bool frameAvailable() const { return !m_frames.empty(); }
    Frame popFrame();

    const Scope& getScope() const { return m_scope; }
    const Statistics& getStatistics() const { return m_stats; }

    //! For diagnostics: enable/disable lanes
    void setLanes(bool coherent, bool nonCoherent) { m_coherentLane = coherent; m_nonCoherentLane = nonCoherent; }

    static const int m_maxPSDULength = 127;

    void setDebug(bool debug) { m_debug = debug; }

private:
    // Normalised correlation preamble symbols must reach at acquisition, and how many must.
    // Noise alone gives a Rayleigh distributed value with mean sqrt(pi/4)/sqrt(chips per symbol):
    // 0.16 for 32 chips, 0.22 for 16 and 15, and P(q > x) = exp(-chips * x^2). A symbol at
    // Ec/N0 = -4 dB, around the sensitivity limit, gives about 0.53.
    static constexpr double m_verifyThreshold32 = 0.4;
    static constexpr double m_verifyThreshold16 = 0.5;
    static constexpr int m_verifySymbols = 3;
    // A frame is abandoned when the mean normalised correlation of recent symbols falls below
    // this multiple of the mean for noise
    static constexpr double m_lostThresholdFactor = 1.6;
    // Carrier phase loop gains, per symbol
    static constexpr double m_phaseGain = 0.25;
    static constexpr double m_phaseFreqGain = 0.02;
    // Symbol timing loop gain, in samples per unit early-late error per chip
    static constexpr double m_timingGain = 0.1;
    // Detector running sums are recalculated when an energy falls below this fraction of the
    // largest energy since the last recalculation, before rounding errors become significant
    static constexpr double m_detectorRelativeError = 1e-9;

    enum State {
        Search,
        Wait,
        Receive
    };

    enum FramerState {
        FramerPreamble,     //!< Looking for SFD
        FramerPHR,
        FramerPSDU,
        FramerDone,         //!< Frame complete
        FramerFailed        //!< No SFD found or invalid length
    };

    // Each detection lane has its own framer, so a symbol error in one lane, such as a false
    // SFD, can't misalign the other
    struct Framer
    {
        FramerState m_state;
        int m_preambleBits;
        uint32_t m_sfd;
        int m_bitCount;
        uint32_t m_phr;
        int m_length;
        std::vector<uint8_t> m_bits;
        std::vector<uint8_t> m_psdu;
        bool m_fcsOK;
        bool m_checked;

        void reset();
        void addBit(int bit);
        bool active() const { return (m_state != FramerDone) && (m_state != FramerFailed); }
    };

    // Configuration
    IEEE_802_15_4_PHY m_phy;
    bool m_bpsk;
    int m_spc;                          //!< Samples per chip
    int m_chipsPerSymbol;
    int m_numSymbols;                   //!< 16 for O-QPSK, 1 (+inverse) for BPSK
    int m_P;                            //!< Samples per symbol
    int m_window;                       //!< Detection window in symbols
    float m_threshold;
    float m_maxFrequencyOffset;
    double m_maxOmega;                  //!< Max offset in radians per sample
    std::vector<float> m_pulse;
    int m_pulseLength;
    std::vector<std::vector<float>> m_chipSigns;    //!< +-1 for each chip of each symbol
    std::vector<Cf> m_chipRotation;     //!< Conjugate of chip carrier: 1 for I chips, -j for Q chips
    std::vector<Cf> m_chipValues;       //!< Matched filter output for each chip of the symbol being decoded, with the I/Q chip carrier removed
    bool m_coherentLane;
    bool m_nonCoherentLane;
    bool m_debug = false;

    // Sample history
    std::vector<Cf> m_x;                //!< Input
    std::vector<Cf> m_xf;               //!< Input averaged over a chip, for the detector
    std::vector<Cf> m_d;                //!< Input de-rotated by frequency estimate (receive state)
    std::vector<Cf> m_y;                //!< Matched filter output of m_d (receive state)
    int64_t m_mask;
    int64_t m_n;                        //!< Number of samples processed

    // Preamble detection
    Cd m_R;
    double m_E1;
    double m_E2;
    double m_detectorPeakEnergy;        //!< Largest of m_E1 and m_E2 since last recalculation
    State m_state;
    int64_t m_waitUntil;
    float m_waitMax;                    //!< Peak detector metric while waiting
    int64_t m_waitMaxN;                 //!< Sample index of peak
    Cd m_waitR;                         //!< Detector correlation at peak
    double m_verifyThreshold;
    double m_lostThreshold;
    int64_t m_holdoffUntil;

    // Receive state
    double m_omega;                     //!< Frequency offset in radians per sample
    int64_t m_ncoRef;                   //!< Sample index where de-rotation phase is 0
    int64_t m_dNext;                    //!< Next index of m_d to calculate
    int64_t m_yNext;                    //!< Next index of m_y to calculate
    Cd m_rot;                           //!< De-rotation phasor for m_dNext
    Cd m_rotStep;
    double m_tau;                       //!< Sample index (in m_y) of first chip of next symbol
    double m_phi;                       //!< Carrier phase
    double m_dphi;                      //!< Carrier phase change per symbol
    bool m_phaseInit;
    int64_t m_frameStart;
    double m_powerSum;
    int64_t m_powerCount;
    double m_qSum;
    int m_qCount;
    double m_ampSum;                    //!< For SNR estimate
    double m_ampSumSq;
    int m_ampCount;
    std::deque<double> m_recentQ;       //!< Normalised correlation of recent symbols
    int64_t m_acquisitionN;             //!< Sample index when acquired
    bool m_detectorCleared;             //!< Detector has dropped below threshold since acquisition

    // Framers for coherent [0] and non-coherent [1] lanes
    Framer m_framers[2];
    int m_prevECoh;                     //!< BPSK previous coherent (differentially encoded) bit
    Cd m_prevC;                         //!< BPSK previous correlation for differential detection

    // Outputs
    std::deque<Frame> m_frames;
    Scope m_scope;
    Statistics m_stats;
    int m_crcScopeCount;

    void allocate();
    Cf yAt(double t) const;
    void updateDetector();
    void recalculateDetector();
    bool acquire();
    void startReceive(double omega, int64_t segStart, double tau);
    void processReceive();
    void decodeSymbol();
    Cd correlate(double tau, int symbol) const;
    void frameSymbol(int symCoh, int symNC);
    void frameBits(int bitCoh, int bitNC);
    void finishFrame(int lane);
    bool searchingForSFD() const;
    void abortFrame();
    void endReceive();
    static bool lengthValid(int length) { return (length >= 4) && (length <= m_maxPSDULength); }
};

#endif // INCLUDE_IEEE_802_15_4_DEMODRECEIVER_H
