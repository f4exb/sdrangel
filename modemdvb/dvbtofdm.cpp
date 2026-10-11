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
#include <cstdarg>
#include <cstdio>

#include <QString>

#include "dsp/fftengine.h"
#include "dsp/fftfactory.h"
#include "util/profiler.h"

#include "cpufeatures.h"
#include "dvbtofdm.h"
#include "dvbtofdmkernel.h"
#include "dvbtinner.h"

#if defined(USE_SSE2) || defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace dvbt {

typedef std::complex<double> Cd;

const double OFDMDemod::m_giLossFraction = 0.35;
const double OFDMDemod::m_giAveraging = 0.02;
const double OFDMDemod::m_twoPi = 6.283185307179586;
const double OFDMDemod::m_acquireThreshold = 0.2;
const double OFDMDemod::m_intCFOThreshold = 0.4;
const double OFDMDemod::m_frequencyLoopGain = 0.1;
const double OFDMDemod::m_delaySmoothing = 0.1;
const float OFDMDemod::m_pilotBoost = 4.0f / 3.0f;
const float OFDMDemod::m_noiseAlpha = 1.0f / 32.0f;
const float OFDMDemod::m_noiseFloor = 0.25f;
const double OFDMDemod::m_notchOffsetTolerance = 1.0;

#if defined(USE_SSE2) || defined(__SSE2__) || defined(_M_X64)
#define DVBTOFDM_SSE2

// Two complex products at once. File-local, as a member would need the intrinsic types in the header
static inline __m128 complexMultiplySSE2(__m128 a, __m128 b)
{
    const __m128 br = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128 bi = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 1, 1));
    const __m128 swapped = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128 negateReal = _mm_castsi128_ps(_mm_set_epi32(0, (int) 0x80000000, 0, (int) 0x80000000));
    return _mm_add_ps(_mm_mul_ps(a, br), _mm_xor_ps(_mm_mul_ps(swapped, bi), negateReal));
}
#endif

// Explicit arithmetic, as std::complex multiplication is slow with MSVC
inline Cf OFDMDemod::complexMultiply(Cf a, Cf b)
{
    return Cf(a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real());
}

// y[i] = x[i] exp(j (phase + step i)), x and y may be the same. The phasors are made in float, a block of 64 at a
// time, from the first 64 and one per block, both by recurrence in double, as sin and cos per value would be slow
void OFDMDemod::rotate(const Cf *x, Cf *y, int n, double phase, double step)
{
    static const int block = 64;
    alignas(16) Cf table[block];
    Cd p(1.0, 0.0);
    const Cd s = std::polar(1.0, step);

    for (int i = 0; i < block; i++)
    {
        table[i] = Cf(p);
        p *= s;
    }

    const Cd blockStep = std::polar(1.0, block * step);
#if defined(MODEMDVB_X86)
    if (modemdvb::cpuHasAVX2())
    {
        rotateAVX2(reinterpret_cast<const float *>(x), reinterpret_cast<float *>(y), n,
            reinterpret_cast<const float *>(table), phase, blockStep.real(), blockStep.imag());
        return;
    }
#endif
    Cd base = std::polar(1.0, phase);

    for (int b = 0; b < n; b += block)
    {
        const int m = std::min(block, n - b);
        const Cf bf(base);
        int i = 0;
#ifdef DVBTOFDM_SSE2
        const __m128 bv = _mm_setr_ps(bf.real(), bf.imag(), bf.real(), bf.imag());

        for (; i + 2 <= m; i += 2)
        {
            const __m128 phasors = complexMultiplySSE2(_mm_load_ps(reinterpret_cast<const float *>(&table[i])), bv);
            const __m128 v = _mm_loadu_ps(reinterpret_cast<const float *>(&x[b + i]));
            _mm_storeu_ps(reinterpret_cast<float *>(&y[b + i]), complexMultiplySSE2(v, phasors));
        }
#endif
        for (; i < m; i++) {
            y[b + i] = complexMultiply(x[b + i], complexMultiply(table[i], bf));
        }

        base *= blockStep;
    }
}

// y[i] *= c
void OFDMDemod::scale(Cf *y, int n, Cf c)
{
    int i = 0;
#ifdef DVBTOFDM_SSE2
    const __m128 cv = _mm_setr_ps(c.real(), c.imag(), c.real(), c.imag());

    for (; i + 2 <= n; i += 2)
    {
        float *p = reinterpret_cast<float *>(&y[i]);
        _mm_storeu_ps(p, complexMultiplySSE2(_mm_loadu_ps(p), cv));
    }
#endif
    for (; i < n; i++) {
        y[i] = complexMultiply(y[i], c);
    }
}

// y[i] *= c[i], with the same arithmetic as std::complex
void OFDMDemod::multiply(Cf *y, const Cf *c, int n)
{
    int i = 0;
#ifdef DVBTOFDM_SSE2
    for (; i + 2 <= n; i += 2)
    {
        float *p = reinterpret_cast<float *>(&y[i]);
        _mm_storeu_ps(p, complexMultiplySSE2(_mm_loadu_ps(p), _mm_loadu_ps(reinterpret_cast<const float *>(&c[i]))));
    }
#endif
    for (; i < n; i++) {
        y[i] = complexMultiply(y[i], c[i]);
    }
}

// Equalise every carrier, e = y conj(h) derotate / |h|^2 (0 where h is), keeping |h|^2. A division per four
// carriers, rather than two per carrier
void OFDMDemod::equalise(const Cf *y, const Cf *h, Cf derotate, int n, Cf *e, float *power)
{
#if defined(MODEMDVB_X86)
    if (modemdvb::cpuHasAVX2())
    {
        equaliseAVX2(reinterpret_cast<const float *>(y), reinterpret_cast<const float *>(h), derotate.real(),
            derotate.imag(), n, reinterpret_cast<float *>(e), power);
        return;
    }
#endif
    int i = 0;
#ifdef DVBTOFDM_SSE2
    const __m128 dv = _mm_setr_ps(derotate.real(), derotate.imag(), derotate.real(), derotate.imag());
    const __m128 conjugate = _mm_castsi128_ps(_mm_set_epi32((int) 0x80000000, 0, (int) 0x80000000, 0));
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 zero = _mm_setzero_ps();

    for (; i + 4 <= n; i += 4)
    {
        const __m128 h01 = _mm_loadu_ps(reinterpret_cast<const float *>(&h[i]));
        const __m128 h23 = _mm_loadu_ps(reinterpret_cast<const float *>(&h[i + 2]));
        const __m128 y01 = _mm_loadu_ps(reinterpret_cast<const float *>(&y[i]));
        const __m128 y23 = _mm_loadu_ps(reinterpret_cast<const float *>(&y[i + 2]));
        const __m128 v01 = complexMultiplySSE2(complexMultiplySSE2(y01, _mm_xor_ps(h01, conjugate)), dv);
        const __m128 v23 = complexMultiplySSE2(complexMultiplySSE2(y23, _mm_xor_ps(h23, conjugate)), dv);
        const __m128 hh01 = _mm_mul_ps(h01, h01);
        const __m128 hh23 = _mm_mul_ps(h23, h23);
        const __m128 p = _mm_add_ps(_mm_shuffle_ps(hh01, hh23, _MM_SHUFFLE(2, 0, 2, 0)),
            _mm_shuffle_ps(hh01, hh23, _MM_SHUFFLE(3, 1, 3, 1))); // |h|^2 of carriers i..i+3
        const __m128 inverse = _mm_and_ps(_mm_div_ps(one, p), _mm_cmpgt_ps(p, zero));
        _mm_storeu_ps(reinterpret_cast<float *>(&e[i]), _mm_mul_ps(v01, _mm_unpacklo_ps(inverse, inverse)));
        _mm_storeu_ps(reinterpret_cast<float *>(&e[i + 2]), _mm_mul_ps(v23, _mm_unpackhi_ps(inverse, inverse)));
        _mm_storeu_ps(&power[i], p);
    }
#endif
    for (; i < n; i++)
    {
        const float p = std::norm(h[i]);
        const Cf v = complexMultiply(complexMultiply(y[i], std::conj(h[i])), derotate);
        e[i] = p > 0.0f ? v * (1.0f / p) : Cf(0.0f, 0.0f);
        power[i] = p;
    }
}

//! Unit magnitude, so that a strong narrowband interferer on one carrier can't dominate a sum over many
inline Cd OFDMDemod::unit(const Cf& v)
{
    float magnitude = std::abs(v);
    return magnitude > 0.0f ? Cd(v / magnitude) : Cd(0.0, 0.0);
}

void OFDMDemod::log(const char *format, ...)
{
    if (!m_log) {
        return;
    }

    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    m_log(message);
}

OFDMDemod::OFDMDemod(int bandwidthHz, FFTFactory *fftFactory) :
    m_sampleRate(sampleRate(bandwidthHz)),
    m_state(Acquire),
    m_mode(Mode::Mode8K),
    m_guard(GuardInterval::G1_32),
    m_tables(nullptr),
    m_fftSize(0),
    m_guardSize(0),
    m_symbolSize(0),
    m_kMax(0),
    m_backoff(0)
{
    m_fft.m_factory = fftFactory;
    m_cirFft.m_factory = fftFactory;
    reset();
}

OFDMDemod::~OFDMDemod()
{
    m_fft.release();
    m_cirFft.release();
}

void OFDMDemod::FFT::configure(int size, bool inverse)
{
    if (m_engine && (size == m_size) && (inverse == m_inverse)) {
        return;
    }

    release();

    if (m_factory)
    {
        m_sequence = m_factory->getEngine(size, inverse, &m_engine);
    }
    else
    {
        m_engine = FFTEngine::create(QString());
        m_engine->configure(size, inverse);
    }

    m_size = size;
    m_inverse = inverse;
}

void OFDMDemod::FFT::release()
{
    if (!m_engine) {
        return;
    }

    if (m_factory) {
        m_factory->releaseEngine(m_size, m_inverse, m_sequence);
    } else {
        delete m_engine;
    }

    m_engine = nullptr;
}

void OFDMDemod::reset()
{
    m_buffer.clear();
    m_bufferOffset = 0;
    m_symbolStart = 0;
    m_ncoFreq = 0.0;
    m_ncoPhase = 0.0;
    m_ncoIndex = 0;
    m_gaCorrelation = 0.0;
    m_clockOffset = 0.0;
    m_clockMeasurements = 0;
    m_symbolCount = 0;
    m_windowSteps = 0;
    m_acquisitions = 0;
    m_acquireFailures = 0;
    m_resyncs = 0;
    m_tpsValid = false;
    m_notchesChanged = true;
    startAcquire();
}

