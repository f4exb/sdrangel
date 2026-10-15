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

#ifndef MODEMDVB_DVBT2OFDM_H
#define MODEMDVB_DVBT2OFDM_H

#include <cstdint>
#include <functional>
#include <array>
#include <map>
#include <vector>

#include "cwcanceller.h"
#include "dvbt2.h"

class FFTFactory;
class FFTEngine;

namespace dvbt2 {

//! The channel's impulse response, for display: power in dB relative to the strongest path, against delay from the
//! FFT window's start, bin b being b * m_resolution samples, wrapping round after m_powerDb.size() bins
struct ImpulseResponse
{
    std::vector<float> m_powerDb;
    double m_resolution = 0.0;  //!< Samples a bin
    int m_guard = 0;            //!< Guard interval, samples
};

//! DVB-T2 OFDM demodulation: P1 detection, synchronisation, channel estimation and frequency de-interleaving.
//! Outputs the data cells of each P2 and data symbol, in frame builder order, with their reliabilities
class MODEMDVB_API OFDMDemod
{
public:
    enum State
    {
        SearchP1,       //!< Looking for a P1 symbol
        FindGuard,      //!< Found a P1, finding the guard interval from the next symbols
        Symbols,        //!< Demodulating the symbols of a frame
        NextP1          //!< Expecting the P1 of the next frame
    };

    struct Symbol
    {
        int m_frame;            //!< Frames since acquisition
        int m_l;                //!< Symbol in the frame, from 0 for the first P2
        bool m_p2;
        bool m_frameClosing;
        const Cf *m_cells;      //!< Equalised and frequency de-interleaved
        const float *m_llrScales; //!< 4 / noise variance per dimension of each equalised cell
        int m_nbCells;
    };
    typedef std::function<void(const Symbol& symbol)> SymbolCallback;
    typedef std::function<void(const char *message)> LogCallback;

    struct Status
    {
        State m_state = SearchP1;
        P1Signalling m_p1;
        int m_fftSize = 0;
        GuardInterval m_guard = GuardInterval::G1_32;
        bool m_frameKnown = false;      //!< L1-pre decoded for the current frame
        double m_frequencyOffset = 0.0; //!< Hz
        double m_clockOffset = 0.0;     //!< ppm, from the continual pilots, or the P1 interval
        bool m_clockFromP1s = false;    //!< Only from the spacing of P1s found by searching, as L1-pre isn't decoded
        uint64_t m_fefParts = 0;        //!< Future Extension Frame parts skipped
        double m_p1Correlation = 0.0;
        double m_snr = 0.0;             //!< dB, from the continual pilots
        uint64_t m_frames = 0;
        uint64_t m_acquisitions = 0;
    };

    explicit OFDMDemod(int bandwidthHz = 8000000, FFTFactory *fftFactory = nullptr);
    ~OFDMDemod();

    void setCallback(SymbolCallback callback) { m_callback = callback; }
    void setLogCallback(LogCallback callback) { m_log = callback; }
    void reset();
    void process(const Cf *samples, int nbSamples);
    //! Call from the symbol callback with the L1-pre of the frame, once decoded from the P2 symbols, so the data
    //! symbols can be demodulated. If it isn't called by the last P2 symbol, the receiver searches for the next P1
    void setFrameParameters(const L1Pre& pre);
    //! Samples in each FEF part (from its P1 to the next T2-frame's), from L1-post, 0 if none or not known
    void setFEFLength(int64_t samples) { m_fefLength = samples; }
    //! The profile to receive. The other's frames are FEF parts. Takes effect from the next search for a P1
    void setProfile(Profile profile) { m_profile = profile; }

