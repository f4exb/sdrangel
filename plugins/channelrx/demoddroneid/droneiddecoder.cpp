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

#include <cmath>
#include <algorithm>

#include "dsp/fftengine.h"
#include "dsp/fftfactory.h"
#include "util/ltegold.h"

#include "droneiddecoder.h"

const int DroneIDDecoder::m_cpLengths[DroneIDDecoder::m_nbSymbols] = {80, 72, 72, 72, 72, 72, 72, 72, 80};
const int DroneIDDecoder::m_zcSymbols[2] = {3, 5};
const int DroneIDDecoder::m_zcRoots[2] = {600, 147};
const int DroneIDDecoder::m_dataSymbols[6] = {1, 2, 4, 6, 7, 8};

DroneIDDecoder::DroneIDDecoder(FFTFactory *fftFactory) :
    m_maxIterations(8),
    m_maxIntegerCFO(8),
    m_fftFactory(fftFactory),
    m_fftEngine(nullptr),
    m_fftSequence(0),
    m_kissFFT(m_fftSize, false)
{
    if (m_fftFactory) {
        m_fftSequence = m_fftFactory->getEngine(m_fftSize, false, &m_fftEngine);
    }

    carrierBins(m_bins);
    zadoffChu(m_zcRoots[0], m_zc[0]);
    zadoffChu(m_zcRoots[1], m_zc[1]);
    m_zcRef.resize(m_nbCarriers);
    for (int c = 0; c < m_nbCarriers; c++) {
        m_zcRef[c] = std::conj(m_zc[0][c]) * m_zc[1][c];
    }
    m_zcDiff.resize(m_fftSize);
    symbolStarts(m_symbolStarts);
    LTEGold::generate(m_scramblerInit, m_codedBits, m_scrambler);
    m_turbo.setBlockSize(m_blockBits);
    m_fftIn.resize(m_fftSize);
    m_symbols.resize(m_nbSymbols);
    for (auto& s : m_symbols) {
        s.resize(m_fftSize);
    }
}

void DroneIDDecoder::carrierBins(std::vector<int>& bins)
{
    bins.resize(m_nbCarriers);
    for (int i = 0; i < m_nbCarriers; i++)
    {
        int k = i < m_nbCarriers / 2 ? i - m_nbCarriers / 2 : i - m_nbCarriers / 2 + 1; // -300..-1, 1..300
        bins[i] = (k + m_fftSize) % m_fftSize;
    }
}

void DroneIDDecoder::zadoffChu(int root, std::vector<cf>& seq)
{
    const int Nzc = m_nbCarriers + 1;

    // Element on DC (n=300) isn't transmitted
    seq.clear();
    for (int n = 0; n < Nzc; n++)
    {
        if (n != Nzc / 2)
        {
            double phase = -M_PI * root * (double) n * (double) (n + 1) / Nzc;
            seq.push_back(cf((float) std::cos(phase), (float) std::sin(phase)));
        }
    }
}

void DroneIDDecoder::symbolStarts(std::vector<int>& starts)
{
    starts.resize(m_nbSymbols);
    int p = 0;
    for (int i = 0; i < m_nbSymbols; i++)
    {
        starts[i] = p;
        p += m_cpLengths[i] + m_fftSize;
    }
}