void OFDMDemod::startAcquire()
{
    m_state = Acquire;
    m_cirResolution = 0.0;
    m_acquisitions++;
    m_windowStep = 0;
    m_stateSymbols = 0;
    m_intCFOCandidate = 0;
    m_seq = -1;
    m_spPhase0 = 0;
    m_symbolIndex = -1;
    m_frameNumber = 0;
    m_delay = 0.0;
    m_spread = 0.0;
    m_strongest = 0.0;
    m_aliasCount = -1; // No measurement yet
    m_cirRejected = 0;
    m_tpsValid = false;
    m_tpsCandidateValid = false;
    m_tpsPendingValid = false;
    m_tpsFailures = 0;
    m_tpsSyncFailed = false;
    m_incoherentSymbols = 0;
    m_tpsBitsLost = false;
    m_tpsHistory = 0;
    m_tpsHistoryBits = 0;
    m_tpsRealign = 0;
    m_giAverage = 0.0;
    m_giLow = 0;
    m_resyncPending = false;
    m_resyncPhase = false;
    m_outputErased = false;
    m_cellIdBytes[0] = -1;
    m_cellIdBytes[1] = -1;
    m_prevTPSValid = false;
    m_noiseSymbols = 0;
    m_pilotNoise = -1.0;
}

void OFDMDemod::configureMode(Mode mode, GuardInterval guard)
{
    m_mode = mode;
    m_guard = guard;
    m_tables = &Tables::get(mode);
    m_fftSize = fftSize(mode);
    m_guardSize = guardSamples(mode, guard);
    m_symbolSize = m_fftSize + m_guardSize;
    m_kMax = kMax(mode);
    m_backoff = std::max(1, m_guardSize / 8);

    m_fft.configure(m_fftSize, false);

    // Impulse response from the pilots on every third carrier
    const int nbPilots = m_kMax / 3 + 1;
    int cirSize = 1;

    while (cirSize < nbPilots) {
        cirSize *= 2;
    }

    m_cirFft.configure(cirSize, true);
    m_cirWindow.resize(nbPilots);
    m_cirPower.resize(cirSize);
    m_cirPhasors.resize(3 * cirSize);
    m_cirModel.resize(m_tables->m_tpsCarriers.size());

    for (int n = 0; n < 3 * cirSize; n++) {
        m_cirPhasors[n] = Cf(std::polar(1.0, -m_twoPi * n / (3.0 * cirSize)));
    }

    for (int j = 0; j < nbPilots; j++) {
        m_cirWindow[j] = (float) (0.5 - 0.5 * std::cos(m_twoPi * (j + 0.5) / nbPilots)); // Hann
    }

    m_carrierBin.resize(m_kMax + 1);
    m_notchesChanged = true; // The carriers near them change with the mode

    for (int k = 0; k <= m_kMax; k++)
    {
        int i = k - m_kMax / 2;
        m_carrierBin[k] = i < 0 ? i + m_fftSize : i;
    }

    for (int i = 0; i < m_ringSize; i++) {
        m_ring[i].m_y.assign(m_kMax + 1, Cf(0.0f, 0.0f));
    }

    m_y.resize(m_kMax + 1);
    m_prevBins.assign(m_fftSize, Cf(0.0f, 0.0f));
    m_hp.resize(m_kMax / 3 + 1);
    m_pilotSign.resize(m_kMax / 3 + 1);

    for (int j = 0; j <= m_kMax / 3; j++) {
        m_pilotSign[j] = m_tables->m_prbs[3 * j] ? -1.0f / m_pilotBoost : 1.0f / m_pilotBoost;
    }

    m_hs.resize(m_kMax / 3 + 1);
    m_hi[0].resize(m_kMax / 3 + 1);
    m_hi[1].resize(m_kMax / 3 + 1);
    designSmoothingFilter(1.0);
    designInterpolationFilter();
    m_h.assign(m_kMax + 1, Cf(1.0f, 0.0f));
    m_cells.resize(m_tables->m_nbData);
    m_csi.resize(m_tables->m_nbData);
    m_equalised.resize(m_kMax + 1);
    m_power.resize(m_kMax + 1);
    m_noise.assign(m_kMax + 1 + 4, -1.0f);
    m_noiseSmoothed.assign(m_kMax + 1, 1.0f);
    m_noiseInverse.assign(m_kMax + 1, 1.0f);

    for (int p = 0; p < 4; p++)
    {
        m_dataMask[p].assign(m_kMax + 1, 0.0f);

        for (int k : m_tables->m_dataCarriers[p]) {
            m_dataMask[p][k] = 1.0f;
        }
    }
    m_noiseSymbols = 0;
    m_pilotNoise = -1.0;
    m_prevTPS.resize(m_tables->m_tpsCarriers.size());
}

void OFDMDemod::process(const Cf *samples, int nbSamples)
{
    m_buffer.insert(m_buffer.end(), samples, samples + nbSamples);

    while (true)
    {
        if (m_state == Acquire)
        {
            if (!acquire()) {
                break;
            }
        }
        else
        {
            if (m_resyncPending)
            {
                if (m_symbolStart + 3 * m_symbolSize + m_fftSize > m_bufferOffset + (int64_t) m_buffer.size()) {
                    break;
                }

                resync();
                continue;
            }

            if (m_symbolStart + m_symbolSize + 1 > m_bufferOffset + (int64_t) m_buffer.size()) {
                break;
            }

            processSymbol();
            m_symbolStart += m_symbolSize + m_windowStep;
            m_windowStep = 0;
        }
    }

    // Discard samples that have been used. A window step can put the next symbol a few samples past the end of the buffer
    int64_t used = std::min(m_symbolStart - m_bufferOffset - 1, (int64_t) m_buffer.size());

    if (used > m_bufferDiscard)
    {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + used);
        m_bufferOffset += used;
    }
}

// Find mode, guard interval, symbol timing and fractional frequency offset from the correlation
// between the guard interval and the end of the symbol it copies, folded over several symbols.
// Returns true if the state has changed or samples have been discarded
bool OFDMDemod::acquire()
{
    const int length = m_acquireSymbols * (8192 + 2048);

    if (m_bufferOffset + (int64_t) m_buffer.size() < m_symbolStart + length) {
        return false;
    }

    const Cf *r = bufferAt(m_symbolStart);
    double bestCorrelation = 0.0;
    Mode bestMode = Mode::Mode8K;
    GuardInterval bestGuard = GuardInterval::G1_32;
    int bestOffset = 0;
    double bestPhase = 0.0;

    for (Mode mode : {Mode::Mode2K, Mode::Mode8K})
    {
        const int n = fftSize(mode);
        const int len = length - n;
        std::vector<Cf> c(len);
        std::vector<float> power(len);

        for (int i = 0; i < len; i++)
        {
            c[i] = r[i] * std::conj(r[i + n]);
            power[i] = 0.5f * (std::norm(r[i]) + std::norm(r[i + n]));
        }

        for (GuardInterval guard : {GuardInterval::G1_32, GuardInterval::G1_16, GuardInterval::G1_8, GuardInterval::G1_4})
        {
            const int ng = guardSamples(mode, guard);
            const int ns = n + ng;
            const int nbSymbols = (len - ng) / ns;

            if (nbSymbols < 2) {
                continue;
            }

            std::vector<Cd> fold(ns, Cd(0.0, 0.0));
            std::vector<double> foldPower(ns, 0.0);
            Cd sum(0.0, 0.0);
            double sumPower = 0.0;

            for (int i = 0; i < ng; i++)
            {
                sum += Cd(c[i]);
                sumPower += power[i];
            }

            for (int i = 0, j = 0; i < nbSymbols * ns; i++)
            {
                fold[j] += sum;
                foldPower[j] += sumPower;
                sum += Cd(c[i + ng]) - Cd(c[i]);
                sumPower += power[i + ng] - power[i];

                if (++j == ns) {
                    j = 0;
                }
            }

            // A narrowband interferer correlates with itself at any lag, so adds the same to every position.
            // Remove that, as measured away from the peak, where the signal doesn't correlate, and its power
            Cd mean(0.0, 0.0);

            for (int i = 0; i < ns; i++) {
                mean += fold[i];
            }

            mean /= ns;
            int peak = 0;
            double peakMag = 0.0;

            for (int i = 0; i < ns; i++)
            {
                double mag = std::abs(fold[i] - mean);

                if (mag > peakMag)
                {
                    peakMag = mag;
                    peak = i;
                }
            }

            Cd bias(0.0, 0.0);
            int nbBias = 0;

            for (int i = 0; i < ns; i++)
            {
                int distance = std::abs(i - peak);
                distance = std::min(distance, ns - distance);

                if (distance > 2 * ng)
                {
                    bias += fold[i];
                    nbBias++;
                }
            }

            bias = nbBias > 0 ? bias / (double) nbBias : Cd(0.0, 0.0);
            Cd value = fold[peak] - bias;
            double signalPower = foldPower[peak] - std::abs(bias);
            double correlation = signalPower > 0.0 ? std::min(1.0, std::abs(value) / signalPower) : 0.0;

            if (correlation > bestCorrelation)
            {
                bestCorrelation = correlation;
                bestMode = mode;
                bestGuard = guard;
                bestOffset = peak;
                bestPhase = std::arg(value);
            }
        }
    }

    if (bestCorrelation < m_acquireThreshold)
    {
        if (++m_acquireFailures % 100 == 1) {
            log("Acquire: no signal, best guard interval correlation %.2f (%s %s)", bestCorrelation, toString(bestMode), toString(bestGuard));
        }

        m_symbolStart += length / 2;
        return true;
    }

    m_acquireFailures = 0;

    m_gaCorrelation = bestCorrelation;
    configureMode(bestMode, bestGuard);

    // The guard interval correlation has phase -2 pi epsilon, epsilon being the offset in carrier spacings
    double epsilon = -bestPhase / m_twoPi;
    m_symbolStart += bestOffset;
    m_ncoFreq = -m_twoPi * epsilon / m_fftSize;
    m_ncoPhase = 0.0;
    m_ncoIndex = m_symbolStart;
    m_state = IntegerCFO;
    m_stateSymbols = 0;
    log("Acquire: %s GI %s, correlation %.2f, fractional frequency offset %.1f Hz",
        toString(bestMode), toString(bestGuard), bestCorrelation, epsilon * m_sampleRate / m_fftSize);

    return true;
}

