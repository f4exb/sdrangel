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

#ifndef MODEMDRM_DRMRESAMPLER_H_
#define MODEMDRM_DRMRESAMPLER_H_

#include <complex>
#include <cstddef>
#include <vector>

#include "export.h"
#include "sdrbase/dsp/kissfft.h"

namespace modemdrm
{

// Streaming band-limited fractional resampler (Kaiser-windowed sinc, table
// lookup with linear interpolation). DRM symbol lengths are integral only at
// multiples of 1 500 Hz (A-D) or 4 000 Hz (E), so arbitrary device rates are
// converted to a fixed internal rate. Output timing is exact: the kernel is
// centred on each output instant, so the only effect is a constant latency.
class MODEMDRM_API FractionalResampler
{
public:
    // passbandHz/stopbandHz are the anti-alias edges at the lower of the two
    // rates. Equal rates bypass the kernel entirely.
    void configure(int inputRate, int outputRate, double passbandHz, double stopbandHz);
    void reset();

    bool bypass() const { return m_bypass; }
    int inputRate() const { return m_inputRate; }
    int outputRate() const { return m_outputRate; }
    // Anti-alias passband edge in Hz (the output Nyquist when bypassed).
    double passbandHz() const { return m_passbandHz; }

    // Appends zero or more output samples for one input sample.
    void push(const std::complex<float>& input, std::vector<std::complex<float>>& output);

private:
    static constexpr int TableResolution = 512; // points per input sample

    bool m_bypass = true;
    int m_inputRate = 0;
    int m_outputRate = 0;
    double m_passbandHz = 0.0;
    int m_halfWidth = 0;          // kernel half-width in input samples
    std::vector<float> m_kernel;  // h(d) for d in [-halfWidth, halfWidth]
    // Output positions take outputRate / gcd(rates) fractional phases; when
    // there are few, the taps of each are tabulated (2 * halfWidth per phase).
    static constexpr int MaximumPhases = 1024;
    int m_phaseStep = 0;          // gcd of the rates
    std::vector<float> m_phaseTaps;
    std::vector<std::complex<float>> m_history;
    long long m_historyStart = 0; // absolute index of m_history[0]
    long long m_received = 0;     // input samples received
    // Output n (n = base outputs + m_outputCount) falls at input time
    // m_baseInput + m_outputCount * inputRate / outputRate, kept exact.
    long long m_baseInput = 0;
    long long m_outputCount = 0;

    float kernel(double offset) const;
    long long outputTime(double& fraction) const;
};

// Streaming complex band-pass FIR by overlap-save fast convolution. The tap
// count is fixed per sample rate and the filter is linear-phase, so changing
// the band (for example narrowing it after lock) never moves symbol timing.
class MODEMDRM_API BandFilter
{
public:
    void configure(int sampleRate);
    // Pass [lowHz, highHz] relative to the reference frequency at DC, with a
    // transition of about transitionHz() outside each edge.
    void setBand(double lowHz, double highHz);
    void reset();

    int sampleRate() const { return m_sampleRate; }
    double lowHz() const { return m_lowHz; }
    double highHz() const { return m_highHz; }
    int taps() const { return m_taps; }
    // Output lags input by this many samples.
    int delaySamples() const { return (m_taps - 1) / 2; }
    double transitionHz() const;

    void push(const std::complex<float>& input, std::vector<std::complex<float>>& output);

private:
    int m_sampleRate = 0;
    int m_taps = 0;
    int m_fftSize = 0;
    int m_blockSize = 0;
    double m_lowHz = 0.0;
    double m_highHz = 0.0;
    kissfft<float, std::complex<float>> m_forward;
    kissfft<float, std::complex<float>> m_inverse;
    std::vector<std::complex<float>> m_response;  // FFT of the taps / fftSize
    std::vector<std::complex<float>> m_buffer;    // overlap + new block
    std::vector<std::complex<float>> m_spectrum;
    std::vector<std::complex<float>> m_result;
    int m_fill = 0;                               // new samples in m_buffer
};

} // namespace modemdrm

#endif // MODEMDRM_DRMRESAMPLER_H_
