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

#ifndef INCLUDE_DSP_FIRFILTERRRC_H
#define INCLUDE_DSP_FIRFILTERRRC_H

#include <complex>
#include <vector>

#include "export.h"

/**
 * @brief FIR root raised cosine filter for complex signals
 *
 * Implements a time-domain FIR root raised cosine (RRC) filter using direct
 * convolution. The RRC filter is commonly used for pulse shaping in digital
 * communications to minimize intersymbol interference (ISI) while satisfying
 * the Nyquist criterion.
 *
 * This filter offers sample-by-sample processing with low latency, making it
 * suitable for real-time applications. For longer filters or higher throughput,
 * consider using FFTFilterRRC which uses frequency-domain processing.
 */
class SDRBASE_API FIRFilterRRC
{
public:
    using Complex = std::complex<float>;

    /**
     * @brief Normalization modes for filter taps
     */
    enum class Normalization {
        Energy,     //!< Impulse response energy normalized (TX+RX = raised cosine peak = 1) - for matched filter pairs
        Amplitude,  //!< Bipolar symbol sequence output amplitude normalized to ±1 - for pulse amplitude preservation
        Gain        //!< Unity gain (0 dB) - tap coefficients sum to 1 - for continuous wave input
    };

    /**
     * @brief Construct FIR root raised cosine filter
     */
    FIRFilterRRC();

    /**
     * @brief Destructor
     */
    ~FIRFilterRRC() = default;

    /**
     * @brief Create root raised cosine filter taps
     *
     * @param symbolRate Symbol rate (normalized to sample rate, 0 to 0.5)
     * @param rolloff Roll-off factor (beta) - typically 0.2 to 0.5
     *                0 = rectangular, 1 = full cosine roll-off
     * @param symbolSpan Number of symbols over which filter is spread
     *                   Typical values: 6-12 (more = better filtering, more delay)
     * @param normalization How to normalize the filter taps
     *
     * Total number of taps = symbolSpan * samplesPerSymbol + 1 (always odd).
     * For symbolRate=0.05 (20 samples/symbol), symbolSpan=8 gives 161 taps.
     */
    void create(float symbolRate, float rolloff, int symbolSpan = 8,
                Normalization normalization = Normalization::Energy);

    /**
     * @brief Process one complex input sample through the filter
     *
     * Direct convolution with stored tap coefficients. Processes samples
     * one at a time with minimal latency.
     *
     * @param input Complex input sample
     * @return Filtered complex output sample
     */
    Complex filter(const Complex& input);

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
     * @brief Get number of taps in filter
     * @return Number of filter taps
     */
    int getNumTaps() const { return static_cast<int>(m_taps.size()); }

    /**
     * @brief Get samples per symbol
     * @return Samples per symbol (1/symbolRate)
     */
    int getSamplesPerSymbol() const { return m_samplesPerSymbol; }

    /**
     * @brief Get filter delay in samples
     * @return Group delay (approximately numTaps/2)
     */
    int getDelay() const { return getNumTaps() / 2; }

    /**
     * @brief Get reference to filter tap coefficients
     *
     * The taps are stored as half + center due to symmetry. The full impulse
     * response can be reconstructed by mirroring the taps around the center.
     *
     * @return Reference to vector of filter tap coefficients (half + center)
     */
    const std::vector<float>& getTaps() const { return m_taps; }

private:
    /**
     * @brief Compute time-domain RRC tap value
     *
     * Implements the mathematical RRC impulse response:
     * h(t) = [sin(π*t/T*(1-β)) + 4*β*t/T*cos(π*t/T*(1+β))] / [π*t/T*(1-(4*β*t/T)²)]
     *
     * @param t Time index (in symbol periods)
     * @param rolloff Roll-off factor (beta)
     * @return Tap value at time t
     */
    float computeRRCTap(float t, float rolloff) const;

    std::vector<float> m_taps;        //!< Filter tap coefficients (symmetric, stored as half + center)
    std::vector<Complex> m_samples;   //!< Circular buffer for input samples
    size_t m_ptr;                     //!< Current position in circular buffer
    float m_symbolRate;               //!< Symbol rate (normalized)
    float m_rolloff;                  //!< Rolloff factor (beta)
    int m_samplesPerSymbol;           //!< Samples per symbol (1/symbolRate)
};

#endif // INCLUDE_DSP_FIRFILTERRRC_H
