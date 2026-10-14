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

#include "drmresampler.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace modemdrm
{

namespace
{

constexpr double Pi = 3.14159265358979323846;

double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    const double quarterSquare = x * x / 4.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= quarterSquare / (static_cast<double>(k) * k);
        sum += term;
        if (term < sum * 1.0e-12) {
            break;
        }
    }

    return sum;
}

// Kaiser window at position u in [-1, 1].
double kaiser(double u, double beta)
{
    if (u <= -1.0 || u >= 1.0) {
        return 0.0;
    }

    return besselI0(beta * std::sqrt(1.0 - u * u)) / besselI0(beta);
}

double sinc(double x)
{
    return std::abs(x) < 1.0e-12 ? 1.0 : std::sin(Pi * x) / (Pi * x);
}

// Kaiser design relations for a given stopband attenuation (dB).
double kaiserBeta(double attenuation)
{
    return attenuation > 50.0 ? 0.1102 * (attenuation - 8.7)
        : 0.5842 * std::pow(attenuation - 21.0, 0.4) + 0.07886 * (attenuation - 21.0);
}

constexpr double ResamplerAttenuation = 70.0;
constexpr double FilterAttenuation = 60.0;

} // namespace

void FractionalResampler::configure(int inputRate, int outputRate, double passbandHz, double stopbandHz)
{
    m_inputRate = inputRate;
    m_outputRate = outputRate;
    m_bypass = inputRate <= 0 || outputRate <= 0 || inputRate == outputRate;
    m_kernel.clear();
    m_phaseTaps.clear();
    m_phaseStep = 0;
    m_halfWidth = 0;
    if (m_bypass)
    {
        m_passbandHz = outputRate / 2.0;
        reset();
        return;
    }

    const double nyquist = std::min(inputRate, outputRate) / 2.0;
    stopbandHz = std::min(stopbandHz, nyquist);
    passbandHz = std::min(passbandHz, stopbandHz * 0.95);
    m_passbandHz = passbandHz;
    const double cutoff = (passbandHz + stopbandHz) / 2.0;
    const double transition = std::max(stopbandHz - passbandHz, 1.0);
    const double durationSeconds = (ResamplerAttenuation - 7.95) / (14.36 * transition);
    m_halfWidth = std::max(2, static_cast<int>(std::ceil(durationSeconds * inputRate / 2.0)));
    const double beta = kaiserBeta(ResamplerAttenuation);
    const int points = 2 * m_halfWidth * TableResolution + 2;
    m_kernel.resize(static_cast<std::size_t>(points));
    const double scale = 2.0 * cutoff / inputRate;
    for (int i = 0; i < points; ++i)
    {
        const double offset = static_cast<double>(i) / TableResolution - m_halfWidth;
        m_kernel[static_cast<std::size_t>(i)] = static_cast<float>(
            scale * sinc(scale * offset) * kaiser(offset / m_halfWidth, beta));
    }

    // Normalise the average DC gain over output phases to unity.
    double gain = 0.0;
    constexpr int Phases = 64;
    for (int phase = 0; phase < Phases; ++phase)
    {
        const double fraction = static_cast<double>(phase) / Phases;
        for (int k = -m_halfWidth; k <= m_halfWidth + 1; ++k) {
            gain += kernel(fraction - k);
        }
    }

    gain /= Phases;
    if (gain > 0.0)
    {
        for (float& value : m_kernel) {
            value = static_cast<float>(value / gain);
        }
    }

    // Tabulate the same kernel lookups push() would make for each phase,
    // ordered by ascending input index.
    const int step = std::gcd(inputRate, outputRate);
    const int phases = outputRate / step;
    if (phases <= MaximumPhases)
    {
        m_phaseStep = step;
        const int taps = 2 * m_halfWidth;
        m_phaseTaps.resize(static_cast<std::size_t>(phases) * static_cast<std::size_t>(taps));
        for (int phase = 0; phase < phases; ++phase)
        {
            const double fraction = static_cast<double>(static_cast<long long>(phase) * step) / m_outputRate;
            for (int tap = 0; tap < taps; ++tap)
            {
                // Tap t multiplies input whole - halfWidth + 1 + t.
                m_phaseTaps[static_cast<std::size_t>(phase) * static_cast<std::size_t>(taps)
                    + static_cast<std::size_t>(tap)] = kernel(static_cast<double>(m_halfWidth - 1 - tap) + fraction);
            }
        }
    }

    reset();
}

