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

#ifndef MODEMDVB_DVBTOFDM_H
#define MODEMDVB_DVBTOFDM_H

#include <cstdint>
#include <functional>
#include <vector>

#include "cwcanceller.h"
#include "dvbt.h"

class FFTEngine;
class FFTFactory;

namespace dvbt {

//! The channel's impulse response, for display: power in dB relative to the strongest path, against delay from the
//! FFT window's start, bin b being b * m_resolution samples, wrapping round after m_powerDb.size() bins
struct ImpulseResponse
{
    std::vector<float> m_powerDb;
    double m_resolution = 0.0;  //!< Samples a bin
    int m_guard = 0;            //!< Guard interval, samples
};

// OFDM demodulation: acquisition, synchronisation, channel estimation, equalisation and TPS decoding.
// Input must be at sampleRate(bandwidth), e.g. 64/7 MS/s for 8 MHz channels.
// Clock error is followed by moving the FFT window a sample at a time
class MODEMDVB_API OFDMDemod
{
public:
    enum State {
        Acquire,        //!< Looking for guard interval correlation
        IntegerCFO,     //!< Finding the frequency offset in whole carriers
        PilotSync,      //!< Finding the scattered pilot phase
        TPSSync,        //!< Looking for the TPS sync word
        Locked          //!< Outputting data cells
    };

    struct Symbol
    {
        int m_index;                //!< In the frame, 0..67
        int m_frame;                //!< In the super-frame, 0..3
        const TPS *m_tps;
        const Cf *m_cells;          //!< Equalised data cells, in carrier order, unit average power
        const float *m_csi;         //!< Per-cell reliability, 1 = average
        int m_nbCells;
    };
    typedef std::function<void(const Symbol& symbol)> SymbolCallback;
    typedef std::function<void(const char *message)> LogCallback;

    struct Status
    {
        State m_state = Acquire;
        Mode m_mode = Mode::Mode8K;
        GuardInterval m_guard = GuardInterval::G1_32;
        double m_frequencyOffset = 0.0; //!< Hz
        double m_clockOffset = 0.0;     //!< ppm
        int m_clockMeasurements = 0;    //!< Symbols it's averaged over, up to 100
        double m_gaCorrelation = 0.0;   //!< Guard interval correlation at acquisition, 0..1
        double m_delay = 0.0;           //!< Delay of the first path from the FFT window start, samples
        double m_spread = 0.0;          //!< Delay between the first and last significant paths, samples
        double m_snr = 0.0;             //!< dB, of the equalised cells, from the continual pilots' error; 0 if not known
        bool m_tpsValid = false;
        TPS m_tps;
        int m_cellId = -1;              //!< -1 if not signalled or not yet received
        uint64_t m_symbols = 0;         //!< Symbols demodulated
        uint64_t m_windowSteps = 0;     //!< Timing corrections
        uint64_t m_acquisitions = 0;
        uint64_t m_resyncs = 0;         //!< Timing jumps recovered from, without acquiring again
    };

    //! If fftFactory is null, FFT engines are created and planned here
    explicit OFDMDemod(int bandwidthHz = 8000000, FFTFactory *fftFactory = nullptr);
    ~OFDMDemod();

    void setSymbolCallback(SymbolCallback callback) { m_callback = callback; }
    void setLogCallback(LogCallback callback) { m_log = callback; } //!< For state changes and their measurements
    void reset();
    void process(const Cf *samples, int nbSamples);
    //! The clock offset of the samples from now has changed by ppm, as when they're resampled to correct it
    void adjustClockOffset(double ppm) { m_clockOffset += ppm; }
    //! Narrowband interferers notched out before the FFT, Hz from the input's centre, so their carriers are erased
    void setNotches(const std::vector<double>& frequencies, double inputRate);
    Status getStatus() const;
    //! For display, from the most recent symbol: up to maxCells equalised data cells, and the channel
    //! frequency response magnitude in dB, relative to its mean, at points evenly spread over the carriers.
    //! Not thread safe: call from the thread that calls process()
    void getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const;
    //! The latest impulse response, in at most points bins, empty if none. Not thread safe, as getDisplayData()
    void getImpulseResponse(ImpulseResponse& response, int points) const;

private:
    struct RingSymbol
    {
        std::vector<Cf> m_y;    //!< FFT output, carriers 0..Kmax
        int m_spPhase;          //!< Scattered pilot phase, l mod 4
        double m_rotation;      //!< Phase removed from the FFT output, to align it with the previous symbol
        bool m_coherent = true; //!< False if its continual pilots didn't correlate with the reference symbol's
        double m_drift = 0.0;   //!< Timing drift from the clock offset, samples, since an arbitrary symbol
        bool m_valid = false;   //!< False if lost or misaligned, as when the timing jumps
    };