void OFDMDemod::processSymbol()
{
    // A timing jump: the guard interval no longer correlates with the end of the symbol at the window. Two
    // symbols in a row, so a burst of interference isn't mistaken for one
    if ((m_state == Locked) && !m_resyncPhase)
    {
        const double gi = guardCorrelation(m_symbolStart);

        if ((m_giAverage > 0.0) && (gi < m_giLossFraction * m_giAverage))
        {
            if (++m_giLow >= 2)
            {
                log("Resync: guard interval correlation %.2f, %.2f usually", gi, m_giAverage);
                m_giLow = 0;
                m_resyncPending = true;
                m_symbolCount++;
                ring(m_seq).m_valid = false; // The last symbol was misaligned too
                advance(false);
                return;
            }
        }
        else
        {
            m_giLow = 0;
            m_giAverage = m_giAverage <= 0.0 ? gi : (1.0 - m_giAveraging) * m_giAverage + m_giAveraging * gi;
        }
    }

    PROFILER_START()
    // Frequency correct the FFT window
    int64_t windowStart = m_symbolStart + m_guardSize - m_backoff;
    m_ncoPhase = std::fmod(m_ncoPhase + m_ncoFreq * (double) (windowStart - m_ncoIndex), m_twoPi);
    m_ncoIndex = windowStart;

    rotate(bufferAt(windowStart), m_fft.m_engine->in(), m_fftSize, m_ncoPhase, m_ncoFreq);
    m_fft.m_engine->transform();
    const Cf *bins = m_fft.m_engine->out();
    PROFILER_STOP("DVB-T OFDM NCO and FFT")
    m_symbolCount++;
    m_stateSymbols++;

    if (m_state == IntegerCFO)
    {
        findIntegerCFO(bins);
        std::copy(bins, bins + m_fftSize, m_prevBins.begin());
        return;
    }

    std::vector<Cf>& y = m_state == PilotSync ? m_y : ring(m_seq + 1).m_y;

    // Carriers 0..kMax / 2 - 1 are at the top of the FFT output (negative frequencies), the rest from bin 0
    // (as m_carrierBin)
    const int below = m_kMax / 2;
    std::copy(bins + m_fftSize - below, bins + m_fftSize, y.begin());
    std::copy(bins, bins + (m_kMax + 1 - below), y.begin() + below);

    // Undo notches of interferers, before the FFT. The carriers near them are found again when the notches or the
    // mode change, or the frequency offset moves
    if (!m_notched.empty())
    {
        const double offset = -m_ncoFreq * m_sampleRate / m_twoPi;

        if (m_notchesChanged || (std::abs(offset - m_notchOffset) > m_notchOffsetTolerance))
        {
            m_notched.prepare(m_kMax, m_sampleRate / m_fftSize, offset);
            m_notchOffset = offset;
            m_notchesChanged = false;
        }

        m_notched.compensate(y.data());
    }

    if (m_state == PilotSync)
    {
        findPilotPhase();
        return;
    }

    // After a timing jump, the scattered pilot phase of the first symbol says how many were lost (up to 3). It
    // belongs that many symbols on, after those lost
    if (m_resyncPhase)
    {
        m_resyncPhase = false;
        const int lost = (scatteredPilotPhase(y) - (m_spPhase0 + m_seq + 1) % 4 + 4) % 4;
        log("Resync: %d symbols lost, or 4n more", lost);

        if (lost > 0)
        {
            std::swap(ring(m_seq + 1 + lost).m_y, ring(m_seq + 1).m_y);

            for (int i = 0; i < lost; i++) {
                advance(false);
            }
        }
    }

    advance(true);
}

// The next symbol in the ring, which has its FFT output if valid, and output of the one in the middle
void OFDMDemod::advance(bool valid)
{
    m_seq++;

    // Not to overflow after weeks locked. It stays past the ring's first symbols, so they aren't treated as such again
    if (m_seq >= 2 * m_seqPeriod) {
        m_seq -= m_seqPeriod;
    }

    RingSymbol& r = ring(m_seq);
    r.m_spPhase = (m_spPhase0 + m_seq) % 4;
    r.m_rotation = 0.0;
    r.m_coherent = true;
    r.m_valid = valid;
    // By the offset as it was. From 0 before the first symbol
    r.m_drift = (m_seq >= 1 ? ring(m_seq - 1).m_drift : 0.0) + m_clockOffset * 1e-6 * m_symbolSize;

    if (valid && (m_seq >= 1) && ring(m_seq - 1).m_valid)
    {
        PROFILER_START()
        trackFrequency();
        PROFILER_STOP("DVB-T OFDM frequency tracking")

        if (m_state == Acquire) {
            return;
        }
    }

    if (m_seq >= m_ringSize - 1) {
        outputSymbol(m_seq - (m_ringSize - 1) / 2);
    }
}

// The channel of a symbol is estimated from those up to 3 either side, so it's output if they were all received
void OFDMDemod::outputSymbol(int t)
{
    const int half = (m_ringSize - 1) / 2;

    for (int d = -half; d <= half; d++)
    {
        if (!ring(t + d).m_valid)
        {
            skipSymbol();
            return;
        }
    }

    estimateChannel(t);
}

// A symbol that couldn't be received: erasures for the FEC, so it stays aligned, and the frame position moves on
void OFDMDemod::skipSymbol()
{
    m_prevTPSValid = false;
    m_outputErased = true;
    decodeTPSBit(0, false);
    m_outputErased = false;
}

// Normalised correlation of a symbol's guard interval with the end of the symbol it copies
double OFDMDemod::guardCorrelation(int64_t start) const
{
    const Cf *r = bufferAt(start);
    const Cf *s = r + m_fftSize;
    double re = 0.0, im = 0.0, e1 = 0.0, e2 = 0.0;

    for (int i = 0; i < m_guardSize; i++)
    {
        const float ar = r[i].real(), ai = r[i].imag(), br = s[i].real(), bi = s[i].imag();
        re += ar * br + ai * bi;
        im += ai * br - ar * bi;
        e1 += ar * ar + ai * ai;
        e2 += br * br + bi * bi;
    }

    return (e1 > 0.0) && (e2 > 0.0) ? std::sqrt(re * re + im * im) / std::sqrt(e1 * e2) : 0.0;
}

// The scattered pilot phase (symbol index mod 4) of a symbol's FFT output, from the correlation of pilots 12 carriers
// apart, which is coherent whatever the channel at the right phase
int OFDMDemod::scatteredPilotPhase(const std::vector<Cf>& y) const
{
    const std::vector<uint8_t>& prbs = m_tables->m_prbs;
    int best = 0;
    double bestMag = -1.0;

    for (int p = 0; p < 4; p++)
    {
        Cd acc(0.0, 0.0);

        for (int k = 3 * p; k + 12 <= m_kMax; k += 12)
        {
            Cf v = y[k + 12] * std::conj(y[k]);
            acc += (prbs[k] ^ prbs[k + 12]) ? -unit(v) : unit(v);
        }

        const double mag = std::abs(acc);

        if (mag > bestMag)
        {
            bestMag = mag;
            best = p;
        }
    }

    return best;
}

// Find the symbol timing again after a jump: the guard interval correlation over a symbol period of start offsets,
// over two symbols, peaks where the guard intervals now start, of the strongest path (the paths' correlations add,
// weighted by their power). The window is moved there, less where the strongest path's guard interval started
// relative to the window as it was, so the timing is as tracked. Small offsets are left to the timing loop. If
// there's no correlation, the signal has gone
void OFDMDemod::resync()
{
    m_resyncPending = false;
    const int S = m_symbolSize, N = m_fftSize, G = m_guardSize;
    const Cf *r = bufferAt(m_symbolStart);
    std::vector<Cf> c(2 * S + G);
    std::vector<float> p(2 * S + G);

    for (int n = 0; n < 2 * S + G; n++)
    {
        c[n] = r[n] * std::conj(r[n + N]);
        p[n] = 0.5f * (std::norm(r[n]) + std::norm(r[n + N]));
    }

    Cd sum(0.0, 0.0);
    double power = 0.0;
    std::vector<Cd> sums(2 * S);
    std::vector<double> powers(2 * S);

    for (int n = 0; n < G; n++)
    {
        sum += Cd(c[n]);
        power += p[n];
    }

    for (int o = 0; o < 2 * S; o++)
    {
        sums[o] = sum;
        powers[o] = power;
        sum += Cd(c[o + G]) - Cd(c[o]);
        power += p[o + G] - p[o];
    }

    int best = 0;
    double bestMetric = -1.0, bestCorrelation = 0.0;

    for (int o = 0; o < S; o++)
    {
        const double metric = std::abs(sums[o]) + std::abs(sums[o + S]);

        if (metric > bestMetric)
        {
            bestMetric = metric;
            best = o;
            bestCorrelation = metric / std::max(powers[o] + powers[o + S], 1e-30);
        }
    }

    if (bestCorrelation < m_giLossFraction * m_giAverage)
    {
        log("Resync: no guard interval correlation (%.2f): acquiring", bestCorrelation);
        startAcquire();
        return;
    }

    const int jump = ((int) std::lround(best - (m_delay + m_strongest - m_backoff)) % S + S) % S;

    if ((jump <= G / 8) || (jump >= S - G / 8))
    {
        log("Resync: timing hasn't moved (%d samples)", jump);
        return;
    }

    m_symbolStart += jump;
    m_resyncPhase = true;
    m_tpsRealign = 2 * symbolsPerFrame;
    m_resyncs++;
    log("Resync: timing jumped %d samples (correlation %.2f)", jump, bestCorrelation);
}

// Continual pilots have the same value in every symbol, so correlating them between consecutive
// symbols is coherent whatever the channel, but only at the right whole-carrier offset
void OFDMDemod::findIntegerCFO(const Cf *bins)
{
    if (m_stateSymbols < 2) {
        return;
    }

    const int range = m_fftSize / 8;
    int best = 0;
    double bestCorrelation = 0.0;

    for (int s = -range; s <= range; s++)
    {
        Cd acc(0.0, 0.0);

        for (int k : m_tables->m_continualPilots)
        {
            int idx = (m_carrierBin[k] + s + m_fftSize) % m_fftSize;
            acc += unit(bins[idx] * std::conj(m_prevBins[idx]));
        }

        double correlation = std::abs(acc) / m_tables->m_continualPilots.size();

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            best = s;
        }
    }

    if (bestCorrelation >= m_intCFOThreshold)
    {
        if ((m_stateSymbols > 2) && (best == m_intCFOCandidate))
        {
            log("IntegerCFO: %d carriers, correlation %.2f, total frequency offset %.1f Hz",
                best, bestCorrelation, (-m_ncoFreq / m_twoPi + (double) best / m_fftSize) * m_sampleRate);
            m_ncoFreq -= m_twoPi * best / m_fftSize;
            m_state = PilotSync;
            m_stateSymbols = 0;
            m_lastSPPhase = -1;
            m_spPhaseRun = 0;
            return;
        }

        m_intCFOCandidate = best;
    }

    if (m_stateSymbols > 10)
    {
        log("IntegerCFO: failed, best correlation %.2f at %d carriers", bestCorrelation, best);
        startAcquire();
    }
}

