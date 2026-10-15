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

#include <algorithm>
#include <cmath>

#include "dsp/fftengine.h"

#include "selcalcarriertracker.h"

SELCALCarrierTracker::SELCALCarrierTracker() :
    m_fft(FFTEngine::create("")),
    m_samples(2 * m_windowSize),
    m_window(m_windowSize)
{
    m_fft->configure(m_fftSize, false);
    for (int i = 0; i < m_windowSize; ++i)
    {
        m_window[i] = 0.5 - 0.5 * std::cos(2 * M_PI * i / (m_windowSize - 1));
        m_windowSum += m_window[i];
    }
    setRange(m_defaultRange);
}

SELCALCarrierTracker::~SELCALCarrierTracker() = default;

void SELCALCarrierTracker::reset()
{
    std::fill(m_samples.begin(), m_samples.end(), Complex{});
    m_position = m_count = m_missed = m_stable = 0;
    m_candidate = m_offset = 0;
    m_locked = false;
}

void SELCALCarrierTracker::setRange(double range)
{
    range = std::isfinite(range) ? std::clamp(range, 10.0, 1500.0) : m_defaultRange;
    if (range != m_range) {
        reset();
    }
    m_range = range;
}

void SELCALCarrierTracker::feed(const Complex& sample)
{
    m_samples[m_position] = m_samples[m_position + m_windowSize] = sample;
    m_position = (m_position + 1) % m_windowSize;
    if (m_count < m_windowSize) {
        ++m_count;
    }
    if (m_count == m_windowSize && m_position % m_hopSize == 0) {
        analyse();
    }
}

void SELCALCarrierTracker::analyse()
{
    // Oldest to newest sample.
    const Complex *samples = &m_samples[m_position];
    Complex *input = m_fft->in();
    for (int i = 0; i < m_windowSize; ++i) {
        input[i] = samples[i] * m_window[i];
    }
    std::fill(input + m_windowSize, input + m_fftSize, Complex{});
    m_fft->transform();
    const Complex *output = m_fft->out();
    double scale = 1.0 / (m_windowSum * m_windowSum);
    auto power = [output, scale](int bin) {
        return static_cast<double>(std::norm(output[(bin + m_fftSize) % m_fftSize])) * scale;
    };

    // Search past the capture range, so that a carrier just outside it cannot
    // be mistaken for a peak at the edge of the search.
    int rangeBins = static_cast<int>(std::ceil(m_range / m_binWidth));
    int searchBins = std::min(rangeBins + 26, m_fftSize / 2 - 2);
    int noiseBins = std::min(rangeBins + 77, m_fftSize / 2 - 1);
    int peak = -searchBins;
    for (int bin = -searchBins + 1; bin <= searchBins; ++bin) {
        if (power(bin) > power(peak)) {
            peak = bin;
        }
    }
    // Excludes the peak's main lobe.
    double secondary = 0;
    for (int bin = -searchBins; bin <= searchBins; ++bin) {
        if (std::abs(bin - peak) > 8) {
            secondary = std::max(secondary, power(bin));
        }
    }
    // The median is robust to the carrier and any tones near it. For noise
    // each bin is exponentially distributed, so mean = median / ln 2.
    m_noise.resize(2 * noiseBins + 1);
    for (int bin = -noiseBins; bin <= noiseBins; ++bin) {
        m_noise[bin + noiseBins] = power(bin);
    }
    auto middle = m_noise.begin() + m_noise.size() / 2;
    std::nth_element(m_noise.begin(), middle, m_noise.end());
    double noise = *middle / M_LN2;

    double left = std::log(std::max(power(peak - 1), 1e-30));
    double centre = std::log(std::max(power(peak), 1e-30));
    double right = std::log(std::max(power(peak + 1), 1e-30));
    double curvature = left - 2 * centre + right;
    double fraction = curvature < -1e-12 ? 0.5 * (left - right) / curvature : 0;
    double frequency = (peak + std::clamp(fraction, -0.5, 0.5)) * m_binWidth;
    double carrierPower = power(peak);
    bool valid = carrierPower > m_minPower && carrierPower > m_minSnr * noise && secondary < m_maxSecondary * carrierPower &&
                 std::abs(frequency) <= m_range;
    bool measured = false;
    if (valid)
    {
        m_stable = std::abs(frequency - m_candidate) < 1 ? std::min(m_stable + 1, 2) : 1;
        m_candidate = frequency;
        if (m_stable >= 2)
        {
            m_offset = m_locked && std::abs(frequency - m_offset) < 2 ? m_offset + 0.25 * (frequency - m_offset) : frequency;
            m_locked = true;
            measured = true;
        }
    }
    else
    {
        m_stable = 0;
    }
    // Retain lock across the inter-pair gap, but expire it during silence or
    // loss of the carrier instead of applying a stale correction indefinitely.
    if (measured)
    {
        m_missed = 0;
    }
    else
    {
        m_missed = std::min(m_missed + 1, m_expiry);
        if (m_missed == m_expiry) {
            m_locked = false;
        }
    }
}
