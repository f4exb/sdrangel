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

#include "cwcanceller.h"

namespace dvbt {

const double CWCanceller::m_twoPi = 6.283185307179586;
const double CWCanceller::m_thresholdDb = 12.0;
const double CWCanceller::m_poleRadius = 0.999;

CWCanceller::CWCanceller() :
    m_enabled(true),
    m_sampleRate(1.0),
    m_bandwidth(1.0)
{
    configure(9142857.0, 7.61e6);
}

void CWCanceller::configure(double sampleRate, double bandwidth)
{
    const int n = m_fftSize;
    m_sampleRate = sampleRate;
    m_bandwidth = bandwidth;
    // Two blocks back to back (their phase difference refines the frequency) every 20 ms
    m_interval = std::max(4 * n, (int) (sampleRate * 0.02));
    m_window.resize(n);

    for (int i = 0; i < n; i++) {
        m_window[i] = 0.5 - 0.5 * std::cos(m_twoPi * (i + 0.5) / n);
    }

    m_twiddles.resize(n / 2);

    for (int i = 0; i < n / 2; i++) {
        m_twiddles[i] = std::polar(1.0, -m_twoPi * i / n);
    }

    m_bitReverse.resize(n);
    int bits = 0;

    while ((1 << bits) < n) {
        bits++;
    }

    for (int i = 0; i < n; i++)
    {
        int r = 0;

        for (int b = 0; b < bits; b++) {
            r |= ((i >> b) & 1) << (bits - 1 - b);
        }

        m_bitReverse[i] = r;
    }

    // The bins within the band
    m_band.clear();

    for (int k = 0; k < n; k++)
    {
        const int signedK = k < n / 2 ? k : k - n;

        if (std::abs(signedK * m_sampleRate / n) < m_bandwidth / 2.0) {
            m_band.push_back(k);
        }
    }

    reset();
}

void CWCanceller::reset()
{
    m_untilNext = 0;
    m_block.assign(2 * m_fftSize, Cf(0.0f, 0.0f));
    m_blockFill = 0;
    m_power.assign(m_fftSize, 0.0);
    m_blockCount = 0;
    m_candidates.clear();
    m_notches.clear();
}

void CWCanceller::setEnabled(bool enabled)
{
    m_enabled = enabled;

    if (!enabled) {
        reset();
    }
}

const CWCanceller::Cf *CWCanceller::process(const Cf *samples, int nbSamples, std::vector<Cf>& out)
{
    if (!m_enabled) {
        return samples;
    }

    // Measure the input (not what's been notched, so a notch stays while its interferer does)
    const int pair = 2 * m_fftSize;
    int i = 0;

    while (i < nbSamples)
    {
        if ((m_blockFill > 0) || (m_untilNext <= 0))
        {
            const int n = std::min(pair - m_blockFill, nbSamples - i);
            std::copy(samples + i, samples + i + n, m_block.begin() + m_blockFill);
            m_blockFill += n;
            i += n;

            if (m_blockFill == pair)
            {
                measure(m_block.data());
                m_blockFill = 0;
                m_untilNext = m_interval - pair;
            }
        }
        else
        {
            const int skip = std::min(m_untilNext, nbSamples - i);
            m_untilNext -= skip;
            i += skip;
        }
    }

    if (m_notches.empty()) {
        return samples;
    }

    // Each notch: w = x + p w', y = w - z w'
    out.assign(samples, samples + nbSamples);
    float *y = reinterpret_cast<float *>(out.data());

    for (Notch& notch : m_notches)
    {
        const float zr = notch.m_zr, zi = notch.m_zi, pr = notch.m_pr, pi = notch.m_pi;
        float wr = notch.m_wr, wi = notch.m_wi;

        for (int k = 0; k < nbSamples; k++)
        {
            const float xr = y[2 * k], xi = y[2 * k + 1];
            const float nr = xr + pr * wr - pi * wi;
            const float ni = xi + pr * wi + pi * wr;
            y[2 * k] = nr - (zr * wr - zi * wi);
            y[2 * k + 1] = ni - (zr * wi + zi * wr);
            wr = nr;
            wi = ni;
        }

        notch.m_wr = wr;
        notch.m_wi = wi;
    }

    return out.data();
}

// The power spectrum of both blocks, averaged until there are enough, and the second block's spectrum relative to
// the first's, for the frequencies of the peaks
void CWCanceller::measure(const Cf *block)
{
    const int n = m_fftSize;
    std::vector<std::complex<double>> *spectra = m_spectra;

    for (int b = 0; b < 2; b++)
    {
        std::vector<std::complex<double>>& fft = spectra[b];
        fft.resize(n);

        for (int i = 0; i < n; i++) {
            fft[m_bitReverse[i]] = std::complex<double>(block[b * n + i]) * m_window[i];
        }

        for (int size = 2; size <= n; size *= 2)
        {
            const int half = size / 2;
            const int stride = n / size;

            for (int start = 0; start < n; start += size)
            {
                for (int j = 0; j < half; j++)
                {
                    const std::complex<double> t = fft[start + j + half] * m_twiddles[j * stride];
                    fft[start + j + half] = fft[start + j] - t;
                    fft[start + j] += t;
                }
            }
        }

        for (int k = 0; k < n; k++) {
            m_power[k] += std::norm(fft[k]);
        }
    }

    m_blockCount += 2;

    // Each pair until there's a notch, so one is soon in place (within about 40 ms), before the receiver locks, as
    // starting one afterwards changes the channel the receiver is following
    if ((m_blockCount < m_blocks) && !m_notches.empty()) {
        return;
    }

    // Peaks well above the signal's median level in the band, strongest first, each refined: roughly by the shape of
    // the window's main lobe, then from the phase advance of a block (a block's length at the frequency)
    const std::vector<int>& band = m_band;
    std::vector<double>& levels = m_levels;
    levels.clear();

    for (int k : band) {
        levels.push_back(m_power[k]);
    }

    std::nth_element(levels.begin(), levels.begin() + levels.size() / 2, levels.end());
    const double median = levels.empty() ? 0.0 : levels[levels.size() / 2];
    const double threshold = median * std::pow(10.0, m_thresholdDb / 10.0);
    std::vector<std::pair<double, int>> peaks; // Power, bin

    for (int k : band)
    {
        const double p = m_power[k];

        if ((median > 0.0) && (p > threshold) && (p >= m_power[(k + n - 1) % n]) && (p >= m_power[(k + 1) % n])) {
            peaks.push_back({p, k});
        }
    }

    std::sort(peaks.begin(), peaks.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b) { return a.first > b.first; });
    std::vector<std::pair<double, double>> detected; // Frequency (cycles per sample), level (dB)