// Adjacent scattered pilots 12 carriers apart have known values, so correlating them is coherent
// for the right pilot phase, if the channel varies little over 12 carriers
void OFDMDemod::findPilotPhase()
{
    const int best = scatteredPilotPhase(m_y);

    if ((m_lastSPPhase >= 0) && (best == (m_lastSPPhase + 1) % 4)) {
        m_spPhaseRun++;
    } else {
        m_spPhaseRun = 0;
    }

    m_lastSPPhase = best;

    if (m_spPhaseRun >= 3)
    {
        log("PilotSync: scattered pilot phase found");
        m_state = TPSSync;
        m_stateSymbols = 0;
        m_spPhase0 = (best + 1) % 4; // Phase of the next symbol, which will be m_seq 0
        m_seq = -1;
        m_symbolIndex = -1;
        m_tpsSync = 0;
        m_prevTPSValid = false;
        m_delay = m_backoff;
        m_spread = 0.0;
        m_strongest = 0.0;
        m_aliasCount = -1;
        m_cirRejected = 0;
    }
    else if (m_stateSymbols > 20)
    {
        log("PilotSync: no consistent scattered pilot phase");
        startAcquire();
    }
}

// Also aligns the phase of the symbol with the previous one. Phase noise of the receiver's oscillator would
// otherwise vary the phase from symbol to symbol, which would make the pilots that channel estimation
// interpolates between in time inconsistent, and so add an error that varies from carrier to carrier
void OFDMDemod::trackFrequency()
{
    // After symbols whose continual pilots didn't correlate (as with impulse noise), against the last that did, so
    // the phase reference carries over them
    int reference = m_seq - 1;

    if (!ring(reference).m_coherent)
    {
        for (int seq = m_seq - 2; seq >= std::max(0, m_seq - (m_ringSize - 1)); seq--)
        {
            if (!ring(seq).m_valid) {
                break;
            }

            if (ring(seq).m_coherent)
            {
                reference = seq;
                break;
            }
        }
    }

    std::vector<Cf>& y = ring(m_seq).m_y;
    const std::vector<Cf>& yp = ring(reference).m_y;
    const int centre = m_kMax / 2;
    Cd sum(0.0, 0.0), lower(0.0, 0.0), upper(0.0, 0.0);
    double kLower = 0.0, kUpper = 0.0, magnitudes = 0.0;
    int nLower = 0, nUpper = 0;

    for (int k : m_tables->m_continualPilots)
    {
        Cd v = unit(y[k] * std::conj(yp[k]));
        sum += v;
        magnitudes += std::abs(v);

        if (k < centre)
        {
            lower += v;
            kLower += k;
            nLower++;
        }
        else if (k > centre)
        {
            upper += v;
            kUpper += k;
            nUpper++;
        }
    }

    // The continual pilots are the same in every symbol, so stop correlating if the signal is lost
    double coherence = magnitudes > 0.0 ? std::abs(sum) / magnitudes : 0.0;
    double rotation = std::arg(sum);

    if (coherence < 0.3)
    {
        // Still aligned as well as it can be, and later symbols aligned with the last that did correlate, so the
        // symbols either side, which the channel estimate interpolates between, keep their phase reference
        scale(y.data(), m_kMax + 1, Cf(std::polar(1.0, -rotation)));
        ring(m_seq).m_rotation = rotation;
        ring(m_seq).m_coherent = false;

        if (++m_incoherentSymbols >= 8)
        {
            log("Signal lost: continual pilots no longer correlate");
            startAcquire();
        }

        return; // Don't let noise disturb the loops
    }

    m_incoherentSymbols = 0;

    // The previous symbol has been rotated by its m_rotation, so allow for that to get the change of phase
    // of the signal itself
    double change = std::remainder(rotation - ring(m_seq - 1).m_rotation, m_twoPi);
    scale(y.data(), m_kMax + 1, Cf(std::polar(1.0, -rotation)));

    ring(m_seq).m_rotation = rotation;

    if (reference != m_seq - 1) {
        return; // Over more than a symbol: the loops are updated from the next
    }

    // Residual frequency offset in carrier spacings rotates each symbol by 2 pi epsilon Ns / N
    double epsilon = change * m_fftSize / (m_twoPi * m_symbolSize);
    m_ncoFreq -= m_frequencyLoopGain * m_twoPi * epsilon / m_fftSize;

    // A sampling clock offset rotates each carrier in proportion to its index
    double spread = kUpper / nUpper - kLower / nLower;
    // averaged over the first 100 symbols, then smoothed, so it's soon known for channel estimation
    double clockOffset = std::arg(upper * std::conj(lower)) * m_fftSize / (m_twoPi * spread * m_symbolSize);
    m_clockMeasurements = std::min(100, m_clockMeasurements + 1);
    const double weight = 1.0 / m_clockMeasurements;
    m_clockOffset = (1.0 - weight) * m_clockOffset + weight * clockOffset * 1e6;
}

void OFDMDemod::applyWindowStep(int step)
{
    // Make the stored symbols look as if they had been taken with the new window position
    m_windowStep += step;
    m_windowSteps++;
    m_delay -= step;

    // The same phasors for each symbol, so made once
    Cd rotStep = std::polar(1.0, m_twoPi * step / m_fftSize);
    Cd rot = std::polar(1.0, -m_twoPi * step * (m_kMax / 2) / m_fftSize);
    m_stepPhasors.resize(m_kMax + 1);

    for (int k = 0; k <= m_kMax; k++)
    {
        m_stepPhasors[k] = Cf(rot);
        rot *= rotStep;
    }

    for (int seq = std::max(0, m_seq - (m_ringSize - 1)); seq <= m_seq; seq++) {
        multiply(ring(seq).m_y.data(), m_stepPhasors.data(), m_kMax + 1);
    }
}

// With the delay of the middle of the impulse response removed, the paths make the channel vary across the
// pilots (every third carrier) at no more than 3 (spread / 2) / N cycles per pilot, so noise above that can
// be filtered out. The cutoff allows for the transition band of the filter
void OFDMDemod::designSmoothingFilter(double cutoff)
{
    m_smoothingCutoff = cutoff;
    m_smoothingTaps.clear();

    if (cutoff >= 0.4) {
        return; // Little to gain
    }

    const int halfLength = m_smoothingHalfLength;
    double sum = 0.0;

    for (int i = -halfLength; i <= halfLength; i++)
    {
        double x = 2.0 * cutoff * i;
        double sinc = i == 0 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
        double window = 0.42 + 0.5 * std::cos(M_PI * i / (halfLength + 1)) + 0.08 * std::cos(2.0 * M_PI * i / (halfLength + 1));
        m_smoothingTaps.push_back((float) (sinc * window));
        sum += sinc * window;
    }

    for (float& tap : m_smoothingTaps) {
        tap /= (float) sum;
    }
}

// Windowed sinc interpolation by 3, as linear interpolation fails for echoes approaching the guard interval
void OFDMDemod::designInterpolationFilter()
{
    const int m = m_interpolationHalfLength;
    const double windowLength = 3.0 * (m + 1);

    for (int r = 0; r < 3; r++)
    {
        std::vector<float>& taps = m_interpolationTaps[r];
        taps.resize(2 * m + 1);
        double sum = 0.0;

        for (int i = -m; i <= m; i++)
        {
            double d = r - 3.0 * i; // Carriers from the pilot
            double x = d / 3.0;
            double sinc = std::abs(x) < 1e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            double window = 0.42 + 0.5 * std::cos(M_PI * d / windowLength) + 0.08 * std::cos(2.0 * M_PI * d / windowLength);
            taps[i + m] = (float) (sinc * window);
            sum += sinc * window;
        }

        for (float& tap : taps) {
            tap /= (float) sum;
        }
    }
}

// Convolve complex x (n values) with real taps w (2 * half + 1), centred, into y. Away from the ends, outputs are
// made a block at a time, accumulating tap by tap in a small array, which vectorises and stays in registers.
// Symmetric taps (w[half - i] == w[half + i]) are folded, halving the multiplications. Near the ends, renormalise
// for the taps used. With AVX2 and FMA, a kernel does most of the interior, unless done (>= 0) says how many floats
// of it have been already
void OFDMDemod::convolveCentred(const Cf *x, int n, const float *w, int half, Cf *y, bool symmetric, int done)
{
    const int interiorStart = std::min(half, n);
    const int interiorEnd = std::max(interiorStart, n - half); // Exclusive
    float *yf = reinterpret_cast<float *>(y);
    const float *xf = reinterpret_cast<const float *>(x);
    const int end = 2 * interiorEnd;
    const float *wc = w + half; // Centre tap
    static const int block = 16; // Floats
    int f = 2 * interiorStart;

    if (done >= 0) {
        f += done;
    }
#if defined(MODEMDVB_X86)
    else if (modemdvb::cpuHasAVX2()) {
        f = convolveAVX2(xf, wc, half, symmetric, f, end, yf);
    }
#endif

    for (; f + block <= end; f += block)
    {
        const float *xc = xf + f;
        float acc[block];

        for (int b = 0; b < block; b++) {
            acc[b] = wc[0] * xc[b];
        }

        if (symmetric)
        {
            for (int i = 1; i <= half; i++)
            {
                const float wi = wc[i];
                const float *lo = xc - 2 * i;
                const float *hi = xc + 2 * i;

                for (int b = 0; b < block; b++) {
                    acc[b] += wi * (lo[b] + hi[b]);
                }
            }
        }
        else
        {
            for (int i = -half; i <= half; i++)
            {
                if (i == 0) {
                    continue;
                }

                const float wi = wc[i];
                const float *xi = xc + 2 * i;

                for (int b = 0; b < block; b++) {
                    acc[b] += wi * xi[b];
                }
            }
        }

        std::copy(acc, acc + block, yf + f);
    }

    for (; f < end; f++)
    {
        float acc = 0.0f;

        for (int i = -half; i <= half; i++) {
            acc += wc[i] * xf[f + 2 * i];
        }

        yf[f] = acc;
    }

    for (int j = 0; j < n; j++)
    {
        if ((j >= interiorStart) && (j < interiorEnd)) {
            j = interiorEnd - 1;
            continue;
        }

        const int first = std::max(0, j - half);
        const int last = std::min(n - 1, j + half);
        float re = 0.0f, im = 0.0f, sum = 0.0f;

        for (int i = first; i <= last; i++)
        {
            const float wi = w[i - j + half];
            re += xf[2 * i] * wi;
            im += xf[2 * i + 1] * wi;
            sum += wi;
        }

        y[j] = Cf(re / sum, im / sum);
    }
}