    Status getStatus() const { return m_status; }
    //! The clock offset of the samples from sample (from the first given since reset()) on has changed by ppm, as
    //! when they're resampled to correct it. Symbols before it keep the old offset, and those after the new, so
    //! the frame's timing stays continuous
    void adjustClockOffset(double ppm, int64_t sample);
    //! Narrowband interferers notched out before the FFT, Hz from the input's centre, so their carriers are erased
    void setNotches(const std::vector<double>& frequencies, double inputRate) { m_notched.setNotches(frequencies, inputRate); }
    //! Equalised cells and the channel's magnitude response (dB), for display. Call from the processing thread
    void getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const;
    //! The latest impulse response, in at most points bins, empty if none. Call from the processing thread
    void getImpulseResponse(ImpulseResponse& response, int points) const;
    //! For analysis: the error power (relative to the signal) and count of the continual pilots of each carrier
    void getPilotErrors(std::vector<double>& error, std::vector<double>& count) const { error = m_pilotError; count = m_pilotCount; }
    //! For analysis: SNR of each data symbol from its pilots, dB
    const std::vector<float>& symbolSNRs() const { return m_symbolSNRs; }

private:
    struct FFT
    {
        FFTFactory *m_factory = nullptr;
        FFTEngine *m_engine = nullptr;
        unsigned int m_sequence = 0;
        int m_size = 0;
        bool m_inverse = false;

        void configure(int size, bool inverse);
        void release();
    };

    static constexpr double m_twoPi = 6.283185307179586;
    static constexpr double m_p1Threshold = 0.4;            //!< Normalised P1 correlation
    static constexpr int m_p1Length = 2048;                 //!< TP1 in samples
    static constexpr int m_nextP1Range = 48;                //!< Samples either side of the expected P1 position to search
    static constexpr double m_frequencyLoopGain = 0.2;
    static constexpr int m_interpolationHalfLength = 6;     //!< Pilots each side, for frequency interpolation

    //! The P1's frequency shift removed: sample n times shift[n mod 1024], one 1K carrier down
    static const std::complex<double> *p1Shifts();
    //! The impulse response's power, reduced to at most points bins (by maximum, which keeps narrow paths), in dB
    //! relative to its peak, down to -60 dB
    static void impulseResponseDb(const std::vector<float>& power, double resolution, int points, std::vector<float>& powerDb,
        double& binResolution);

    bool searchP1();
    //! The measurements of the frames before (of timing, clock and channel) are stale: a P1 has been found by searching
    void startAcquisition();
    bool decodeP1(int64_t start, double fraction, P1Signalling& p1, double& frequency);
    double p1Metric(int64_t start, std::complex<double>& c1, std::complex<double>& c2) const;
    bool findGuard();
    bool processSymbol();
    bool nextP1();
    //! Sample index of a position in the frame, nominally offset samples after the P1 start, allowing for the clock offset
    int64_t framePosition(int64_t offset) const;
    int64_t frameLength() const;    //!< Nominal samples from the frame's (or FEF part's) P1 to the next P1
    double m_exactDrift = 0.0;          //!< Of this symbol from its nominal position, samples, from the clock offset
    //! Of this symbol: its exact position (from m_exactDrift) less the sample framePosition() put it at. Usually a
    //! fraction of a sample, but several when the frame's clock measurement differs from the estimate it started with
    double m_timingFraction = 0.0;
    double m_previousTimingFraction = 0.0;
    bool m_repeatSymbol = false;        //!< The symbol is being demodulated again, after measuring its impulse response
    //! Of the window from start, with the frequency corrected, into the FFT's output, which extractCarriers() reads
    void fft(int64_t start);
    void estimateChannel(const std::vector<Tables::Pilot>& pilots, int spacing, int offset, int l, bool hold, float amplitude);