    for (const auto& peak : peaks)
    {
        const int k = peak.second;
        const int signedK = k < n / 2 ? k : k - n;
        bool near = false;

        for (const auto& d : detected)
        {
            if (std::abs(d.first * n - signedK) < 2 * m_matchBins) {
                near = true; // A sidelobe of a stronger one
            }
        }

        if (near || ((int) detected.size() >= m_maxNotches)) {
            continue;
        }

        const double lm = std::log(m_power[(k + n - 1) % n]), l0 = std::log(m_power[k]), lp = std::log(m_power[(k + 1) % n]);
        const double denominator = lm - 2.0 * l0 + lp;
        const double delta = denominator != 0.0 ? 0.5 * (lm - lp) / denominator : 0.0;
        const double coarse = signedK + std::max(-0.5, std::min(0.5, delta)); // Bins
        // The second block is n samples on, so the tone has advanced by 2 pi f n: whole cycles from the coarse
        // estimate, and the fraction from the phase
        const double advance = std::arg(spectra[1][k] * std::conj(spectra[0][k])) / m_twoPi;
        const double fine = advance + std::round(coarse - advance);
        detected.push_back({fine / n, 10.0 * std::log10(peak.first / median)});
    }

    // Follow the notches, start new ones for interferers seen twice in a row, and stop those that have gone
    std::vector<bool> used(detected.size(), false);