    static const int m_ringSize = 7; //!< Channel estimation interpolates over +/-3 symbols
    //! m_seq is kept below twice this, so it doesn't overflow. A multiple of the ring size and the scattered pilot
    //! period, so subtracting it leaves the ring positions and pilot phases as they were
    static const int m_seqPeriod = 4 * m_ringSize * (1 << 20);

    static const double m_giLossFraction;       //!< Of the usual guard interval correlation, below which the timing is lost
    static const double m_giAveraging;          //!< Of the guard interval correlation, per symbol
    static const double m_twoPi;
    static const int m_acquireSymbols = 12;     //!< Symbols of the longest mode analysed per acquisition attempt
    static const double m_acquireThreshold;     //!< Minimum normalised guard interval correlation
    static const double m_intCFOThreshold;      //!< Minimum normalised continual pilot correlation
    static const double m_frequencyLoopGain;
    static const double m_delaySmoothing;
    //! Symbols whose significant paths all spread over more than the guard interval (net of those that didn't) before
    //! the least spread is taken, rather than the last measurement kept, so bursts of noise don't count
    static const int m_cirRejectSymbols = 8;
    static const float m_pilotBoost;            //!< Scattered and continual pilot amplitude (4.5.3)
    static const float m_noiseAlpha;            //!< Per symbol, for the noise on each carrier
    static const int m_noiseSettleSymbols = 64; //!< Before the per-carrier noise is used
    static const float m_noiseFloor;            //!< Least noise assumed on a carrier, relative to the mean
    //! The carriers near notches are worked out again when the frequency offset has moved this far, as the frequency
    //! loop moves it a little every symbol. Their compensation's phase changes by at most 0.03 degrees a Hz
    static const double m_notchOffsetTolerance; //!< Hz
    //! Samples used are discarded from the buffer once there are this many, keeping it small enough to stay in cache
    static const int64_t m_bufferDiscard = 1 << 16;

    static inline Cf complexMultiply(Cf a, Cf b);
    static void rotate(const Cf *x, Cf *y, int n, double phase, double step);
    static void scale(Cf *y, int n, Cf c);
    static void multiply(Cf *y, const Cf *c, int n);
    static void equalise(const Cf *y, const Cf *h, Cf derotate, int n, Cf *e, float *power);
    static inline std::complex<double> unit(const Cf& v);
    static void convolveCentred(const Cf *x, int n, const float *w, int half, Cf *y, bool symmetric, int done = -1);
    static void impulseResponseDb(const std::vector<float>& power, double resolution, int points, std::vector<float>& powerDb,
        double& binResolution);

    void configureMode(Mode mode, GuardInterval guard);
    bool acquire();
    void processSymbol();
    void findIntegerCFO(const Cf *bins);
    void findPilotPhase();
    void estimateChannel(int seq);
    void designSmoothingFilter(double cutoff);
    void designInterpolationFilter();
    bool measureImpulseResponse(int t, double& firstPath, double& spread, double& strongest);
    void prepareHypotheses();
    double hypothesisMetric(int t, int startIndex, int alias);
    void smoothChannel();
    void erasePilotOutliers();
    void updateNoise(int phase);
    void decodeTPSBit(int bit, bool known);
    void advance(bool valid);
    void outputSymbol(int t);
    void skipSymbol();
    double guardCorrelation(int64_t start) const;
    int scatteredPilotPhase(const std::vector<Cf>& y) const;
    void resync();
    void trackFrequency();
    void trackTiming();
    void applyWindowStep(int step);
    void startAcquire();
    RingSymbol& ring(int seq) { return m_ring[(seq % m_ringSize + m_ringSize) % m_ringSize]; }
    const Cf *bufferAt(int64_t index) const { return &m_buffer[index - m_bufferOffset]; }
    void log(const char *format, ...);