void OFDMDemod::smoothChannel()
{
    if (m_smoothingTaps.empty()) {
        std::copy(m_hp.begin(), m_hp.end(), m_hs.begin());
    } else {
        convolveCentred(m_hp.data(), (int) m_hp.size(), m_smoothingTaps.data(), m_smoothingHalfLength, m_hs.data(), true);
    }
}

// A narrowband interferer on a pilot carrier would spread across the channel estimate through the smoothing,
// so replace pilots much stronger than the median, which the channel alone is very unlikely to make them,
// with the average of their nearest neighbours that aren't
void OFDMDemod::erasePilotOutliers()
{
    const int nbPilots = (int) m_hp.size();
    m_pilotPower.resize(nbPilots);

    for (int j = 0; j < nbPilots; j++) {
        m_pilotPower[j] = std::norm(m_hp[j]);
    }

    // The median of every fourth pilot's power is near enough, for a quarter of the work
    m_pilotSorted.clear();

    for (int j = 0; j < nbPilots; j += 4) {
        m_pilotSorted.push_back(m_pilotPower[j]);
    }

    const size_t middle = m_pilotSorted.size() / 2;
    std::nth_element(m_pilotSorted.begin(), m_pilotSorted.begin() + middle, m_pilotSorted.end());
    const float threshold = 16.0f * m_pilotSorted[middle];
    m_pilotErased.assign(nbPilots, 0);
    bool any = false;

    for (int j = 0; j < nbPilots; j++)
    {
        if (m_pilotPower[j] > threshold)
        {
            m_pilotErased[j] = 1;
            any = true;
        }
    }

    if (!any) {
        return;
    }

    for (int j = 0; j < nbPilots; j++)
    {
        if (!m_pilotErased[j]) {
            continue;
        }

        Cf sum(0.0f, 0.0f);
        int n = 0;

        for (int d = 1; (d <= 4) && (n == 0); d++)
        {
            if ((j - d >= 0) && !m_pilotErased[j - d])
            {
                sum += m_hp[j - d];
                n++;
            }

            if ((j + d < nbPilots) && !m_pilotErased[j + d])
            {
                sum += m_hp[j + d];
                n++;
            }
        }

        m_hp[j] = n > 0 ? sum / (float) n : Cf(0.0f, 0.0f);
    }
}

