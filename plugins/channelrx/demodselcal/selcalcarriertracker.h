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

#ifndef INCLUDE_SELCALCARRIERTRACKER_H
#define INCLUDE_SELCALCARRIERTRACKER_H

#include <memory>
#include <vector>

#include "dsp/dsptypes.h"

class FFTEngine;

// Measures the carrier of an 8 kHz channel: a residual USB carrier before the
// audio sideband filter removes it, or an AM carrier for coherent detection.
// The carrier must be the strongest signal within the capture range and stand
// out from the local noise floor.
class SELCALCarrierTracker {
public:
    static constexpr int m_sampleRate = 8000;
    static constexpr double m_defaultRange = 100.0;

    SELCALCarrierTracker();
    ~SELCALCarrierTracker();

    void reset();
    // Capture range either side of the channel frequency, in Hz.
    void setRange(double range);
    void feed(const Complex& sample);
    bool locked() const { return m_locked; }
    double offset() const { return m_offset; }

    // Samples by which the signal should be delayed so that carrier
    // acquisition precedes the corresponding audio.
    static constexpr int latency();

private:

    static constexpr int m_windowSize = 2048; // 256 ms
    static constexpr int m_fftSize = 4096;
    static constexpr int m_hopSize = 256;
    static constexpr int m_expiry = 6;         // Analyses without a carrier before lock is lost
    static constexpr double m_binWidth = static_cast<double>(m_sampleRate) / m_fftSize;
    static constexpr double m_minSnr = 16.0;       // Carrier power relative to the mean noise power in a bin (12 dB)
    static constexpr double m_maxSecondary = 0.5;  // Any other peak in the capture range must be at least 3 dB weaker
    static constexpr double m_minPower = 1e-12;    // Absolute floor, so that digital silence cannot acquire lock

    std::unique_ptr<FFTEngine> m_fft;
    std::vector<Complex> m_samples;
    std::vector<float> m_window;
    std::vector<double> m_noise;
    double m_windowSum = 0;
    double m_range = m_defaultRange;
    int m_position = 0;
    int m_count = 0;
    int m_missed = 0;
    int m_stable = 0;
    double m_candidate = 0;
    double m_offset = 0;
    bool m_locked = false;

    void analyse();
};

constexpr int SELCALCarrierTracker::latency()
{
    return m_windowSize + 2 * m_hopSize;
}

#endif
