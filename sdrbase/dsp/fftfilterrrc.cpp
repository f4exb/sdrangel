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

#include "dsp/fftfilterrrc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

FFTFilterRRC::FFTFilterRRC(int len) :
    m_fftLen(len),
    m_fftLen2(len / 2),
    m_fft(nullptr),
    m_filter(nullptr),
    m_fftBuffer(nullptr),
    m_overlapBuffer(nullptr),
    m_outputBuffer(nullptr),
    m_inputIndex(0),
    m_symbolRate(0.0f),
    m_rolloff(0.0f)
{
    initFilter();
}

FFTFilterRRC::~FFTFilterRRC()
{
    delete m_fft;
    delete[] m_filter;
    delete[] m_fftBuffer;
    delete[] m_overlapBuffer;
    delete[] m_outputBuffer;
}

void FFTFilterRRC::initFilter()
{
    m_fft = new g_fft<float>(m_fftLen);

    m_filter = new Complex[m_fftLen];
    m_fftBuffer = new Complex[m_fftLen];
    m_overlapBuffer = new Complex[m_fftLen2];
    m_outputBuffer = new Complex[m_fftLen2];

    // Initialize all buffers to zero
    std::fill(m_filter, m_filter + m_fftLen, Complex(0.0f, 0.0f));
    std::fill(m_fftBuffer, m_fftBuffer + m_fftLen, Complex(0.0f, 0.0f));
    std::fill(m_overlapBuffer, m_overlapBuffer + m_fftLen2, Complex(0.0f, 0.0f));
    std::fill(m_outputBuffer, m_outputBuffer + m_fftLen2, Complex(0.0f, 0.0f));

    m_inputIndex = 0;
}

void FFTFilterRRC::create(float symbolRate, float rolloff)
{
    m_symbolRate = symbolRate;
    m_rolloff = rolloff;

    // Clamp rolloff to valid range [0, 1]
    if (m_rolloff < 0.0f) {
        m_rolloff = 0.0f;
    } else if (m_rolloff > 1.0f) {
        m_rolloff = 1.0f;
    }

    // Create filter directly in frequency domain
    std::fill(m_filter, m_filter + m_fftLen, Complex(0.0f, 0.0f));

    for (int i = 0; i < m_fftLen; i++) {
        m_filter[i] = computeRRCResponse(m_symbolRate, m_rolloff, i, m_fftLen);
    }

    // Normalize for unity gain in passband
    float maxMag = 0.0f;
    for (int i = 0; i < m_fftLen; i++) {
        float mag = std::abs(m_filter[i]);
        if (mag > maxMag) {
            maxMag = mag;
        }
    }

    if (maxMag > 0.0f) {
        for (int i = 0; i < m_fftLen; i++) {
            m_filter[i] /= maxMag;
        }
    }
}

FFTFilterRRC::Complex FFTFilterRRC::computeRRCResponse(
    float symbolRate,
    float rolloff,
    int binIndex,
    int fftLen) const
{
    // Normalize frequency to [-0.5, 0.5]
    // FFT bins: 0 to fftLen/2-1 are positive frequencies (0 to 0.5)
    //           fftLen/2 to fftLen-1 are negative frequencies (-0.5 to 0)
    float freq = static_cast<float>(binIndex) / static_cast<float>(fftLen);

    // Map to [-0.5, 0.5] range
    if (freq > 0.5f) {
        freq -= 1.0f;
    }

    // Absolute frequency
    float absFreq = std::abs(freq);

    // Compute frequency boundaries
    // For RRC: passband is Rs/2, where Rs is symbol rate
    // Transition band: Rs/2 * (1-beta) to Rs/2 * (1+beta)
    float f1 = symbolRate * (1.0f - rolloff) / 2.0f;  // Start of transition band
    float f2 = symbolRate * (1.0f + rolloff) / 2.0f;  // End of transition band

    Complex response;

    if (absFreq <= f1) {
        // Passband: constant response
        response = Complex(1.0f, 0.0f);
    } else if (absFreq > f1 && absFreq <= f2) {
        // Transition band: raised cosine roll-off
        // H(f) = 0.5 * [1 + cos(pi/beta * (|f|/Rs - (1-beta)/2))]
        float normalizedFreq = absFreq / symbolRate;  // Normalize by symbol rate
        float arg = (M_PI / rolloff) * (normalizedFreq - (1.0f - rolloff) / 2.0f);
        float amplitude = 0.5f * (1.0f + std::cos(arg));
        response = Complex(amplitude, 0.0f);
    } else {
        // Stopband: zero response
        response = Complex(0.0f, 0.0f);
    }

    return response;
}

int FFTFilterRRC::process(const Complex& input, Complex** output)
{
    // Store input sample in first half of FFT buffer
    m_fftBuffer[m_inputIndex] = input;
    m_inputIndex++;

    // Process when first half is full
    if (m_inputIndex < m_fftLen2) {
        return 0;  // Not enough samples yet
    }

    // Second half of FFT buffer is already zero-padded from initialization
    // or previous processing (overlap-add method)

    // Perform forward FFT
    m_fft->ComplexFFT(m_fftBuffer);

    // Apply filter in frequency domain (element-wise multiplication)
    for (int i = 0; i < m_fftLen; i++) {
        m_fftBuffer[i] *= m_filter[i];
    }

    // Perform inverse FFT
    m_fft->InverseComplexFFT(m_fftBuffer);

    // Overlap-add: first half gets added to overlap buffer, second half becomes new overlap
    for (int i = 0; i < m_fftLen2; i++) {
        m_outputBuffer[i] = m_fftBuffer[i] + m_overlapBuffer[i];
        m_overlapBuffer[i] = m_fftBuffer[i + m_fftLen2];
    }

    // Reset input index and clear first half of FFT buffer for next block
    m_inputIndex = 0;
    std::fill(m_fftBuffer, m_fftBuffer + m_fftLen2, Complex(0.0f, 0.0f));

    // Return pointer to output buffer
    *output = m_outputBuffer;
    return m_fftLen2;
}
