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

#ifndef INCLUDE_DSDTONECANCELLER_H
#define INCLUDE_DSDTONECANCELLER_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <vector>

#include "dsp/kissfft.h"

/**
 * A carrier canceller for the Angel61 receivers (ANGEL61.md 5, interference): a CW signal or a
 * birdie in the channel. On VHF it is noise of its own power; on HF it is worse: in one OFDM carrier
 * it is that carrier's cells wrong at full confidence and the pilots' estimate around them, and at
 * the signal's level it breaks the acquisition (it correlates with itself across the prefix).
 *
 * The spectrum over the signal's band is averaged (4096 points at 48 kS/s, a third of a second): a
 * random-data signal's is smooth, and a line more than DetectDB above its median is a carrier. Each
 * (MaxTones at most) is tracked - its complex amplitude averaged over 25 ms at its frequency, the
 * frequency refined from the amplitude's turn from block to block - and subtracted. The signal loses
 * the few hertz around it; a carrier that fades below the median's level is let go.
 */
class DSDToneCanceller
{
public:
    static const int FFTSize = 4096;
    static const int MaxTones = 3;
    static constexpr float DetectDB = 12.0f;     //!< a line this far above the band's median is a carrier
    static constexpr float DropDB = 3.0f;        //!< ... and one below this, gone
    static constexpr float Mu = 1.0f / 1200.0f;  //!< the amplitude's average: 25 ms at 48 kS/s
    static constexpr float SampleRate = 48000.0f;

    DSDToneCanceller() :
        m_fft(FFTSize, false),
        m_window(FFTSize),
        m_buffer(FFTSize),
        m_spectrum(FFTSize, 0.0f),
        m_out(FFTSize)
    {
        for (int i = 0; i < FFTSize; i++) {
            m_window[i] = 0.5f - 0.5f * std::cos(2.0f * (float) M_PI * i / FFTSize);
        }
    }

    //! The band searched: the signal's, +- halfWidthHz about the centre
    void setBand(float halfWidthHz) { m_halfWidthHz = halfWidthHz; }
    //! An Angel61 profile's signal band: NB 6.25 kHz, WB 12.5, HF-NB 2.5, HF-WB 5.5 (2.1)
    static float halfWidthHz(int profile) { return profile == 0 ? 3200.0f : profile == 1 ? 6300.0f : profile == 2 ? 1300.0f : 2800.0f; }
    //! On unless DSD_ANGEL61_CANCELLER=0 (studies)
    static bool enabledByDefault() { return !getenv("DSD_ANGEL61_CANCELLER") || (atoi(getenv("DSD_ANGEL61_CANCELLER")) != 0); }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    int getTones() const { return (int) m_tones.size(); }
    float getToneHz(int i) const { return m_tones[i].m_omega * SampleRate / (2.0f * (float) M_PI); }

    void reset()
    {
        m_tones.clear();
        std::fill(m_spectrum.begin(), m_spectrum.end(), 0.0f);
        m_blocks = 0;
        m_position = 0;
    }

    //! In place, at 48 kS/s
    void process(std::complex<float> *x, int n)
    {
        if (!m_enabled) {
            return;
        }

        for (int i = 0; i < n; i++)
        {
            for (Tone& t : m_tones)
            {
                const std::complex<float> osc(std::cos(t.m_phase), std::sin(t.m_phase));
                t.m_amplitude += Mu * (x[i] * std::conj(osc) - t.m_amplitude);
                x[i] -= t.m_amplitude * osc;
                t.m_phase += t.m_omega;

                if (t.m_phase > (float) M_PI) {
                    t.m_phase -= 2.0f * (float) M_PI;
                } else if (t.m_phase < -(float) M_PI) {
                    t.m_phase += 2.0f * (float) M_PI;
                }
            }

            // (the spectrum of what is left: a carrier cancelled no longer shows, a new one does)
            m_buffer[m_position++] = x[i];

            if (m_position == FFTSize)
            {
                m_position = 0;
                block();
            }
        }
    }

private:
    struct Tone
    {
        float m_omega = 0.0f;                   //!< radians a sample
        float m_phase = 0.0f;
        std::complex<float> m_amplitude = 0.0f;
        std::complex<float> m_lastAmplitude = 0.0f;
    };