    double m_sampleRate;
    SymbolCallback m_callback;
    LogCallback m_log;
    State m_state;

    // Mode
    Mode m_mode;
    GuardInterval m_guard;
    const Tables *m_tables;
    int m_fftSize;
    int m_guardSize;
    int m_symbolSize;
    int m_kMax;
    int m_backoff;              //!< FFT window starts this many samples before the end of the guard interval
    // An FFT engine, from the factory if there is one
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

    FFT m_fft;
    FFT m_cirFft;               //!< For the channel impulse response, from the pilots
    std::vector<float> m_cirWindow;
    std::vector<float> m_cirPower;
    double m_cirResolution = 0.0;       //!< Samples a bin of m_cirPower, 0 until measured
    std::vector<int> m_cirBins;         //!< Significant paths
    std::vector<int> m_cirAbove;        //!< For findAboveAVX2's output, a CIR size
    std::vector<Cf> m_cirPhasors;       //!< exp(-j 2 pi n / (3 * CIR size))
    std::vector<Cf> m_cirModel;         //!< Channel at the TPS carriers, from a hypothesis
    std::vector<Cf> m_cirPrefix;        //!< For each TPS carrier, prefix sums over the paths of their contributions
    std::vector<Cf> m_cirWrap;          //!< For each TPS carrier, the phasor for a path a CIR period later
    std::vector<Cf> m_cirAlias;         //!< For each TPS carrier, phasors for an alias of -1, 0 and 1
    int m_aliasCount;                   //!< Consecutive symbols the first path appears to have jumped
    int m_cirRejected;                  //!< Symbols whose paths spread over the guard interval, net of others

    // Samples
    std::vector<Cf> m_buffer;
    int64_t m_bufferOffset;     //!< Absolute index of m_buffer[0]
    int64_t m_symbolStart;      //!< Absolute index of the start of the next symbol's guard interval
    int m_windowStep;           //!< Applied to the next symbol start

    // Frequency correction
    double m_ncoFreq;           //!< Radians per sample, applied to the input
    double m_ncoPhase;
    int64_t m_ncoIndex;         //!< Absolute sample index at which m_ncoPhase applies

    // Symbol ring and channel estimate
    std::vector<int> m_carrierBin;  //!< FFT bin of each carrier
    RingSymbol m_ring[m_ringSize];
    int m_seq;                  //!< Symbols since pilot sync, less multiples of m_seqPeriod
    int m_spPhase0;             //!< Scattered pilot phase of m_seq 0
    std::vector<Cf> m_y;
    std::vector<Cf> m_prevBins;
    std::vector<Cf> m_hp;       //!< Channel estimate on every third carrier
    std::vector<float> m_pilotSign; //!< Of each scattered pilot (on every third carrier), divided by its boost
    std::vector<float> m_pilotPower;
    std::vector<float> m_pilotSorted;
    std::vector<uint8_t> m_pilotErased;
    std::vector<Cf> m_hs;       //!< m_hp smoothed across frequency
    std::vector<Cf> m_hi[2];    //!< Interpolated 1 and 2 carriers after each pilot
    std::vector<float> m_smoothingTaps; //!< Low pass filter matched to the delay spread, empty for none
    double m_smoothingCutoff;           //!< Cycles per pilot
    static const int m_smoothingHalfLength = 40;
    std::vector<float> m_interpolationTaps[3]; //!< Interpolate from every third carrier, per carrier index mod 3
    static const int m_interpolationHalfLength = 8; //!< Pilots each side
    std::vector<Cf> m_h;        //!< Channel estimate of the symbol being output
    std::vector<Cf> m_equalised;        //!< Every carrier of the symbol being output
    std::vector<float> m_power;         //!< |m_h|^2
    std::vector<float> m_dataMask[4];   //!< 1 for data carriers, 0 for others, per scattered pilot phase
    std::vector<Cf> m_cells;
    std::vector<float> m_csi;
    std::vector<float> m_noise;         //!< Noise power per carrier (from index 2), before equalisation, -1 until measured
    std::vector<float> m_noiseSmoothed; //!< Smoothed across carriers and limited, for the CSI
    std::vector<float> m_noiseInverse;  //!< 1 / m_noiseSmoothed
    NotchedCarriers m_notched;
    std::vector<double> m_notchFrequencies; //!< As last set
    double m_notchInputRate = 0.0;
    bool m_notchesChanged = true;       //!< So the carriers near them are to be found again
    double m_notchOffset = 0.0;         //!< Frequency offset they were found for, Hz
    int m_noiseSymbols;                 //!< Symbols that have contributed to m_noise
    double m_pilotNoise = -1.0;         //!< Mean error power of the equalised continual pilots, -1 if not measured
    Constellation m_noiseConstellation; //!< Of the decisions m_noise is from
    std::vector<Cf> m_stepPhasors;      //!< For window steps
    std::vector<Cf> m_prevTPS;  //!< Equalised TPS carriers of the previous output symbol
    bool m_prevTPSValid;
    double m_delay;             //!< Smoothed delay of the first path from the window start, samples
    double m_spread;            //!< Smoothed spread of the paths, samples
    double m_strongest;         //!< Smoothed delay of the strongest path from the first, samples

