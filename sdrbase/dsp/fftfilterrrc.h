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

#ifndef INCLUDE_DSP_FFTFILTERRRC_H
#define INCLUDE_DSP_FFTFILTERRRC_H

#include <complex>
#include <vector>

#include "gfft.h"
#include "export.h"

/**
 * @brief FFT-based root raised cosine filter for complex signals
 *
 * Implements a frequency-domain root raised cosine (RRC) filter using overlap-add
 * FFT convolution. The RRC filter is commonly used for pulse shaping in digital
 * communications to minimize intersymbol interference (ISI) while satisfying
 * the Nyquist criterion.
 *
 * The filter operates in the frequency domain for computational efficiency,
 * applying the RRC frequency response directly to the FFT bins of the input signal.
 */
class SDRBASE_API FFTFilterRRC
{
public:
    using Complex = std::complex<float>;

    /**
     * @brief Construct FFT-based root raised cosine filter
     *
     * @param len FFT length (should be power of 2 for efficiency)
     */
    explicit FFTFilterRRC(int len);

    /**
     * @brief Destructor
     */
    ~FFTFilterRRC();

    // Disable copy semantics (manages raw pointers)
    FFTFilterRRC(const FFTFilterRRC&) = delete;
    FFTFilterRRC& operator=(const FFTFilterRRC&) = delete;

    /**
     * @brief Create root raised cosine filter in frequency domain
     *
     * @param symbolRate Symbol rate in Hz (normalized to sample rate)
     * @param rolloff Roll-off factor (beta) - typically 0.2 to 0.5
     *                0 = rectangular, 1 = full cosine roll-off
     *
     * The filter bandwidth extends from -symbolRate*(1+rolloff)/2 to
     * +symbolRate*(1+rolloff)/2 in normalized frequency (0 to 0.5 = Nyquist).
     *
     * Creates mathematically correct RRC frequency response for use in
     * digital communications applications.
     */
    void create(float symbolRate, float rolloff);
    /**
     * @brief Process one complex input sample through the filter
     *
     * Uses overlap-add FFT convolution. Accumulates input samples until
     * a half-buffer is filled, then performs FFT, frequency-domain multiplication
     * with the RRC filter, IFFT, and overlap-add to produce output samples.
     *
     * @param input Complex input sample
     * @param output Pointer to output buffer (will point to internal buffer)
     * @return Number of output samples available (0 or fftLen/2)
     */
    int process(const Complex& input, Complex** output);

    /**
     * @brief Get FFT length
     * @return FFT length in samples
     */
    int getFFTLength() const { return m_fftLen; }

    /**
     * @brief Get current symbol rate
     * @return Symbol rate (normalized)
     */
    float getSymbolRate() const { return m_symbolRate; }

    /**
     * @brief Get current rolloff factor
     * @return Rolloff factor (beta)
     */
    float getRolloff() const { return m_rolloff; }

    /**
     * @brief Get pointer to frequency-domain filter coefficients
     *
     * The filter coefficients are in the frequency domain, corresponding to the FFT bins.
     *
     * @return Pointer to array of complex filter coefficients (length = FFT length)
     */
    const Complex* getFilter() const { return m_filter; }

private:
    /**
     * @brief Initialize internal buffers and FFT engine
     */
    void initFilter();

    /**
     * @brief Compute RRC frequency response for a given frequency bin
     *
     * Implements the RRC transfer function in frequency domain:
     * - For |f| <= (1-beta)/(2T): H(f) = T
     * - For (1-beta)/(2T) < |f| <= (1+beta)/(2T): H(f) = T/2 * [1 + cos(pi*T/beta * (|f| - (1-beta)/(2T)))]
     * - For |f| > (1+beta)/(2T): H(f) = 0
     *
     * @param symbolRate Symbol rate (normalized to sample rate)
     * @param rolloff Roll-off factor (beta)
     * @param binIndex FFT bin index
     * @param fftLen FFT length
     * @return Complex frequency response at this bin
     */
    Complex computeRRCResponse(float symbolRate, float rolloff, int binIndex, int fftLen) const;

    int m_fftLen;              //!< FFT length
    int m_fftLen2;             //!< Half FFT length
    g_fft<float>* m_fft;       //!< FFT engine
    Complex* m_filter;         //!< Frequency-domain filter coefficients
    Complex* m_fftBuffer;      //!< Time-domain input buffer for FFT
    Complex* m_overlapBuffer;  //!< Overlap buffer for overlap-add
    Complex* m_outputBuffer;   //!< Output buffer
    int m_inputIndex;          //!< Current input buffer index
    float m_symbolRate;        //!< Current symbol rate (normalized)
    float m_rolloff;           //!< Current rolloff factor
};

#endif // INCLUDE_DSP_FFTFILTERRRC_H
