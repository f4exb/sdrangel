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

#ifndef INCLUDE_DSDSYNCSEARCHER_H
#define INCLUDE_DSDSYNCSEARCHER_H

#include <algorithm>
#include <cstdint>
#include <vector>

#include "dsp/dv/dvsync.h"

/**
 * Synchronisation pattern search by waveform regression.
 *
 * At every matched filter sample n the K symbol spaced values
 * r[k] = y[n - (K - 1 - k) sps] are fitted to the pattern levels t[k] by least
 * squares, r = a t + b. The correlation coefficient rho detects the pattern
 * whatever the deviation, DC offset or polarity; a is the level per template
 * unit (hertz, negative for an inverted signal), b the DC offset (hertz, the
 * residual frequency error) and the fit residual a first noise estimate. The
 * peak of |rho| over successive samples gives the symbol timing to a fraction
 * of a sample by parabolic interpolation.
 */
class DSDSyncSearcher
{
public:
    struct Detection
    {
        DVSync::Pattern m_pattern;
        int64_t m_sample;          //!< Index of the sample nearest the centre of the last sync symbol
        float m_fraction;       //!< Fractional offset of the symbol centre from m_sample, in samples (-0.5 .. 0.5)
        float m_rho;            //!< Correlation coefficient (signed: negative for an inverted signal)
        float m_scale;          //!< Hertz per template unit, |a|; the outer level is 3 |a|
        float m_offset;         //!< DC offset in hertz
        float m_sigma2;         //!< Residual variance (hertz squared)
        bool m_inverted;
    };

    DSDSyncSearcher();

    //! Patterns to search and their symbol period in samples. All patterns must share the period.
    //! The optional kernel is the symbol spaced response of the transmit and receive pulse shaping
    //! (odd length, centre tap in the middle, unit sum) with which the ideal levels are convolved
    //! to give the templates: an identity for Nyquist shaping, the intersymbol interference of the
    //! Gaussian pulses for GMSK. Symbols outside the pattern are taken as zero.
    void setPatterns(const std::vector<DVSync::Pattern>& patterns, int samplesPerSymbol, const std::vector<float>& kernel = std::vector<float>());
    //! Minimum |rho| to consider a candidate (0.85 for 24 symbol patterns is a good search threshold)
    void setThreshold(float rho) { m_threshold = rho; }
    /**
     * The threshold a pattern of this many symbols has to meet. A correlation is only as good as
     * the length it is taken over: 0.75 over the 24 symbols of a DMR sync is a thousand to one
     * against in noise, the same figure over M17's 8 is about one in thirty, and the demodulator
     * asks about four patterns at five offsets for every frame it expects - which is why an M17
     * lock in noise used to hold itself together for as long as it liked. A short pattern has to
     * correlate better than a long one to be believed.
     */
    static float thresholdFor(float base, int symbols) { return symbols <= ShortPattern ? std::max(base, ShortPatternRho) : base; }

    static const int ShortPattern = 12;                 //!< A pattern of this many symbols or fewer is short
    static constexpr float ShortPatternRho = 0.9f;      //!< and has to reach this |rho| (real M17 syncs come in at 0.99 median, 0.78 worst of 45 at C/N 6 dB)
    //! Forget the samples; the fits wait for the window to fill plus settleSamples more (the
    //! matched filter ahead of the searcher settling after a gap in its input)
    void reset(int settleSamples = 0);

    //! Push one matched filter sample. Returns true when a detection completed, with the peak
    //! sample and the fit at that sample.
    bool push(float y, int64_t sampleIndex, Detection& detection);

    //! Fit one pattern at the current position (the last pushed sample as the last sync symbol)
    bool fit(DVSync::Pattern pattern, Detection& detection) const;

private:
    struct PatternState
    {
        DVSync::Pattern m_pattern;
        int m_length;
        float m_sumT;
        float m_sumTT;
        std::vector<float> m_levels;
        float m_rhoHistory[3]; //!< rho at the last three evaluations
    };

    int m_sps;
    float m_threshold;
    float m_polarityBias;
    std::vector<PatternState> m_patterns;
    // The samples kept symbol spaced: one buffer per sample phase (sample index modulo sps), each
    // holding the last m_windowLength samples of that phase written twice, m_windowLength apart,
    // so that the window ending at any sample is contiguous
    int m_windowLength;
    std::vector<float> m_phaseBuffers;  //!< sps buffers of 2 * m_windowLength
    int m_stride;                       //!< Samples between evaluations of the fits (a fifth of a symbol)
    const float *m_window;              //!< The last m_windowLength symbol spaced samples up to the current one, oldest first
    int m_maxLength;                    //!< The longest pattern
    mutable std::vector<float> m_sumR, m_sumRR; //!< Sums of the last k window samples and their squares, by k
    int64_t m_sampleIndex;
    int64_t m_pushed;
    int64_t m_settle;                      //!< Samples to skip after a reset before fitting
    // Candidate peak
    bool m_hasCandidate;
    int m_candidatePattern;
    int64_t m_candidateSample;
    float m_candidateRho;
    Detection m_candidate;
    float m_candidateRhoBefore;

    void updateSums() const;
    bool fitAt(const PatternState& p, Detection& detection, float& rho) const;
};

#endif // INCLUDE_DSDSYNCSEARCHER_H