    // Acquisition sub-states
    int m_stateSymbols;         //!< Symbols processed in the current state
    int m_intCFOCandidate;
    int m_lastSPPhase;
    int m_spPhaseRun;           //!< Consecutive symbols with consistent scattered pilot phase

    // TPS
    uint16_t m_tpsSync;
    int m_symbolIndex;          //!< Of the symbol being output, -1 if not known
    int m_frameNumber;
    uint8_t m_tpsBits[symbolsPerFrame];
    TPS m_tps;
    bool m_tpsValid;
    TPS m_tpsCandidate;
    bool m_tpsCandidateValid;
    TPS m_tpsPending;           //!< Changed parameters, for the next super-frame
    bool m_tpsPendingValid;
    int m_tpsFailures;
    bool m_tpsSyncFailed;       //!< The current frame's sync word was wrong, which m_tpsFailures has counted
    int m_incoherentSymbols;    //!< Consecutive symbols where the continual pilots don't correlate
    bool m_tpsBitsLost;         //!< Some of the current frame's TPS bits weren't received
    uint16_t m_tpsHistory;      //!< The last 16 TPS bits, to find the sync word in
    int m_tpsHistoryBits;       //!< Consecutive bits in it that were received
    int m_tpsRealign;           //!< Symbols left to find the frame position in after a resync, 0 if not looking

    // Timing jumps: the guard interval correlation at the window collapses when samples are lost or repeated (as
    // by an SDR that can't keep up, or a recording of one). The timing is found again, keeping the frequency,
    // and the scattered pilot phase says how many symbols were lost, so the frame and the FEC stay aligned
    double m_giAverage;         //!< Guard interval correlation at the window, 0..1
    int m_giLow;                //!< Consecutive symbols with too little
    bool m_resyncPending;       //!< Find the timing again, once there are enough samples
    bool m_resyncPhase;         //!< Check the scattered pilot phase of the next symbol
    bool m_outputErased;        //!< Output m_erasedCells, for a symbol that wasn't received
    std::vector<Cf> m_erasedCells;
    std::vector<float> m_erasedCSI;
    uint64_t m_resyncs;
    int m_cellIdBytes[2];       //!< High and low bytes of the cell id, -1 until received

    // Status
    double m_gaCorrelation;
    double m_clockOffset;
    int m_clockMeasurements;    //!< Averaged into m_clockOffset, up to 100
    uint64_t m_symbolCount;
    uint64_t m_windowSteps;
    uint64_t m_acquisitions;
    int m_acquireFailures;
};

} // namespace dvbt

#endif // MODEMDVB_DVBTOFDM_H
