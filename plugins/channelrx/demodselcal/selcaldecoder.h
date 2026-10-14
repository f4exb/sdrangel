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

#ifndef INCLUDE_SELCALDECODER_H
#define INCLUDE_SELCALDECODER_H

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class FFTEngine;

// Audio-only decoder. All timing is measured in samples, independently of the
// caller's block sizes, wall clock and audio output device.
//
// Tone levels are measured relative to the noise floor in the SELCAL band, so
// detection does not depend on the receive bandwidth or on the total audio
// power. A common tone offset is searched for around the expected offset, and
// both tone pairs of a call must share it.
class SELCALDecoder {
public:
    static constexpr int m_sampleRate = 8000;
    static constexpr int m_toneCount = 32;
    static constexpr double m_defaultThreshold = 8.0;   // dB above the noise in a tone bin
    // Hz either side of the expected offset. Pairs of adjacent tones resemble
    // their neighbours shifted by about 17.5 Hz, so a wider search would be
    // ambiguous.
    static constexpr double m_searchRange = 6.0;
    static constexpr double m_maxFrequencyOffset = 100.0;

    struct Tone {
        char m_symbol;
        double m_frequency;
    };

    struct Call {
        std::string m_code;
        std::uint64_t m_startSample;
        std::uint64_t m_firstEndSample;
        std::uint64_t m_secondStartSample;
        std::uint64_t m_endSample;
        double m_snr;             // dB, weaker tone relative to the noise in a tone bin
        double m_frequencyOffset; // Hz, common tone offset measured from the tones
    };
    enum class State { Idle, FirstPair, Gap, SecondPair, WaitForSilence };
    using Pair = std::array<int, 2>;

    SELCALDecoder();
    ~SELCALDecoder();
    void reset();
    void configure(double frequencyOffset, double threshold);
    // Expected common tone offset and its uncertainty, in Hz. A call in
    // progress keeps the offset it was acquired with.
    void setFrequencyOffset(double frequencyOffset, double uncertainty = m_searchRange);
    void feed(double sample);
    // Don't learn steady signals from the next samples, e.g. audio received
    // before retuning that is still being delivered.
    void holdLearning(std::uint64_t samples);

    void setCallHandler(std::function<void(const Call&)> handler)
    {
        m_handler = std::move(handler);
    }

    // Receives the decimated audio analysed by the decoder, for display.
    void setSampleHandler(std::function<void(float)> handler)
    {
        m_sampleHandler = std::move(handler);
    }

    State state() const;

    // Tone levels in dB relative to the noise in a tone bin.
    const std::array<double, m_toneCount>& levels() const
    {
        return m_levels;
    }

    // Pair detected in the latest analysis, or {-1, -1}.
    Pair pair() const
    {
        return m_detected;
    }

    // Tone offset of the call in progress, or of the latest analysis, in Hz.
    double frequencyOffset() const;

    static const std::array<Tone, m_toneCount>& tones();
    static const char *stateName(State state);
    static constexpr int m_analysisSampleRate = m_sampleRate / 2;

private:
    static constexpr int m_decimatorTaps = 63;
    static constexpr int m_halfbandTaps = (m_decimatorTaps + 1) / 4; // Non-zero taps either side of the centre
    static constexpr int m_windowSize = 400;           // 100 ms
    static constexpr int m_hopSize = 80;               // 20 ms
    static constexpr int m_fftSize = 2048;
    static constexpr double m_binWidth = static_cast<double>(m_analysisSampleRate) / m_fftSize;
    static constexpr int m_holdHops = 3;               // Dropouts tolerated within a burst
    static constexpr int m_preRollHops = 8;            // History kept to locate a burst's leading edge
    static constexpr int m_anchorSearch = 2;           // Bins either side of a call's offset
    static constexpr int m_mainLobeBins = 10;          // Half width of a tone's main lobe, and some
    static constexpr int m_peakCount = 4;
    static constexpr int m_backgroundHops = 50;        // Time constant for learning steady signals
    static constexpr double m_steadyLevel = 4.0;       // Steady signal power relative to noise
    static constexpr double m_steadyMargin = 1.25;     // Proportion of a steady signal's power removed
    // Lower bound for the noise estimate, so digital silence cannot produce tones.
    static constexpr double m_noiseFloor = 1e-15;
    // Burst and gap limits in seconds, with an allowance for edge measurement.
    static constexpr double m_minBurst = 0.725;
    static constexpr double m_maxBurst = 1.275;
    static constexpr double m_minGap = 0.075;
    static constexpr double m_maxGap = 0.325;
    // Fades can only shorten bursts and lengthen gaps. One burst of a call may be
    // measured as short as this, and the gap may grow by however much each burst
    // is shorter than nominal.
    static constexpr double m_nominalBurst = 1.0;
    static constexpr double m_minFadedBurst = 0.45;
    // A fade can split a burst. Parts of the same pair this close are joined.
    static constexpr double m_maxFadeGap = 0.45;
    // Impulse blanker: samples this much larger than the mean magnitude are
    // removed, along with those around them.
    static constexpr double m_impulseLevel = 5.0;
    static constexpr int m_impulseHold = 8;            // 2 ms at 4 kHz
    static constexpr int m_impulseGap = 12;            // 3 ms at 4 kHz, between impulses of the same rise
    static constexpr double m_impulseAveraging = 1.0 / 64; // 16 ms at 4 kHz
    static constexpr int m_maxImpulse = 64;            // 16 ms, longer increases are new signals
    // Each tone may differ from its nominal frequency by this proportion (0.15%),
    // so the offsets of two tones may differ by this proportion of the sum of
    // their frequencies, plus an allowance in Hz for measuring the offsets.
    static constexpr double m_toneTolerance = 0.0015;
    static constexpr double m_offsetMeasurementError = 2.5;
    // Weaker tone of a pair relative to the stronger (-15 dB). The lowest tones are
    // near the low end of SSB audio filters, so can be much weaker than their partner.
    static constexpr double m_minBalance = 0.0316;
    using Peaks = std::array<int, m_peakCount>;

