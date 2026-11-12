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

#include <algorithm>
#include <cmath>

#include "dsdfrontend.h"

DSDFrontEnd::DSDFrontEnd() :
    m_delayIndex(0),
    m_correctionHz(0.0f),
    m_phase(0.0),
    m_phaseStep(0.0),
    m_previous(1.0f, 0.0f),
    m_dcBlock(true),
    m_dc(0.0f, 0.0f),
    m_powerFast(0.0f),
    m_powerSlow(0.0f),
    m_dropped(false),
    m_dropUsed(false),
    m_dropSamples(0),
    m_dcDrop(0.0f, 0.0f),
    m_dcSaved(0.0f, 0.0f),
    m_dropDetection(true)
{
    setBandwidth(6250.0f);
}

void DSDFrontEnd::setBandwidth(float cutoffHz)
{
    // Kaiser windowed sinc, 64 taps: about 60 dB of stop band attenuation with a transition of
    // 2 kHz, which keeps adjacent channel noise out without ringing on the symbol transitions
    const int n = 65;
    const double beta = 5.65;
    std::vector<float> taps(n);
    double fc = cutoffHz / m_sampleRate;
    double sum = 0.0;
    auto bessel0 = [](double x) {
        double s = 1.0, t = 1.0;
        for (int k = 1; k < 30; k++) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; if (t < 1e-12 * s) break; }
        return s;
    };
    double denominator = bessel0(beta);

    for (int i = 0; i < n; i++)
    {
        double k = i - (n - 1) / 2.0;
        double sinc = (k == 0.0) ? 2.0 * fc : std::sin(2.0 * M_PI * fc * k) / (M_PI * k);
        double r = 2.0 * k / (n - 1);
        double window = bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / denominator;
        taps[i] = (float) (sinc * window);
        sum += taps[i];
    }

    for (float& t : taps) {
        t /= (float) sum;
    }

    m_taps = taps;

    if (m_delay.size() != m_taps.size())
    {
        m_delay.assign(m_taps.size(), std::complex<float>(0.0f, 0.0f));
        m_delayIndex = 0;
    }
}

void DSDFrontEnd::setCorrection(float hz)
{
    m_correctionHz = hz;
    m_phaseStep = -2.0 * M_PI * hz / m_sampleRate;
}

void DSDFrontEnd::reset()
{
    std::fill(m_delay.begin(), m_delay.end(), std::complex<float>(0.0f, 0.0f));
    m_delayIndex = 0;
    m_phase = 0.0;
    m_previous = std::complex<float>(1.0f, 0.0f);
    m_dc = std::complex<float>(0.0f, 0.0f);
    m_powerFast = 0.0f;
    m_powerSlow = 0.0f;
    m_dropped = false;
    m_dropUsed = false;
    m_dropSamples = 0;
    m_dcDrop = std::complex<float>(0.0f, 0.0f);
    m_dcSaved = std::complex<float>(0.0f, 0.0f);
}

std::complex<float> DSDFrontEnd::removeDC(const std::complex<float>& in)
{
    std::complex<float> x = in;

    // The receiver's DC first, while it is still at 0 Hz: the correction (processDCFree) moves it
    if (m_dcBlock)
    {
        // The slow average also holds the mean of the signal itself (an FM carrier's is about
        // 28 dB below it), harmless while the signal is there, but left behind as a phantom
        // carrier when it goes off - a steady tone that the decoders flywheel on, and that takes
        // 0.3 s per 8.7 dB to decay under the noise (after a strong signal, half a second of
        // frames decoded from nothing). So when the input power falls 10 dB within about 1 ms,
        // the input since the fall is watched: once it is the receiver's own DC and noise - it
        // has fallen 20 dB (1 ms of it is then enough: symbols that far down are lost anyway), or
        // over 5 ms its mean holds half its power (a DC spike the carrier was less than 20 dB
        // above), or the fall has lasted 50 ms - the DC is taken from it. A fade looks the same
        // at first, but a faded signal keeps moving, so its mean is small: a fade that stays
        // above 20 dB down is left exactly as the slow estimate has it, and one that goes deeper
        // and ends within 50 ms gives the slow estimate back as it was, so that a fade leaves
        // nothing behind (within it, symbols 20 dB down lose the mean of 1 to 5 ms of themselves)
        float p = std::norm(x);
        m_powerFast += m_fastAlpha * (p - m_powerFast);
        m_powerSlow += m_slowAlpha * (p - m_powerSlow);

        if (m_dropDetection && (m_powerFast < m_dropRatio * m_powerSlow))
        {
            if (!m_dropped)
            {
                m_dropped = true;
                m_dropUsed = false;
                m_dropSamples = 0;
                m_dcDrop = std::complex<float>(0.0f, 0.0f);
            }

            m_dropSamples++;
            m_dcDrop += (x - m_dcDrop) / (float) std::min(m_dropSamples, m_dropMeanSamples);

            if (!m_dropUsed && (((m_powerFast < m_deepRatio * m_powerSlow) && (m_dropSamples >= m_deepMeanSamples))
                || ((m_dropSamples >= m_dropMeanSamples) && (std::norm(m_dcDrop) >= m_dcDominated * m_powerFast))
                || (m_dropSamples >= m_dropCommitSamples)))
            {
                m_dropUsed = true;
                m_dcSaved = m_dc;
            }

            if (m_dropUsed) {
                m_dc = m_dcDrop;
            } else {
                m_dc += m_dcAlpha * (x - m_dc);
            }
        }
        else
        {
            if (m_dropped && m_dropUsed && (m_dropSamples < m_dropCommitSamples)) {
                m_dc = m_dcSaved; // a deep fade: as if it had not happened
            }

            m_dropped = false;
            m_dc += m_dcAlpha * (x - m_dc);
        }

        x -= m_dc;
    }

    return x;
}

void DSDFrontEnd::processDCFree(const std::complex<float>& in, float& frequencyHz, float& power)
{
    std::complex<float> x = in;

    if (m_phaseStep != 0.0)
    {
        m_phase += m_phaseStep;

        if (m_phase > M_PI) {
            m_phase -= 2.0 * M_PI;
        } else if (m_phase < -M_PI) {
            m_phase += 2.0 * M_PI;
        }

        x *= std::complex<float>((float) std::cos(m_phase), (float) std::sin(m_phase));
    }

    m_rotated = x;
    m_delay[m_delayIndex] = x;
    int n = (int) m_taps.size();
    std::complex<float> acc(0.0f, 0.0f);
    int idx = m_delayIndex;

    for (int i = 0; i < n; i++)
    {
        acc += m_taps[i] * m_delay[idx];
        idx = (idx == 0) ? n - 1 : idx - 1;
    }

    m_delayIndex = (m_delayIndex + 1) % n;

    power = std::norm(acc);
    std::complex<float> d = acc * std::conj(m_previous);
    m_previous = acc;
    frequencyHz = (power > 1e-20f) ? std::atan2(d.imag(), d.real()) * (float) (m_sampleRate / (2.0 * M_PI)) : 0.0f;
}
