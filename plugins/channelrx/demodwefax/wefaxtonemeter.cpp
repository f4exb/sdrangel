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

#include "wefaxtonemeter.h"

#include <algorithm>
#include <cmath>

void WefaxToneMeter::configure(int sampleRate, double shiftHz)
{
    // Decimate to about 2 kHz: the tones lie within +/-(shift/2 + 25%).
    m_decimation = std::max(1, sampleRate / 2000);
    m_decimatedRate = static_cast<double>(sampleRate) / m_decimation;
    m_shiftHz = shiftHz;
    // A steep filter, so that adjacent signals cannot alias onto the tones.
    m_lowpass.create(2 * (sampleRate / 200) + 1, sampleRate, 0.375 * m_decimatedRate);

    m_window.resize(FFTSize);
    for (int i = 0; i < FFTSize; ++i) {
        m_window[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / FFTSize);
    }
    // Average over about 30 s of blocks.
    m_decay = std::exp(-(FFTSize / m_decimatedRate) / 30.0);
    reset();
}

void WefaxToneMeter::reset()
{
    m_phase = 0;
    m_block.clear();
    m_block.reserve(FFTSize);
    m_spectrum.assign(FFTSize, 0.0);
    m_result = Result();
}

bool WefaxToneMeter::feed(const Complex& sample)
{
    const Complex filtered = m_lowpass.filter(sample);
    if (++m_phase < m_decimation) {
        return false;
    }
    m_phase = 0;

    m_block.emplace_back(filtered.real(), filtered.imag());
    if (static_cast<int>(m_block.size()) < FFTSize) {
        return false;
    }

    for (int i = 0; i < FFTSize; ++i) {
        m_block[i] *= m_window[i];
    }
    fft(m_block);
    for (int i = 0; i < FFTSize; ++i) {
        m_spectrum[i] = m_decay * m_spectrum[i] + std::norm(m_block[i]);
    }
    m_block.clear();

    evaluate();
    return true;
}

void WefaxToneMeter::evaluate()
{
    const double binHz = m_decimatedRate / FFTSize;
    const auto frequency = [&](int bin) { return (bin < FFTSize / 2 ? bin : bin - FFTSize) * binHz; };
    const auto binOf = [&](double hz) {
        return (static_cast<int>(std::lround(hz / binHz)) + FFTSize) % FFTSize;
    };

    std::vector<double> sorted(m_spectrum);
    std::nth_element(sorted.begin(), sorted.begin() + FFTSize / 2, sorted.end());
    const double median = sorted[FFTSize / 2];
    if (median <= 0.0) {
        return;
    }

    // Local maxima at least 10 dB above the median, within 25% of the shift
    // of each nominal tone.
    struct Peak { int bin; double power; };
    const auto peaks = [&](double centreHz) {
        std::vector<Peak> found;
        const double halfWidth = 0.25 * m_shiftHz;
        for (double hz = centreHz - halfWidth; hz <= centreHz + halfWidth; hz += binHz)
        {
            const int bin = binOf(hz);
            const double power = m_spectrum[bin];
            if ((power > 10.0 * median)
                && (power > m_spectrum[(bin + FFTSize - 1) % FFTSize])
                && (power >= m_spectrum[(bin + 1) % FFTSize])) {
                found.push_back({bin, power});
            }
        }
        return found;
    };
    const std::vector<Peak> lows = peaks(-0.5 * m_shiftHz);
    const std::vector<Peak> highs = peaks(0.5 * m_shiftHz);

    // Strongest pair one shift apart, within 8%: tight enough to reject the
    // start tone's comb lines (pairs 600 or 700 Hz apart for an 800 Hz
    // shift), loose enough for an 850 Hz station received with 800 Hz set.
    double bestScore = 0.0;
    Result best;
    for (const Peak& low : lows)
    {
        for (const Peak& high : highs)
        {
            const double separation = frequency(high.bin) - frequency(low.bin);
            if ((separation < 0.92 * m_shiftHz) || (separation > 1.08 * m_shiftHz)) {
                continue;
            }
            const double score = std::log(low.power) + std::log(high.power);
            if (!best.valid || (score > bestScore))
            {
                bestScore = score;
                best.valid = true;
                best.lowHz = frequency(low.bin);
                best.highHz = frequency(high.bin);
            }
        }
    }

    if (!best.valid) {
        return;
    }

    // Parabolic interpolation on log power refines each peak within a bin.
    const auto refine = [&](double hz) {
        const int bin = binOf(hz);
        const double left = std::log(m_spectrum[(bin + FFTSize - 1) % FFTSize]);
        const double centre = std::log(m_spectrum[bin]);
        const double right = std::log(m_spectrum[(bin + 1) % FFTSize]);
        const double curvature = left - 2.0 * centre + right;
        return hz + (curvature < 0.0 ? 0.5 * (left - right) / curvature : 0.0) * binHz;
    };
    best.lowHz = refine(best.lowHz);
    best.highHz = refine(best.highHz);
    m_result = best;
}

void WefaxToneMeter::fft(std::vector<std::complex<double>>& data)
{
    const std::size_t n = data.size();

    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const std::complex<double> step = std::polar(1.0, -2.0 * M_PI / length);
        for (std::size_t start = 0; start < n; start += length)
        {
            std::complex<double> twiddle(1.0, 0.0);
            for (std::size_t k = 0; k < length / 2; ++k)
            {
                const std::complex<double> even = data[start + k];
                const std::complex<double> odd = data[start + k + length / 2] * twiddle;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                twiddle *= step;
            }
        }
    }
}
