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

#ifndef INCLUDE_DRONEIDDETECTOR_H
#define INCLUDE_DRONEIDDETECTOR_H

#include <complex>
#include <cstdint>
#include <vector>

#include "droneiddecoder.h"

// Streaming detector for DroneID bursts at 15.36 MS/s.
//
// Correlates each sample window with the window one FFT length earlier (as the cyclic prefix
// is a copy of the end of each symbol), and sums this over the 9 symbol positions of a frame.
// The same sum with a lag of half an FFT is subtracted, so continuous tones, which correlate
// at all lags, are rejected. When a frame is detected, a window of samples around it is
// made available for the decoder.
class DroneIDDetector
{
public:
    typedef std::complex<float> cf;

    // Samples either side of frame in burst window. Allows for the detector's timing being a
    // couple of symbols out at low SNR, which the decoder can correct.
    static constexpr int m_margin = 3300;

    DroneIDDetector();

    void setThreshold(float threshold) { m_threshold = threshold; }
    void reset();

    // Process one sample. Returns true when a burst window is ready.
    bool process(cf x);

    const std::vector<cf>& getBurst() const { return m_burst; }
    float getBurstMetric() const { return m_burstMetric; }
    // Sample number (count of samples processed) of first sample in burst window
    int64_t getBurstSampleIndex() const { return m_burstIndex; }

private:
    static constexpr int m_historySize = 32768;         // Samples, for burst capture
    static constexpr int m_historyMask = m_historySize - 1;
    static constexpr int m_corrSize = 16384;            // Correlations, for frame metric
    static constexpr int m_corrMask = m_corrSize - 1;
    static constexpr int m_productSize = 128;           // Products, for windowed sums
    static constexpr int m_productMask = m_productSize - 1;
    static constexpr int m_window = 72;
    static constexpr int m_lag = 1024;
    static constexpr int m_lag2 = 512;
    static constexpr int m_recalcInterval = 4096;
    static constexpr int m_searchStride = 4;    // Correlation peak is ~2 CPs wide, so needn't check every sample when searching
    // The normalised metric is also high when the frame is offset by a whole number of symbols
    // from the template, as most of the cyclic prefixes still align. So after the threshold is
    // first crossed, we search for the peak of the unnormalised score for a frame's duration,
    // which is greatest when all symbols align.
    static constexpr int m_peakSearchLength = DroneIDDecoder::m_frameLength;

    float m_threshold;
    std::vector<cf> m_x;        // Sample history
    std::vector<cf> m_c;        // Product of sample with that m_lag earlier
    std::vector<cf> m_c2;       // Product of sample with that m_lag2 earlier
    std::vector<float> m_e;     // Sample energy
    std::vector<cf> m_p;        // Windowed correlation at lag m_lag
    std::vector<cf> m_p2;       // Windowed correlation at lag m_lag2
    std::vector<float> m_r;     // Windowed energy
    std::vector<float> m_rr;    // Average of windowed energy and that m_lag earlier
    std::complex<double> m_pSum;
    std::complex<double> m_p2Sum;
    double m_rSum;
    int64_t m_n;                // Number of samples processed
    int m_offsets[9];           // Offset from last symbol's correlation peak to each symbol's

    enum State {
        Searching,
        FindingPeak,
        Capturing
    } m_state;
    float m_peakMetric;
    float m_peakScore;
    int64_t m_peakIndex;
    int64_t m_searchStart;
    int64_t m_captureEnd;
    int64_t m_holdoffEnd;

    std::vector<cf> m_burst;
    float m_burstMetric;
    int64_t m_burstIndex;

    float metric(int64_t n, float& score, bool gate) const;
    void recalculate();
};

#endif // INCLUDE_DRONEIDDETECTOR_H
