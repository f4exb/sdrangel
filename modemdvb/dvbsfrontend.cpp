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

#include "dvbs.h"
#include "dvbsfrontend.h"
#include "resampler.h"

namespace dvbs {

// Raised cosine pulse (the transmit and receive filters together)
double FrontEnd::raisedCosine(double t, double a)
{
    const double x = 2.0 * a * t;
    const double sinc = std::fabs(t) < 1e-9 ? 1.0 : std::sin(m_pi * t) / (m_pi * t);

    if (std::fabs(std::fabs(x) - 1.0) < 1e-9) {
        return m_pi / 4.0 * (a > 0.0 ? std::sin(m_pi / (2.0 * a)) / (m_pi / (2.0 * a)) : 1.0);
    }

    return sinc * std::cos(m_pi * a * t) / (1.0 - x * x);
}

double FrontEnd::besselI0(double x)
{
    double sum = 1.0, term = 1.0;

    for (int k = 1; k < 50; k++)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;

        if (term < sum * 1e-12) {
            break;
        }
    }

    return sum;
}

FrontEnd::FrontEnd() :
    m_inputRate(2e6),
    m_symbolRate(1e6),
    m_rollOff(0.35),
    m_samplesPerSymbol(2.0),
    m_taps(0),
    m_ncoFrequency(0.0),
    m_ncoPhase(0.0),
    m_frequencyKnown(false),
    m_spectrumFill(0),
    m_estimateBlocks(0),
    m_untilDisplayBlock(0),
    m_time(0.0),
    m_step(1.0),
    m_onSymbol(true),
    m_haveLast(false),
    m_integrator(0.0),
    m_kp(0.0),
    m_ki(0.0),
    m_bandwidth(2e-3),
    m_detectorGain(1.0),
    m_power(1.0),
    m_powerValid(false),
    m_inputPowerSum(0.0),
    m_inputPowerCount(0),
    m_eqEnabled(true),
    m_eqLocked(false),
    m_eqEsN0(10.0),
    m_eqBlocks(0)
{
    m_window.resize(m_fftSize);
    m_twiddles.resize(m_fftSize / 2);
    m_bitReverse.resize(m_fftSize);
    int bits = 0;

    while ((1 << bits) < m_fftSize) {
        bits++;
    }

    for (int i = 0; i < m_fftSize; i++)
    {
        m_window[i] = 0.5 - 0.5 * std::cos(2.0 * m_pi * i / m_fftSize);
        int r = 0;

        for (int b = 0; b < bits; b++) {
            r |= ((i >> b) & 1) << (bits - 1 - b);
        }

        m_bitReverse[i] = r;
    }

    for (int i = 0; i < m_fftSize / 2; i++) {
        m_twiddles[i] = std::polar(1.0, -2.0 * m_pi * i / m_fftSize);
    }

    configure(m_inputRate, m_symbolRate, m_rollOff);
}

void FrontEnd::configure(double inputRate, double symbolRate, double rollOff)
{
    m_inputRate = inputRate;
    m_symbolRate = symbolRate;
    m_rollOff = rollOff;
    m_samplesPerSymbol = inputRate / symbolRate;
    m_step = m_samplesPerSymbol / 2.0;
    m_taps = 0;
    m_eqGainDb.clear();
    makeFilter();
    computeGains();
    reset();
}

void FrontEnd::setRollOff(double rollOff)
{
    if (rollOff == m_rollOff) {
        return;
    }

    // The timing and carrier carry on: only the filter changes
    m_rollOff = rollOff;
    makeFilter();
    computeGains();
}

