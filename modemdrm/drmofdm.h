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

#ifndef MODEMDRM_DRMOFDM_H_
#define MODEMDRM_DRMOFDM_H_

#include <complex>
#include <cstddef>
#include <vector>

#include "export.h"
#include "drmparameters.h"
#include "sdrbase/dsp/kissfft.h"

namespace modemdrm
{

class MODEMDRM_API OFDMDemodulator
{
public:
    OFDMDemodulator(Mode mode, int sampleRate);

    Mode mode() const { return m_mode; }
    int sampleRate() const { return m_sampleRate; }
    int usefulSamples() const { return m_usefulSamples; }
    int guardSamples() const { return m_guardSamples; }
    // The FFT window starts this many samples before the end of the guard
    // interval; bins are phase-corrected back to the nominal window.
    int backoffSamples() const { return m_backoffSamples; }
    static int backoffSamplesFor(int guardSamples) { return guardSamples / 8; }

    bool demodulate(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        std::size_t prefixStart,
        double carrierOffsetHz,
        std::vector<std::complex<float>>& bins,
        bool spectralInversion = false
    );

    std::complex<float> carrier(const std::vector<std::complex<float>>& bins, int carrier) const;

private:
    Mode m_mode;
    int m_sampleRate;
    int m_usefulSamples;
    int m_guardSamples;
    int m_backoffSamples;
    kissfft<float, std::complex<float>> m_fft;
    std::vector<std::complex<float>> m_timeDomain;
    std::vector<std::complex<float>> m_backoffPhase;
};

} // namespace modemdrm

#endif // MODEMDRM_DRMOFDM_H_