void FractionalResampler::reset()
{
    m_history.clear();
    m_historyStart = 0;
    m_received = 0;
    m_baseInput = 0;
    m_outputCount = 0;
}

long long FractionalResampler::outputTime(double& fraction) const
{
    const long long scaled = m_outputCount * static_cast<long long>(m_inputRate);
    fraction = static_cast<double>(scaled % m_outputRate) / m_outputRate;
    return m_baseInput + scaled / m_outputRate;
}

float FractionalResampler::kernel(double offset) const
{
    const double position = (offset + m_halfWidth) * TableResolution;
    if (position < 0.0 || position >= static_cast<double>(m_kernel.size() - 1)) {
        return 0.0f;
    }

    const std::size_t index = static_cast<std::size_t>(position);
    const float fraction = static_cast<float>(position - static_cast<double>(index));
    return m_kernel[index] + fraction * (m_kernel[index + 1] - m_kernel[index]);
}

void FractionalResampler::push(const std::complex<float>& input, std::vector<std::complex<float>>& output)
{
    if (m_bypass)
    {
        output.push_back(input);
        return;
    }

    m_history.push_back(input);
    ++m_received;
    // Output times are exact rationals, so the timeline cannot drift however
    // long the stream runs. Every outputRate outputs span inputRate inputs.
    for (;;)
    {
        double fraction = 0.0;
        const long long whole = outputTime(fraction);
        if (whole + m_halfWidth >= m_received) {
            break;
        }

        std::complex<float> sum(0.0f, 0.0f);
        const long long first = std::max<long long>(whole - m_halfWidth + 1, m_historyStart);
        const long long last = whole + m_halfWidth;
        if (m_phaseStep > 0)
        {
            const long long phase = (m_outputCount * static_cast<long long>(m_inputRate)) % m_outputRate / m_phaseStep;
            const float* taps = m_phaseTaps.data() + phase * 2 * m_halfWidth;
            const long long firstTapInput = whole - m_halfWidth + 1;
            for (long long k = first; k <= last; ++k) {
                sum += taps[k - firstTapInput] * m_history[static_cast<std::size_t>(k - m_historyStart)];
            }
        }
        else
        {
            for (long long k = first; k <= last; ++k)
            {
                const float weight = kernel(static_cast<double>(whole - k) + fraction);
                sum += weight * m_history[static_cast<std::size_t>(k - m_historyStart)];
            }
        }

        output.push_back(sum);
        if (++m_outputCount == m_outputRate)
        {
            m_baseInput += m_inputRate;
            m_outputCount = 0;
        }
    }

    // Keep only the history the next output can still reach.
    double fraction = 0.0;
    const long long keepFrom = outputTime(fraction) - m_halfWidth + 1;
    if (keepFrom - m_historyStart > 4096)
    {
        m_history.erase(m_history.begin(),
            m_history.begin() + static_cast<std::ptrdiff_t>(keepFrom - m_historyStart));
        m_historyStart = keepFrom;
    }
}

void BandFilter::configure(int sampleRate)
{
    m_sampleRate = sampleRate;
    int taps = std::max(31, static_cast<int>(std::lround(sampleRate / 750.0)));
    if ((taps & 1) == 0) {
        ++taps;
    }

    m_taps = taps;
    m_fftSize = 1;
    while (m_fftSize < 4 * m_taps) {
        m_fftSize <<= 1;
    }

    m_blockSize = m_fftSize - (m_taps - 1);
    m_forward.configure(m_fftSize, false);
    m_inverse.configure(m_fftSize, true);
    m_buffer.assign(static_cast<std::size_t>(m_fftSize), {0.0f, 0.0f});
    m_spectrum.assign(static_cast<std::size_t>(m_fftSize), {0.0f, 0.0f});
    m_result.assign(static_cast<std::size_t>(m_fftSize), {0.0f, 0.0f});
    setBand(-sampleRate / 2.0, sampleRate / 2.0);
    reset();
}