// Find candidate frame starts by correlating each symbol's cyclic prefix with the end of the symbol.
// As symbols are similar lengths, the metric is also high when offset by a whole number of symbols,
// particularly at low SNR, so we return the best start along with the best starts nearby offset by
// whole symbols, to be checked with the ZC sequences.
void DroneIDDecoder::findTiming(const cf *samples, int length, std::vector<Candidate>& candidates)
{
    const int N = m_fftSize;
    const int symbolLength = m_cpLengths[1] + N;
    const int search = 24;
    std::vector<std::complex<double>> q(length + 1);
    std::vector<double> e(length + 1);
    const int nbStarts = length - m_frameLength + 1;

    candidates.clear();
    if (nbStarts <= 0) {
        return;
    }

    q[0] = 0.0;
    e[0] = 0.0;
    for (int i = 0; i < length; i++)
    {
        std::complex<double> x = samples[i];
        q[i + 1] = q[i] + ((i + N < length) ? x * std::conj((std::complex<double>) samples[i + N]) : 0.0);
        e[i + 1] = e[i] + std::norm(x);
    }

    std::vector<float> metrics(nbStarts);
    std::vector<cf> corrs(nbStarts);
    int best = 0;

    for (int s = 0; s < nbStarts; s++)
    {
        std::complex<double> acc = 0.0;
        double energy = 0.0;

        for (int k = 0; k < m_nbSymbols; k++)
        {
            int a = s + m_symbolStarts[k];
            int cp = m_cpLengths[k];
            acc += q[a + cp] - q[a];
            energy += 0.5 * ((e[a + cp] - e[a]) + (e[a + N + cp] - e[a + N]));
        }
        metrics[s] = energy > 0.0 ? (float) (std::abs(acc) / energy) : 0.0f;
        corrs[s] = cf((float) acc.real(), (float) acc.imag());
        if (metrics[s] > metrics[best]) {
            best = s;
        }
    }

    for (int j = -m_maxSymbolOffset; j <= m_maxSymbolOffset; j++)
    {
        int centre = best + j * symbolLength;
        int lo = std::max(0, centre - (j == 0 ? 0 : search));
        int hi = std::min(nbStarts - 1, centre + (j == 0 ? 0 : search));
        if (lo > hi) {
            continue;
        }
        int s = lo;
        for (int i = lo + 1; i <= hi; i++)
        {
            if (metrics[i] > metrics[s]) {
                s = i;
            }
        }
        Candidate c;
        c.m_start = s;
        c.m_correlation = corrs[s];
        c.m_metric = metrics[s];
        candidates.push_back(c);
    }
}

DroneIDDecoder::~DroneIDDecoder()
{
    if (m_fftFactory && m_fftEngine) {
        m_fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
    }
}

// FFT a symbol after correcting frequency offset
void DroneIDDecoder::fftSymbol(const cf *samples, int length, int frameStart, double cyclesPerSample, int symbol)
{
    const int start = frameStart + m_symbolStarts[symbol] + m_cpLengths[symbol] - m_fftBackoff;
    cf *in = m_fftEngine ? m_fftEngine->in() : m_fftIn.data();

    // Use absolute sample index for phase, so symbols are phase coherent
    std::complex<double> rotation = std::polar(1.0, -2.0 * M_PI * std::fmod(cyclesPerSample * start, 1.0));
    const std::complex<double> step = std::polar(1.0, -2.0 * M_PI * cyclesPerSample);

    for (int i = 0; i < m_fftSize; i++)
    {
        int idx = start + i;
        if ((idx >= 0) && (idx < length)) {
            in[i] = samples[idx] * cf((float) rotation.real(), (float) rotation.imag());
        } else {
            in[i] = 0.0f;
        }
        rotation *= step;
    }

    if (m_fftEngine)
    {
        m_fftEngine->transform();
        std::copy(m_fftEngine->out(), m_fftEngine->out() + m_fftSize, m_symbols[symbol].begin());
    }
    else
    {
        m_kissFFT.transform(m_fftIn.data(), m_symbols[symbol].data());
    }
}

// FFT each symbol after correcting frequency offset
void DroneIDDecoder::fftSymbols(const cf *samples, int length, int frameStart, double cyclesPerSample)
{
    for (int k = 0; k < m_nbSymbols; k++) {
        fftSymbol(samples, length, frameStart, cyclesPerSample, k);
    }
}

// Phase difference between the ZC symbols in each FFT bin, for zcConsistency
void DroneIDDecoder::zcPhaseDifference()
{
    const std::vector<cf>& y4 = m_symbols[m_zcSymbols[0]];
    const std::vector<cf>& y6 = m_symbols[m_zcSymbols[1]];

    // Only phase is used, so a strong narrowband interferer on one carrier doesn't dominate
    for (int i = 0; i < m_fftSize; i++)
    {
        cf p = y4[i] * std::conj(y6[i]);
        float mag = std::abs(p);
        m_zcDiff[i] = mag > 0.0f ? p / mag : 0.0f;
    }
}

// Estimate channel from both ZC symbols, assuming a frequency offset of shift bins.
// With the correct shift, both estimates are the same apart from a common phase rotation.
// As the ZC sequences have unit magnitude, h4*conj(h6) = y4*conj(y6) * conj(zc4)*zc6.
float DroneIDDecoder::zcConsistency(int shift)
{
    cf corr = 0.0f;

    for (int c = 0; c < m_nbCarriers; c++)
    {
        int bin = (m_bins[c] + shift + m_fftSize) % m_fftSize;
        corr += m_zcDiff[bin] * m_zcRef[c];
    }

    return std::abs(corr) / m_nbCarriers;
}

