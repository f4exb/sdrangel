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

#ifndef INCLUDE_ATVGHOSTESTIMATOR_H
#define INCLUDE_ATVGHOSTESTIMATOR_H

#include <complex>
#include <vector>

/**
 * Estimates an echo (ghost) from multipath, for ATVSynchronousDetector to cancel.
 *
 * An echo is a delayed copy of the whole signal, so it puts a copy of each horizontal sync
 * pulse's edges on the sync tip and back porch, which are otherwise flat. The detector's
 * complex output, before cancellation and with the carrier's phase removed, is averaged
 * around the sync pulses of many lines (which averages out the noise and the picture, and
 * for NTSC and PAL, the colour burst). The echo is the delay and complex gain that, cancelled
 * from that average, leave the sync tip and back porch flattest. The gain is complex as the
 * echo's carrier arrives with a different phase, so part of it is in quadrature: after the
 * vestigial sideband filter that is a distorted copy of the picture's detail.
 *
 * No reference waveform is needed, so the transmitter's filtering, which shapes the pulse
 * edges, is not mistaken for an echo. Echoes shorter than 1 us (longer when the video
 * bandwidth is narrow, as the edges ring for longer) cannot be told apart from that
 * filtering, and only one echo, up to 4 us, is found. Other linear distortion, such as from
 * the vestigial sideband filter when mistuned, flattens the tip almost as well over a wide
 * range of delays, so only a delay clearly better than those either side is taken as an echo.
 */
class ATVGhostEstimator
{
public:
    ATVGhostEstimator();

    /** videoBandwidth limits how short an echo can be told apart from the edges' own ringing */
    void configure(float sampleRate, int samplesPerLine, float videoBandwidth);
    void reset(); //!< Forget the estimate

    /**
     * Feed the detector's output before ghost cancellation, with the sample's position in the
     * line (0 at the sync pulse's leading edge). Returns true when the estimate is updated
     */
    bool feed(int sampleOffset, std::complex<float> z);

    float getDelay() const { return m_delay; }                //!< samples
    std::complex<float> getGain() const { return m_gain; }   //!< 0 when there is no echo
    float getDelaySeconds() const { return m_delay / m_sampleRate; }

private:
    static constexpr int m_linesPerEstimate = 500;    //!< ~1 frame
    static constexpr double m_minImprovement = 0.9;   //!< the echo must make the sync region at least 10% flatter (in power)
    static constexpr double m_minSharpness = 0.4;     //!< and delays 0.5 us either side must leave at least 40% of that improvement
    static constexpr double m_keepSharpness = 0.2;    //!< or 20%, for an echo already being cancelled
    static constexpr int m_maxMisses = 4;             //!< estimates without the echo before it fades
    static constexpr float m_smoothing = 0.3f;        //!< of successive estimates
    static constexpr float m_maxGain = 0.7f;

    float m_sampleRate;
    int m_samplesPerLine;
    int m_pre;                          //!< samples kept before the sync's leading edge
    int m_post;                         //!< samples kept after it
    int m_tipStart, m_tipMid, m_tipEnd; //!< sync tip, after the edge (two halves, to recognise sync pulses)
    int m_backPorchStart, m_backPorchEnd;
    int m_frontPorchStart, m_frontPorchEnd; //!< before the edge (negative)
    double m_minDelay, m_maxDelay;      //!< samples
    double m_delayStep;                 //!< samples, of the search
    double m_sharpness;                 //!< samples either side of an echo's delay that must flatten the tip less
    std::vector<std::complex<float>> m_line; //!< the current line's samples, from -m_pre
    std::vector<std::complex<float>> m_next; //!< the end of the current line, which starts the next
    std::vector<std::complex<double>> m_sum;
    int m_lines;
    int m_lastOffset;
    int m_misses;                       //!< successive estimates without the echo
    float m_delay;
    std::complex<float> m_gain;

    bool endLine();
    void estimate();
    static std::complex<double> limitGain(std::complex<double> gain);
};

#endif // INCLUDE_ATVGHOSTESTIMATOR_H
