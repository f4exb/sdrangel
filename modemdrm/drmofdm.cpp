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

#include "drmofdm.h"

#include <cmath>
#include <stdexcept>

namespace modemdrm
{

namespace
{
constexpr double Pi = 3.14159265358979323846;
}

OFDMDemodulator::OFDMDemodulator(Mode mode, int sampleRate) :
    m_mode(mode),
    m_sampleRate(sampleRate),
    m_usefulSamples(Parameters::forMode(mode).usefulSamples(sampleRate)),
    m_guardSamples(Parameters::forMode(mode).guardSamples(sampleRate)),
    m_backoffSamples(backoffSamplesFor(m_guardSamples)),
    m_fft(),
    m_timeDomain()
{
    if (sampleRate <= 0 || m_usefulSamples <= 0 || m_guardSamples <= 0) {
        throw std::invalid_argument("Sample rate does not give integral DRM symbol lengths");
    }

    m_fft.configure(m_usefulSamples, false);
    m_timeDomain.resize(static_cast<std::size_t>(m_usefulSamples));
    // Starting the FFT b samples early delays every carrier k by
    // exp(-j 2 pi k b / N). Undo it so callers keep the nominal window's
    // phase reference; the cyclic prefix makes the result identical when the
    // timing estimate is exact, and free of ISI when it is up to b late.
    m_backoffPhase.resize(static_cast<std::size_t>(m_usefulSamples));
    for (int bin = 0; bin < m_usefulSamples; ++bin)
    {
        const double phase = 2.0 * Pi * static_cast<double>(bin)
            * m_backoffSamples / m_usefulSamples;
        m_backoffPhase[static_cast<std::size_t>(bin)] = std::complex<float>(
            static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
}

bool OFDMDemodulator::demodulate(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    std::size_t prefixStart,
    double carrierOffsetHz,
    std::vector<std::complex<float>>& bins,
    bool spectralInversion
)
{
    // Require the whole nominal symbol, as callers size spans in symbols.
    const std::size_t nominalUsefulStart = prefixStart + static_cast<std::size_t>(m_guardSamples);
    if (!samples || nominalUsefulStart > sampleCount
        || static_cast<std::size_t>(m_usefulSamples) > sampleCount - nominalUsefulStart) {
        return false;
    }

    const std::size_t usefulStart = nominalUsefulStart - static_cast<std::size_t>(m_backoffSamples);

    // The correction phase is absolute (from the start of the input), so
    // separately demodulated symbols share one reference. Within the symbol
    // it advances by a recurrence in double precision from that exact start.
    const double radiansPerSample = -2.0 * Pi * carrierOffsetHz / m_sampleRate;
    const std::complex<double> step = std::polar(1.0, radiansPerSample);
    std::complex<double> rotation = std::polar(1.0, radiansPerSample * static_cast<double>(usefulStart));
    for (int i = 0; i < m_usefulSamples; ++i)
    {
        const std::size_t absoluteIndex = usefulStart + static_cast<std::size_t>(i);
        const std::complex<float> correction(rotation);
        const std::complex<float> sample = spectralInversion
            ? std::conj(samples[absoluteIndex]) : samples[absoluteIndex];
        m_timeDomain[static_cast<std::size_t>(i)] = sample * correction;
        rotation *= step;
    }

    bins.resize(static_cast<std::size_t>(m_usefulSamples));
    m_fft.transform(m_timeDomain.data(), bins.data());
    if (m_backoffSamples != 0)
    {
        for (int bin = 0; bin < m_usefulSamples; ++bin) {
            bins[static_cast<std::size_t>(bin)] *= m_backoffPhase[static_cast<std::size_t>(bin)];
        }
    }

    return true;
}

std::complex<float> OFDMDemodulator::carrier(
    const std::vector<std::complex<float>>& bins,
    int carrierNumber
) const
{
    if (bins.size() != static_cast<std::size_t>(m_usefulSamples)
        || carrierNumber <= -m_usefulSamples || carrierNumber >= m_usefulSamples) {
        throw std::out_of_range("DRM carrier is outside FFT output");
    }

    const int index = carrierNumber < 0 ? m_usefulSamples + carrierNumber : carrierNumber;
    return bins[static_cast<std::size_t>(index)];
}

} // namespace modemdrm