double BandFilter::transitionHz() const
{
    return m_taps > 1
        ? (FilterAttenuation - 7.95) * m_sampleRate / (14.36 * (m_taps - 1)) : 0.0;
}

void BandFilter::setBand(double lowHz, double highHz)
{
    if (m_sampleRate <= 0 || m_taps <= 0) {
        return;
    }

    if (highHz < lowHz) {
        std::swap(lowHz, highHz);
    }

    m_lowHz = lowHz;
    m_highHz = highHz;
    const int middle = (m_taps - 1) / 2;
    std::vector<std::complex<float>> taps(static_cast<std::size_t>(m_fftSize), {0.0f, 0.0f});
    const double halfWidth = (highHz - lowHz) / 2.0 + transitionHz() / 2.0;
    if (halfWidth >= m_sampleRate / 2.0) {
        // Everything passes: a pure delay keeps the timing identical.
        taps[static_cast<std::size_t>(middle)] = {1.0f, 0.0f};
    }
    else
    {
        const double centre = (lowHz + highHz) / 2.0;
        const double scale = 2.0 * halfWidth / m_sampleRate;
        const double beta = kaiserBeta(FilterAttenuation);
        for (int n = 0; n < m_taps; ++n)
        {
            const double offset = static_cast<double>(n - middle);
            const double lowpass = scale * sinc(scale * offset)
                * kaiser(offset / (middle + 1), beta);
            const double phase = 2.0 * Pi * centre * offset / m_sampleRate;
            taps[static_cast<std::size_t>(n)] = std::complex<float>(
                static_cast<float>(lowpass * std::cos(phase)),
                static_cast<float>(lowpass * std::sin(phase)));
        }

        // Unity gain at the band centre.
        std::complex<double> gain(0.0, 0.0);
        for (int n = 0; n < m_taps; ++n)
        {
            const double phase = -2.0 * Pi * centre * (n - middle) / m_sampleRate;
            gain += std::complex<double>(taps[static_cast<std::size_t>(n)])
                * std::polar(1.0, phase);
        }

        const float magnitude = static_cast<float>(std::abs(gain));
        if (magnitude > 0.0f)
        {
            for (int n = 0; n < m_taps; ++n) {
                taps[static_cast<std::size_t>(n)] /= magnitude;
            }
        }
    }

    m_response.resize(static_cast<std::size_t>(m_fftSize));
    m_forward.transform(taps.data(), m_response.data());
    const float normalise = 1.0f / static_cast<float>(m_fftSize);
    for (auto& value : m_response) {
        value *= normalise;
    }
}

void BandFilter::reset()
{
    std::fill(m_buffer.begin(), m_buffer.end(), std::complex<float>(0.0f, 0.0f));
    m_fill = 0;
}

void BandFilter::push(const std::complex<float>& input, std::vector<std::complex<float>>& output)
{
    if (m_fftSize == 0)
    {
        output.push_back(input);
        return;
    }

    const int overlap = m_taps - 1;
    m_buffer[static_cast<std::size_t>(overlap + m_fill)] = input;
    if (++m_fill < m_blockSize) {
        return;
    }

    m_forward.transform(m_buffer.data(), m_spectrum.data());
    for (int i = 0; i < m_fftSize; ++i) {
        m_spectrum[static_cast<std::size_t>(i)] *= m_response[static_cast<std::size_t>(i)];
    }

    m_inverse.transform(m_spectrum.data(), m_result.data());
    output.insert(output.end(), m_result.begin() + overlap, m_result.end());
    std::copy(m_buffer.end() - overlap, m_buffer.end(), m_buffer.begin());
    m_fill = 0;
}

} // namespace modemdrm
