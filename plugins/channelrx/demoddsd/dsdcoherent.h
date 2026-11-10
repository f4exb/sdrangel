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

#ifndef INCLUDE_DSDCOHERENT_H
#define INCLUDE_DSDCOHERENT_H

#include <complex>
#include <cstdint>
#include <vector>

/**
 * Coherent sequence detector for the root raised cosine 4FSK protocols
 * (experimental, DESIGN.md phase 6).
 *
 * Runs after the discriminator path has found the sync, the symbol instants
 * and the levels of a burst: it re-detects the burst's symbols from the
 * channel filtered IQ samples by a Viterbi search over the phase trajectory
 * of the continuous phase 4FSK signal. The frequency pulse is the transmit
 * root raised cosine (alpha 0.2, 8 symbols); the trellis keeps the symbol
 * and its two neighbours (16 states, 64 branches) and takes the pulse tails
 * beyond them from the discriminator path's decisions. The carrier phase is
 * unknown and drifts with the residual frequency error, so each survivor
 * carries its own phase reference, corrected by a fraction of the phase
 * error it measures on every symbol (per survivor processing). The branch
 * metric is the correlation of the samples of the symbol interval with the
 * predicted unit phasor, the maximum likelihood metric for a constant
 * envelope signal in white noise.
 *
 * The channel (carrier phase and amplitude, and any echo within two symbols)
 * is estimated by least squares over the burst from the first search's
 * decisions, which makes the detector a sync and data trained equaliser as
 * well: the burst is searched again with the distance to the prediction
 * through the channel as the metric, each survivor keeping the phasors of
 * its own recent past and tracking the channel's amplitude as well as its
 * phase. The samples may be channel filtered (setChannelFilter): the filter
 * is then part of the channel model, exactly, its precursors reached by
 * extending each branch's trajectory past the symbol interval.
 *
 * Soft outputs are forward path metric differences (no backward recursion),
 * scaled to log likelihood ratios with the noise power of the fit.
 */
class DSDCoherentDetector
{
public:
    struct Result
    {
        uint8_t m_dibit;
        float m_llrMsb;
        float m_llrLsb;
        float m_reliability;
    };

    //! alpha: the transmit pulse's root raised cosine roll off (M17's is 0.5)
    DSDCoherentDetector(int samplesPerSymbol = 10, float sampleRate = 48000.0f, double alpha = 0.2);

    //! Detect a burst. samples: channel filtered IQ; centres: symbol instants in samples of
    //! that array (fractional); firstPass: the discriminator path's levels (+3, +1, -1, -3) for
    //! every symbol, with the levels of the tail symbols before and after the burst in
    //! before/after (up to 4 each, nearest first; zero when unknown); levelHz: the frequency of
    //! one level unit (a third of the outer level); offsetHz: the DC (frequency) offset.
    //! Returns false when the burst could not be processed (too few samples).
    bool detect(const std::vector<std::complex<float>>& samples, const std::vector<double>& centres,
        const std::vector<int8_t>& firstPass, const int8_t *before, const int8_t *after,
        float levelHz, float offsetHz, std::vector<Result>& results);

    void setLoopGain(float gain) { m_loopGain = gain; }
    //! The samples given to detect() are channel filtered with this symmetric FIR (odd length):
    //! the filter becomes part of the channel model, exactly, the fit estimating the echo
    //! channel on the filtered reference. The burst needs the filter length more samples before
    //! its first symbol interval. Empty (the default): unfiltered samples.
    void setChannelFilter(const std::vector<float>& taps) { m_filter = taps; }
    int getFilterDelay() const { return (int) m_filter.size() / 2; }
    void setPasses(int passes) { m_passes = passes; }
    //! Relative residual reduction of the full channel fit over the main taps alone above which
    //! the channel is used with all its taps
    void setDispersionThreshold(float fraction) { m_dispersionThreshold = fraction; }
    //! Last burst: whether the channel estimate was used with its echo taps, its residual noise
    //! power and the channel energy (signal power)
    bool wasDispersive() const { return m_dispersive; }
    float getNoisePower() const { return m_lastNoise; }
    float getChannelEnergy() const { return m_lastEnergy; }
    //! Residual frequency error found on the last burst, on top of the offset given
    float getFrequencyHz() const { return m_lastFrequencyHz; }

private:
    int m_sps;
    float m_sampleRate;
    float m_loopGain;
    std::vector<float> m_filter;    //!< Channel filter the samples went through (empty: none)
    int m_passes;
    float m_dispersionThreshold;
    bool m_dispersive;
    float m_lastNoise;
    float m_lastEnergy;
    float m_lastFrequencyHz;
    std::vector<float> m_pulse;     //!< Transmit frequency pulse at 1/sps symbol resolution, symmetric
    int m_pulseCentre;

    float pulseAt(double tSamples) const;
    bool detectOnce(const std::vector<std::complex<float>>& samples, const std::vector<double>& centres,
        const std::vector<int8_t>& tails, const int8_t *before, const int8_t *after,
        float levelHz, float offsetHz, std::vector<Result>& results);
};

#endif // INCLUDE_DSDCOHERENT_H
