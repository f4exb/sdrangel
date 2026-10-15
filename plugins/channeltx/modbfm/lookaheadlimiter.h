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

#ifndef PLUGINS_CHANNELTX_MODBFM_LOOKAHEADLIMITER_H_
#define PLUGINS_CHANNELTX_MODBFM_LOOKAHEADLIMITER_H_

#include <vector>

#include "dsp/dsptypes.h"

/** Stereo-linked look-ahead peak limiter. The left channel is the real part
 * and the right channel the imaginary part. Neither output channel exceeds 1.
 *
 * The output is delayed by the look-ahead time. The gain is reduced over a
 * linear ramp lasting the look-ahead time before each peak, then recovers
 * exponentially. Gain changes are therefore smooth, so the limiter adds little
 * energy outside the bandwidth of the input. Input that never exceeds 1 is
 * passed through unchanged, apart from the delay.
 */
class LookaheadLimiter
{
public:
    LookaheadLimiter();

    void configure(int lookaheadSamples, int releaseSamples);
    void reset();
    Complex process(const Complex& input);
    float getGain() const { return m_gain; }

private:
    int m_lookahead;          //!< Look-ahead in samples
    float m_release;          //!< Release coefficient per sample
    std::vector<Complex> m_delay;
    int m_delayIdx;

    // Minimum required gain over the last m_lookahead + 1 samples, as a
    // monotonic queue of (sample count, gain) in a ring buffer
    std::vector<qint64> m_minCount;
    std::vector<float> m_minGain;
    int m_minHead;
    int m_minSize;

    // Moving average of those minimums over m_lookahead samples
    std::vector<float> m_box;
    double m_boxSum;
    int m_boxIdx;

    qint64 m_count;
    float m_gain;
};

#endif // PLUGINS_CHANNELTX_MODBFM_LOOKAHEADLIMITER_H_