    void interpolate(int spacing, int offset, bool hold);
    void interpolateGrid(const Cf *source, Cf *dest, int spacing, int offset, double delay);
    // MISO (9.1, 9.2.8): the second group of transmitters sends each pair of data cells Alamouti coded, and its pilots
    // inverted on alternate pilot-bearing carriers, so the pilots on a grid are H1 + (-1)^j H2: the two channels are
    // the grid, and it alternated, half-band filtered
    static void edgeFilter(double pass, double stop, int left, int right, std::vector<float>& taps);
    void separateMISO(const Cf *source, int spacing, int offset, int kMax, bool useCentres);
    double meanDelay(const Cf *grid, int spacing, int offset, int nbPilots) const;
    float misoSign(int k, int l) const;     //!< Of the second group's pilot on carrier k of symbol l
    void misoImpulseResponse(int spacing, int offset, int windowOffset, int l);
    //! Delays of the clusters of paths in the last impulse response measured, strongest first, of those with at least
    //! a thirtieth of its power
    void cirClusters(int kMax, int spacing, int offset, int windowOffset, std::vector<double>& delays) const;
    void equalise(const std::vector<int>& data, const std::vector<int>& read, const std::vector<Tables::Pilot>& pilots, int l);
    //! What equalise() changes of the noise estimates, so trials of the channel estimate at acquisition can be undone
    struct NoiseState
    {
        double m_noise;
        double m_noiseRatio;
        double m_snr;
        std::vector<float> m_carrierNoise;
        std::vector<float> m_carrierWeight;
    };
    void saveNoise(NoiseState& state) const;
    void restoreNoise(NoiseState& state);
    //! Of the P2 symbol just equalised: its first nbCells (L1-pre's, BPSK) squared distance from their points
    double l1PreError(int nbCells) const;
    void trackFrequency(int64_t start);
    void measureClock(int64_t window);
    const Cf *bufferAt(int64_t index) const { return &m_buffer[index - m_bufferOffset]; }
    int64_t bufferEnd() const { return m_bufferOffset + (int64_t) m_buffer.size(); }
    void log(const char *format, ...);

    double m_sampleRate;
    SymbolCallback m_callback;
    LogCallback m_log;
    Status m_status;
    FFT m_fft;
    FFT m_p1Fft;
    FFT m_cirFft;                   //!< Impulse response from the pilots

    // FFT window position: the paths' first delay and spread, measured over a frame, place the window for the next,
    // so they all fall within the guard interval
    //! A reading of the impulse response: delays from the window start
    struct CIRReading
    {
        double m_delay;     //!< Of the first path
        double m_spread;
    };
    //! Accumulates the measurement for the window placement, or if readings is given, returns those the guard interval
    //! could hold (most likely first), for the caller to choose
    void measureImpulseResponse(const Cf *estimates, int kMax, int spacing, int offset, int windowOffset, int l,
        bool resolveAmbiguity, std::vector<CIRReading> *readings = nullptr);
    void useImpulseResponse(const CIRReading& reading, int windowOffset);
    int windowOffsetFor(double firstPath, double spread) const;
    void extractCarriers(int kMax);
    std::vector<CIRReading> m_cirReadings;
    std::vector<double> m_p1Metrics;        //!< Over the next P1's search range
    std::vector<std::complex<double>> m_p1C1, m_p1C2;
    double continualPilotFit(const Cf *cir, int size, double resolution, int first, double firstDelay, int offset, int l) const;
    void updateWindowOffset();
    int windowOffset() const;
    int m_windowOffset = -1;        //!< Window start after the nominal symbol start, -1 for the default
    int m_windowGuard = 0;          //!< Guard interval it is for
    double m_firstPathSum = 0.0;    //!< Over the frame: first path relative to the nominal symbol start, samples
    double m_spreadSum = 0.0;
    int m_cirCount = 0;
    double m_spread = 0.0;          //!< Delay spread over the last frame, samples
    bool m_timeInterpolation = false;   //!< Use the scattered pilots of earlier symbols too, for a long spread
    double m_centre = 0.0;          //!< Delay of the middle of the paths from the window start, from the latest measurement
    bool m_centreValid = false;
    std::vector<float> m_cirPower;
    double m_cirResolution = 0.0;   //!< Samples a bin of m_cirPower, 0 until measured
    std::vector<float> m_cirSorted;
    std::vector<int> m_cirBins;

