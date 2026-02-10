///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2026 SDRangel contributors                                      //
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

#include "dsp/firfilterrrc.h"

#include <algorithm>
#include <cmath>

FIRFilterRRC::FIRFilterRRC() :
    m_ptr(0),
    m_symbolRate(0.0f),
    m_rolloff(0.0f),
    m_samplesPerSymbol(0)
{
}

void FIRFilterRRC::create(float symbolRate, float rolloff, int symbolSpan, Normalization normalization)
{
    m_symbolRate = symbolRate;
    m_rolloff = rolloff;

    // Clamp rolloff to valid range [0, 1]
    if (m_rolloff < 0.0f) {
        m_rolloff = 0.0f;
    } else if (m_rolloff > 1.0f) {
        m_rolloff = 1.0f;
    }

    // Calculate samples per symbol
    m_samplesPerSymbol = static_cast<int>(std::round(1.0f / symbolRate));

    // Calculate number of taps (always odd for symmetry)
    int numTaps = symbolSpan * m_samplesPerSymbol + 1;
    if ((numTaps & 1) == 0) {
        numTaps++;  // Ensure odd number of taps
    }

    // Allocate tap storage (only half + center due to symmetry)
    int halfTaps = numTaps / 2 + 1;
    m_taps.resize(halfTaps);

    // Calculate filter taps
    int center = numTaps / 2;
    for (int i = 0; i < halfTaps; i++)
    {
        // Time offset from center in symbol periods
        float t = static_cast<float>(i - center) / static_cast<float>(m_samplesPerSymbol);
        m_taps[i] = computeRRCTap(t, m_rolloff);
    }

    // Apply normalization
    if (normalization == Normalization::Energy)
    {
        // Normalize energy: sqrt(sum of squares) = 1
        float sum = 0.0f;
        for (int i = 0; i < halfTaps - 1; i++) {
            sum += m_taps[i] * m_taps[i] * 2.0f;  // Two symmetric taps
        }
        sum += m_taps[halfTaps - 1] * m_taps[halfTaps - 1];  // Center tap
        float scale = std::sqrt(sum);

        if (scale > 0.0f) {
            for (int i = 0; i < halfTaps; i++) {
                m_taps[i] /= scale;
            }
        }
    }
    else if (normalization == Normalization::Amplitude)
    {
        // Normalize amplitude: max output for bipolar sequence ≈ 1
        float maxGain = 0.0f;
        for (int offset = 0; offset < m_samplesPerSymbol; offset++)
        {
            float gain = 0.0f;
            for (int i = offset; i < halfTaps - 1; i += m_samplesPerSymbol) {
                gain += std::abs(m_taps[i]) * 2.0f;  // Both sides
            }
            // Add center tap if aligned
            if ((center - offset) % m_samplesPerSymbol == 0) {
                gain += std::abs(m_taps[halfTaps - 1]);
            }
            if (gain > maxGain) {
                maxGain = gain;
            }
        }

        if (maxGain > 0.0f) {
            for (int i = 0; i < halfTaps; i++) {
                m_taps[i] /= maxGain;
            }
        }
    }
    else if (normalization == Normalization::Gain)
    {
        // Normalize for unity gain: sum of taps = 1
        float sum = 0.0f;
        for (int i = 0; i < halfTaps - 1; i++) {
            sum += m_taps[i] * 2.0f;  // Two symmetric taps
        }
        sum += m_taps[halfTaps - 1];  // Center tap

        if (sum > 0.0f) {
            for (int i = 0; i < halfTaps; i++) {
                m_taps[i] /= sum;
            }
        }
    }

    // Initialize sample buffer
    m_samples.resize(numTaps);
    std::fill(m_samples.begin(), m_samples.end(), Complex(0.0f, 0.0f));
    m_ptr = 0;
}

float FIRFilterRRC::computeRRCTap(float t, float rolloff) const
{
    constexpr float Ts = 1.0f;  // Symbol period (normalized)
    const float beta = rolloff;

    // Handle special case: t = 0
    if (t == 0.0f) {
        // h(0) = (1/T) * (1 + beta*(4/π - 1))
        return (1.0f / Ts) * (1.0f + beta * (4.0f / M_PI - 1.0f));
    }

    // Check for zeros of denominator: 1 - (4*β*t/T)² = 0
    // This occurs at t = ±T/(4*β)
    const float denomCheck = 4.0f * beta * t / Ts;
    if (std::abs(std::abs(denomCheck) - 1.0f) < 1e-6f && beta > 0.0f)
    {
        // h(±T/4β) = (β/(T*√2)) * [(1+2/π)*sin(π/4β) + (1-2/π)*cos(π/4β)]
        const float arg = M_PI / (4.0f * beta);
        return (beta / (Ts * std::sqrt(2.0f))) *
               ((1.0f + 2.0f / M_PI) * std::sin(arg) +
                (1.0f - 2.0f / M_PI) * std::cos(arg));
    }

    // General case
    const float numerator = std::sin(M_PI * t / Ts * (1.0f - beta)) +
                            4.0f * beta * t / Ts * std::cos(M_PI * t / Ts * (1.0f + beta));
    const float denominator = M_PI * t / Ts * (1.0f - denomCheck * denomCheck);

    return numerator / (denominator * Ts);
}

FIRFilterRRC::Complex FIRFilterRRC::filter(const Complex& input)
{
    const int numTaps = static_cast<int>(m_samples.size());
    const int halfTaps = static_cast<int>(m_taps.size()) - 1;

    // Store input sample in circular buffer
    m_samples[m_ptr] = input;

    // Perform convolution using symmetry
    Complex acc(0.0f, 0.0f);
    int a = m_ptr;
    int b = (a == numTaps - 1) ? 0 : a + 1;

    // Process symmetric pairs
    for (int i = 0; i < halfTaps; i++)
    {
        acc += (m_samples[a] + m_samples[b]) * m_taps[i];

        a = (a == 0) ? numTaps - 1 : a - 1;
        b = (b == numTaps - 1) ? 0 : b + 1;
    }

    // Add center tap
    acc += m_samples[a] * m_taps[halfTaps];

    // Advance circular buffer pointer
    m_ptr = (m_ptr == static_cast<size_t>(numTaps) - 1) ? 0 : m_ptr + 1;

    return acc;
}