void FrontEnd::makeFilter()
{
    // A low roll-off's pulse falls off over about 1 / roll-off symbols: spanned twice that (at 0.05 and 0.10, MER 19.7
    // to 19.8 dB at Es/N0 20, against 19.9 at 0.35; once that, 18.7 to 19.0), at the cost of as many more taps
    const double span = std::max(m_filterSpan, 2.0 / std::max(0.01, m_rollOff));
    int taps = (int) std::ceil(span * m_samplesPerSymbol);
    taps = std::max(8, std::min(1024, (taps + 3) & ~3));
    const int oldTaps = m_taps;
    m_taps = taps;
    const double half = m_taps / 2.0;
    const double beta = 5.0;
    const double i0Beta = besselI0(beta);
    const double sps = m_samplesPerSymbol;
    const double a = m_rollOff;

    if (m_eqGainDb.empty())
    {
        dvbt::makePolyphaseFilter(m_taps, m_phases, [half, beta, i0Beta, sps, a](double x) {
            const double arg = x / half;
            const double window = std::fabs(arg) < 1.0 ? besselI0(beta * std::sqrt(1.0 - arg * arg)) / i0Beta : 0.0;
            return rootRaisedCosine(x / sps, a) * window;
        }, m_pairs);
    }
    else
    {
        // Equalised: the root raised cosine's frequency response times the correction (both even, so the impulse is
        // real), its impulse the integral of that by cosines, from points across the signal's band
        const double outer = (1.0 + a) / 2.0;   // Of the symbol rate
        const double binRate = m_inputRate / m_fftSize / m_symbolRate;
        const double spacing = outer / m_eqFrequencies / sps;    // Of the points, cycles an input sample
        std::vector<double> response(m_eqFrequencies);

        for (int k = 0; k < m_eqFrequencies; k++)
        {
            const double f = (k + 0.5) * outer / m_eqFrequencies;   // Of the symbol rate
            const double inner = (1.0 - a) / 2.0;
            const double rc = f <= inner ? 1.0 : f >= outer ? 0.0 : 0.5 * (1.0 + std::cos(m_pi * (f - inner) / a));
            const int bin = std::min((int) m_eqGainDb.size() - 1, (int) std::lround(f / binRate));
            response[k] = std::sqrt(rc) * std::pow(10.0, m_eqGainDb[bin] / 20.0);
        }

        // Point k's at (k + 0.5) spacing: its cosine the real part of a phasor turned by 2 pi spacing x a point (in
        // double, to about 1e-14 over the 128), rather than a cosine each, taps x phases x 128 of them
        dvbt::makePolyphaseFilter(m_taps, m_phases, [&](double x) {
            const double arg = x / half;

            if (std::fabs(arg) >= 1.0) {
                return 0.0;
            }

            const double window = besselI0(beta * std::sqrt(1.0 - arg * arg)) / i0Beta;
            const double stepRe = std::cos(2.0 * m_pi * spacing * x), stepIm = std::sin(2.0 * m_pi * spacing * x);
            double re = std::cos(m_pi * spacing * x), im = std::sin(m_pi * spacing * x);
            double sum = 0.0;

            for (int k = 0; k < m_eqFrequencies; k++)
            {
                sum += response[k] * re;
                const double t = re * stepRe - im * stepIm;
                im = re * stepIm + im * stepRe;
                re = t;
            }

            return sum * window;
        }, m_pairs);
    }

    // The output's time is its centre's (the filter's window starts taps / 2 - 1 before it, whatever its length), kept
    // so the timing doesn't jump. A longer filter's first taps reach before the history kept: zeros there (the pulse's
    // tails, a small transient), and the time moved with the samples (left, the symbols jumped 5 at 2.5 samples a
    // symbol as a low roll-off's longer filter came in, and the lock was lost). A shorter one's are within it
    const int grow = (m_taps / 2 - 1) - (oldTaps / 2 - 1);

    if (oldTaps && (grow > 0) && !m_buffer.empty())
    {
        m_buffer.insert(m_buffer.begin(), grow, Cf(0.0f, 0.0f));
        m_time += grow;
    }
}

// The normalised Gardner detector's gain, from the raised cosine pulse, and the loop's gains from it
void FrontEnd::computeGains()
{
    auto detector = [this](double tau) {
        double e = 0.0, p = 0.0;

        for (int m = -40; m <= 40; m++)
        {
            e += raisedCosine(m - 0.5 + tau, m_rollOff) * (raisedCosine(m + tau, m_rollOff) - raisedCosine(m - 1 + tau, m_rollOff));
            p += raisedCosine(m + tau, m_rollOff) * raisedCosine(m + tau, m_rollOff);
        }

        return e / p;
    };

    m_detectorGain = (detector(0.01) - detector(-0.01)) / 0.02;
    const double zeta = 0.707;
    const double theta = m_bandwidth / (zeta + 1.0 / (4.0 * zeta));
    const double d = 1.0 + 2.0 * zeta * theta + theta * theta;
    m_kp = 4.0 * zeta * theta / d / m_detectorGain;
    m_ki = 4.0 * theta * theta / d / m_detectorGain;
}