    std::vector<Cf> m_buffer;
    int64_t m_bufferOffset;
    int64_t m_searchPos;        //!< Next P1 start to test
    double m_frequency;         //!< Correction, cycles per sample
    double m_ncoPhase;          //!< Cycles, at m_ncoIndex, so changes of frequency don't make the phase jump
    int64_t m_ncoIndex;

    // Frame
    int m_fftSize;
    int m_guardSamples;
    int m_np2;
    int64_t m_frameStart;       //!< P1 start
    int64_t m_previousFrameStart;
    int m_l;                    //!< Next symbol
    int m_lf;                   //!< Symbols in the frame, once known
    bool m_frameClosing;
    bool m_frameKnown;
    std::vector<GuardInterval> m_guardCandidates;
    const Tables *m_p2Tables;
    const Tables *m_tables;
    L1Pre m_pre;
    int m_frameCount;

    // Symbol
    std::vector<Cf> m_y;            //!< Carriers 0..Kmax
    std::vector<Cf> m_h;            //!< Channel estimate (of the first group of transmitters, in MISO)
    std::vector<Cf> m_h2;           //!< Of the second group, in MISO
    std::vector<Cf> m_misoGrid[2];  //!< The two groups' pilots, separated
    std::vector<Cf> m_misoPilots;   //!< separateMISO()'s, one group's at a time, with its delay removed
    std::vector<Cf> m_p2Hold;       //!< P2 pilots in MISO, whose signs differ from the data symbols' on the same carriers
    std::vector<Cf> m_p2Channels[2];    //!< The last P2 symbol's two channels, in MISO
    std::vector<Cf> m_misoCells;    //!< Alamouti decoded, in carrier order
    std::vector<float> m_misoGain;
    std::vector<float> m_misoTaps[2];   //!< Each group's low-pass filter
    std::vector<std::complex<double>> m_cpeTerms; //!< Each pilot's part of the common phase's measurement
    //! Filters at the edges, where the pilots either side are fewer: by their pass and stop bands (in 1/200 cycles a
    //! pilot) and the pilots either side, kept for P2's and the data symbols' spacings
    std::map<std::array<int, 4>, std::vector<float>> m_misoEdgeTaps;
    double m_misoCutoff[2] = {0.25, 0.25};      //!< Of each group's filter, cycles a pilot
    float m_misoTapSum[2] = {1.0f, 1.0f};
    double m_misoSpread[2] = {-1.0, -1.0};      //!< Of each group's paths, from its impulse response, -1 if not known
    bool m_miso = false;
    double m_misoCentre[2] = {0.0, 0.0};    //!< Middle of each group's paths, from their impulse responses
    bool m_misoCentreValid[2] = {false, false};
    bool m_misoPresent[2] = {true, true};   //!< Not far weaker than the other group, as last separated
    double m_misoDelay[2] = {0.0, 0.0};     //!< Each group's channel is centred on, as separated
    std::vector<Cf> m_hold;         //!< Latest pilot estimate on every pilot carrier, for time interpolation
    std::vector<float> m_holdAmplitude; //!< Of the pilot each estimate is from, 0 for none yet
    double m_holdNoise = 0.0;       //!< This symbol's pilots' change from the estimates held from earlier ones
    double m_holdWeight = 0.0;
    int m_holdCount = 0;
    std::vector<double> m_pilotError;
    std::vector<float> m_symbolSNRs;
    std::vector<double> m_pilotCount;
    std::vector<Cf> m_cells;
    std::vector<float> m_llrScales;
    std::vector<Cf> m_eqCells;      //!< Equalised, in carrier order (SISO)
    std::vector<float> m_eqScales;
    std::vector<Cf> m_previousCP;   //!< Continual pilots of the previous data symbol
    int64_t m_previousWindow;       //!< -1 if there wasn't one
    double m_clockSum;              //!< Of the measurements in this frame, ppm
    int m_clockFrames;              //!< Frames the clock offset has been measured over
    int64_t m_lastSearchedP1;       //!< Start of the last P1 found by searching, -1 if none
    int64_t m_lastP1Symbols;        //!< Symbols between the last two P1s found by searching, -1 if not known
    int64_t m_fefLength = 0;        //!< Samples in a FEF part, 0 if none or not known
    Profile m_profile = Profile::Auto;
    bool m_lite = false;            //!< Receiving T2-Lite
    bool profileWanted(const P1Signalling& p1) const;
    bool m_inFEF = false;           //!< m_frameStart is the P1 of a FEF part, not of a T2-frame
    // The first P2 symbol after acquisition gives the impulse response, from its dense pilots, and is demodulated
    // again with the window and channel estimation for it, so a long echo doesn't stop L1 being decoded
    bool m_p2Measured = true;
    int m_noiseSettling = 0;        //!< Data symbols left whose own noise estimate is used, rather than an average
    std::vector<Cf> m_p2Estimates;
    dvbt::NotchedCarriers m_notched;
    // Timing jumps (samples lost): the guard interval correlation at each symbol, which collapses
    double m_giAverage;             //!< 0 until measured
    int m_giLow;                    //!< Consecutive symbols with too little
    double guardCorrelation(int64_t start) const;
    bool guardMoved(int64_t start, double threshold) const;
    // A change of the clock offset part way through a frame: m_status.m_clockOffset is that of the samples from
    // m_clockChangeSample on, and before them it's m_clockChangeDelta less. Cleared when a frame starts after it
    int64_t m_clockChangeSample;
    double m_clockChangeDelta;      //!< ppm, 0 for none
    int p1Range() const;            //!< Samples either side of the next P1's expected position to search
    int m_clockCount;
    double m_noise;                 //!< Noise variance per dimension of the received cells, relative to the mean |h|^2
    double m_noiseRatio = -1.0;     //!< The noise's part proportional to |h|^2, over the rest, -1 until measured
    double m_delay;                 //!< Of the channel estimate, samples
    std::vector<std::vector<float>> m_interpolationTaps;
    std::vector<float> m_tapPower;  //!< Sum of the squares of the taps, for each position between pilots
    std::vector<float> m_carrierTapPower;   //!< m_tapPower of each carrier's position
    // Noise on each carrier (from its pilots), so narrowband interference (spurs, leakage, what's left near a notch)
    // makes its cells less confident
    std::vector<float> m_carrierNoise;      //!< Variance, -1 until measured
    std::vector<float> m_carrierWeight;     //!< Of each carrier's cells, the median noise over its smoothed noise
    int m_carrierWeightAge = 0;             //!< Symbols since m_carrierWeight was worked out
    std::vector<float> m_measuredNoise;     //!< updateCarrierWeights()'s, kept so it isn't allocated each time
    void carrierNoise(int k, float variance);
    //! If the carrier's noise is much more than the others', so it's left out of the overall estimate
    bool noisyCarrier(int k) const { return (k < (int) m_carrierWeight.size()) && (m_carrierWeight[k] < 1.0f); }
    void updateCarrierWeights(int kMax);
    std::vector<Cf> m_padded;       //!< Pilots with the mean delay removed, the first and last repeated at each end
    std::vector<Cf> m_phaseOut;     //!< Interpolated, for one position between pilots
    std::vector<Cf> m_pilotRamp;    //!< To restore the delay: at each pilot, and at each position after a pilot
    std::vector<Cf> m_phaseRamp;
    int m_interpolationSpacing;
    int m_gridSpacing;              //!< Of the pilots the channel estimate is from
    int m_gridOffset;
    float m_gridAmplitude;
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2OFDM_H