// The impulse response, from an IFFT of the pilots on every third carrier, has a resolution of
// N / (3 * size) samples, but repeats every N / 3 samples. Paths within 20 dB of the strongest are significant.
// Each gap between them could be where the response starts, and the whole response could be N / 3 earlier
// or later. These are told apart by the TPS carriers, which aren't on every third carrier and are always
// real (+/-1): with the right hypothesis, the channel modelled from the paths equalises them to the real axis
bool OFDMDemod::measureImpulseResponse(int t, double& firstPath, double& spread, double& strongest)
{
    const int nbPilots = (int) m_hp.size();
    const int size = m_cirFft.m_size;
    Cf *in = m_cirFft.m_engine->in();

#if defined(MODEMDVB_X86)
    if (modemdvb::cpuHasAVX2()) {
        windowAVX2(reinterpret_cast<const float *>(m_hp.data()), m_cirWindow.data(), nbPilots, reinterpret_cast<float *>(in));
    } else
#endif
    {
        for (int j = 0; j < nbPilots; j++) {
            in[j] = m_hp[j] * m_cirWindow[j];
        }
    }

    std::fill(in + nbPilots, in + size, Cf(0.0f, 0.0f));
    m_cirFft.m_engine->transform();
    const float *out = reinterpret_cast<const float *>(m_cirFft.m_engine->out());
    float peak = 0.0f;
#if defined(MODEMDVB_X86)
    const bool avx2 = modemdvb::cpuHasAVX2();

    if (avx2) {
        peak = powerAVX2(out, size, m_cirPower.data());
    } else
#endif
    {
        float peaks[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Independent, so they don't wait for each other

        for (int b = 0; b < size; b++)
        {
            const float power = out[2 * b] * out[2 * b] + out[2 * b + 1] * out[2 * b + 1];
            m_cirPower[b] = power;
            peaks[b & 3] = std::max(peaks[b & 3], power);
        }

        peak = std::max(std::max(peaks[0], peaks[1]), std::max(peaks[2], peaks[3]));
    }

    if (peak <= 0.0f) {
        return false;
    }

    m_cirResolution = (double) m_fftSize / (3.0 * size);
    const float threshold = peak * 0.01f;
    m_cirBins.clear();
#if defined(MODEMDVB_X86)
    if (avx2)
    {
        if (m_cirAbove.size() < (size_t) size) {
            m_cirAbove.resize(size);
        }

        const int count = findAboveAVX2(m_cirPower.data(), size, threshold, m_cirAbove.data());
        m_cirBins.assign(m_cirAbove.begin(), m_cirAbove.begin() + count);
    }
    else
#endif
    {
        for (int b = 0; b < size; b++)
        {
            if (m_cirPower[b] >= threshold) {
                m_cirBins.push_back(b);
            }
        }
    }

    // Limit the cost, keeping the strongest
    const size_t maxBins = 64;

    if (m_cirBins.size() > maxBins)
    {
        std::nth_element(m_cirBins.begin(), m_cirBins.begin() + maxBins, m_cirBins.end(),
            [this](int a, int b) { return m_cirPower[a] > m_cirPower[b]; });
        m_cirBins.resize(maxBins);
        std::sort(m_cirBins.begin(), m_cirBins.end());
    }

    const double resolution = (double) m_fftSize / (3.0 * size);
    const int nbBins = (int) m_cirBins.size();
    prepareHypotheses();
    double bestMetric = -1.0;
    int bestStart = 0;
    int bestAlias = 0;
    int bestSpread = 0;
    int widest = 0;     // The largest gap follows this path: the hypothesis of least spread
    int widestGap = -1;

    for (int i = 0; i < nbBins; i++)
    {
        int next = m_cirBins[(i + 1) % nbBins];
        int gap = nbBins == 1 ? size : (next - m_cirBins[i] + size) % size;
        int spreadBins = size - gap;

        if (gap > widestGap)
        {
            widestGap = gap;
            widest = i;
        }

        if ((nbBins > 1) && (spreadBins * resolution > 1.1 * m_guardSize)) {
            continue; // Too long to be free of inter-symbol interference
        }

        for (int alias = -1; alias <= 1; alias++)
        {
            double metric = hypothesisMetric(t, (i + 1) % nbBins, alias);

            if (metric > bestMetric)
            {
                bestMetric = metric;
                bestStart = next;
                bestAlias = alias;
                bestSpread = spreadBins;
            }
        }
    }

    // The paths span more than the guard interval (a long echo, or an SFN), so there's inter-symbol interference
    // whatever the timing. Once that persists, take the least spread, the alias from the TPS carriers as usual, so
    // the smoothing keeps every path and the timing is still tracked
    if (bestMetric < 0.0)
    {
        m_cirRejected = std::min(m_cirRejected + 1, 2 * m_cirRejectSymbols);

        if (m_cirRejected < m_cirRejectSymbols) {
            return false;
        }

        const int startIndex = (widest + 1) % nbBins;

        for (int alias = -1; alias <= 1; alias++)
        {
            double metric = hypothesisMetric(t, startIndex, alias);

            if (metric > bestMetric)
            {
                bestMetric = metric;
                bestStart = m_cirBins[startIndex];
                bestAlias = alias;
                bestSpread = size - widestGap;
            }
        }
    }
    else if (m_cirRejected > 0)
    {
        m_cirRejected--;
    }

    firstPath = bestStart * resolution + bestAlias * (m_fftSize / 3.0);
    spread = bestSpread * resolution;
    int strongestBin = bestStart;

    for (int b : m_cirBins)
    {
        if (m_cirPower[b] > m_cirPower[strongestBin]) {
            strongestBin = b;
        }
    }

    strongest = ((strongestBin - bestStart + size) % size) * resolution;

    return true;
}

// The channel at TPS carrier k, modelled from the significant paths starting at bin start, moved by alias * N / 3,
// is the sum over paths b of out[b] * exp(-j 2 pi k position / (3 * size)), where the position is b, plus size for
// the paths before the start (the response wraps around), plus alias * size. So it is exp(-j 2 pi k alias / 3)
// times the sum over the paths from the start, plus exp(-j 2 pi k / 3) times the sum over those before. With prefix
// sums over the paths (sorted by bin), each hypothesis then costs a few operations per TPS carrier, rather than
// one per path, which matters when there are many paths
void OFDMDemod::prepareHypotheses()
{
    const int size = m_cirFft.m_size;
    const int period = 3 * size;
    const Cf *out = m_cirFft.m_engine->out();
    const std::vector<int>& tps = m_tables->m_tpsCarriers;
    const int nbTPS = (int) tps.size();
    const int nbBins = (int) m_cirBins.size();
    m_cirPrefix.resize((size_t) nbTPS * (nbBins + 1));
    m_cirWrap.resize(nbTPS);
    m_cirAlias.resize(3 * nbTPS);

    for (int i = 0; i < nbTPS; i++)
    {
        const int k = tps[i];
        Cf *prefix = &m_cirPrefix[(size_t) i * (nbBins + 1)];
        Cf sum(0.0f, 0.0f);
        prefix[0] = sum;

        for (int q = 0; q < nbBins; q++)
        {
            const int b = m_cirBins[q];
            sum += out[b] * m_cirPhasors[(k * b) % period];
            prefix[q + 1] = sum;
        }

        const int wrap = (k * size) % period;
        m_cirWrap[i] = m_cirPhasors[wrap];
        m_cirAlias[3 * i] = m_cirPhasors[(period - wrap) % period];
        m_cirAlias[3 * i + 1] = Cf(1.0f, 0.0f);
        m_cirAlias[3 * i + 2] = m_cirPhasors[wrap];
    }
}

// How well the channel, modelled from the significant paths starting at m_cirBins[startIndex], moved by
// alias * N / 3, equalises the TPS carriers to the real axis. 1 is perfect
double OFDMDemod::hypothesisMetric(int t, int startIndex, int alias)
{
    const std::vector<int>& tps = m_tables->m_tpsCarriers;
    const std::vector<Cf>& y = ring(t).m_y;
    const int nbBins = (int) m_cirBins.size();
    Cd sumSquares(0.0, 0.0);
    double sumPower = 0.0;

    for (size_t i = 0; i < tps.size(); i++)
    {
        const Cf *prefix = &m_cirPrefix[i * (nbBins + 1)];
        const Cf fromStart = prefix[nbBins] - prefix[startIndex];
        const Cf h = (fromStart + m_cirWrap[i] * prefix[startIndex]) * m_cirAlias[3 * i + alias + 1];
        Cf z = y[tps[i]] * std::conj(h);
        sumSquares += Cd(z * z);
        sumPower += std::norm(z);
    }

    return sumPower > 0.0 ? std::abs(sumSquares) / sumPower : 0.0;
}

void OFDMDemod::estimateChannel(int t)
{
    const std::vector<uint8_t>& prbs = m_tables->m_prbs;
    const int phase = ring(t).m_spPhase;
    const int nbPilots = m_kMax / 3 + 1;
    PROFILER_START()

    // Scattered pilots: interpolate in time to every third carrier. Pilot j was last on its carrier d symbols back,
    // d = (phase - j) mod 4, and is next 4 - d symbols on
    const Cf *last[4], *next[4];
    float w0[4], w1[4];

    for (int q = 0; q < 4; q++)
    {
        const int d = (phase - q + 4) & 3;
        last[q] = ring(t - d).m_y.data();
        next[q] = ring(t - d + 4).m_y.data();
        w1[q] = d / 4.0f;
        w0[q] = 1.0f - w1[q];
    }

    const float *sign = m_pilotSign.data();

    // A clock offset moves the symbols by its fraction of a symbol each, which turns carrier k (from the centre) by
    // 2 pi k offset Ns / N a symbol: at the edges in 8K, 0.66 radians a symbol at 30 ppm, too much to interpolate
    // linearly over 4 symbols. So the pilots are first turned to the time of the symbol being estimated, by the
    // drift between them, from the offset measured from the continual pilots when each was received (which changes
    // when the receiver corrects the clock), unless it's too small to matter
    double driftLast[4], driftNext[4], largest = 0.0;

    for (int q = 0; q < 4; q++)
    {
        const int d = (phase - q + 4) & 3;
        driftLast[q] = m_twoPi * (ring(t).m_drift - ring(t - d).m_drift) / m_fftSize;      // Radians a carrier
        driftNext[q] = m_twoPi * (ring(t).m_drift - ring(t - d + 4).m_drift) / m_fftSize;
        largest = std::max(largest, std::max(std::abs(driftLast[q]), std::abs(driftNext[q])));
    }

    if (largest * m_kMax < 0.01)
    {
        for (int j = 0; j < nbPilots; j++)
        {
            const int q = j & 3;
            const int k = 3 * j;
            const Cf a = last[q][k], b = next[q][k];
            const float u = w0[q] * sign[j], v = w1[q] * sign[j];
            m_hp[j] = Cf(u * a.real() + v * b.real(), u * a.imag() + v * b.imag());
        }
    }
    else
    {
        const int centre = m_kMax / 2;

        for (int q = 0; q < 4; q++)
        {
            // Every fourth pilot is 12 carriers on
            Cd turnLast = std::polar(1.0, driftLast[q] * (3 * q - centre));
            Cd turnNext = std::polar(1.0, driftNext[q] * (3 * q - centre));
            const Cd stepLast = std::polar(1.0, driftLast[q] * 12.0);
            const Cd stepNext = std::polar(1.0, driftNext[q] * 12.0);

            for (int j = q; j < nbPilots; j += 4)
            {
                const int k = 3 * j;
                const Cf a = complexMultiply(last[q][k], Cf(turnLast));
                const Cf b = complexMultiply(next[q][k], Cf(turnNext));
                const float u = w0[q] * sign[j], v = w1[q] * sign[j];
                m_hp[j] = Cf(u * a.real() + v * b.real(), u * a.imag() + v * b.imag());
                turnLast *= stepLast;
                turnNext *= stepNext;
            }
        }
    }

    erasePilotOutliers();
    PROFILER_STOP("DVB-T OFDM pilots")
    PROFILER_RESTART()

    // First path and spread, from the impulse response
    double firstPath, spread, strongest;

    if (measureImpulseResponse(t, firstPath, spread, strongest))
    {
        if (m_aliasCount < 0)
        {
            m_delay = firstPath;
            m_spread = spread;
            m_strongest = strongest;
            m_aliasCount = 0;
        }
        else if (std::abs(firstPath - m_delay) > m_fftSize / 6.0)
        {
            // A different interpretation of the impulse response: accept it if it persists
            if (++m_aliasCount >= 8)
            {
                m_delay = firstPath;
                m_spread = spread;
                m_strongest = strongest;
                m_aliasCount = 0;
            }
        }
        else
        {
            m_aliasCount = 0;
            m_delay = (1.0 - m_delaySmoothing) * m_delay + m_delaySmoothing * firstPath;
            m_spread = (1.0 - m_delaySmoothing) * m_spread + m_delaySmoothing * spread;
            m_strongest = (1.0 - m_delaySmoothing) * m_strongest + m_delaySmoothing * strongest;
        }
    }

    PROFILER_STOP("DVB-T OFDM impulse response")
    PROFILER_RESTART()

    // Interpolate in frequency, with the phase slope of the middle of the impulse response removed,
    // so the channel varies as little as possible from pilot to pilot
    double delay = m_delay + m_spread / 2.0;
    rotate(m_hp.data(), m_hp.data(), nbPilots, 0.0, m_twoPi * 3.0 * delay / m_fftSize);

    // Passband to the furthest path from the middle, plus a margin, plus half the transition band of the filter
    const double transition = 5.5 / (2 * m_smoothingHalfLength + 1);
    double cutoff = 3.0 * (m_spread / 2.0 + m_guardSize / 16.0) / m_fftSize + transition / 2.0;
    cutoff = std::round(cutoff * 200.0) / 200.0;

    if (cutoff != m_smoothingCutoff) {
        designSmoothingFilter(cutoff);
    }

    smoothChannel();

    // Values 1 and 2 carriers after each pilot. With AVX2, the interior of both at once, sharing the loads
    const int half = m_interpolationHalfLength;
    int done = 0;
#if defined(MODEMDVB_X86)
    if (modemdvb::cpuHasAVX2())
    {
        const int start = 2 * std::min(half, nbPilots);
        const int end = std::max(start, 2 * (nbPilots - half));
        done = convolve2AVX2(reinterpret_cast<const float *>(m_hs.data()), m_interpolationTaps[1].data() + half,
            m_interpolationTaps[2].data() + half, half, start, end, reinterpret_cast<float *>(m_hi[0].data()),
            reinterpret_cast<float *>(m_hi[1].data())) - start;
    }
#endif

    for (int r = 1; r <= 2; r++) {
        convolveCentred(m_hs.data(), nbPilots, m_interpolationTaps[r].data(), half, m_hi[r - 1].data(), false, done);
    }

    for (int j = 0; j < nbPilots; j++)
    {
        const int k = 3 * j;
        m_h[k] = m_hs[j];

        if (k + 2 <= m_kMax)
        {
            m_h[k + 1] = m_hi[0][j];
            m_h[k + 2] = m_hi[1][j];
        }
    }

    rotate(m_h.data(), m_h.data(), m_kMax + 1, 0.0, -m_twoPi * delay / m_fftSize);

    PROFILER_STOP("DVB-T OFDM smoothing and interpolation")
    PROFILER_RESTART()

    // Common phase error, from the continual pilots
    const std::vector<Cf>& y = ring(t).m_y;
    Cd cpe(0.0, 0.0);

    for (int k : m_tables->m_continualPilots)
    {
        Cf v = y[k] * std::conj(m_h[k]);
        cpe += prbs[k] ? -unit(v) : unit(v);
    }

    Cf derotate = Cf(std::polar(1.0, -std::arg(cpe)));

    // Equalise every carrier, then pick out the data cells. The reliability of each is its signal to noise ratio,
    // relative to the average, with the noise measured per carrier once there's enough history, so interference
    // counts against it
    equalise(y.data(), m_h.data(), derotate, m_kMax + 1, m_equalised.data(), m_power.data());

    // The SNR the data cells have, from the continual pilots' error after equalisation: they're the same in every
    // symbol, so it's the noise and the channel estimate's error, as on the data cells, without the decision errors
    // that make the MER read high when the SNR is low. Not those that are scattered pilots in this symbol, which
    // the estimate is from
    {
        double error = 0.0;
        int count = 0;

        for (int k : m_tables->m_continualPilots)
        {
            if (((k % 12) == 3 * phase) || (!m_notched.empty() && m_notched.erased(k))) {
                continue;
            }

            error += std::norm(m_equalised[k] - Cf(prbs[k] ? -m_pilotBoost : m_pilotBoost, 0.0f));
            count++;
        }

        if (count > 0) {
            m_pilotNoise = m_pilotNoise < 0.0 ? error / count : 0.95 * m_pilotNoise + 0.05 * error / count;
        }
    }
    const std::vector<int>& dataCarriers = m_tables->m_dataCarriers[phase];
    const int nbData = (int) dataCarriers.size();
    const bool noiseKnown = m_noiseSymbols >= m_noiseSettleSymbols;
    const float *noiseInverse = m_noiseInverse.data();
    float sums[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Independent, so they don't wait for each other

    for (int i = 0; i < nbData; i++)
    {
        const int k = dataCarriers[i];
        m_cells[i] = m_equalised[k];
        m_csi[i] = noiseKnown ? m_power[k] * noiseInverse[k] : m_power[k];
        sums[i & 3] += m_csi[i];
    }

    // Erase the carriers under notches
    if (!m_notched.empty())
    {
        for (int i = 0; i < nbData; i++)
        {
            if (m_notched.erased(dataCarriers[i]))
            {
                sums[i & 3] -= m_csi[i];
                m_csi[i] = 0.0f;
                m_cells[i] = Cf(0.0f, 0.0f);
            }
        }
    }

    const double sumCSI = (double) sums[0] + sums[1] + sums[2] + sums[3];
    const float invMeanCSI = sumCSI > 0.0 ? (float) (nbData / sumCSI) : 0.0f;

    for (float& csi : m_csi) {
        csi *= invMeanCSI;
    }

    PROFILER_STOP("DVB-T OFDM equalisation")
    PROFILER_RESTART()

    if (m_tpsValid) {
        updateNoise(phase);
    }

    PROFILER_STOP("DVB-T OFDM noise")
    PROFILER_RESTART()

    // TPS: DBPSK between consecutive symbols (4.6)
    Cd tpsSum(0.0, 0.0);
    const std::vector<int>& tpsCarriers = m_tables->m_tpsCarriers;

    for (int i = 0; i < (int) tpsCarriers.size(); i++)
    {
        int k = tpsCarriers[i];
        Cf v = y[k] * std::conj(m_h[k]) * derotate;

        if (m_prevTPSValid) {
            // Weighted by the noise on the carrier once known, as interference would otherwise dominate
            tpsSum += noiseKnown ? Cd(v * std::conj(m_prevTPS[i])) / (double) m_noiseSmoothed[k] : unit(v * std::conj(m_prevTPS[i]));
        }

        m_prevTPS[i] = v;
    }

    decodeTPSBit(tpsSum.real() < 0.0 ? 1 : 0, m_prevTPSValid); // Not known without the previous symbol's
    m_prevTPSValid = true;

    if (m_state != Acquire) {
        trackTiming();
    }

    PROFILER_STOP("DVB-T OFDM TPS, timing and output")
}

// Noise on each carrier, from the distance of its cells to the nearest constellation point, scaled back to
// before equalisation, so it doesn't depend on the channel. Decision errors make this an underestimate at
// low SNR, but similarly on every carrier, so the relative values the CSI depends on are about right. Every carrier
// is worked out, contiguously, and only the data carriers' noise updated
void OFDMDemod::updateNoise(int phase)
{
    const Constellation constellation = m_tps.m_constellation;
    float *noise = m_noise.data() + 2; // Two unmeasured carriers each side, for the smoothing
    const int n = m_kMax + 1;

    if ((m_noiseSymbols > 0) && (constellation != m_noiseConstellation)) {
        m_noiseSymbols = 0;
    }

    if (m_noiseSymbols == 0) {
        std::fill(m_noise.begin(), m_noise.end(), -1.0f);
    }

    m_noiseConstellation = constellation;
    const int v = bitsPerCell(constellation);
    const Hierarchy hierarchy = constellation == Constellation::QPSK ? Hierarchy::None : m_tps.m_hierarchy;
    const float scale = InnerDecoder::normalisation(constellation, hierarchy);
    const float maxPoint = (float) ((1 << (v / 2)) - 1);
    // Non-uniform (hierarchical) constellations' points are alpha - 1 further from the axes
    const float offset = (float) (InnerDecoder::alpha(hierarchy) - 1);
    const float invScale2 = 1.0f / (scale * scale);
    const float *e = reinterpret_cast<const float *>(m_equalised.data());
    const float *power = m_power.data();
    const float *mask = m_dataMask[phase].data();

    // The nearest point, 2 floor(x / 2) + 1, clamped. Adding 4 makes it positive, so truncation floors, which is
    // faster than std::floor
    auto error = [maxPoint, offset](float x) {
        if (offset > 0.0f)
        {
            // Of the magnitude less the offset, from an odd integer of at least 1
            const float a = std::fabs(x) - offset;
            const float point = 2.0f * (float) ((int) (std::max(-maxPoint, std::min(maxPoint, a)) * 0.5f + 4.0f) - 4) + 1.0f;
            return a - std::max(1.0f, point);
        }

        float point = 2.0f * (float) ((int) (std::max(-maxPoint, std::min(maxPoint, x)) * 0.5f + 4.0f) - 4) + 1.0f;
        return x - point;
    };

    // Also the sum and number of the carriers measured (others are -1), for the average. The kernels don't do the
    // non-uniform constellations
    int k = 0;
    float total = 0.0f, measuredCount = 0.0f;
    const bool simd = offset == 0.0f;
#if defined(MODEMDVB_X86)
    const bool avx2 = modemdvb::cpuHasAVX2();

    if (avx2 && simd) {
        k = updateNoiseAVX2(e, power, mask, n, scale, maxPoint, invScale2, m_noiseAlpha, noise, total, measuredCount);
    }
#endif
#ifdef DVBTOFDM_SSE2
    if ((k == 0) && simd)
    {
        __m128 totals = _mm_setzero_ps(), counts = _mm_setzero_ps();
        const __m128 sv = _mm_set1_ps(scale);
        const __m128 maxv = _mm_set1_ps(maxPoint);
        const __m128 minv = _mm_set1_ps(-maxPoint);
        const __m128 half = _mm_set1_ps(0.5f);
        const __m128 four = _mm_set1_ps(4.0f);
        const __m128i fouri = _mm_set1_epi32(4);
        const __m128 two = _mm_set1_ps(2.0f);
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 zero = _mm_setzero_ps();
        const __m128 alpha = _mm_set1_ps(m_noiseAlpha);
        const __m128 is2 = _mm_set1_ps(invScale2);

        auto squaredError = [&](__m128 x) {
            x = _mm_mul_ps(x, sv);
            const __m128 c = _mm_max_ps(minv, _mm_min_ps(maxv, x));
            const __m128i f = _mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(c, half), four)), fouri);
            const __m128 err = _mm_sub_ps(x, _mm_add_ps(_mm_mul_ps(two, _mm_cvtepi32_ps(f)), one));
            return _mm_mul_ps(err, err);
        };

        for (; k + 4 <= n; k += 4)
        {
            const __m128 a = squaredError(_mm_loadu_ps(e + 2 * k));     // Carriers k, k + 1: re, im
            const __m128 b = squaredError(_mm_loadu_ps(e + 2 * k + 4)); // k + 2, k + 3
            const __m128 sum = _mm_add_ps(_mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0)),
                _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)));
            const __m128 measured = _mm_mul_ps(_mm_mul_ps(sum, is2), _mm_loadu_ps(power + k));
            const __m128 old = _mm_loadu_ps(noise + k);
            const __m128 first = _mm_cmplt_ps(old, zero);
            const __m128 averaged = _mm_add_ps(old, _mm_mul_ps(alpha, _mm_sub_ps(measured, old)));
            const __m128 updated = _mm_or_ps(_mm_and_ps(first, measured), _mm_andnot_ps(first, averaged));
            const __m128 data = _mm_cmpgt_ps(_mm_loadu_ps(mask + k), zero);
            const __m128 result = _mm_or_ps(_mm_and_ps(data, updated), _mm_andnot_ps(data, old));
            _mm_storeu_ps(noise + k, result);
            totals = _mm_add_ps(totals, _mm_max_ps(result, zero));
            counts = _mm_add_ps(counts, _mm_and_ps(_mm_cmpge_ps(result, zero), one));
        }

        alignas(16) float t[4], c[4];
        _mm_store_ps(t, totals);
        _mm_store_ps(c, counts);
        total = (t[0] + t[1]) + (t[2] + t[3]);
        measuredCount = (c[0] + c[1]) + (c[2] + c[3]);
    }