    struct Hop {
        double m_time = 0;
        double m_noise = 0;
        std::vector<float> m_power;
    };

    struct Segment {
        bool m_active = false;
        bool m_overlong = false;
        Pair m_pair{{-1, -1}};
        double m_offset = 0;   // Bins, follows the measured tone offset
        double m_acquired = 0; // Bins, offset at acquisition
        int m_first = 0;
        int m_last = 0;
        int m_missed = 0;
        std::vector<double> m_power;
        std::vector<double> m_times;
        std::vector<bool> m_detected;
        std::vector<double> m_snr;
        double m_offsetSum = 0;
        double m_offsetWeight = 0;
        std::vector<float> m_spectrum; // Sum of levels relative to noise
        int m_spectrumHops = 0;
        double m_searchCentre = 0;     // Offsets within which the burst was acquired
        int m_searchBins = 0;
    };

    struct Burst {
        bool m_valid = false;
        bool m_consistent = false;   // Both tones have the same offset
        bool m_overlong = false;
        Pair m_pair{{-1, -1}};
        double m_start = 0;
        double m_end = 0;
        double m_offset = 0;
        double m_snr = 0;
        std::uint64_t m_startSample = 0;
    };

    static std::uint64_t sampleIndex(double time);
    static double besselI0(double x);
    static double median(std::vector<double> values);
    static double maxOffsetDifference(double frequency1, double frequency2);

    std::unique_ptr<FFTEngine> m_fft;
    std::array<double, m_halfbandTaps> m_halfband{};
    double m_halfbandCentre = 0.5;
    std::array<double, 2 * m_decimatorTaps> m_history{};
    int m_historyPosition = 0;
    std::array<double, 2 * m_windowSize> m_samples{};
    std::array<float, m_windowSize> m_window{};
    double m_powerScale = 0;
    int m_position = 0;
    int m_bandStart = 0;
    int m_bandEnd = 0;
    std::array<double, m_toneCount> m_toneBins{};
    std::vector<Hop> m_hops;
    int m_hopIndex = 0;
    std::vector<float> m_scratch;
    std::vector<float> m_raw;              // Power before background removal
    std::vector<double> m_background;      // Power of steady signals, learnt while idle
    double m_backgroundNoise = 0;
    int m_backgroundCount = 0;
    std::uint64_t m_learnAfter = 0;
    std::array<double, m_toneCount> m_levels{};
    std::function<void(const Call&)> m_handler;
    std::function<void(float)> m_sampleHandler;
    double m_threshold = 0;
    double m_baseOffset = 0;  // Bins
    int m_searchBins = 0;
    double m_currentOffset = 0; // Bins
    Pair m_detected{{-1, -1}};
    std::uint64_t m_inputCount = 0;
    std::uint64_t m_analysisCount = 0;
    std::uint64_t m_hopCount = 0;
    Segment m_segment;
    Burst m_pending;
    Burst m_held;          // Burst that may continue after a fade
    bool m_holding = false;
    Burst m_called;        // Second burst of the latest call, which may also continue
    double m_blankLevel = 0;  // Mean magnitude, for the impulse blanker
    int m_blankCount = 0;
    int m_blankRun = 0;       // Samples since blanking started
    int m_blankQuiet = 0;     // Samples since the last impulse
    std::array<double, m_maxImpulse> m_blanked{}; // Samples since blanking started, before blanking
    void blank(double& sample);

    void analyse();
    const Hop& hop(int age) const;
    double pairPower(const Hop& hop, const Pair& pair, double offset) const;
    int bin(int tone, double offset) const;
    void startSegment(const Pair& pair, double offset, double snr);
    void findPeaks(const std::vector<float>& power, Peaks& peaks) const;
    bool evaluate(const std::vector<float>& power, const Peaks& peaks, double scale, double offset, Pair& pair, double& snr) const;
    int peakRange(int tone, int partner) const;
    bool peaked(const std::vector<float>& power, int tone, int partner, double offset) const;
    void accumulate(const Hop& current);
    void learnBackground(double noise);
    void measurePeak(const std::vector<float>& power, int tone, int partner, double offset, double& sum, double& weight) const;
    bool identify(Pair& pair, double& offset) const;
    void closeSegment();
    void segmentClosed(const Burst& burst);
    void releaseHeld();
    bool validBurst(const Burst& burst) const;
    bool completesCall(const Burst& burst) const;
    bool continues(const Burst& burst, const Burst& previous) const;
    bool brief(const Burst& burst) const;
    const Burst *previousPair() const;
    void burstEnded(const Burst& burst);
    void emitCall(const Burst& first, const Burst& second);
};
#endif