bool DroneIDDecoder::checkCRC(const std::vector<uint8_t>& bits)
{
    uint8_t bytes[m_blockBytes];

    for (int i = 0; i < m_blockBytes; i++)
    {
        uint8_t b = 0;
        for (int j = 0; j < 8; j++) {
            b = (b << 1) | bits[i * 8 + j];
        }
        bytes[i] = b;
    }
    m_crc.init();
    m_crc.calculate(bytes, m_blockBytes);
    return m_crc.get() == 0;
}

bool DroneIDDecoder::decode(const cf *samples, int length, Result& result)
{
    // Pad so a frame without the first symbol can start at the beginning of the buffer
    const int pad = m_cpLengths[0] + m_fftSize;
    const int paddedLength = length + 2 * pad;

    result = Result();
    m_derotated.assign(paddedLength, 0.0f);
    std::copy(samples, samples + length, m_derotated.begin() + pad);

    // Candidate timings and fractional frequency offsets from cyclic prefix
    std::vector<Candidate> candidates;
    findTiming(m_derotated.data(), paddedLength, candidates);
    for (const auto& candidate : candidates) {
        result.m_cpMetric = std::max(result.m_cpMetric, candidate.m_metric);
    }

    // For each candidate, find integer frequency offset where both ZC symbols give consistent
    // channel estimates. Choose candidate with best consistency.
    int frameStart = -1;
    double cyclesPerSample = 0.0;
    int bestShift = 0;
    float bestMetric = -1.0f;

    for (const auto& candidate : candidates)
    {
        if (candidate.m_metric < m_minCPMetric) {
            continue;
        }
        double cycles = -std::arg(candidate.m_correlation) / (2.0 * M_PI * m_fftSize);
        // Only the ZC symbols are needed to evaluate the candidate
        fftSymbol(m_derotated.data(), paddedLength, candidate.m_start, cycles, m_zcSymbols[0]);
        fftSymbol(m_derotated.data(), paddedLength, candidate.m_start, cycles, m_zcSymbols[1]);
        zcPhaseDifference();

        for (int shift = -m_maxIntegerCFO; shift <= m_maxIntegerCFO; shift++)
        {
            float m = zcConsistency(shift);
            if (m > bestMetric)
            {
                bestMetric = m;
                bestShift = shift;
                frameStart = candidate.m_start;
                cyclesPerSample = cycles;
                result.m_cpMetric = candidate.m_metric;
            }
        }
    }

    result.m_zcMetric = std::max(bestMetric, 0.0f);
    if ((frameStart < 0) || (bestMetric < m_minZCMetric)) {
        return false;
    }
    result.m_frameStart = frameStart - pad;
    cyclesPerSample += bestShift / (double) m_fftSize;
    fftSymbols(m_derotated.data(), paddedLength, frameStart, cyclesPerSample);
    result.m_cfo = (float) (cyclesPerSample * m_sampleRate);

    // Channel estimate from ZC symbols
    std::vector<cf> h4(m_nbCarriers), h6(m_nbCarriers), h(m_nbCarriers);
    cf drift = 0.0f;

    for (int c = 0; c < m_nbCarriers; c++)
    {
        h4[c] = m_symbols[m_zcSymbols[0]][m_bins[c]] * std::conj(m_zc[0][c]);
        h6[c] = m_symbols[m_zcSymbols[1]][m_bins[c]] * std::conj(m_zc[1][c]);
        cf p = h6[c] * std::conj(h4[c]);
        float mag = std::abs(p);
        if (mag > 0.0f) {
            drift += p / mag;
        }
    }

    // Residual phase rotation between the ZC symbols
    float phi = std::arg(drift);
    cf derot = std::polar(1.0f, -phi);
    std::vector<float> noise(m_nbCarriers);
    std::vector<cf> havg(m_nbCarriers);

    for (int c = 0; c < m_nbCarriers; c++)
    {
        cf h6r = h6[c] * derot;
        havg[c] = 0.5f * (h4[c] + h6r);
        // Each estimate has the noise variance of the received signal, the difference has twice that
        noise[c] = 0.5f * std::norm(h4[c] - h6r);
    }

    // Smooth channel estimate across adjacent carriers, and noise estimate over a wider range.
    // Noise is estimated per carrier, so carriers with narrowband interference have low confidence.
    std::vector<float> sigma2(m_nbCarriers);
    float signal = 0.0f;
    float noiseTotal = 0.0f;
    for (int c = 0; c < m_nbCarriers; c++)
    {
        cf sum = 0.0f;
        int n = 0;
        for (int d = -m_channelSmoothing; d <= m_channelSmoothing; d++)
        {
            int i = c + d;
            if ((i >= 0) && (i < m_nbCarriers))
            {
                sum += havg[i];
                n++;
            }
        }
        h[c] = sum / (float) n;
        signal += std::norm(h[c]);

        float nsum = 0.0f;
        n = 0;
        for (int d = -m_noiseSmoothing; d <= m_noiseSmoothing; d++)
        {
            int i = c + d;
            if ((i >= 0) && (i < m_nbCarriers))
            {
                nsum += noise[i];
                n++;
            }
        }
        sigma2[c] = nsum / n;
        noiseTotal += noise[c];
    }
    signal /= m_nbCarriers;
    noiseTotal = std::max(noiseTotal / m_nbCarriers, signal * 1e-6f);
    for (int c = 0; c < m_nbCarriers; c++) {
        sigma2[c] = std::max(sigma2[c], signal * 1e-6f);
    }
    result.m_snr = 10.0f * std::log10(signal / noiseTotal);

    // Channel for each symbol, interpolating phase drift linearly in time
    const float t4 = (float) (m_symbolStarts[m_zcSymbols[0]] + m_cpLengths[m_zcSymbols[0]]);
    const float t6 = (float) (m_symbolStarts[m_zcSymbols[1]] + m_cpLengths[m_zcSymbols[1]]);
    auto symbolRotation = [&](int k) {
        float t = (float) (m_symbolStarts[k] + m_cpLengths[k]);
        return std::polar(1.0f, phi * (t - t4) / (t6 - t4));
    };

    // Check first symbol, which is scrambled zeros, to see if it was transmitted
    {
        cf rot = symbolRotation(0);
        int zeros = 0;
        for (int c = 0; c < m_nbCarriers; c++)
        {
            cf r = m_symbols[0][m_bins[c]] * std::conj(h[c] * rot);
            zeros += ((r.real() < 0.0f ? 1 : 0) == m_scrambler[2 * c]);
            zeros += ((r.imag() < 0.0f ? 1 : 0) == m_scrambler[2 * c + 1]);
        }
        result.m_legacy = zeros < (int) (0.75f * 2 * m_nbCarriers);
    }

    // Soft demap QPSK, descramble
    std::vector<float> llrs(m_codedBits);
    const float scale = 2.0f * (float) M_SQRT2;
    float evm = 0.0f;

    for (int d = 0; d < 6; d++)
    {
        int k = m_dataSymbols[d];
        cf rot = symbolRotation(k);

        for (int c = 0; c < m_nbCarriers; c++)
        {
            cf hk = h[c] * rot;
            cf y = m_symbols[k][m_bins[c]];
            cf r = y * std::conj(hk);
            int idx = 2 * (d * m_nbCarriers + c);

            llrs[idx] = scale * r.real() / sigma2[c] * (m_scrambler[idx] ? -1.0f : 1.0f);
            llrs[idx + 1] = scale * r.imag() / sigma2[c] * (m_scrambler[idx + 1] ? -1.0f : 1.0f);

            float hn = std::norm(hk);
            if (hn > 0.0f)
            {
                cf eq = r / hn;
                cf ref((eq.real() < 0.0f ? -1.0f : 1.0f) / (float) M_SQRT2, (eq.imag() < 0.0f ? -1.0f : 1.0f) / (float) M_SQRT2);
                evm += std::norm(eq - ref);
            }
        }
    }
    result.m_evm = std::sqrt(evm / (6 * m_nbCarriers));

    // Rate dematching and turbo decoding
    std::vector<float> d0, d1, d2;
    std::vector<uint8_t> bits;

    LTERateMatcher::dematch(llrs, m_blockBits + 4, 0, d0, d1, d2);
    int iterations = m_turbo.decode(d0, d1, d2, bits, m_maxIterations,
        [this](const std::vector<uint8_t>& b) { return checkCRC(b); });

    result.m_iterations = std::abs(iterations);
    result.m_crcValid = iterations > 0;
    if (result.m_crcValid)
    {
        result.m_payload.resize(m_dataBytes);
        for (int i = 0; i < m_dataBytes; i++)
        {
            uint8_t b = 0;
            for (int j = 0; j < 8; j++) {
                b = (b << 1) | bits[i * 8 + j];
            }
            result.m_payload[i] = b;
        }
    }

    return true;
}