#endif
    for (; k < n; k++)
    {
        if (mask[k] > 0.0f)
        {
            const float ei = error(e[2 * k] * scale);
            const float eq = error(e[2 * k + 1] * scale);
            const float measured = (ei * ei + eq * eq) * invScale2 * power[k];
            noise[k] = noise[k] < 0.0f ? measured : noise[k] + m_noiseAlpha * (measured - noise[k]);
        }

        total += std::max(noise[k], 0.0f);
        measuredCount += noise[k] >= 0.0f ? 1.0f : 0.0f;
    }

    m_noiseSymbols++;

    if (m_noiseSymbols < m_noiseSettleSymbols) {
        return;
    }

    // Average over 5 carriers, which a narrowband interferer spreads over anyway, of those measured (unmeasured are
    // -1), and limit how much more reliable a carrier can be than the average, as decision errors underestimate the
    // noise in fades
    const float floor = measuredCount > 0.0f ? m_noiseFloor * total / measuredCount : 1.0f;

    int j = 0;
#if defined(MODEMDVB_X86)
    if (avx2) {
        j = smoothNoiseAVX2(noise, n, floor, m_noiseSmoothed.data(), m_noiseInverse.data());
    }
#endif
#ifdef DVBTOFDM_SSE2
    if (j == 0)
    {
        const __m128 floorv = _mm_set1_ps(floor);
        const __m128 zero = _mm_setzero_ps();
        const __m128 one = _mm_set1_ps(1.0f);

        for (; j + 4 <= n; j += 4)
        {
            __m128 s = zero, c = zero;

            for (int d = -2; d <= 2; d++)
            {
                const __m128 x = _mm_loadu_ps(noise + j + d);
                s = _mm_add_ps(s, _mm_max_ps(x, zero));
                c = _mm_add_ps(c, _mm_and_ps(_mm_cmpge_ps(x, zero), one));
            }

            const __m128 smoothed = _mm_max_ps(floorv, _mm_div_ps(s, _mm_max_ps(c, one)));
            _mm_storeu_ps(&m_noiseSmoothed[j], smoothed);
            _mm_storeu_ps(&m_noiseInverse[j], _mm_div_ps(one, smoothed));
        }
    }
#endif
    for (; j < n; j++)
    {
        float s = 0.0f, c = 0.0f;

        for (int d = -2; d <= 2; d++)
        {
            const float x = noise[j + d];
            s += std::max(x, 0.0f);
            c += x >= 0.0f ? 1.0f : 0.0f;
        }

        // None measured gives the floor
        const float smoothed = std::max(floor, s / std::max(c, 1.0f));
        m_noiseSmoothed[j] = smoothed;
        m_noiseInverse[j] = 1.0f / smoothed;
    }
}

// Paths are free of inter-symbol interference if their delay from the window start is between 0 and the guard
// interval. Aim for the first path to be m_backoff in, or less if the spread needs it
void OFDMDemod::trackTiming()
{
    double target = std::min((double) m_backoff, std::max(0.0, (m_guardSize - m_spread) / 2.0));

    // Paths spread over more than the guard interval can't all be free of it: keep the strongest so, by the backoff,
    // at the cost of earlier paths
    if (m_spread > m_guardSize) {
        target = std::min(target, m_guardSize - m_backoff - m_strongest);
    }

    double error = m_delay - target;
    int maxStep = std::max(1, m_guardSize / 32);

    if (error > 1.0) {
        applyWindowStep(std::min(maxStep, (int) std::round(error)));
    } else if (error < -1.0) {
        applyWindowStep(std::max(-maxStep, (int) std::round(error)));
    }
}

