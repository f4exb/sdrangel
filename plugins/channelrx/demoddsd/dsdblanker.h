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

#ifndef INCLUDE_DSDBLANKER_H
#define INCLUDE_DSDBLANKER_H

#include <algorithm>
#include <complex>
#include <cstdint>

/**
 * An impulse blanker for the Angel61 receivers (ANGEL61.md 5, interference and impulsive noise):
 * a click - ignition, a switching supply, a lightning crash - is a few samples far above
 * everything else, and through the channel filter and the OFDM symbol's FFT it spreads its energy
 * over every cell of a symbol, which the decoder then takes at full confidence. Blanked (zeroed) at
 * the 48 kS/s input instead, it costs a few samples of signal.
 *
 * A sample whose power is more than the threshold above the running mean power (50 ms) is
 * blanked, with a few after it. A run of them longer than MaxRun is no click - a transmission
 * starting after silence, a fade ending - and the mean snaps to it instead: a key up loses at most
 * MaxRun samples (a quarter of a millisecond). The mean starts as the first WarmUp samples' average,
 * nothing blanked meanwhile: started from the first sample's power alone, one 20 dB down blanked
 * most samples for hundreds of milliseconds (a sample under the threshold is the only thing that
 * moves the mean), and NB missed a first frame in 2000 at Es/N0 6 dB (VHF results section 13).
 */
class DSDBlanker
{
public:
    static constexpr float DefaultThresholdDB = 15.0f;   //!< 18 let a +20 dB click's tail through (HF results section 40)
    static const int MaxRun = 12;           //!< samples above the threshold in a row that are still a click
    static const int Hold = 4;              //!< samples blanked after the last one above it
    static constexpr float Alpha = 1.0f / 2400.0f;   //!< the mean's time constant: 50 ms at 48 kS/s
    static const int WarmUp = 480;          //!< samples averaged for the first mean (10 ms), none blanked

    DSDBlanker() { setThresholdDB(DefaultThresholdDB); }

    void setThresholdDB(float dB) { m_threshold = std::pow(10.0f, dB / 10.0f); }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    void reset()
    {
        m_mean = 0.0f;
        m_warm = 0;
        m_run = 0;
        m_hold = 0;
    }
    //! Samples blanked so far
    int64_t getBlanked() const { return m_blanked; }

    //! In place, at 48 kS/s
    void process(std::complex<float> *x, int n)
    {
        if (!m_enabled) {
            return;
        }

        for (int i = 0; i < n; i++)
        {
            const float p = std::norm(x[i]);

            if (m_warm < WarmUp)
            {
                m_mean += (p - m_mean) / (float) ++m_warm;
                continue;
            }

            if (m_mean <= 0.0f)
            {
                m_mean = p;     // (digital silence so far)
                continue;
            }

            if (p > m_threshold * m_mean)
            {
                if (++m_run > MaxRun)
                {
                    // Not a click: the level itself has changed
                    m_mean = p;
                    m_run = 0;
                    m_hold = 0;
                    continue;
                }

                m_hold = Hold;
                x[i] = 0.0f;
                m_blanked++;
                continue;
            }

            m_run = 0;

            if (m_hold > 0)
            {
                m_hold--;
                x[i] = 0.0f;
                m_blanked++;
                continue;
            }

            m_mean += Alpha * (p - m_mean);
        }
    }

private:
    bool m_enabled = true;
    float m_threshold = 31.6f;
    float m_mean = 0.0f;
    int m_warm = 0;                         //!< samples in the first mean so far
    int m_run = 0;
    int m_hold = 0;
    int64_t m_blanked = 0;
};

#endif // INCLUDE_DSDBLANKER_H