void FrontEnd::adjustClock(double ppm)
{
    m_integrator = std::max(-m_maxClockOffset, std::min(m_maxClockOffset, m_integrator + ppm * 1e-6));
    m_status.m_clockAdjustments++;
}

void FrontEnd::setTimingBandwidth(double bnT)
{
    if (bnT != m_bandwidth)
    {
        m_bandwidth = bnT;
        computeGains();
    }
}

void FrontEnd::reset()
{
    m_buffer.assign(m_taps, Cf(0.0f, 0.0f));
    m_time = m_taps - 1;
    m_onSymbol = true;
    m_haveLast = false;
    m_lastSymbol = Cf(0.0f, 0.0f);
    m_lastMid = Cf(0.0f, 0.0f);
    m_integrator = 0.0;
    m_power = 1.0;
    m_powerValid = false;
    m_spectrumFill = 0;
    m_spectrumBlock.assign(m_fftSize, Cf(0.0f, 0.0f));
    m_estimatePower.assign(m_fftSize, 0.0);
    m_estimateBlocks = 0;
    m_untilDisplayBlock = 0;
    m_ncoPhase = 0.0;
    m_inputPowerSum = 0.0;
    m_inputPowerCount = 0;
    const double power = m_status.m_power;
    // The clock adjustments made count on (a receiver may be waiting for one)
    const uint64_t clockAdjustments = m_status.m_clockAdjustments;
    m_status = Status();
    m_status.m_clockAdjustments = clockAdjustments;
    m_status.m_power = power;
    clearEqualiser();

    if (m_frequencyKnown)
    {
        m_status.m_estimating = false;
        m_frequencyKnown = false;
    }
    else
    {
        m_ncoFrequency = 0.0;
    }

    m_status.m_frequencyOffset = m_ncoFrequency;
}

void FrontEnd::setFrequency(double hz)
{
    m_ncoFrequency = hz;
    m_frequencyKnown = true;
    m_status.m_frequencyOffset = hz;
}

int FrontEnd::heldSymbols() const
{
    // The next output's at m_time in the buffer
    return m_samplesPerSymbol > 0.0 ? std::max(0, (int) std::lround(((double) m_buffer.size() - m_time) / m_samplesPerSymbol)) : 0;
}

void FrontEnd::adjustFrequency(double hz)
{
    m_ncoFrequency += hz;
    m_status.m_frequencyOffset = m_ncoFrequency;
}