void OFDMDemod::decodeTPSBit(int bit, bool known)
{
    if (m_symbolIndex < 0)
    {
        // Look for the sync word in s1..s16, after which the symbol index is 16
        m_tpsSync = known ? (uint16_t) ((m_tpsSync << 1) | bit) : 0;

        if ((m_tpsSync == TPS::m_syncWord) || (m_tpsSync == (uint16_t) ~TPS::m_syncWord))
        {
            int t = m_seq - (m_ringSize - 1) / 2;

            if (ring(t).m_spPhase == 16 % 4)
            {
                for (int i = 0; i < 16; i++) {
                    m_tpsBits[1 + i] = (m_tpsSync >> (15 - i)) & 1;
                }

                m_symbolIndex = 17; // Of the next symbol
                m_frameNumber = 0;
                log("TPSSync: sync word %04x found", m_tpsSync);
            }
        }
        else if (m_stateSymbols > 3 * symbolsPerFrame)
        {
            log("TPSSync: no sync word in %d symbols", m_stateSymbols);
            startAcquire();
            return;
        }
    }
    else
    {
        m_tpsBits[m_symbolIndex] = bit;
        m_tpsBitsLost = m_tpsBitsLost || !known;
        m_tpsHistory = known ? (uint16_t) ((m_tpsHistory << 1) | bit) : 0;
        m_tpsHistoryBits = known ? m_tpsHistoryBits + 1 : 0;

        // After a resync, the scattered pilots give the symbols lost only mod 4, so the frame position is found
        // again from the sync word, s1..s16 (the parity of the index, all the FEC needs, is right meanwhile)
        if (m_tpsRealign > 0)
        {
            if ((m_tpsHistoryBits >= 16) && ((m_tpsHistory == TPS::m_syncWord) || (m_tpsHistory == (uint16_t) ~TPS::m_syncWord))
                && (((m_symbolIndex - 16) & 3) == 0))
            {
                if (m_symbolIndex != 16)
                {
                    log("TPS: frame position corrected by %d symbols", 16 - m_symbolIndex);

                    for (int i = 0; i < 16; i++) {
                        m_tpsBits[1 + i] = (m_tpsHistory >> (15 - i)) & 1;
                    }

                    m_symbolIndex = 16;
                    m_tpsBitsLost = false;
                }

                m_tpsRealign = 0;
            }
            else if (--m_tpsRealign == 0)
            {
                log("TPS: frame position not found after resync");
                startAcquire();
                return;
            }
        }

        // Check the sync word as soon as it has been received, to notice loss of lock sooner
        if ((m_symbolIndex == 16) && !m_tpsBitsLost && (m_tpsRealign == 0))
        {
            int sync = 0;

            for (int i = 1; i <= 16; i++) {
                sync = (sync << 1) | m_tpsBits[i];
            }

            // Counted once a frame, though its TPS then fails to decode too
            m_tpsSyncFailed = (sync != TPS::m_syncWord) && (sync != (~TPS::m_syncWord & 0xffff));

            if (m_tpsSyncFailed && (++m_tpsFailures >= 3))
            {
                log("TPS: lost lock (sync word %04x)", sync);
                startAcquire();
                return;
            }
        }

        if ((m_state == Locked) && m_callback)
        {
            Symbol symbol;
            symbol.m_index = m_symbolIndex;
            symbol.m_frame = m_frameNumber;
            symbol.m_tps = &m_tps;
            if (m_outputErased && (m_erasedCells.size() != m_cells.size()))
            {
                m_erasedCells.assign(m_cells.size(), Cf(0.0f, 0.0f));
                m_erasedCSI.assign(m_cells.size(), 0.0f);
            }

            symbol.m_cells = m_outputErased ? m_erasedCells.data() : m_cells.data();
            symbol.m_csi = m_outputErased ? m_erasedCSI.data() : m_csi.data();
            symbol.m_nbCells = (int) m_cells.size();
            m_callback(symbol);
        }

        if ((m_symbolIndex == symbolsPerFrame - 1) && (m_tpsBitsLost || (m_tpsRealign > 0)))
        {
            log("TPS: frame %d incomplete", m_frameNumber); // Not a failure: the symbols were lost
        }
        else if (m_symbolIndex == symbolsPerFrame - 1)
        {
            TPS tps;
            bool ok = tps.decode(m_tpsBits);
            // Accept parameters with a valid BCH, or that two consecutive frames agree on
            bool accept = ok && (tps.m_bchOK || (m_tpsCandidateValid && tps.sameParameters(m_tpsCandidate)));
            m_tpsCandidate = tps;
            m_tpsCandidateValid = ok;

            if (!ok || !accept || !m_tpsValid || !tps.sameParameters(m_tps))
            {
                // Not s0, the DBPSK reference
                char bits[symbolsPerFrame];

                for (int i = 1; i < symbolsPerFrame; i++) {
                    bits[i - 1] = (char) ('0' + m_tpsBits[i]);
                }

                bits[symbolsPerFrame - 1] = 0;
                log("TPS: %s frame %d %s %s %s GI %s %s%s, length %d, BCH %s, %s: s1..s67 %s",
                    ok ? "decoded" : "invalid", tps.m_frameNumber, toString(tps.m_mode), toString(tps.m_constellation),
                    toString(tps.m_hpCodeRate), toString(tps.m_guard), toString(tps.m_hierarchy),
                    tps.m_inDepth ? " in-depth interleaver" : "", tps.m_lengthIndicator,
                    tps.m_bchOK ? "ok" : "failed", accept ? "accepted" : "not accepted", bits);
            }

            if (accept && !m_tpsValid && ((tps.m_mode != m_mode) || (tps.m_guard != m_guard)))
            {
                // Not the mode and guard interval acquisition found (and the demodulator is set up for)
                log("TPS: %s GI %s signalled, acquiring again", toString(tps.m_mode), toString(tps.m_guard));
                startAcquire();
                return;
            }

            if (accept)
            {
                // Constellation, rates, guard and mode signalled in one super-frame apply to the next
                if (m_tpsValid && !tps.sameParameters(m_tps))
                {
                    if (!m_tpsPendingValid || !tps.sameParameters(m_tpsPending)) {
                        log("TPS: parameters change from the next super-frame");
                    }

                    m_tpsPending = tps;
                    m_tpsPendingValid = true;
                    m_tps.m_frameNumber = tps.m_frameNumber;
                    m_tps.m_bchOK = tps.m_bchOK;
                }
                else
                {
                    m_tps = tps;
                    m_tpsPendingValid = false;
                }

                m_tpsValid = true;
                m_tpsFailures = 0;
                m_frameNumber = tps.m_frameNumber;

                // Cell id: high byte in frames 1 and 3 of the super-frame, low byte in 2 and 4 (V1.6.1 4.6.2.10),
                // also with DVB-H signalling, length 33 (F.3.4.2.1)
                if (tps.m_lengthIndicator >= 31) {
                    m_cellIdBytes[tps.m_frameNumber & 1] = tps.m_cellIdByte;
                }

                if (m_state == TPSSync) {
                    m_state = Locked;
                }
            }
            else if ((m_tpsSyncFailed ? m_tpsFailures : ++m_tpsFailures) >= 3)
            {
                log("TPS: lost lock");
                startAcquire();
                return;
            }
        }

        if (++m_symbolIndex == symbolsPerFrame)
        {
            m_symbolIndex = 0;
            m_tpsBitsLost = false;
            m_tpsSyncFailed = false;
            m_frameNumber = (m_frameNumber + 1) % framesPerSuperFrame;

            if ((m_frameNumber == 0) && m_tpsPendingValid)
            {
                // The demodulator is set up for the mode and guard interval found by acquisition, and the FEC
                // decodes the cells by the TPS, so a change of either is acquired again
                if ((m_tpsPending.m_mode != m_mode) || (m_tpsPending.m_guard != m_guard))
                {
                    log("TPS: now %s GI %s, acquiring again", toString(m_tpsPending.m_mode), toString(m_tpsPending.m_guard));
                    startAcquire();
                    return;
                }

                m_tps = m_tpsPending;
                m_tpsPendingValid = false;
                log("TPS: now %s %s %s GI %s", toString(m_tps.m_mode), toString(m_tps.m_constellation),
                    toString(m_tps.m_hpCodeRate), toString(m_tps.m_guard));
            }
        }
    }
}

void OFDMDemod::setNotches(const std::vector<double>& frequencies, double inputRate)
{
    if ((frequencies == m_notchFrequencies) && (inputRate == m_notchInputRate)) {
        return;
    }

    m_notchFrequencies = frequencies;
    m_notchInputRate = inputRate;
    m_notched.setNotches(frequencies, inputRate);
    m_notchesChanged = true;
}

OFDMDemod::Status OFDMDemod::getStatus() const
{
    Status status;
    status.m_state = m_state;
    status.m_mode = m_mode;
    status.m_guard = m_guard;
    status.m_frequencyOffset = -m_ncoFreq * m_sampleRate / m_twoPi;
    status.m_clockOffset = m_clockOffset;
    status.m_clockMeasurements = m_clockMeasurements;
    status.m_gaCorrelation = m_gaCorrelation;
    status.m_delay = m_delay;
    status.m_spread = m_spread;
    status.m_snr = m_pilotNoise > 0.0 ? 10.0 * std::log10(1.0 / m_pilotNoise) : 0.0;
    status.m_tpsValid = m_tpsValid;
    status.m_tps = m_tps;
    status.m_cellId = (m_cellIdBytes[0] >= 0) && (m_cellIdBytes[1] >= 0) ? (m_cellIdBytes[0] << 8) | m_cellIdBytes[1] : -1;
    status.m_symbols = m_symbolCount;
    status.m_windowSteps = m_windowSteps;
    status.m_resyncs = m_resyncs;
    status.m_acquisitions = m_acquisitions;
    return status;
}

void OFDMDemod::getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const
{
    cells.clear();
    responseDb.clear();

    if ((m_state != TPSSync) && (m_state != Locked)) {
        return;
    }

    int step = std::max(1, (int) m_cells.size() / std::max(1, maxCells));

    for (size_t i = 0; i < m_cells.size(); i += step) {
        cells.push_back(m_cells[i]);
    }

    double mean = 0.0;

    for (const Cf& h : m_h) {
        mean += std::norm(h);
    }

    mean /= std::max((size_t) 1, m_h.size());

    for (int p = 0; p < points; p++)
    {
        size_t k = (size_t) ((double) p * (m_h.size() - 1) / std::max(1, points - 1));
        double power = std::norm(m_h[k]) / std::max(mean, 1e-30);
        responseDb.push_back((float) (10.0 * std::log10(std::max(power, 1e-6))));
    }
}


// The impulse response's power, reduced to at most points bins (by maximum, which keeps narrow paths), in dB
// relative to its peak, down to -60 dB
void OFDMDemod::impulseResponseDb(const std::vector<float>& power, double resolution, int points, std::vector<float>& powerDb,
    double& binResolution)
{
    const int size = (int) power.size();
    const int group = std::max(1, (size + points - 1) / points);
    const int bins = size / group;
    float peak = 0.0f;

    for (float p : power) {
        peak = std::max(peak, p);
    }

    powerDb.resize(bins);

    for (int b = 0; b < bins; b++)
    {
        float p = 0.0f;

        for (int i = 0; i < group; i++) {
            p = std::max(p, power[b * group + i]);
        }

        powerDb[b] = p > peak * 1e-6f ? 10.0f * std::log10(p / peak) : -60.0f;
    }

    binResolution = resolution * group;
}

void OFDMDemod::getImpulseResponse(ImpulseResponse& response, int points) const
{
    response.m_powerDb.clear();

    if (((m_state != TPSSync) && (m_state != Locked)) || (m_cirResolution <= 0.0) || m_cirPower.empty() || (points <= 0)) {
        return;
    }

    impulseResponseDb(m_cirPower, m_cirResolution, points, response.m_powerDb, response.m_resolution);
    response.m_guard = m_guardSize;
}

} // namespace dvbt