    for (Notch& notch : m_notches)
    {
        notch.m_missed++;

        for (size_t d = 0; d < detected.size(); d++)
        {
            if (!used[d] && (std::abs(detected[d].first - notch.m_frequency) * n < m_matchBins))
            {
                used[d] = true;
                notch.m_missed = 0;
                notch.m_level = detected[d].second;
                setFrequency(notch, detected[d].first);
            }
        }
    }

    m_notches.erase(std::remove_if(m_notches.begin(), m_notches.end(), [](const Notch& notch) {
        return notch.m_missed >= m_missesToRemove;
    }), m_notches.end());
    std::vector<double> candidates;

    for (size_t d = 0; d < detected.size(); d++)
    {
        if (used[d]) {
            continue;
        }

        const bool seen = std::any_of(m_candidates.begin(), m_candidates.end(), [&](double f) {
            return std::abs(f - detected[d].first) * n < m_matchBins;
        });

        if (seen && ((int) m_notches.size() < m_maxNotches))
        {
            Notch notch{};
            notch.m_level = detected[d].second;
            setFrequency(notch, detected[d].first);
            m_notches.push_back(notch);
        }
        else
        {
            candidates.push_back(detected[d].first);
        }
    }

    m_candidates = candidates;
    std::fill(m_power.begin(), m_power.end(), 0.0);
    m_blockCount = 0;
}

void NotchedCarriers::setNotches(const std::vector<double>& frequencies, double inputRate)
{
    m_notches = frequencies;
    m_inputRate = inputRate;

    if (m_notches.empty())
    {
        m_carriers.clear();
        m_factors.clear();
        m_erased.clear();
    }
}

void NotchedCarriers::prepare(int kMax, double spacing, double offset)
{
    for (int k : m_carriers)
    {
        if (k < (int) m_erased.size()) {
            m_erased[k] = 0;
        }
    }

    m_carriers.clear();
    m_factors.clear();
    m_erased.resize(kMax + 1, 0);
    const int width = (int) std::ceil(50e3 / spacing);
    std::vector<std::complex<double>> response;

    for (int k = 0; k <= kMax; k++)
    {
        const double f = (k - kMax / 2) * spacing + offset;
        std::complex<double> h(1.0, 0.0);
        bool near = false;

        for (double notch : m_notches)
        {
            if (std::abs(f - notch) <= width * spacing)
            {
                // H = (1 - z e^-jw) / (1 - r z e^-jw), with z the zero, at the carrier's frequency w
                const std::complex<double> e = std::polar(1.0, CWCanceller::m_twoPi * (notch - f) / m_inputRate);
                h *= (1.0 - e) / (1.0 - CWCanceller::m_poleRadius * e);
                near = true;
            }
        }

        if (near)
        {
            const double h2 = std::norm(h);
            m_carriers.push_back(k);
            m_factors.push_back(Cf(std::conj(h) / std::max(h2, 0.09)));
            m_erased[k] = h2 < 0.25;
        }
    }
}

void NotchedCarriers::compensate(Cf *y) const
{
    for (size_t i = 0; i < m_carriers.size(); i++) {
        y[m_carriers[i]] *= m_factors[i];
    }
}

void CWCanceller::setFrequency(Notch& notch, double frequency)
{
    notch.m_frequency = frequency;
    notch.m_zr = (float) std::cos(m_twoPi * frequency);
    notch.m_zi = (float) std::sin(m_twoPi * frequency);
    notch.m_pr = (float) (m_poleRadius * notch.m_zr);
    notch.m_pi = (float) (m_poleRadius * notch.m_zi);
}

} // namespace dvbt