void FrontEnd::fft(std::vector<std::complex<double>>& x) const
{
    const int n = m_fftSize;

    for (int i = 0; i < n; i++)
    {
        if (i < m_bitReverse[i]) {
            std::swap(x[i], x[m_bitReverse[i]]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int stride = n / len;

        for (int i = 0; i < n; i += len)
        {
            for (int j = 0; j < half; j++)
            {
                const std::complex<double> t = x[i + j + half] * m_twiddles[j * stride];
                x[i + j + half] = x[i + j] - t;
                x[i + j] += t;
            }
        }
    }
}

void FrontEnd::setEqualiserReference(bool locked, double esn0)
{
    m_eqLocked = locked;

    if (locked) {
        m_eqEsN0 = esn0;
    }

    if (!m_eqEnabled && !m_eqGainDb.empty()) {
        clearEqualiser();
    }
}

void FrontEnd::clearEqualiser()
{
    m_eqPower.clear();
    m_eqBlocks = 0;
    m_status.m_equalising = false;
    m_status.m_equaliserEdgeDb = 0.0;

    if (!m_eqGainDb.empty())
    {
        m_eqGainDb.clear();
        makeFilter();
    }
}

// A spectrum block (of the input, taken for the display), averaged while a receiver's locked, the equaliser updated from
// each m_eqUpdateBlocks
void FrontEnd::accumulateEqualiser(const std::vector<double>& power)
{
    if (!m_eqEnabled || !m_eqLocked || (m_eqEsN0 < m_eqMinEsN0) || (m_samplesPerSymbol > m_eqMaxSamplesPerSymbol))
    {
        m_eqPower.clear();
        m_eqBlocks = 0;
        return;
    }

    if (m_eqPower.size() != power.size()) {
        m_eqPower.assign(power.size(), 0.0);
    }

    for (size_t k = 0; k < power.size(); k++) {
        m_eqPower[k] += power[k];
    }

    if (++m_eqBlocks >= m_eqUpdateBlocks)
    {
        computeEqualiser();
        m_eqPower.assign(power.size(), 0.0);
        m_eqBlocks = 0;
    }
}

// The receive chain's response: the averaged spectrum over what it'd be (the raised cosine at the Es/N0 measured, over
// flat noise), relative to the middle of the band, smoothed, made symmetric (the matched filter's real), and the
// correction its inverse, limited. Averaged with the last, so it changes gradually
void FrontEnd::computeEqualiser()
{
    const int n = m_fftSize;
    const double binHz = m_inputRate / n;
    const double snr = std::pow(10.0, std::min(30.0, m_eqEsN0) / 10.0);
    const double inner = (1.0 - m_rollOff) * m_symbolRate / 2.0;
    const double outer = (1.0 + m_rollOff) * m_symbolRate / 2.0;
    const int maxBin = (int) std::ceil(outer / binHz) + 1;
    // The spectrum's in order from -inputRate/2, of the input before the NCO: the signal's centre is at its frequency
    const double centreBin = n / 2 + m_ncoFrequency / binHz;
    std::vector<double> ratio(2 * maxBin + 1, 0.0);
    std::vector<double> core;

    for (int d = -maxBin; d <= maxBin; d++)
    {
        const double fb = centreBin + d;
        const int i0 = (int) std::floor(fb);
        const double t = fb - i0;
        const double p = (i0 >= 0) && (i0 + 1 < n) ? (1.0 - t) * m_eqPower[i0] + t * m_eqPower[i0 + 1] : 0.0;
        const double f = std::fabs(d * binHz);
        const double rc = f <= inner ? 1.0 : f >= outer ? 0.0 : 0.5 * (1.0 + std::cos(m_pi * (f - inner) / (m_rollOff * m_symbolRate)));
        ratio[d + maxBin] = p / (rc * snr + 1.0);

        if (f < 0.3 * m_symbolRate) {
            core.push_back(ratio[d + maxBin]);
        }
    }

    if (core.empty()) {
        return;
    }

    std::sort(core.begin(), core.end());
    const double reference = core[core.size() / 2];

    if (reference <= 0.0) {
        return;
    }

    // Smoothed, and the two sides averaged (in dB)
    const int smooth = std::max(1, (int) std::lround(m_eqSmoothing * m_symbolRate / binHz));
    std::vector<double> responseDb(2 * maxBin + 1);

    for (int d = -maxBin; d <= maxBin; d++)
    {
        double sum = 0.0;
        int count = 0;

        for (int j = std::max(-maxBin, d - smooth); j <= std::min(maxBin, d + smooth); j++, count++) {
            sum += ratio[j + maxBin];
        }

        responseDb[d + maxBin] = 10.0 * std::log10(std::max(1e-6, sum / count / reference));
    }

    std::vector<double> gainDb(maxBin + 1);

    for (int d = 0; d <= maxBin; d++)
    {
        const double response = 0.5 * (responseDb[maxBin + d] + responseDb[maxBin - d]);
        gainDb[d] = std::max(m_eqMinGainDb, std::min(m_eqMaxGainDb, -response));
    }

    if (m_eqGainDb.size() == gainDb.size())
    {
        double change = 0.0;

        for (size_t k = 0; k < gainDb.size(); k++)
        {
            gainDb[k] = 0.5 * (gainDb[k] + m_eqGainDb[k]);
            change = std::max(change, std::fabs(gainDb[k] - m_eqGainDb[k]));
        }

        // Too little to matter: the filter (made in this, the front end's, thread) kept, and the correction it's of
        if (change < m_eqMinChangeDb) {
            return;
        }
    }

    m_eqGainDb = gainDb;
    makeFilter();
    m_status.m_equalising = true;
    m_status.m_equaliserEdgeDb = m_eqGainDb[std::min(maxBin, (int) std::lround(m_symbolRate / 2.0 / binHz))];
}

// Windowed power spectrum of a block, added to power, in order from -inputRate/2
void FrontEnd::spectrumBlock(const Cf *block, std::vector<double>& power)
{
    std::vector<std::complex<double>> x(m_fftSize);

    for (int i = 0; i < m_fftSize; i++) {
        x[i] = std::complex<double>(block[i].real(), block[i].imag()) * m_window[i];
    }

    fft(x);

    for (int i = 0; i < m_fftSize; i++) {
        power[(i + m_fftSize / 2) % m_fftSize] += std::norm(x[i]);
    }
}

// The signal's centre, from the averaged spectrum: where it correlates best with the raised cosine's power spectrum
// (less the noise floor, so the shape's what counts), interpolated between bins. And its half power width, which is
// the symbol rate
void FrontEnd::estimateCarrier()
{
    const int n = m_fftSize;
    const double binHz = m_inputRate / n;
    std::vector<double> smooth(n, 0.0);

    for (int i = 0; i < n; i++)
    {
        double sum = 0.0;
        int count = 0;

        for (int j = std::max(0, i - 2); j <= std::min(n - 1, i + 2); j++, count++) {
            sum += m_estimatePower[j];
        }

        smooth[i] = sum / count;
    }

    std::vector<double> sorted(smooth);
    std::sort(sorted.begin(), sorted.end());
    const double floor = sorted[n / 5];
    std::vector<double> excess(n);

    for (int i = 0; i < n; i++) {
        excess[i] = std::max(0.0, smooth[i] - floor);
    }

    const double inner = (1.0 - m_rollOff) * m_symbolRate / 2.0;
    const double outer = (1.0 + m_rollOff) * m_symbolRate / 2.0;
    const int width = (int) std::ceil(outer / binHz);
    std::vector<double> shape(2 * width + 1);

    for (int d = -width; d <= width; d++)
    {
        const double f = std::fabs(d * binHz);
        shape[d + width] = f <= inner ? 1.0 : f >= outer ? 0.0 : 0.5 * (1.0 + std::cos(m_pi * (f - inner) / (m_rollOff * m_symbolRate)));
    }

    const int maxOffset = std::max(0, (int) ((m_inputRate / 2.0 - outer * 0.9) / binHz));
    std::vector<double> scores(2 * maxOffset + 1, 0.0);
    int best = 0;

    for (int c = -maxOffset; c <= maxOffset; c++)
    {
        double score = 0.0;

        for (int d = -width; d <= width; d++)
        {
            const int k = n / 2 + c + d;

            if ((k >= 0) && (k < n)) {
                score += shape[d + width] * excess[k];
            }
        }

        scores[c + maxOffset] = score;

        if (score > scores[best + maxOffset]) {
            best = c;
        }
    }

    double offset = best;

    if ((best > -maxOffset) && (best < maxOffset))
    {
        const double a = scores[best + maxOffset - 1], b = scores[best + maxOffset], c = scores[best + maxOffset + 1];
        const double denom = a - 2.0 * b + c;

        if (denom < 0.0) {
            offset += std::max(-0.5, std::min(0.5, 0.5 * (a - c) / denom));
        }
    }

    if (scores[best + maxOffset] > 0.0) {
        m_ncoFrequency = offset * binHz;
    }

    // Half power width: the in-band level, then outwards from the centre to where it's half way to the floor
    const int centre = n / 2 + best;
    const int core = std::max(1, (int) (0.6 * inner / binHz));
    std::vector<double> band;

    for (int k = centre - core; k <= centre + core; k++)
    {
        if ((k >= 0) && (k < n)) {
            band.push_back(smooth[k]);
        }
    }

    m_status.m_measuredSymbolRate = 0.0;
    m_status.m_spectralEsN0 = 99.0;

    if (!band.empty())
    {
        std::sort(band.begin(), band.end());
        const double level = band[band.size() / 2];

        // The raised cosine's power spectrum integrates to the symbol rate times its in-band level, which noise in the
        // estimate doesn't bias as it does the half power points. Only if a quarter of the input's band is outside the
        // signal, so the floor (its 20th percentile) is noise: at 1.25 or 1.5 times the symbol rate, it was the signal's
        // edges, and the rate measured 75 to 90 % of the true one
        if ((level > 2.0 * floor) && (outer <= 0.375 * m_inputRate))
        {
            double area = 0.0;
            const int extent = (int) std::ceil(1.25 * outer / binHz);

            // Limited to twice the level, so a line (an unmodulated carrier, or the transmitter's) doesn't count much
            for (int k = std::max(0, centre - extent); k <= std::min(n - 1, centre + extent); k++) {
                area += std::min(smooth[k] - floor, 2.0 * (level - floor));
            }

            m_status.m_measuredSymbolRate = area / (level - floor) * binHz;
        }

        // A raised cosine's in-band level is its Es: over the noise's, Es/N0
        if ((outer <= 0.375 * m_inputRate) && (floor > 0.0)) {
            m_status.m_spectralEsN0 = 10.0 * std::log10(std::max(1e-3, (level - floor) / floor));
        }
    }

    std::lock_guard<std::mutex> lock(m_displayMutex);
    m_displayPower = m_estimatePower;

    for (double& p : m_displayPower) {
        p /= m_spectrumBlocks;
    }
}

void FrontEnd::getSpectrum(std::vector<float>& db, int points) const
{
    std::lock_guard<std::mutex> lock(m_displayMutex);
    db.clear();

    if (m_displayPower.empty() || (points <= 0)) {
        return;
    }

    db.resize(points);
    const int per = std::max(1, m_fftSize / points);

    for (int p = 0; p < points; p++)
    {
        double sum = 0.0;
        const int first = (int) ((int64_t) p * m_fftSize / points);
        int count = 0;

        for (int i = first; i < std::min(m_fftSize, first + per); i++, count++) {
            sum += m_displayPower[i];
        }

        db[p] = (float) (10.0 * std::log10(std::max(1e-20, sum / std::max(1, count))));
    }
}

// A block of the input for display (and the equaliser), the next a tenth of a second on
void FrontEnd::displayBlock(const Cf *block)
{
    std::vector<double> power(m_fftSize, 0.0);
    spectrumBlock(block, power);

    {
        std::lock_guard<std::mutex> lock(m_displayMutex);

        if (m_displayPower.size() != power.size()) {
            m_displayPower = power;
        }

        for (int k = 0; k < m_fftSize; k++) {
            m_displayPower[k] = 0.8 * m_displayPower[k] + 0.2 * power[k];
        }
    }

    accumulateEqualiser(power);
    m_untilDisplayBlock = (uint64_t) (m_inputRate / 10.0);
}

void FrontEnd::process(const Cf *in, int nbIn, std::vector<Cf>& symbols)
{
    if (nbIn <= 0) {
        return;
    }

    // The input's power, for the status, from up to 4096 samples of each block, 8 partial sums at a time (as the
    // compiler won't reorder a float sum to vectorise it)
    {
        const int n = std::min(nbIn, 4096) & ~3;
        float sums[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        const float *f = reinterpret_cast<const float *>(in);

        for (int i = 0; i < 2 * n; i += 8)
        {
            for (int j = 0; j < 8; j++) {
                sums[j] += f[i + j] * f[i + j];
            }
        }

        m_inputPowerSum += (sums[0] + sums[1]) + (sums[2] + sums[3]) + (sums[4] + sums[5]) + (sums[6] + sums[7]);
        m_inputPowerCount += n;
    }

    if (m_inputPowerCount >= (uint64_t) (m_inputRate / 100.0))
    {
        m_status.m_power = 10.0 * std::log10(std::max(1e-20, m_inputPowerSum / m_inputPowerCount));
        m_inputPowerSum = 0.0;
        m_inputPowerCount = 0;
    }

    // The spectrum, of the raw input: for the carrier's estimate, all the blocks, then for display a block now and then
    if (m_status.m_estimating)
    {
        int i = 0;

        while (i < nbIn)
        {
            const int n = std::min(nbIn - i, m_fftSize - m_spectrumFill);
            std::copy(in + i, in + i + n, m_spectrumBlock.begin() + m_spectrumFill);
            m_spectrumFill += n;
            i += n;

            if (m_spectrumFill == m_fftSize)
            {
                spectrumBlock(m_spectrumBlock.data(), m_estimatePower);
                m_spectrumFill = 0;

                if (++m_estimateBlocks == m_spectrumBlocks)
                {
                    estimateCarrier();
                    m_status.m_estimating = false;
                    m_status.m_frequencyOffset = m_ncoFrequency;
                    m_untilDisplayBlock = (uint64_t) (m_inputRate / 10.0);
                    break;
                }
            }
        }

        if (m_status.m_estimating) {
            return;
        }

        // The rest of the samples are demodulated
        in += i;
        nbIn -= i;
    }
    else if ((m_spectrumFill > 0) || (m_untilDisplayBlock <= (uint64_t) nbIn))
    {
        if ((m_spectrumFill == 0) && (nbIn >= m_fftSize))
        {
            displayBlock(in);
        }
        else
        {
            // Blocks shorter than the FFT gathered (the estimate's buffer's free once it's made), from the one due
            const int n = std::min(nbIn, m_fftSize - m_spectrumFill);
            std::copy(in, in + n, m_spectrumBlock.begin() + m_spectrumFill);
            m_spectrumFill += n;

            if (m_spectrumFill == m_fftSize)
            {
                m_spectrumFill = 0;
                displayBlock(m_spectrumBlock.data());
            }
        }
    }
    else
    {
        m_untilDisplayBlock -= nbIn;
    }

    // NCO, in float, from the exact phase every block, into the buffer grown for it (its capacity doubled when short,
    // so it's seldom reallocated)
    const size_t base = m_buffer.size();

    if (m_buffer.capacity() < base + nbIn) {
        m_buffer.reserve(2 * (base + nbIn));
    }

    m_buffer.resize(base + nbIn);
    const double stepAngle = -2.0 * m_pi * m_ncoFrequency / m_inputRate;
    const Cf stepRot((float) std::cos(stepAngle), (float) std::sin(stepAngle));

    if (m_ncoFrequency == 0.0)
    {
        std::copy(in, in + nbIn, m_buffer.begin() + base);
    }
    else
    {
        // A block of 64 phasors from 64 steps, then each block's start from the exact phase
        float stepRe[64], stepIm[64];
        Cf p(1.0f, 0.0f);

        for (int k = 0; k < 64; k++)
        {
            stepRe[k] = p.real();
            stepIm[k] = p.imag();
            p *= stepRot;
        }

        const float *src = reinterpret_cast<const float *>(in);
        float *dst = reinterpret_cast<float *>(m_buffer.data() + base);

        for (int j = 0; j < nbIn; j += 64)
        {
            const int n = std::min(64, nbIn - j);
            const double phase = m_ncoPhase + j * stepAngle;
            const float rr = (float) std::cos(phase), ri = (float) std::sin(phase);

            for (int k = 0; k < n; k++)
            {
                const float pr = rr * stepRe[k] - ri * stepIm[k], pi_ = rr * stepIm[k] + ri * stepRe[k];
                const float xr = src[2 * (j + k)], xi = src[2 * (j + k) + 1];
                dst[2 * (j + k)] = xr * pr - xi * pi_;
                dst[2 * (j + k) + 1] = xr * pi_ + xi * pr;
            }
        }

        m_ncoPhase = std::fmod(m_ncoPhase + nbIn * stepAngle, 2.0 * m_pi);
    }

    // Matched filter and interpolation, a block of symbols at a time, the timing loop updated after each
    const int maxOut = 2 * m_symbolsPerUpdate;
    m_outputs.resize(maxOut);

    while (true)
    {
        const double step = m_step * (1.0 - m_integrator);
        const int made = dvbt::resampleFilter(m_buffer.data(), 0, (int) m_buffer.size(), m_pairs.data(), m_taps, m_phases,
            m_time, step, m_outputs.data(), maxOut);
        float error = 0.0f, power = 0.0f;
        int count = 0;
        int k = 0;
        bool endedOnSymbol = false;     // The last output a symbol, so the next's a half way sample
        const size_t blockFirst = symbols.size();   // This block's symbols, for the AGC: the one held from the last too

        if (!m_onSymbol && (made > 0))
        {
            m_lastMid = m_outputs[0];
            m_onSymbol = true;
            k = 1;
        }

        if (!m_haveLast && (k < made))
        {
            m_lastSymbol = m_outputs[k];
            symbols.push_back(m_lastSymbol);
            m_haveLast = true;
            m_onSymbol = false;

            if (++k < made)
            {
                m_lastMid = m_outputs[k++];
                m_onSymbol = true;
            }
            else
            {
                endedOnSymbol = true;
            }
        }

        // Pairs of a symbol and the half way sample after it, after any symbol just output. Gardner: positive when late,
        // the half way sample past the zero crossing of a transition
        const size_t first = symbols.size();
        symbols.resize(first + (made - k + 1) / 2);
        Cf *out = symbols.data() + first;
        const Cf *outputs = m_outputs.data();
        float lr = m_lastSymbol.real(), li = m_lastSymbol.imag(), mr = m_lastMid.real(), mi = m_lastMid.imag();
        int s = 0;

        for (; k + 1 < made; k += 2)
        {
            const float yr = outputs[k].real(), yi = outputs[k].imag();
            error += mr * (yr - lr) + mi * (yi - li);
            power += yr * yr + yi * yi;
            out[s++] = outputs[k];
            lr = yr;
            li = yi;
            mr = outputs[k + 1].real();
            mi = outputs[k + 1].imag();
        }

        count = s;

        if (k < made)
        {
            // A symbol whose half way sample is in the next block
            const float yr = m_outputs[k].real(), yi = m_outputs[k].imag();
            error += mr * (yr - lr) + mi * (yi - li);
            power += yr * yr + yi * yi;
            out[s++] = m_outputs[k];
            lr = yr;
            li = yi;
            count++;
            m_onSymbol = false;
        }
        else if ((made > 0) && !endedOnSymbol)
        {
            m_onSymbol = true;
        }

        symbols.resize(first + s);
        m_lastSymbol = Cf(lr, li);
        m_lastMid = Cf(mr, mi);

        if ((count > 0) && (power > 0.0))
        {
            const double e = error / power;
            m_integrator = std::max(-m_maxClockOffset, std::min(m_maxClockOffset, m_integrator + m_ki * e * count));
            const double move = std::max(-m_maxPhaseStep, std::min(m_maxPhaseStep, m_kp * e * count));
            m_time = std::max((double) (m_taps / 2 - 1), m_time - move * m_samplesPerSymbol);
            const double mean = power / count;
            m_power = m_powerValid ? 0.9 * m_power + 0.1 * mean : mean;
            m_powerValid = true;
        }

        if (m_powerValid)
        {
            const float gain = (float) (1.0 / std::sqrt(std::max(1e-30, m_power)));

            for (size_t s = blockFirst; s < symbols.size(); s++) {
                symbols[s] *= gain;
            }
        }

        m_status.m_symbols += symbols.size() - blockFirst;

        if (made < maxOut) {
            break;
        }
    }

    m_status.m_clockOffset = (1.0 / (1.0 - m_integrator) - 1.0) * 1e6;

    // Keep the input the next outputs need
    const int consumed = std::min((int) m_buffer.size(), std::max(0, (int) m_time - (m_taps / 2 - 1)));

    if (consumed > 0)
    {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + consumed);
        m_time -= consumed;
    }
}

} // namespace dvbs