    kissfft<float, std::complex<float>> m_fft;
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_buffer;
    std::vector<float> m_spectrum;
    std::vector<std::complex<float>> m_out;
    std::vector<Tone> m_tones;
    float m_halfWidthHz = 6250.0f;
    bool m_enabled = true;
    int m_position = 0;
    int m_blocks = 0;

    void block()
    {
        for (int i = 0; i < FFTSize; i++) {
            m_buffer[i] *= m_window[i];
        }

        m_fft.transform(m_buffer.data(), m_out.data());

        for (int i = 0; i < FFTSize; i++) {
            m_spectrum[i] = m_blocks == 0 ? std::norm(m_out[i]) : 0.75f * m_spectrum[i] + 0.25f * std::norm(m_out[i]);
        }

        m_blocks++;

        // The band's bins (the FFT's negative frequencies at its top) and their median
        const int half = std::min(FFTSize / 2 - 2, (int) (m_halfWidthHz * FFTSize / SampleRate));
        std::vector<int> bins;

        for (int k = -half; k <= half; k++) {
            bins.push_back(k);
        }

        auto at = [this](int k) { return m_spectrum[(k + FFTSize) % FFTSize]; };
        std::vector<float> powers;

        for (int k : bins) {
            powers.push_back(at(k));
        }

        std::nth_element(powers.begin(), powers.begin() + powers.size() / 2, powers.end());
        const float median = powers[powers.size() / 2];
        // A carrier of amplitude a in a Hann window's bin: |a N / 2|^2
        const float binGain = (FFTSize / 2.0f) * (FFTSize / 2.0f);

        // Each tone's frequency from its amplitude's turn since the last block, and the faded ones let go
        for (size_t i = 0; i < m_tones.size(); )
        {
            Tone& t = m_tones[i];

            if (std::abs(t.m_lastAmplitude) > 0.0f) {
                t.m_omega += 0.5f * std::arg(t.m_amplitude * std::conj(t.m_lastAmplitude)) / FFTSize;
            }

            t.m_lastAmplitude = t.m_amplitude;

            if ((m_blocks > 8) && (std::norm(t.m_amplitude) * binGain < median * std::pow(10.0f, DropDB / 10.0f))) {
                m_tones.erase(m_tones.begin() + i);
            } else {
                i++;
            }
        }

        if ((m_blocks < 4) || ((int) m_tones.size() >= MaxTones)) {
            return;
        }

        // The strongest line not already a tone's, when it stands far enough above the median
        int best = 0;
        float bestPower = 0.0f;

        for (int k = -half + 1; k < half; k++)
        {
            const float p = at(k);

            if ((p > bestPower) && (p >= at(k - 1)) && (p >= at(k + 1)))
            {
                bool taken = false;

                for (const Tone& t : m_tones) {
                    taken = taken || (std::fabs(t.m_omega * FFTSize / (2.0f * (float) M_PI) - k) < 3.0f);
                }

                if (!taken)
                {
                    best = k;
                    bestPower = p;
                }
            }
        }

        if (bestPower > median * std::pow(10.0f, DetectDB / 10.0f))
        {
            // Its frequency between the bins: the parabola through the log powers
            const float l = std::log(std::max(1e-30f, at(best - 1))), c = std::log(bestPower), r = std::log(std::max(1e-30f, at(best + 1)));
            const float denominator = l - 2.0f * c + r;
            const float delta = denominator != 0.0f ? std::max(-0.5f, std::min(0.5f, 0.5f * (l - r) / denominator)) : 0.0f;
            Tone t;
            t.m_omega = 2.0f * (float) M_PI * (best + delta) / FFTSize;
            m_tones.push_back(t);
        }
    }
};

#endif // INCLUDE_DSDTONECANCELLER_H
