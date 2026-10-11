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

#include "dvbssymbolrate.h"

namespace dvbs {

// Rates in common use (broadcast, and amateur DATV), which a measurement this close to is taken as
const double SymbolRateSearch::m_commonRates[20] = {
    25000, 33000, 35000, 66000, 125000, 250000, 333000, 500000, 1000000, 1500000, 2000000, 4000000,
    22000000, 22500000, 23000000, 27500000, 28000000, 29500000, 29900000, 30000000
};

void SymbolRateSearch::configure(double inputRate, double minRate, double maxRate)
{
    m_inputRate = inputRate;
    m_minRate = minRate;
    m_maxRate = maxRate;
    reset();
}

void SymbolRateSearch::setBand(double frequency, double width)
{
    m_bandFrequency = frequency;
    m_bandWidth = std::max(0.0, width);
    // What's left of a holdoff kept (as reset(seconds)'s after a retune, which the band's set just after)
    const int64_t holdoff = m_holdoff;
    reset();
    m_holdoff = holdoff;
}

void SymbolRateSearch::reset(double holdoffSeconds)
{
    m_buffer.clear();
    m_buffer.reserve(searchSamples);
    m_result = Result();
    m_holdoff = (int64_t) (std::max(0.0, holdoffSeconds) * m_inputRate);
}

bool SymbolRateSearch::process(const Cf *samples, int nbSamples)
{
    bool ended = false;

    while (nbSamples > 0)
    {
        if (m_holdoff > 0)
        {
            const int skip = (int) std::min((int64_t) nbSamples, m_holdoff);
            m_holdoff -= skip;
            samples += skip;
            nbSamples -= skip;
            continue;
        }

        const int n = std::min(nbSamples, searchSamples - (int) m_buffer.size());
        m_buffer.insert(m_buffer.end(), samples, samples + n);
        samples += n;
        nbSamples -= n;

        if ((int) m_buffer.size() == searchSamples)
        {
            // Not with zeros, as a device gives while it retunes (more than a run of them can be by chance): measured,
            // they gave lines at 0 Hz hundreds of dB up, and the spectrum a dip that made its neighbours look like signals
            if (zeroRunSamples() > searchSamples / 100)
            {
                m_buffer.clear();
                continue;
            }

            search();
            m_buffer.clear();
            m_holdoff = m_result.m_found ? 0 : (int64_t) (m_retrySeconds * m_inputRate);
            ended = true;
        }
    }

    return ended;
}

// Samples in runs of zeros (exactly) 64 or longer
int SymbolRateSearch::zeroRunSamples() const
{
    int total = 0, run = 0;

    for (const Cf& s : m_buffer)
    {
        if ((s.real() == 0.0f) && (s.imag() == 0.0f))
        {
            run++;
        }
        else
        {
            total += run >= 64 ? run : 0;
            run = 0;
        }
    }

    return total + (run >= 64 ? run : 0);
}

std::vector<SymbolRateSearch::Signal> SymbolRateSearch::findSignals(const std::vector<double>& smooth, double floor,
    double binHz, double origin, double minWidth, double over)
{
    std::vector<Signal> signals;
    const int n = (int) smooth.size();

    // Every signal: each run over the threshold, its in-band level the median of its middle half, its half power
    // points where it's half way from that to the floor (interpolated). Neighbours with no gap between run together
    for (int i = 0; i < n; )
    {
        if (smooth[i] <= over * floor)
        {
            i++;
            continue;
        }

        const int a = i;

        while ((i < n) && (smooth[i] > over * floor)) {
            i++;
        }

        const int b = i - 1;
        const int inner = (b - a) / 4;
        std::vector<double> mid(smooth.begin() + a + inner, smooth.begin() + b - inner + 1);
        std::sort(mid.begin(), mid.end());
        const double runLevel = mid[mid.size() / 2];
        const double half = floor + 0.5 * (runLevel - floor);
        // From the run's ends (3 dB up, which may be below half way), in or out to the first bins over half way
        int lo = a, hi = b;

        while ((lo < b) && (smooth[lo] <= half)) {
            lo++;
        }

        while ((lo > 0) && (smooth[lo - 1] > half)) {
            lo--;
        }

        while ((hi > lo) && (smooth[hi] <= half)) {
            hi--;
        }

        while ((hi < n - 1) && (smooth[hi + 1] > half)) {
            hi++;
        }

        const double low = lo > 0 ? lo - (smooth[lo] - half) / std::max(1e-30, smooth[lo] - smooth[lo - 1]) : lo;
        const double high = hi < n - 1 ? hi + (smooth[hi] - half) / std::max(1e-30, smooth[hi] - smooth[hi + 1]) : hi;
        Signal signal;
        signal.m_frequency = origin + 0.5 * (low + high) * binHz;
        signal.m_width = (high - low) * binHz;
        signal.m_levelDb = 10.0 * std::log10(std::max(1e-30, runLevel - floor) / floor);

        if (signal.m_width >= minWidth) {
            signals.push_back(signal);
        }
    }

    return signals;
}

// A Hann window of n points, in window (made again only if its size changes)
const std::vector<double>& SymbolRateSearch::hann(std::vector<double>& window, int n)
{
    if ((int) window.size() != n)
    {
        window.resize(n);

        for (int i = 0; i < n; i++) {
            window[i] = 0.5 - 0.5 * std::cos(2.0 * m_pi * i / n);
        }
    }

    return window;
}

void SymbolRateSearch::search()
{
    m_result = Result();

    if (m_inputRate <= 0.0) {
        return;
    }

    // The averaged spectrum, centred
    const int n = m_spectrumSize;
    const double binHz = m_inputRate / n;
    std::vector<double> power(n, 0.0);
    const std::vector<double>& window = hann(m_spectrumWindow, n);
    std::vector<std::complex<double>> x(n);

    for (int b = 0; b + n <= searchSamples; b += n / 2)
    {
        for (int i = 0; i < n; i++) {
            x[i] = std::complex<double>(m_buffer[b + i].real(), m_buffer[b + i].imag()) * window[i];
        }

        m_fft.transform(x);

        for (int i = 0; i < n; i++) {
            power[(i + n / 2) % n] += std::norm(x[i]);
        }
    }

    // Smoothed with a median over half the lowest rate looked for (a quarter either side): that removes anything
    // narrower than a quarter of it (spurs, as an SDR's LO leaking, offset from its centre, which can peak above the
    // signal, and would be taken for it), while a signal's flat top and edges (monotonic) are kept
    const int halfWindow = std::max(3, (int) std::lround(0.25 * m_minRate / binHz));
    std::vector<double> smooth(n), window2;

    for (int i = 0; i < n; i++)
    {
        const int from = std::max(0, i - halfWindow), to = std::min(n - 1, i + halfWindow);
        window2.assign(power.begin() + from, power.begin() + to + 1);
        std::nth_element(window2.begin(), window2.begin() + window2.size() / 2, window2.end());
        smooth[i] = window2[window2.size() / 2];
    }

    std::vector<double> sorted(smooth);
    std::sort(sorted.begin(), sorted.end());
    const double floor = sorted[n / 5];

    m_result.m_minWidth = 0.5 * m_minRate;
    m_result.m_signals = findSignals(smooth, floor, binHz, -(n / 2) * binHz, m_result.m_minWidth);
    m_result.m_binHz = binHz;
    m_result.m_spectrum.resize(n);

    for (int i = 0; i < n; i++) {
        m_result.m_spectrum[i] = (float) smooth[i];
    }

    if (m_spectrumOnly) {
        return;
    }

    // In a band, only its bins
    int first = 0, last = n - 1;

    if (m_bandWidth > 0.0)
    {
        first = std::max(0, (int) std::floor((m_bandFrequency - m_bandWidth / 2.0) / binHz) + n / 2);
        last = std::min(n - 1, (int) std::ceil((m_bandFrequency + m_bandWidth / 2.0) / binHz) + n / 2);

        if (last <= first) {
            return;
        }
    }

    const int peak = (int) (std::max_element(smooth.begin() + first, smooth.begin() + last + 1) - smooth.begin());

    // The strongest signal's half power points: from its peak, then from its in-band level (the median of the middle
    // of the band, as the peak's raised by noise), outwards to where it's half way to the floor, interpolated
    double level = smooth[peak];
    double lowEdge = peak, highEdge = peak;

    for (int pass = 0; pass < 3; pass++)
    {
        const double half = floor + 0.5 * (level - floor);
        int a = peak, b = peak;

        while ((a > first) && (smooth[a - 1] > half)) {
            a--;
        }

        while ((b < last) && (smooth[b + 1] > half)) {
            b++;
        }

        lowEdge = (a > first) ? a - (smooth[a] - half) / std::max(1e-30, smooth[a] - smooth[a - 1]) : a;
        highEdge = (b < last) ? b + (smooth[b] - half) / std::max(1e-30, smooth[b] - smooth[b + 1]) : b;
        const int inner = std::max(1, (b - a) / 4);
        const int from = std::min(a + inner, b), to = std::max(b - inner, from);
        std::vector<double> band(smooth.begin() + from, smooth.begin() + to + 1);
        std::sort(band.begin(), band.end());
        level = band[band.size() / 2];
    }

    if (level < m_minLevel * floor) {
        return;
    }

    m_result.m_width = (highEdge - lowEdge) * binHz;
    m_result.m_frequency = (0.5 * (lowEdge + highEdge) - n / 2) * binHz;

    // The squared magnitude's spectrum, over the whole buffer (its resolution sets the precision), near the width. Its
    // line can be near half the sample rate (as at 2 samples a symbol), so then the samples are interpolated to twice
    // the rate first (zero padding their spectrum)
    const bool upsample = m_searchHigh * m_result.m_width > 0.4 * m_inputRate;
    const int m = upsample ? 2 * searchSamples : searchSamples;
    std::vector<std::complex<double>> z(searchSamples);

    for (int i = 0; i < searchSamples; i++) {
        z[i] = std::complex<double>(m_buffer[i].real(), m_buffer[i].imag());
    }

    // In a band, the input filtered to it first (the others' lines, and their mixing with it, gone)
    if (m_bandWidth > 0.0)
    {
        m_fft.transform(z);
        const double fullHz = m_inputRate / searchSamples;

        for (int k = 0; k < searchSamples; k++)
        {
            const double f = (k < searchSamples / 2 ? k : k - searchSamples) * fullHz;

            if (std::fabs(f - m_bandFrequency) > m_bandWidth / 2.0) {
                z[k] = 0.0;
            }
        }

        if (!upsample) {
            m_fft.transform(z, true);
        }
    }

    if (upsample)
    {
        if (m_bandWidth <= 0.0) {
            m_fft.transform(z);
        }

        std::vector<std::complex<double>> padded(m, std::complex<double>(0.0, 0.0));
        const int h = searchSamples / 2;

        for (int i = 0; i < h; i++)
        {
            padded[i] = z[i];
            padded[m - h + i] = z[searchSamples - h + i];
        }

        m_fft.transform(padded, true);
        z.swap(padded);
    }

    std::vector<std::complex<double>> y(m);
    double mean = 0.0;

    for (int i = 0; i < m; i++) {
        mean += std::norm(z[i]);
    }

    mean /= m;
    const std::vector<double>& lineWindow = hann(m_lineWindow, m);

    for (int i = 0; i < m; i++) {
        y[i] = (std::norm(z[i]) - mean) * lineWindow[i];
    }

    m_fft.transform(y);
    const double lineHz = (upsample ? 2.0 * m_inputRate : m_inputRate) / m;
    // A little beyond the limits, so a rate at one is found (not taken for the edge of the range), then checked
    const int lo = std::max(1, (int) std::floor(std::max(0.95 * m_minRate, m_searchLow * m_result.m_width) / lineHz));
    const int hi = std::min(m / 2 - 2, (int) std::ceil(std::min(1.05 * m_maxRate, m_searchHigh * m_result.m_width) / lineHz));

    if (hi <= lo + 4) {
        return;
    }

    std::vector<double> mag(hi - lo + 1);
    int best = lo;

    for (int k = lo; k <= hi; k++)
    {
        mag[k - lo] = std::norm(y[k]);

        if (mag[k - lo] > mag[best - lo]) {
            best = k;
        }
    }

    std::vector<double> magSorted(mag);
    std::sort(magSorted.begin(), magSorted.end());
    const double median = std::max(1e-30, magSorted[magSorted.size() / 2]);
    m_result.m_lineSnr = 10.0 * std::log10(mag[best - lo] / median);

    if ((m_result.m_lineSnr < m_minLineSnr) || (best <= lo) || (best >= hi)) {
        return;
    }

    // Interpolated on the log magnitude (Gaussian), which for the Hann window's main lobe is close
    const double a = std::log(std::norm(y[best - 1]) + 1e-30), b = std::log(std::norm(y[best]) + 1e-30);
    const double c = std::log(std::norm(y[best + 1]) + 1e-30);
    const double denom = a - 2.0 * b + c;
    const double offset = denom < 0.0 ? std::max(-0.5, std::min(0.5, 0.5 * (a - c) / denom)) : 0.0;
    double rate = (best + offset) * lineHz;

    for (double common : m_commonRates)
    {
        if (std::fabs(rate - common) <= m_snapTolerance * common)
        {
            rate = common;
            break;
        }
    }

    m_result.m_symbolRate = rate;
    m_result.m_found = (rate >= m_minRate * (1.0 - m_snapTolerance)) && (rate <= m_maxRate * (1.0 + m_snapTolerance));
}

} // namespace dvbs
