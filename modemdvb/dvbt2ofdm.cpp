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
#include <cstdlib>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include <QString>

#include "dsp/fftengine.h"
#include "dsp/fftfactory.h"
#include "util/profiler.h"

#include "dvbt2ofdm.h"

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define DVBT2OFDM_SSE2
#endif

namespace dvbt2 {

typedef std::complex<double> Cd;

#if defined(DVBT2OFDM_SSE2)
// File-local, not an OFDMDemod member: its signature's __m128 would need the intrinsics' header in dvbt2ofdm.h
// Two complex products at once, a[0] b[0] and a[1] b[1], each as std::complex's (re = ar br - ai bi, im = ar bi + ai br),
// so the same
static inline __m128 complexMultiply2(__m128 a, __m128 b)
{
    const __m128 brr = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128 bii = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 1, 1));
    const __m128 swapped = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128 negateRe = _mm_castsi128_ps(_mm_set_epi32(0, (int) 0x80000000, 0, (int) 0x80000000));
    return _mm_add_ps(_mm_mul_ps(a, brr), _mm_xor_ps(_mm_mul_ps(swapped, bii), negateRe));
}
#endif

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

OFDMDemod::OFDMDemod(int bandwidthHz, FFTFactory *fftFactory) :
    m_sampleRate(sampleRate(bandwidthHz)),
    m_p2Tables(nullptr),
    m_tables(nullptr),
    m_interpolationSpacing(0),
    m_gridSpacing(0),
    m_gridOffset(0),
    m_gridAmplitude(0.0f)
{
    m_fft.m_factory = fftFactory;
    m_p1Fft.m_factory = fftFactory;
    m_cirFft.m_factory = fftFactory;
    reset();
}

OFDMDemod::~OFDMDemod()
{
    m_fft.release();
    m_p1Fft.release();
    m_cirFft.release();
}

void OFDMDemod::reset()
{
    m_cirResolution = 0.0;
    m_buffer.clear();
    m_bufferOffset = 0;
    m_searchPos = 0;
    m_frequency = 0.0;
    m_ncoPhase = 0.0;
    m_ncoIndex = 0;
    m_fftSize = 0;
    m_guardSamples = 0;
    m_np2 = 0;
    m_frameStart = -1;
    m_previousFrameStart = -1;
    m_l = 0;
    m_lf = 0;
    m_frameClosing = false;
    m_frameKnown = false;
    m_frameCount = 0;
    m_noise = -1.0;
    m_noiseRatio = -1.0;
    m_delay = 0.0;
    m_hold.clear();
    m_holdAmplitude.clear();
    m_carrierNoise.clear();
    m_carrierWeight.clear();
    m_carrierWeightAge = 0;
    m_windowOffset = -1;
    m_windowGuard = 0;
    m_centreValid = false;
    m_spread = 0.0;
    m_timeInterpolation = false;
    m_firstPathSum = 0.0;
    m_spreadSum = 0.0;
    m_cirCount = 0;
    m_previousWindow = -1;
    m_clockSum = 0.0;
    m_clockCount = 0;
    m_clockFrames = 0;
    m_clockChangeSample = 0;
    m_clockChangeDelta = 0.0;
    m_lastSearchedP1 = -1;
    m_lastP1Symbols = -1;
    m_giAverage = 0.0;
    m_giLow = 0;
    m_repeatSymbol = false;
    m_fefLength = 0;
    m_inFEF = false;
    m_status = Status();
}

void OFDMDemod::startAcquisition()
{
    // The window placement measured over the last frame (from before a timing jump, or of another signal), which would
    // place the first frame's window, and in MISO repeat its first P2 symbol as if it had just been measured
    m_firstPathSum = 0.0;
    m_spreadSum = 0.0;
    m_cirCount = 0;
    m_timeInterpolation = false;
    // The clock measurements of a frame cut short, which nextP1() would take as the new frame's
    m_clockSum = 0.0;
    m_clockCount = 0;
    m_previousWindow = -1;
    m_giAverage = 0.0;
    m_giLow = 0;
    m_repeatSymbol = false;
}

void OFDMDemod::process(const Cf *samples, int nbSamples)
{
    m_buffer.insert(m_buffer.end(), samples, samples + nbSamples);

    bool progress = true;

    while (progress)
    {
        switch (m_status.m_state)
        {
        case SearchP1: progress = searchP1(); break;
        case FindGuard: progress = findGuard(); break;
        case Symbols: progress = processSymbol(); break;
        case NextP1: progress = nextP1(); break;
        }
    }

    // Discard samples no longer needed
    int64_t keep;

    switch (m_status.m_state)
    {
    case SearchP1: keep = m_searchPos; break;
    case FindGuard: keep = m_frameStart - 8; break; // The symbol position (m_l) is that of the last frame
    case NextP1: keep = framePosition(frameLength() - 2 * (int64_t) (m_fftSize + m_guardSamples)) - p1Range() - 8; break;
    default: keep = framePosition(m_p1Length + (int64_t) m_l * (m_fftSize + m_guardSamples)) - 8; break;
    }

    int64_t discard = keep - m_bufferOffset;

    if (discard > (1 << 18))
    {
        discard = std::min(discard, (int64_t) m_buffer.size());
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + discard);
        m_bufferOffset += discard;
    }
}

// The P1's frequency shift removed: sample n times shift[n mod 1024], one 1K carrier down
const Cd *OFDMDemod::p1Shifts()
{
    static const std::vector<Cd> shifts = []() {
        std::vector<Cd> s(1024);

        for (int i = 0; i < 1024; i++) {
            s[i] = std::polar(1.0, -m_twoPi * i / 1024.0);
        }

        return s;
    }();
    return shifts.data();
}

// P1 correlation (9.8.2.4): C is the start of A, and B its end, shifted up in frequency by one 1K carrier.
// Removing the shift and correlating each with the part of A it copies gives c1 and c2, whose phases are
// -2 pi f 542 - 2 pi n0 / 1024 and -2 pi f 482 + 2 pi n0 / 1024, f being the frequency offset in cycles per sample
double OFDMDemod::p1Metric(int64_t n0, Cd& c1, Cd& c2) const
{
    const Cf *r = bufferAt(n0);
    const Cd *shifts = p1Shifts();
    c1 = Cd(0.0, 0.0);
    c2 = Cd(0.0, 0.0);
    double energy = 0.0;

    for (int i = 0; i < 542; i++)
    {
        const Cd shift = shifts[(n0 + i) & 1023];
        c1 += Cd(r[i]) * shift * std::conj(Cd(r[542 + i]));
    }

    for (int i = 0; i < 482; i++)
    {
        const Cd shift = shifts[(n0 + 1566 + i) & 1023];
        c2 += Cd(r[1084 + i]) * std::conj(Cd(r[1566 + i]) * shift);
    }

    for (int i = 0; i < m_p1Length; i++) {
        energy += std::norm(r[i]);
    }

    return energy > 0.0 ? 2.0 * (std::abs(c1) + std::abs(c2)) / energy : 0.0;
}

bool OFDMDemod::searchP1()
{
    const int64_t last = bufferEnd() - m_p1Length - 1;

    if (last < m_searchPos) {
        return false;
    }

    // Running sums over the window starting at n0
    const Cd *shift = p1Shifts();
    auto s = [&](int64_t n) { return Cd(*bufferAt(n)) * shift[n & 1023]; };
    auto r = [&](int64_t n) { return Cd(*bufferAt(n)); };
    auto product1 = [&](int64_t n) { return s(n) * std::conj(r(n + 542)); };
    auto product2 = [&](int64_t m) { return r(m) * std::conj(s(m + 482)); };

    int64_t n0 = m_searchPos;
    Cd c1(0.0, 0.0), c2(0.0, 0.0);
    double energy = 0.0;

    for (int i = 0; i < 542; i++) {
        c1 += product1(n0 + i);
    }

    for (int i = 0; i < 482; i++) {
        c2 += product2(n0 + 1084 + i);
    }

    for (int i = 0; i < m_p1Length; i++) {
        energy += std::norm(r(n0 + i));
    }

    int64_t crossing = -1, best = -1;
    double bestMetric = 0.0;
    Cd bestProduct;

    for (; n0 <= last; n0++)
    {
        double metric = energy > 0.0 ? 2.0 * (std::abs(c1) + std::abs(c2)) / energy : 0.0;

        if (metric > m_p1Threshold)
        {
            if (crossing < 0) {
                crossing = n0;
            }

            if (metric > bestMetric)
            {
                bestMetric = metric;
                best = n0;
                bestProduct = c1 * c2;
            }
        }

        if ((crossing >= 0) && (n0 > crossing + 1100)) {
            break; // Past the peak
        }

        if (n0 < last)
        {
            c1 += product1(n0 + 542) - product1(n0);
            c2 += product2(n0 + 1566) - product2(n0 + 1084);
            energy += std::norm(r(n0 + m_p1Length)) - std::norm(r(n0));
        }
    }

    if ((crossing < 0) || (n0 <= crossing + 1100))
    {
        // Nothing yet, or the peak may still be to come: continue from before the crossing
        m_searchPos = crossing >= 0 ? crossing : last + 1;
        return false;
    }

    double fraction = -std::arg(bestProduct) / (m_twoPi * 1024.0);
    P1Signalling p1;
    double frequency;

    if (decodeP1(best, fraction, p1, frequency) && profileWanted(p1) && (p1.fftSize() > 0))
    {
        m_status.m_p1 = p1;
        m_status.m_p1Correlation = bestMetric;
        m_status.m_acquisitions++;
        m_frequency = frequency;

        // At a large clock offset, interference between the carriers stops L1-pre being decoded (from 40 ppm in 32K),
        // so the frame's length isn't known, nor the offset measured from the continual pilots. But P1s are a whole
        // number of symbols (and a P1) apart, which is unambiguous to well beyond any offset, so their spacing gives
        // the offset, to correct it. Not if a frame was missed between them, as then they aren't: that adds a P1,
        // which isn't a whole number of symbols. From 4K that's well off one, or hundreds of ppm, but in 1K and 2K
        // (the frames short, and the symbols near a P1's length) it can look like -15 to -60 ppm. So the spacing
        // must be near a whole number of symbols, and there the same number as between the last two
        if ((m_lastSearchedP1 >= 0) && (m_guardSamples > 0) && (m_fftSize == p1.fftSize()) && (m_lite == p1.isT2Lite())
            && (m_clockFrames == 0) && !p1.hasFEF())
        {
            const int ns = m_fftSize + m_guardSamples;
            const int64_t spacing = best - m_lastSearchedP1;
            const double exact = (double) (spacing - m_p1Length) / ns;
            const int64_t symbols = std::llround(exact);
            const double ppm = 1e6 * ((double) spacing / (m_p1Length + symbols * ns) - 1.0);
            const bool consistent = (m_fftSize >= 4096) || (symbols == m_lastP1Symbols);
            m_lastP1Symbols = symbols;

            if ((symbols >= 2) && (std::abs(exact - (double) symbols) < 0.1) && consistent && (std::abs(ppm) < 300.0))
            {
                m_status.m_clockOffset = ppm;
                m_status.m_clockFromP1s = true;
                log("Clock offset %.1f ppm, from the spacing of P1s", ppm);
            }
        }
        else
        {
            m_lastP1Symbols = -1;
        }

        m_lastSearchedP1 = best;
        m_inFEF = false;
        m_lite = p1.isT2Lite();
        m_miso = p1.isMISO();
        startAcquisition();
        m_fftSize = p1.fftSize();
        m_np2 = numP2(m_fftSize);
        m_frameStart = best;
        m_previousFrameStart = -1;
        m_l = 0;

        // A clock change before the frame is all in the offset it starts with, as in nextP1(), or framePosition()
        // would move its symbols by the change again (330 samples, 2 M samples after a 150 ppm correction)
        if (m_clockChangeSample <= best) {
            m_clockChangeDelta = 0.0;
        }

        bool newGuards;
        fftSizeFromS2(p1.m_s2, newGuards, m_lite);
        m_guardCandidates.clear();

        for (int g = 0; g < 7; g++)
        {
            GuardInterval guard = (GuardInterval) g;
            bool isNew = (guard == GuardInterval::G1_128) || (guard == GuardInterval::G19_128) || (guard == GuardInterval::G19_256);

            // T2-base's 16K S2 is for any guard interval, T2-Lite's for one set or the other
            if (guardAllowed(m_fftSize, guard) && (((m_fftSize == 16384) && !m_lite) || (isNew == newGuards))) {
                m_guardCandidates.push_back(guard);
            }
        }

        log("P1: S1 %d S2 %d (%s%s), correlation %.2f, frequency offset %.1f Hz", p1.m_s1, p1.m_s2, fftSizeString(m_fftSize),
            m_lite ? " T2-Lite" : "", bestMetric, frequency * m_sampleRate);
        m_status.m_state = FindGuard;
    }
    else
    {
        m_searchPos = best + m_p1Length;
    }

    return true;
}

bool OFDMDemod::decodeP1(int64_t start, double fraction, P1Signalling& p1, double& frequency)
{
    m_p1Fft.configure(1024, false);
    Cf *in = m_p1Fft.m_engine->in();
    const int64_t a = start + 542;

    for (int i = 0; i < 1024; i++) {
        in[i] = *bufferAt(a + i) * Cf(std::polar(1.0, -m_twoPi * fraction * (double) (a + i)));
    }

    m_p1Fft.m_engine->transform();
    const Cf *x = m_p1Fft.m_engine->out();
    const P1Tables& t = P1Tables::get();

    // Whole 1K carrier offset, from the energy in the active carriers
    int bestShift = 0;
    double bestEnergy = -1.0;

    for (int s = -64; s <= 64; s++)
    {
        double energy = 0.0;

        for (int i = 0; i < 384; i++) {
            energy += std::norm(x[(t.m_carriers[i] - 426 + s + 1024) & 1023]);
        }

        if (energy > bestEnergy)
        {
            bestEnergy = energy;
            bestShift = s;
        }
    }

    frequency = fraction + bestShift / 1024.0;

    // DBPSK between consecutive active carriers, after descrambling
    float v[384];
    Cf previous(0.0f, 0.0f);

    for (int i = 0; i < 384; i++)
    {
        Cf value = x[(t.m_carriers[i] - 426 + bestShift + 1024) & 1023] * (t.m_scrambling[i] ? -1.0f : 1.0f);
        Cf z = value * std::conj(previous);
        float magnitude = std::abs(z);
        v[i] = (i > 0) && (magnitude > 0.0f) ? z.real() / magnitude : 0.0f;
        previous = value;
    }

    int bestS1 = 0, bestS2 = 0;
    float bestScore1 = -1e9f, bestScore2 = -1e9f;

    for (int c = 0; c < 8; c++)
    {
        float score = 0.0f;

        for (int i = 1; i < 64; i++) {
            score += t.m_s1[c][i] ? -v[i] : v[i];
        }

        for (int i = 0; i < 64; i++) {
            score += t.m_s1[c][i] ? -v[320 + i] : v[320 + i];
        }

        if (score > bestScore1)
        {
            bestScore1 = score;
            bestS1 = c;
        }
    }

    for (int c = 0; c < 16; c++)
    {
        float score = 0.0f;

        for (int i = 0; i < 256; i++) {
            score += t.m_s2[c][i] ? -v[64 + i] : v[64 + i];
        }

        if (score > bestScore2)
        {
            bestScore2 = score;
            bestS2 = c;
        }
    }

    p1.m_s1 = bestS1;
    p1.m_s2 = bestS2;
    return (bestScore1 > 0.3f * 127) && (bestScore2 > 0.3f * 256);
}

// The guard interval is the candidate whose correlation is high for both of the first two symbols, as a
// shorter one also correlates in the first. The phase of the correlation refines the frequency offset
bool OFDMDemod::findGuard()
{
    const int n = m_fftSize;
    const int64_t t0 = m_frameStart + m_p1Length;

    if (bufferEnd() < t0 + 2 * (n + 19 * n / 128) + n) {
        return false;
    }

    auto correlate = [&](int64_t t, int ng, Cd& sum) {
        const Cf *r = bufferAt(t);
        Cd c(0.0, 0.0);
        double energy = 0.0;

        for (int i = 0; i < ng; i++)
        {
            c += Cd(r[i]) * std::conj(Cd(r[i + n]));
            energy += 0.5 * (std::norm(r[i]) + std::norm(r[i + n]));
        }

        sum += c;
        return energy > 0.0 ? std::abs(c) / energy : 0.0;
    };

    double bestScore = -1.0;
    GuardInterval bestGuard = GuardInterval::G1_32;
    Cd bestSum;

    for (GuardInterval guard : m_guardCandidates)
    {
        int ng = guardSamples(n, guard);
        Cd sum(0.0, 0.0);
        double score = correlate(t0, ng, sum) + correlate(t0 + n + ng, ng, sum);

        if (score > bestScore)
        {
            bestScore = score;
            bestGuard = guard;
            bestSum = sum;
        }
    }

    // Combine with the P1 estimate, which is good to well within a carrier
    double fraction = -std::arg(bestSum) / (m_twoPi * n);
    double difference = fraction - m_frequency;
    difference -= std::round(difference * n) / n;
    m_frequency += difference;

    m_guardSamples = guardSamples(n, bestGuard);
    m_status.m_guard = bestGuard;
    m_status.m_fftSize = n;
    m_status.m_frequencyOffset = m_frequency * m_sampleRate;
    m_p2Tables = &Tables::get(n, false, PilotPattern::PP7, false, m_miso);
    m_l = 0;
    m_lf = m_np2;
    m_frameKnown = false;
    m_frameCount = 0;
    m_p2Measured = false;
    m_misoCentreValid[0] = m_misoCentreValid[1] = false;
    m_misoSpread[0] = m_misoSpread[1] = -1.0;
    m_noiseSettling = -1; // From the first data symbol
    log("Guard interval %s, correlation %.2f, frequency offset %.1f Hz", toString(bestGuard), bestScore / 2.0, m_frequency * m_sampleRate);
    m_status.m_state = Symbols;
    return true;
}

void OFDMDemod::setFrameParameters(const L1Pre& pre)
{
    m_pre = pre;
    m_frameKnown = true;
    m_lf = m_np2 + pre.m_numDataSymbols;
    m_tables = &Tables::get(m_fftSize, pre.m_bwtExt, pre.m_pilotPattern, pre.toneReservation(), m_miso);
    m_frameClosing = frameClosingSymbol(m_fftSize, pre.m_guard, pre.m_pilotPattern, m_miso);
    m_status.m_frameKnown = true;

    // P2 symbols of later frames with the same carrier mode as the data symbols, so the channel estimates held for
    // time interpolation carry over
    if (m_p2Tables->m_extended != m_tables->m_extended) {
        m_p2Tables = &Tables::get(m_fftSize, m_tables->m_extended, PilotPattern::PP7, false, m_miso);
    }

    // The guard interval found by correlation can be wrong (as 1/16 for 19/256 with a long echo), which would leave the
    // data symbols misplaced, and every frame lost, as L1 decodes. L1-pre's, CRC protected, is right, so used. This is
    // called with the last P2 symbol, so it's in time for the data symbols. The P2 symbols were placed as of the first
    // (whose position doesn't depend on the guard interval), so the last was late by the difference times its index
    if ((pre.m_guard != m_status.m_guard) && guardAllowed(m_fftSize, pre.m_guard))
    {
        log("L1-pre guard interval %s, not %s as found: using it", toString(pre.m_guard), toString(m_status.m_guard));
        const int previousGuard = m_guardSamples;
        const int guard = guardSamples(m_fftSize, pre.m_guard);
        const int previousOffset = windowOffset();
        const int late = (m_np2 - 1) * (previousGuard - guard);

        // A window placed from the paths measured stays where it is relative to them (as the first P2 symbol's
        // position is the same), within the new guard interval, else the default for it
        if ((m_windowOffset >= 0) && (m_windowGuard == previousGuard))
        {
            m_windowOffset = std::min(m_windowOffset, guard);
            m_windowGuard = guard;
        }

        // The paths measured are relative to the useful parts' nominal starts, a guard interval after the symbols'
        m_firstPathSum -= m_cirCount * (double) (guard - previousGuard);
        m_exactDrift -= late * m_status.m_clockOffset * 1e-6; // Accumulated over the P2 symbols with the old length
        m_guardSamples = guard;
        m_status.m_guard = pre.m_guard;
        m_giAverage = 0.0;
        m_giLow = 0;

        // The estimates held from the last P2 symbol are as if from its window: rotated to where the data symbols'
        // will be, relative to their useful parts (as updateWindowOffset() does when it moves)
        const int step = windowOffset() - previousOffset - late;

        if (step != 0)
        {
            auto rotate = [this, step](std::vector<Cf>& estimates) {
                const int kMax = (int) estimates.size() - 1;
                const Cd rotStep = std::polar(1.0, m_twoPi * step / m_fftSize);
                Cd rot = std::polar(1.0, -m_twoPi * step * (kMax / 2) / m_fftSize);

                for (int k = 0; k <= kMax; k++)
                {
                    estimates[k] *= Cf(rot);
                    rot *= rotStep;
                }
            };

            rotate(m_hold);
            rotate(m_p2Channels[0]);
            rotate(m_p2Channels[1]);
            m_centre -= step;
            m_misoCentre[0] -= step;
            m_misoCentre[1] -= step;
        }
    }
}

void OFDMDemod::fft(int64_t start)
{
    Cf *in = m_fft.m_engine->in();
    const Cf *r = bufferAt(start);
    // Continue the phase from the last window, with the current frequency
    m_ncoPhase = std::fmod(m_ncoPhase + m_frequency * (double) (start - m_ncoIndex), 1.0);
    m_ncoIndex = start;
    // Explicit arithmetic, as std::complex multiplication is slow with MSVC. Four phasors, each stepping four samples,
    // so they don't wait for each other
    const double stepPhase = -m_twoPi * m_frequency;
    const double step4Re = std::cos(4.0 * stepPhase), step4Im = std::sin(4.0 * stepPhase);
    double rotRe[4], rotIm[4];

    for (int j = 0; j < 4; j++)
    {
        rotRe[j] = std::cos(-m_twoPi * m_ncoPhase + j * stepPhase);
        rotIm[j] = std::sin(-m_twoPi * m_ncoPhase + j * stepPhase);
    }

    int i = 0;
#if defined(DVBT2OFDM_SSE2)
    // The same arithmetic, the four phasors in registers: their real and imaginary parts, two per register
    __m128d re01 = _mm_set_pd(rotRe[1], rotRe[0]), re23 = _mm_set_pd(rotRe[3], rotRe[2]);
    __m128d im01 = _mm_set_pd(rotIm[1], rotIm[0]), im23 = _mm_set_pd(rotIm[3], rotIm[2]);
    const __m128d s4Re = _mm_set1_pd(step4Re), s4Im = _mm_set1_pd(step4Im);

    for (; i < m_fftSize; i += 4)
    {
        const __m128 cr = _mm_movelh_ps(_mm_cvtpd_ps(re01), _mm_cvtpd_ps(re23));
        const __m128 ci = _mm_movelh_ps(_mm_cvtpd_ps(im01), _mm_cvtpd_ps(im23));
        const __m128 x01 = _mm_loadu_ps(reinterpret_cast<const float *>(r + i));
        const __m128 x23 = _mm_loadu_ps(reinterpret_cast<const float *>(r + i + 2));
        const __m128 xr = _mm_shuffle_ps(x01, x23, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 xi = _mm_shuffle_ps(x01, x23, _MM_SHUFFLE(3, 1, 3, 1));
        const __m128 yr = _mm_sub_ps(_mm_mul_ps(xr, cr), _mm_mul_ps(xi, ci));
        const __m128 yi = _mm_add_ps(_mm_mul_ps(xr, ci), _mm_mul_ps(xi, cr));
        _mm_storeu_ps(reinterpret_cast<float *>(in + i), _mm_unpacklo_ps(yr, yi));
        _mm_storeu_ps(reinterpret_cast<float *>(in + i + 2), _mm_unpackhi_ps(yr, yi));
        const __m128d nextRe01 = _mm_sub_pd(_mm_mul_pd(re01, s4Re), _mm_mul_pd(im01, s4Im));
        const __m128d nextRe23 = _mm_sub_pd(_mm_mul_pd(re23, s4Re), _mm_mul_pd(im23, s4Im));
        im01 = _mm_add_pd(_mm_mul_pd(re01, s4Im), _mm_mul_pd(im01, s4Re));
        im23 = _mm_add_pd(_mm_mul_pd(re23, s4Im), _mm_mul_pd(im23, s4Re));
        re01 = nextRe01;
        re23 = nextRe23;
    }
#endif

    for (; i < m_fftSize; i += 4)
    {
        for (int j = 0; j < 4; j++)
        {
            const float cr = (float) rotRe[j], ci = (float) rotIm[j];
            const float xr = r[i + j].real(), xi = r[i + j].imag();
            in[i + j] = Cf(xr * cr - xi * ci, xr * ci + xi * cr);
            const double re = rotRe[j] * step4Re - rotIm[j] * step4Im;
            rotIm[j] = rotRe[j] * step4Im + rotIm[j] * step4Re;
            rotRe[j] = re;
        }
    }

    m_fft.m_engine->transform();
}

// Frequency offset from the guard interval correlation of each symbol, past where echoes may make it differ
void OFDMDemod::trackFrequency(int64_t start)
{
    const int n = m_fftSize;
    const Cf *r = bufferAt(start);
    Cd c(0.0, 0.0);

    for (int i = m_guardSamples / 4; i < m_guardSamples; i++) {
        c += Cd(r[i]) * std::conj(Cd(r[i + n]));
    }

    double measured = -std::arg(c) / (m_twoPi * n);
    double error = measured - m_frequency;
    error -= std::round(error * n) / n;
    m_frequency += m_frequencyLoopGain * error;
    m_status.m_frequencyOffset = m_frequency * m_sampleRate;
}

// Least squares estimates on a regular grid of pilots (spacing, offset), optionally holding the latest estimate on
// each pilot carrier for time interpolation, then interpolated in frequency
void OFDMDemod::estimateChannel(const std::vector<Tables::Pilot>& pilots, int spacing, int offset, int l, bool hold, float amplitude)
{
    PROFILER_START()
    m_gridSpacing = spacing;
    m_gridOffset = offset;
    m_gridAmplitude = amplitude;
    const Tables *t = m_p2Tables && (l < m_np2) ? m_p2Tables : m_tables;
    const int pn = framePN(l);
    m_holdNoise = 0.0;
    m_holdWeight = 0.0;
    m_holdCount = 0;

    if (m_holdAmplitude.size() != m_hold.size()) {
        m_holdAmplitude.assign(m_hold.size(), 0.0f);
    }

    if (hold)
    {
        // Held estimates are from earlier symbols, whose phase may differ by a common rotation, from residual
        // frequency error and phase noise. Rotate this symbol to their phase, from its pilots' change from them. Only
        // if that's measured precisely (its standard deviation, from the pilots' spread across the common direction,
        // under a degree): in MISO with the groups' channels nearly the same, alternate symbols' scattered pilots are
        // nearly all H1 - H2, so little more than noise, and a rotation of 10 to 45 degrees from it spoilt every cell
        Cd change(0.0, 0.0);
        m_cpeTerms.clear();

        for (const Tables::Pilot& pilot : pilots)
        {
            const int k = pilot.m_k;

            if ((m_holdAmplitude[k] != 0.0f) && !(m_miso && ((k == 0) || (k == t->m_kMax))))
            {
                float value = (t->m_w[k] ^ pn) ? -pilot.m_amplitude : pilot.m_amplitude;
                const Cd term = Cd(m_y[k] * std::conj(m_hold[k] * value));
                change += term;
                m_cpeTerms.push_back(term);
            }
        }

        // The quadrature parts about the mean direction are noise; their sum's spread over the sum is the error
        double quadrature = 0.0;
        const Cd direction = std::abs(change) > 0.0 ? std::conj(change) / std::abs(change) : Cd(1.0, 0.0);

        for (const Cd& term : m_cpeTerms) {
            quadrature += std::norm((term * direction).imag());
        }

        if ((std::norm(change) > 0.0) && (std::sqrt(quadrature) < 0.02 * std::abs(change)))
        {
            const Cf rotate = Cf(std::polar(1.0, -std::arg(change)));

            for (Cf& y : m_y) {
                y *= rotate;
            }
        }
    }

    for (const Tables::Pilot& pilot : pilots)
    {
        int k = pilot.m_k;
        float value = (t->m_w[k] ^ pn) ? -pilot.m_amplitude : pilot.m_amplitude;
        Cf h = m_y[k] / value;

        if (m_miso && (((k - offset) % spacing) == 0) && (misoSign(k, l) != ((((k - offset) / spacing) & 1) ? -1.0f : 1.0f))) {
            continue; // An edge pilot with the other sign: not as the grid needs
        }

        if (hold || ((k - offset) % spacing == 0))
        {
            // With time interpolation, nearly every pilot is used for the estimate, leaving too few to measure the
            // noise on. But the change of each from the estimate held from an earlier symbol is the noise of the
            // two (plus any change of the channel, which errs towards less confident LLRs): 2 sigma^2 (1/A^2 + 1/A0^2)
            if (hold && (m_holdAmplitude[k] > 0.0f) && !m_notched.erased(k))
            {
                const double change = std::norm(h - m_hold[k]);
                const double weight = 1.0 / (pilot.m_amplitude * pilot.m_amplitude) + 1.0 / (m_holdAmplitude[k] * m_holdAmplitude[k]);
                carrierNoise(k, (float) (change / (2.0 * weight)));

                if (!noisyCarrier(k)) // The overall noise, of the carriers without interference
                {
                    m_holdNoise += change;
                    m_holdWeight += weight;
                    m_holdCount++;
                }
            }

            if (m_miso && (l < m_np2))
            {
                m_p2Hold.resize(m_hold.size());
                m_p2Hold[k] = h;
                continue;
            }

            m_hold[k] = h;
            m_holdAmplitude[k] = pilot.m_amplitude;
        }
    }

    interpolate(spacing, offset, hold);
    PROFILER_STOP("DVB-T2 OFDM channel estimation")
}

void OFDMDemod::interpolate(int spacing, int offset, bool hold)
{
    const int kMax = (int) m_y.size() - 1;
    const int nbPilots = (kMax - offset) / spacing + 1;
    const int n = m_fftSize;

    if (spacing != m_interpolationSpacing)
    {
        // Windowed sinc for each position between pilots
        m_interpolationTaps.assign(spacing, std::vector<float>(2 * m_interpolationHalfLength));

        for (int r = 0; r < spacing; r++)
        {
            double sum = 0.0;

            for (int m = 0; m < 2 * m_interpolationHalfLength; m++)
            {
                double x = (m - m_interpolationHalfLength + 1) - (double) r / spacing;
                double sinc = std::abs(x) < 1e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
                double window = 0.5 + 0.5 * std::cos(M_PI * x / m_interpolationHalfLength);
                m_interpolationTaps[r][m] = (float) (sinc * window);
                sum += sinc * window;
            }

            for (float& tap : m_interpolationTaps[r]) {
                tap = (float) (tap / sum);
            }
        }

        m_tapPower.assign(spacing, 0.0f);

        for (int r = 0; r < spacing; r++)
        {
            for (float tap : m_interpolationTaps[r]) {
                m_tapPower[r] += tap * tap;
            }
        }

        m_interpolationSpacing = spacing;
    }

    if (m_miso)
    {
        // Each group of transmitters' channel, centred on its own mean delay
        separateMISO(m_l < m_np2 ? m_p2Hold.data() : m_hold.data(), spacing, offset, kMax, true);
        m_h2.resize(kMax + 1);
        m_delay = m_misoDelay[0];
        interpolateGrid(m_misoGrid[0].data(), m_h.data(), spacing, offset, m_misoDelay[0]);
        interpolateGrid(m_misoGrid[1].data(), m_h2.data(), spacing, offset, m_misoDelay[1]);
    }
    else
    {
        // Remove the mean delay, so the response varies slowly from pilot to pilot
        Cd slope(0.0, 0.0);

        for (int j = 1; j < nbPilots; j++) {
            slope += Cd(m_hold[offset + spacing * j] * std::conj(m_hold[offset + spacing * (j - 1)]));
        }

        // Centred on the mean delay, from the phase change from pilot to pilot, which is nearer the stronger paths, so
        // interpolates them best. But with time interpolation, used for long echoes, or a spread over a fifth of the
        // span the pilots can represent (N / spacing, as in P2 symbols), on the middle of the paths, from the impulse
        // response, which keeps them all well within it
        if ((hold || (m_spread > 0.2 * n / spacing)) && m_centreValid) {
            m_delay = m_centre;
        } else {
            m_delay = -std::arg(slope) * n / (m_twoPi * spacing);
        }
        interpolateGrid(m_hold.data(), m_h.data(), spacing, offset, m_delay);
    }

    // The power gain of each carrier's taps, for the noise in its estimate
    m_carrierTapPower.resize(kMax + 1);

    for (int k = 0, r = ((-offset) % spacing + spacing) % spacing; k <= kMax; k++)
    {
        m_carrierTapPower[k] = m_tapPower[r];

        if (++r == spacing) {
            r = 0;
        }
    }
}

// The second group's pilot sign (9.2.8): P2 pilots inverted on odd multiples of 3; edge pilots on odd symbols;
// scattered, frame closing and continual pilots on the scattered pilot carriers inverted on alternate ones (Kext is
// a multiple of Dx, so those are the multiples of Dx); the rest not
float OFDMDemod::misoSign(int k, int l) const
{
    if (l < m_np2) {
        return (k % 6 == 3) ? -1.0f : 1.0f;
    }

    const Tables *t = m_tables;

    if ((k == 0) || (k == t->m_kMax)) {
        return (l & 1) ? -1.0f : 1.0f;
    }

    if (k % t->m_dx == 0) {
        return ((k / t->m_dx) & 1) ? -1.0f : 1.0f;
    }

    return 1.0f;
}

double OFDMDemod::meanDelay(const Cf *grid, int spacing, int offset, int nbPilots) const
{
    Cd slope(0.0, 0.0);

    for (int j = 1; j < nbPilots; j++) {
        slope += Cd(grid[offset + spacing * j] * std::conj(grid[offset + spacing * (j - 1)]));
    }

    return -std::arg(slope) * m_fftSize / (m_twoPi * spacing);
}

// The held pilots on the grid are H1 + (-1)^j H2. Each channel, its mean delay removed (found from adjacent pilots,
// where each varies little), is the low half of the band of the grid (for H1) or of it alternated (for H2), so is
// separated by a half-band filter: delays up to a quarter of N / spacing either side of the mean
// A filter of the pilots from left before to right after the one estimated, by least squares: passing the group's
// paths (within pass, cycles a pilot, of the centre), and stopping the rest, above all the other group's (from stop
// to half a cycle), which would make the cells confidently wrong, with some regard to the noise gain. A narrow pass
// band, for a channel without echoes, makes it a smoother, continuing the channel as it is. The integrals of each
// tap pair's product over the bands are sinc functions, so the normal equations are exact
void OFDMDemod::edgeFilter(double pass, double stop, int left, int right, std::vector<float>& taps)
{
    const int n = left + right + 1;
    const double transition = pass + 0.25; // Beyond, the rest of the stop band has a little weight
    const double restWeight = 1.0, otherWeight = 100.0, noiseWeight = 1e-4;
    // Of cos(2 pi f k) over |f| < pass, and over from < |f| < 0.5
    auto passIntegral = [pass](int k) { return k == 0 ? 2.0 * pass : std::sin(m_twoPi * pass * k) / (M_PI * k); };
    auto above = [](double from, int k) { return k == 0 ? 1.0 - 2.0 * from : -std::sin(m_twoPi * from * k) / (M_PI * k); };
    std::vector<double> a(n * n), b(n);

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            a[i * n + j] = passIntegral(i - j) + restWeight * above(transition, i - j) + otherWeight * above(stop, i - j)
                + (i == j ? noiseWeight : 0.0);
        }

        b[i] = passIntegral(i - left);
    }

    // Symmetric and positive definite: Gaussian elimination without pivoting is stable
    for (int c = 0; c < n; c++)
    {
        for (int r = c + 1; r < n; r++)
        {
            const double f = a[r * n + c] / a[c * n + c];

            for (int k = c; k < n; k++) {
                a[r * n + k] -= f * a[c * n + k];
            }

            b[r] -= f * b[c];
        }
    }

    taps.resize(n);

    for (int r = n - 1; r >= 0; r--)
    {
        double v = b[r];

        for (int k = r + 1; k < n; k++) {
            v -= a[r * n + k] * taps[k];
        }

        taps[r] = (float) (v / a[r * n + r]);
    }
}

void OFDMDemod::separateMISO(const Cf *source, int spacing, int offset, int kMax, bool useCentres)
{
    const int nbPilots = (kMax - offset) / spacing + 1;
    static const int half = 63;

    // The mean delay of both, from pilots two apart, which have the same sign (from adjacent ones, the second
    // group's would subtract)
    Cd lag2(0.0, 0.0);

    for (int j = 2; j < nbPilots; j++) {
        lag2 += Cd(source[offset + spacing * j] * std::conj(source[offset + spacing * (j - 2)]));
    }

    const double common = -std::arg(lag2) * m_fftSize / (m_twoPi * 2 * spacing);

    // With it removed, the channels vary little from pilot to pilot, so roughly H1 is the sum of adjacent pilots, and
    // H2 the difference: their mean delays, relative to it
    std::vector<Cf>& g1 = m_misoGrid[0];
    std::vector<Cf>& g2 = m_misoGrid[1];
    g1.assign(kMax + 1, Cf(0.0f, 0.0f));
    g2.assign(kMax + 1, Cf(0.0f, 0.0f));
    const Cd commonStep = std::polar(1.0, m_twoPi * common * spacing / m_fftSize);
    Cd commonRamp = std::polar(1.0, m_twoPi * common * offset / m_fftSize);
    Cf previous = source[offset] * Cf(commonRamp);

    for (int j = 0; j + 1 < nbPilots; j++)
    {
        commonRamp *= commonStep;
        const Cf a = previous, b = source[offset + spacing * (j + 1)] * Cf(commonRamp);
        g1[offset + spacing * j] = 0.5f * (a + b);
        g2[offset + spacing * j] = 0.5f * (a - b) * ((j & 1) ? -1.0f : 1.0f);
        previous = b;
    }

    // With time interpolation, each is centred on the middle of its paths, from its impulse response, as the mean
    // delay is nearer the stronger, which would leave a long echo near the edge of the filter's band
    double *delays = m_misoDelay;
    delays[0] = common + meanDelay(g1.data(), spacing, offset, nbPilots - 1);
    delays[1] = common + meanDelay(g2.data(), spacing, offset, nbPilots - 1);

    // Only one known: the other, if there, is most likely near it (the rough estimate of an absent group's delay is
    // that of the other's image)
    for (int g = 0; g < 2; g++)
    {
        if (useCentres && m_misoCentreValid[g]) {
            delays[g] = m_misoCentre[g];
        } else if (useCentres && m_misoCentreValid[1 - g]) {
            delays[g] = m_misoCentre[1 - g];
        }
    }

    // Each group's filter passes delays up to half way to the other's image, which is half the grid's period
    // (N / spacing) from it: a half-band filter, unless the groups' delays differ, when the image is nearer
    const double period = (double) m_fftSize / spacing;

    for (int g = 0; g < 2; g++)
    {
        double distance = 0.5 * period;

        if (useCentres && m_misoCentreValid[0] && m_misoCentreValid[1])
        {
            double d = delays[1 - g] + 0.5 * period - delays[g];
            d -= period * std::round(d / period);
            distance = std::abs(d);
        }

        // Rounded, so the edges' filters, made for each, are kept as the groups' delays wander
        const double cutoff = std::round(200.0 * std::max(0.05, std::min(0.25, 0.5 * distance / period))) / 200.0; // Cycles a pilot
        std::vector<float>& taps = m_misoTaps[g];
        taps.resize(2 * half + 1);
        m_misoCutoff[g] = cutoff;
        m_misoTapSum[g] = 0.0f;

        for (int m = -half; m <= half; m++)
        {
            const double x = 2.0 * cutoff * m;
            const double sinc = m == 0 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            taps[m + half] = (float) (2.0 * cutoff * sinc * (0.5 + 0.5 * std::cos(M_PI * m / (half + 1))));
            m_misoTapSum[g] += taps[m + half];
        }
    }

    std::vector<Cf>& u = m_misoPilots;
    u.resize(nbPilots);

    for (int g = 0; g < 2; g++)
    {
        const Cd step = std::polar(1.0, m_twoPi * delays[g] * spacing / m_fftSize);
        Cd ramp = std::polar(1.0, m_twoPi * delays[g] * offset / m_fftSize);

        for (int j = 0; j < nbPilots; j++)
        {
            const float sign = (g == 1) && (j & 1) ? -1.0f : 1.0f;
            u[j] = source[offset + spacing * j] * Cf(ramp) * sign;
            ramp *= step;
        }

        // Near the ends, where there are fewer pilots on one side, the filter is truncated, and renormalised it
        // doesn't cancel the other group's alternating part, which leaks into the edge carriers' estimates (by up to
        // a third), and the cells there, solved with them, are confidently wrong, keeping LDPC from converging. So
        // there, each position has its own filter of the pilots it has, designed to pass this group and stop the
        // other, by least squares
        // The pass band, the group's paths about its centre, if measured, else what the full filter passes; the stop
        // band, the other group's, half a cycle away from the centre, offset by the difference of their delays.
        // Rounded, so the filters are reused as these wander
        auto cycles = [this, spacing](double samples) { return samples * spacing / m_fftSize; };
        const double margin = 0.02;
        double pass = m_misoCutoff[g] - 0.05;
        double stop = m_misoCutoff[g] + 0.05;

        if (useCentres && m_misoCentreValid[0] && m_misoCentreValid[1] && (m_misoSpread[0] >= 0.0) && (m_misoSpread[1] >= 0.0))
        {
            pass = cycles(m_misoSpread[g] / 2.0) + margin;
            double offsetCycles = cycles(delays[1 - g] - delays[g]);
            offsetCycles -= std::round(offsetCycles);
            stop = 0.5 - std::abs(offsetCycles) - margin;
        }

        const int passKey = (int) std::lround(200.0 * std::max(0.02, std::min(pass, m_misoCutoff[g] - 0.02)));
        const int stopKey = std::max(passKey + 10, (int) std::lround(200.0 * std::min(stop, 0.48)));

        if (m_misoEdgeTaps.size() > 8192) {
            m_misoEdgeTaps.clear(); // Bands that have come and gone
        }

        const int last = nbPilots - 1;
        std::vector<Cf>& out = m_misoGrid[g];
        Cd unramp = std::polar(1.0, -m_twoPi * delays[g] * offset / m_fftSize);
        const Cd unstep = std::conj(step);
        const float *taps = m_misoTaps[g].data();

        for (int j = 0; j < nbPilots; j++)
        {
            const int left = std::min(half, j), right = std::min(half, last - j);
            Cf sum(0.0f, 0.0f);

            if ((left == half) && (right == half))
            {
                for (int m = -half; m <= half; m++) {
                    sum += u[j + m] * taps[m + half];
                }

                sum /= m_misoTapSum[g];
            }
            else
            {
                std::vector<float>& edge = m_misoEdgeTaps[{passKey, stopKey, left, right}];

                if (edge.empty()) {
                    edgeFilter(passKey / 200.0, stopKey / 200.0, left, right, edge);
                }

                for (int m = -left; m <= right; m++) {
                    sum += u[j + m] * edge[m + left];
                }
            }

            out[offset + spacing * j] = sum * Cf(unramp);
            unramp *= unstep;
        }
    }

    // A group that's absent (or far weaker): what's separated is the noise, and any leak of the other's, which would
    // spoil the decoding, so none
    double power[2] = {0.0, 0.0};

    for (int g = 0; g < 2; g++)
    {
        for (int k = offset; k <= kMax; k += spacing) {
            power[g] += std::norm(m_misoGrid[g][k]);
        }
    }

    for (int g = 0; g < 2; g++)
    {
        m_misoPresent[g] = power[g] >= power[1 - g] / 30.0;

        if (!m_misoPresent[g]) {
            std::fill(m_misoGrid[g].begin(), m_misoGrid[g].end(), Cf(0.0f, 0.0f));
        }
    }
}

void OFDMDemod::cirClusters(int kMax, int spacing, int offset, int windowOffset, std::vector<double>& delays) const
{
    delays.clear();
    const int nbPilots = (kMax - offset) / spacing + 1;
    int size = 64;

    while (size < 2 * nbPilots) {
        size <<= 1;
    }

    const int nbBins = (int) m_cirBins.size();

    if ((nbBins == 0) || ((int) m_cirPower.size() != size)) {
        return;
    }

    const double period = (double) m_fftSize / spacing;
    const double resolution = period / size;
    const double expected = m_guardSamples - windowOffset;

    // Bins within 4 of each other are a cluster, starting after the largest gap so none wraps
    int start = 0, largest = -1;

    for (int i = 0; i < nbBins; i++)
    {
        const int gap = (m_cirBins[(i + 1) % nbBins] - m_cirBins[i] + size) % size;

        if (gap > largest)
        {
            largest = gap;
            start = (i + 1) % nbBins;
        }
    }

    std::vector<std::pair<double, double>> clusters; // Power, delay
    double power = 0.0, moment = 0.0;
    const int origin = m_cirBins[start];

    for (int n = 0; n < nbBins; n++)
    {
        const int b = m_cirBins[(start + n) % nbBins];
        const int position = (b - origin + size) % size;
        power += m_cirPower[b];
        moment += m_cirPower[b] * position;
        const bool last = (n == nbBins - 1) || ((m_cirBins[(start + n + 1) % nbBins] - b + size) % size > 4);

        if (last)
        {
            double delay = (origin + moment / power) * resolution;
            delay -= period * std::round((delay - expected) / period);
            clusters.push_back({power, delay});
            power = 0.0;
            moment = 0.0;
        }
    }

    std::sort(clusters.begin(), clusters.end(), [](const std::pair<double, double>& a, const std::pair<double, double>& b) { return a.first > b.first; });

    for (const auto& c : clusters)
    {
        if (c.first >= clusters[0].first / 30.0) {
            delays.push_back(c.second);
        }
    }
}

// The window is placed for the paths of both groups: their responses, measured separately, together
void OFDMDemod::misoImpulseResponse(int spacing, int offset, int windowOffset, int l)
{
    const int kMax = (int) m_hold.size() - 1;

    if ((int) m_misoGrid[0].size() != kMax + 1) {
        return;
    }

    double first = 1e30, end = -1e30;

    // Not a group that's absent, or far weaker
    for (int g = 0; g < 2; g++)
    {
        if (!m_misoPresent[g]) {
            continue;
        }

        measureImpulseResponse(m_misoGrid[g].data(), kMax, spacing, offset, windowOffset, l, false, &m_cirReadings);

        if (!m_cirReadings.empty())
        {
            first = std::min(first, m_cirReadings[0].m_delay);
            m_misoCentre[g] = m_cirReadings[0].m_delay + m_cirReadings[0].m_spread / 2.0;
            m_misoSpread[g] = m_cirReadings[0].m_spread;
            m_misoCentreValid[g] = true;
            end = std::max(end, m_cirReadings[0].m_delay + m_cirReadings[0].m_spread);
        }
    }

    if (end > first) {
        useImpulseResponse({first, end - first}, windowOffset);
    }
}

// Windowed sinc interpolation of the pilots in source (every spacing carriers from offset) to every carrier of dest,
// after removing a delay, so the response varies slowly from pilot to pilot, which is then restored
void OFDMDemod::interpolateGrid(const Cf *source, Cf *dest, int spacing, int offset, double delay)
{
    const int kMax = (int) m_y.size() - 1;
    const int nbPilots = (kMax - offset) / spacing + 1;
    const int n = m_fftSize;
    const Cd rampStep = std::polar(1.0, m_twoPi * delay * spacing / n);
    Cd ramp = std::polar(1.0, m_twoPi * delay * offset / n);

    // Pilots padded at each end by repeating the first and last, so every carrier's taps are in range
    const int taps = 2 * m_interpolationHalfLength;
    const int pad = m_interpolationHalfLength;
    m_padded.resize(nbPilots + 2 * pad);

    for (int j = 0; j < nbPilots; j++)
    {
        m_padded[j + pad] = source[offset + spacing * j] * Cf(ramp);
        ramp *= rampStep;
    }

    for (int j = 0; j < pad; j++)
    {
        m_padded[j] = m_padded[pad];
        m_padded[nbPilots + pad + j] = m_padded[nbPilots + pad - 1];
    }

    // The delay is restored at carrier k = offset + spacing * j + r by the product of a phasor for the pilot, j,
    // and one for the position after it, r
    const double unrampPhase = -m_twoPi * delay / n;
    m_pilotRamp.resize(nbPilots);
    m_phaseRamp.resize(spacing);
    const Cd pilotStep = std::polar(1.0, unrampPhase * spacing);
    Cd pilotRamp(1.0, 0.0);

    for (int j = 0; j < nbPilots; j++)
    {
        m_pilotRamp[j] = Cf(pilotRamp);
        pilotRamp *= pilotStep;
    }

    for (int r = 0; r < spacing; r++) {
        m_phaseRamp[r] = Cf(std::polar(1.0, unrampPhase * (offset + r)));
    }

    // For each position between pilots, convolve the pilots with its taps: out[j] = sum of taps[m] * pilot[j - h + 1 + m],
    // a block of outputs at a time, accumulating tap by tap, which vectorises. Then restore the delay
    m_h.resize(kMax + 1);
    m_phaseOut.resize(nbPilots);
    const float *in = reinterpret_cast<const float *>(m_padded.data() + pad - m_interpolationHalfLength + 1);
    float *out = reinterpret_cast<float *>(m_phaseOut.data());
    const int floats = 2 * nbPilots;
    static const int block = 16;

    for (int r = 0; r < spacing; r++)
    {
        const float *h = m_interpolationTaps[r].data();
        int f = 0;

        for (; f + block <= floats; f += block)
        {
#if defined(DVBT2OFDM_SSE2)
            // In registers, as the compiler doesn't keep the array there
            __m128 acc0 = _mm_setzero_ps(), acc1 = _mm_setzero_ps(), acc2 = _mm_setzero_ps(), acc3 = _mm_setzero_ps();

            for (int m = 0; m < taps; m++)
            {
                const __m128 hm = _mm_set1_ps(h[m]);
                const float *x = in + f + 2 * m;
                acc0 = _mm_add_ps(acc0, _mm_mul_ps(hm, _mm_loadu_ps(x)));
                acc1 = _mm_add_ps(acc1, _mm_mul_ps(hm, _mm_loadu_ps(x + 4)));
                acc2 = _mm_add_ps(acc2, _mm_mul_ps(hm, _mm_loadu_ps(x + 8)));
                acc3 = _mm_add_ps(acc3, _mm_mul_ps(hm, _mm_loadu_ps(x + 12)));
            }

            _mm_storeu_ps(out + f, acc0);
            _mm_storeu_ps(out + f + 4, acc1);
            _mm_storeu_ps(out + f + 8, acc2);
            _mm_storeu_ps(out + f + 12, acc3);
#else
            float acc[block] = {};

            for (int m = 0; m < taps; m++)
            {
                const float hm = h[m];
                const float *x = in + f + 2 * m;

                for (int i = 0; i < block; i++) {
                    acc[i] += hm * x[i];
                }
            }

            std::copy(acc, acc + block, out + f);
#endif
        }

        for (; f < floats; f++)
        {
            float acc = 0.0f;

            for (int m = 0; m < taps; m++) {
                acc += h[m] * in[f + 2 * m];
            }

            out[f] = acc;
        }

        const Cf phaseRamp = m_phaseRamp[r];
        int j = 0, k = offset + r;
#if defined(DVBT2OFDM_SSE2)
        // Two carriers at a time
        const __m128 phaseRamp2 = _mm_castpd_ps(_mm_load1_pd(reinterpret_cast<const double *>(&phaseRamp)));

        for (; k + spacing <= kMax; j += 2, k += 2 * spacing)
        {
            const __m128 ramp = complexMultiply2(_mm_loadu_ps(reinterpret_cast<const float *>(&m_pilotRamp[j])), phaseRamp2);
            const __m128 value = complexMultiply2(_mm_loadu_ps(reinterpret_cast<const float *>(&m_phaseOut[j])), ramp);
            _mm_storel_pi(reinterpret_cast<__m64 *>(&dest[k]), value);
            _mm_storeh_pi(reinterpret_cast<__m64 *>(&dest[k + spacing]), value);
        }
#endif

        for (; k <= kMax; j++, k += spacing) {
            dest[k] = m_phaseOut[j] * (m_pilotRamp[j] * phaseRamp);
        }

        if (r == 0)
        {
            // Carriers before the first pilot take its value
            for (int k = 0; k < offset; k++) {
                dest[k] = m_phaseOut[0] * Cf(std::polar(1.0, unrampPhase * k));
            }
        }
    }
}

// Equalise the data cells and de-interleave them. The noise is measured on the pilots not used for the channel
// estimate. As the estimate is interpolated from noisy pilots, its error adds to the noise at those pilots, by
// the noise of the pilots (relative to their amplitude) times the power gain of the interpolation taps
void OFDMDemod::equalise(const std::vector<int>& data, const std::vector<int>& read, const std::vector<Tables::Pilot>& pilots, int l)
{
    PROFILER_START()
    const Tables *t = m_p2Tables && (l < m_np2) ? m_p2Tables : m_tables;
    const int pn = framePN(l);
    const float invGrid2 = 1.0f / (m_gridAmplitude * m_gridAmplitude);
    auto estimateGain = [this, invGrid2](int k) {
        return m_carrierTapPower[k] * invGrid2;
    };
    double noise = 0.0, weight = 0.0, power = 0.0;
    int count = 0;

    for (const Tables::Pilot& pilot : pilots)
    {
        int k = pilot.m_k;

        if (((k - m_gridOffset) % m_gridSpacing) == 0) {
            continue; // Used for the estimate
        }

        if (m_notched.erased(k)) {
            continue; // Under a notch
        }

        float value = (t->m_w[k] ^ pn) ? -pilot.m_amplitude : pilot.m_amplitude;
        const Cf h = m_miso ? m_h[k] + m_h2[k] * misoSign(k, l) : m_h[k];
        Cf e = m_y[k] - h * value;
        const double sampleWeight = 1.0 + pilot.m_amplitude * pilot.m_amplitude * estimateGain(k);
        carrierNoise(k, (float) (std::norm(e) / (2.0 * sampleWeight)));

        if (l >= m_np2)
        {
            if (m_pilotError.size() != m_y.size())
            {
                m_pilotError.assign(m_y.size(), 0.0);
                m_pilotCount.assign(m_y.size(), 0.0);
            }

            m_pilotError[k] += std::norm(e) / std::max(1e-12f, std::norm(h * value));
            m_pilotCount[k] += 1.0;
        }

        // The overall noise is of the carriers without interference
        if (!noisyCarrier(k))
        {
            noise += std::norm(e);
            weight += sampleWeight;
            count++;
        }
    }

    // The changes of the pilots from the held estimates, with time interpolation
    noise += m_holdNoise;
    weight += m_holdWeight;
    count += m_holdCount;

    for (int k : data) {
        power += std::norm(m_h[k]) + (m_miso ? std::norm(m_h2[k]) : 0.0f);
    }

    power /= std::max<size_t>(1, data.size());

    if (count > 8)
    {
        double estimate = noise / (2.0 * weight);

        if ((l >= m_np2) && (m_symbolSNRs.size() < 100000)) {
            m_symbolSNRs.push_back((float) (10.0 * std::log10(power / std::max(2.0 * estimate, 1e-12))));
        }
        m_noise = (m_noise < 0.0) || (m_noiseSettling > 0) ? estimate : 0.9 * m_noise + 0.1 * estimate;

        if ((m_noiseSettling > 0) && (l >= m_np2)) {
            m_noiseSettling--;
        }
        m_status.m_snr = 10.0 * std::log10(power / std::max(2.0 * m_noise, 1e-12));
    }

    double noiseVariance = m_noise > 0.0 ? m_noise : 0.005 * power;

    // Not 0 (a signal of zeros), whose infinite scale would make the cells' scales NaN
    if (!(noiseVariance > 1e-30)) {
        noiseVariance = 1e-30;
    }

    const int nbCells = (int) data.size();
    m_cells.resize(nbCells);
    m_llrScales.resize(nbCells);

    const float scale = (float) (4.0 / noiseVariance);

    if (++m_carrierWeightAge >= 4) {
        updateCarrierWeights(t->m_kMax);
    }

    const bool weighted = (int) m_carrierWeight.size() == t->m_kMax + 1;

    if (m_miso)
    {
        // Alamouti (9.1): the first group sends a0, a1 on a pair of data carriers, the second -a1*, a0*, so with each
        // carrier's channels, y0 = H1(0) a0 - H2(0) a1*, y1* = H1(1)* a1* + H2(1)* a0, solved for a0 and a1* exactly,
        // as the channels differ between the carriers (the window's delay alone turns them 0.2 rad a carrier in 8K at
        // GI 1/8): a0 = (H1(1)* y0 + H2(0) y1*) / D, a1* = (H1(0) y1* - H2(1)* y0) / D, D = H1(0) H1(1)* + H2(0) H2(1)*.
        // Each cell's reliability is |D|^2 over its row's norm (|H1|^2 + |H2|^2 when the channels are the same)
        m_misoCells.resize(nbCells);
        m_misoGain.resize(nbCells);

        for (int i = 0; i + 1 < nbCells; i += 2)
        {
            const int k0 = data[i], k1 = data[i + 1];
            const Cf h10 = m_h[k0], h11 = m_h[k1], h20 = m_h2[k0], h21 = m_h2[k1];
            const Cf y0 = m_y[k0], y1c = std::conj(m_y[k1]);
            const Cf d = h10 * std::conj(h11) + h20 * std::conj(h21);
            const float d2 = std::norm(d);

            if (d2 <= 0.0f)
            {
                m_misoCells[i] = m_misoCells[i + 1] = Cf(0.0f, 0.0f);
                m_misoGain[i] = m_misoGain[i + 1] = 0.0f;
                continue;
            }

            const Cf inv = std::conj(d) / d2;
            m_misoCells[i] = (std::conj(h11) * y0 + h20 * y1c) * inv;
            m_misoCells[i + 1] = std::conj((h10 * y1c - std::conj(h21) * y0) * inv);
            m_misoGain[i] = d2 / std::max(1e-12f, std::norm(h11) + std::norm(h20));
            m_misoGain[i + 1] = d2 / std::max(1e-12f, std::norm(h10) + std::norm(h21));
        }

        if (nbCells & 1)
        {
            m_misoCells[nbCells - 1] = Cf(0.0f, 0.0f);
            m_misoGain[nbCells - 1] = 0.0f;
        }

        for (int p = 0; p < nbCells; p++)
        {
            const int i = read[p];
            const int k = data[i];
            m_cells[p] = m_misoCells[i];
            m_llrScales[p] = scale * m_misoGain[i] / (1.0f + estimateGain(k));

            if (weighted) {
                m_llrScales[p] *= m_carrierWeight[k];
            }
        }
    }


    if (!m_miso)
    {
        // In carrier order, then de-interleaved, rather than reading each carrier's values in de-interleaved order
        m_eqCells.resize(nbCells);
        m_eqScales.resize(nbCells);
        int i = 0;
        // Part of the noise may be in proportion to the channel's power (as from phase noise, interference between
        // the carriers, or the channel changing between its pilots), measured across the carriers (in
        // updateCarrierWeights()): the cells of the stronger carriers are then less reliable than the noise overall
        // says, and confidently wrong, keeping LDPC from converging. So their confidence is reduced by the noise at
        // their power over that at the mean. Not raised for the weaker: there the channel estimate's own error, in
        // fades, matters more
        const bool shaped = m_noiseRatio > 0.0;
        const float shapeNumerator = (float) (1.0 + std::max(0.0, m_noiseRatio) * power);
        const float noiseRatio = (float) std::max(0.0, m_noiseRatio);
#if defined(DVBT2OFDM_SSE2)
        // Four at a time, with the same arithmetic as one at a time
        const __m128 vScale = _mm_set1_ps(scale);
        const __m128 vInvGrid2 = _mm_set1_ps(invGrid2);
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 zero = _mm_setzero_ps();
        const float *tapPower = m_carrierTapPower.data();
        const float *carrierWeight = m_carrierWeight.data();
        auto pairs = [zero](const Cf *v, int k0, int k1) {
            return _mm_loadh_pi(_mm_loadl_pi(zero, reinterpret_cast<const __m64 *>(&v[k0])), reinterpret_cast<const __m64 *>(&v[k1]));
        };

        const __m128 vShapeNumerator = _mm_set1_ps(shapeNumerator);
        const __m128 vNoiseRatio = _mm_set1_ps(noiseRatio);

        for (; i + 4 <= nbCells; i += 4)
        {
            const int k0 = data[i], k1 = data[i + 1], k2 = data[i + 2], k3 = data[i + 3];
            const __m128 h01 = pairs(m_h.data(), k0, k1), h23 = pairs(m_h.data(), k2, k3);
            const __m128 y01 = pairs(m_y.data(), k0, k1), y23 = pairs(m_y.data(), k2, k3);
            const __m128 hr = _mm_shuffle_ps(h01, h23, _MM_SHUFFLE(2, 0, 2, 0));
            const __m128 hi = _mm_shuffle_ps(h01, h23, _MM_SHUFFLE(3, 1, 3, 1));
            const __m128 yr = _mm_shuffle_ps(y01, y23, _MM_SHUFFLE(2, 0, 2, 0));
            const __m128 yi = _mm_shuffle_ps(y01, y23, _MM_SHUFFLE(3, 1, 3, 1));
            const __m128 h2 = _mm_add_ps(_mm_mul_ps(hr, hr), _mm_mul_ps(hi, hi));
            const __m128 inv = _mm_and_ps(_mm_cmpgt_ps(h2, zero), _mm_div_ps(one, h2));
            const __m128 re = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(yr, hr), _mm_mul_ps(yi, hi)), inv);
            const __m128 im = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(yi, hr), _mm_mul_ps(yr, hi)), inv);
            float *cells = reinterpret_cast<float *>(&m_eqCells[i]);
            _mm_storeu_ps(cells, _mm_unpacklo_ps(re, im));
            _mm_storeu_ps(cells + 4, _mm_unpackhi_ps(re, im));
            const __m128 gain = _mm_mul_ps(_mm_set_ps(tapPower[k3], tapPower[k2], tapPower[k1], tapPower[k0]), vInvGrid2);
            __m128 llrScale = _mm_div_ps(_mm_mul_ps(vScale, h2), _mm_add_ps(one, gain));

            if (shaped) {
                llrScale = _mm_mul_ps(llrScale, _mm_min_ps(one, _mm_div_ps(vShapeNumerator, _mm_add_ps(one, _mm_mul_ps(vNoiseRatio, h2)))));
            }

            if (weighted) {
                llrScale = _mm_mul_ps(llrScale, _mm_set_ps(carrierWeight[k3], carrierWeight[k2], carrierWeight[k1], carrierWeight[k0]));
            }

            _mm_storeu_ps(&m_eqScales[i], llrScale);
        }
#endif

        for (; i < nbCells; i++)
        {
            const int k = data[i];
            const float hr = m_h[k].real(), hi = m_h[k].imag();
            const float yr = m_y[k].real(), yi = m_y[k].imag();
            float h2 = hr * hr + hi * hi;
            float inv = h2 > 0.0f ? 1.0f / h2 : 0.0f;
            m_eqCells[i] = Cf((yr * hr + yi * hi) * inv, (yi * hr - yr * hi) * inv);
            float llrScale = scale * h2 / (1.0f + estimateGain(k));

            if (shaped) {
                llrScale *= std::min(1.0f, shapeNumerator / (1.0f + noiseRatio * h2));
            }

            if (weighted) {
                llrScale *= m_carrierWeight[k];
            }

            m_eqScales[i] = llrScale;
        }

        for (int p = 0; p < nbCells; p++)
        {
            const int i = read[p];
            m_cells[p] = m_eqCells[i];
            m_llrScales[p] = m_eqScales[i];
        }
    }

    // Erase the carriers under notches
    if (!m_notched.empty())
    {
        for (int p = 0; p < nbCells; p++)
        {
            if (m_notched.erased(data[read[p]]))
            {
                m_cells[p] = Cf(0.0f, 0.0f);
                m_llrScales[p] = 0.0f;
            }
        }
    }

    // In SISO with normal carriers, every P2 pilot is on the estimate's grid, leaving none to measure the noise on.
    // Until the data symbols had, the guess (noise of 0.005 of the channel's power per dimension) saturated L1's LLRs,
    // costing it 1.5 to 2 dB, so near the threshold L1-pre failed at every acquisition, and the data symbols were
    // never reached. So it's measured on the L1-pre cells (BPSK, on the real axis), whose imaginary parts are noise
    // (and any phase error, which only makes them less confident): relative to the noise their scales allow for
    // (4 / scale per dimension), which allows for each cell's channel and estimate. Every cell's scale is then for it
    if ((l < m_np2) && (m_noise <= 0.0) && (count <= 8))
    {
        const int preCells = std::min(l1PreCells / std::max(1, m_np2), nbCells);
        double ratio = 0.0;
        int measured = 0;

        for (int p = 0; p < preCells; p++)
        {
            if (m_llrScales[p] > 0.0f)
            {
                const double im = m_cells[p].imag();
                ratio += im * im * m_llrScales[p];
                measured++;
            }
        }

        if (measured >= 32)
        {
            // At most 37 dB (the noise 1/50 of the guess), as with no noise at all the scales would be infinite
            ratio = std::max(0.02, ratio / (4.0 * measured));
            const float rescale = (float) (1.0 / ratio);

            for (float& s : m_llrScales) {
                s *= rescale;
            }
        }
    }

    PROFILER_STOP("DVB-T2 OFDM equalisation")
}

// The carriers' noise and weights (which equalise() may work out again) too, and the SNR it reports
void OFDMDemod::saveNoise(NoiseState& state) const
{
    state.m_noise = m_noise;
    state.m_noiseRatio = m_noiseRatio;
    state.m_snr = m_status.m_snr;
    state.m_carrierNoise = m_carrierNoise;
    state.m_carrierWeight = m_carrierWeight;
}

void OFDMDemod::restoreNoise(NoiseState& state)
{
    m_noise = state.m_noise;
    m_noiseRatio = state.m_noiseRatio;
    m_status.m_snr = state.m_snr;
    m_carrierNoise.swap(state.m_carrierNoise);
    m_carrierWeight.swap(state.m_carrierWeight);
}

double OFDMDemod::l1PreError(int nbCells) const
{
    double error = 0.0;

    for (int p = 0; p < std::min(nbCells, (int) m_cells.size()); p++)
    {
        const float re = std::abs(m_cells[p].real()) - 1.0f;
        error += re * re + m_cells[p].imag() * m_cells[p].imag();
    }

    return error;
}

// A noise measurement on a carrier, averaged over time
void OFDMDemod::carrierNoise(int k, float variance)
{
    if (m_carrierNoise.size() != m_y.size()) {
        m_carrierNoise.assign(m_y.size(), -1.0f);
    }

    // Starting from the overall noise, as a single measurement is too noisy (exponentially distributed, it's over twice
    // the median a quarter of the time), and interference raises it within a few
    float& noise = m_carrierNoise[k];

    if (noise < 0.0f) {
        noise = m_noise > 0.0 ? (float) m_noise : variance;
    }

    noise += 0.05f * (variance - noise);
}

// Each carrier's weight: the median noise over its own, smoothed over the carriers either side (pilots are on a few of
// them each symbol), where that's more than twice the median, so carriers with interference are less confident. The
// rest are left alone, as the estimates' own noise would otherwise cost a little sensitivity (0.05 dB)
void OFDMDemod::updateCarrierWeights(int kMax)
{
    m_carrierWeightAge = 0;

    if ((int) m_carrierNoise.size() != kMax + 1) {
        return;
    }

    std::vector<float>& measured = m_measuredNoise;
    measured.clear();

    for (float n : m_carrierNoise)
    {
        if (n >= 0.0f) {
            measured.push_back(n);
        }
    }

    if (measured.size() < 64) {
        return;
    }

    std::nth_element(measured.begin(), measured.begin() + measured.size() / 2, measured.end());
    const float median = measured[measured.size() / 2];

    if (median <= 0.0f) {
        return;
    }

    // The noise's part proportional to the channel's power, relative to the rest: a least squares line through
    // each measured carrier's noise against its |H|^2, without those with interference
    {
        double n = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;

        for (int k = 0; k <= kMax; k++)
        {
            const float noise = m_carrierNoise[k];

            if ((noise < 0.0f) || (noise > 4.0f * median) || (k >= (int) m_h.size())) {
                continue;
            }

            const double x = std::norm(m_h[k]) + (m_miso && (k < (int) m_h2.size()) ? std::norm(m_h2[k]) : 0.0f);
            n += 1.0;
            sx += x;
            sy += noise;
            sxx += x * x;
            sxy += x * noise;
        }

        const double det = n * sxx - sx * sx;

        if ((n >= 64.0) && (det > 0.0))
        {
            const double slope = std::max(0.0, (n * sxy - sx * sy) / det);
            const double intercept = (sy - slope * sx) / n;
            // At most 10 times the rest at the mean power, and no more than that if the intercept is near nothing
            const double meanX = sx / n;
            double ratio = intercept > 0.0 ? slope / intercept : 10.0 / std::max(meanX, 1e-12);
            ratio = std::min(ratio, 10.0 / std::max(meanX, 1e-12));
            m_noiseRatio = m_noiseRatio < 0.0 ? ratio : 0.8 * m_noiseRatio + 0.2 * ratio;
        }
    }

    // Sliding sums over k - half..k + half of the measured carriers
    const int half = 24;
    m_carrierWeight.resize(kMax + 1);
    double sum = 0.0;
    int count = 0;

    for (int k = 0; k <= std::min(kMax, half); k++)
    {
        if (m_carrierNoise[k] >= 0.0f)
        {
            sum += m_carrierNoise[k];
            count++;
        }
    }

    for (int k = 0; k <= kMax; k++)
    {
        const float smoothed = count > 0 ? (float) (sum / count) : median;
        const float weight = median / std::max(smoothed, 1e-12f);
        m_carrierWeight[k] = weight < 0.5f ? weight : 1.0f;

        if ((k + half + 1 <= kMax) && (m_carrierNoise[k + half + 1] >= 0.0f))
        {
            sum += m_carrierNoise[k + half + 1];
            count++;
        }

        if ((k - half >= 0) && (m_carrierNoise[k - half] >= 0.0f))
        {
            sum -= m_carrierNoise[k - half];
            count--;
        }
    }
}

// Normalised correlation of a symbol's guard interval (from start) with the end of the symbol it copies
double OFDMDemod::guardCorrelation(int64_t start) const
{
    const Cf *r = bufferAt(start);
    const Cf *s = r + m_fftSize;
    double re = 0.0, im = 0.0, e1 = 0.0, e2 = 0.0;

    for (int i = 0; i < m_guardSamples; i++)
    {
        const float ar = r[i].real(), ai = r[i].imag(), br = s[i].real(), bi = s[i].imag();
        re += ar * br + ai * bi;
        im += ai * br - ar * bi;
        e1 += ar * ar + ai * ai;
        e2 += br * br + bi * bi;
    }

    return (e1 > 0.0) && (e2 > 0.0) ? std::sqrt(re * re + im * im) / std::sqrt(e1 * e2) : 0.0;
}

// Whether the guard interval correlation is at least threshold elsewhere in the symbol from start (up to N samples
// later, which are there): the symbols have moved, as when samples are lost, rather than faded, which lowers it
// everywhere. A move into the last Ng samples of the symbol still overlaps one end or the other by half the guard
// interval, so correlates by half as much
bool OFDMDemod::guardMoved(int64_t start, double threshold) const
{
    const int n = m_fftSize;
    const int ng = m_guardSamples;
    const Cf *r = bufferAt(start);
    double re = 0.0, im = 0.0, e1 = 0.0, e2 = 0.0;
    // Sliding sums over the guard interval's length, at each offset d
    auto add = [&](int i, double sign) {
        const double ar = r[i].real(), ai = r[i].imag(), br = r[i + n].real(), bi = r[i + n].imag();
        re += sign * (ar * br + ai * bi);
        im += sign * (ai * br - ar * bi);
        e1 += sign * (ar * ar + ai * ai);
        e2 += sign * (br * br + bi * bi);
    };

    for (int i = 0; i < ng; i++) {
        add(i, 1.0);
    }

    const double threshold2 = threshold * threshold;

    for (int d = 0; d <= n; d++)
    {
        if ((d > 0) && (e1 > 0.0) && (e2 > 0.0) && (re * re + im * im >= threshold2 * e1 * e2)) {
            return true;
        }

        if (d < n)
        {
            add(d + ng, 1.0);
            add(d, -1.0);
        }
    }

    return false;
}

// Piecewise, if the clock offset changes part way through the frame
int64_t OFDMDemod::framePosition(int64_t offset) const
{
    const double before = m_status.m_clockOffset - m_clockChangeDelta;
    int64_t position = m_frameStart + offset + std::llround(offset * before * 1e-6);

    if ((m_clockChangeDelta != 0.0) && (position > m_clockChangeSample)) {
        position += std::llround((position - m_clockChangeSample) * m_clockChangeDelta * 1e-6);
    }

    return position;
}

void OFDMDemod::adjustClockOffset(double ppm, int64_t sample)
{
    // One change at a time: an earlier one is taken as having been at the frame's start
    m_clockChangeSample = sample;
    m_clockChangeDelta = (m_frameStart >= 0) && (sample > m_frameStart) ? ppm : 0.0;
    m_status.m_clockOffset += ppm;
    m_clockSum += ppm * m_clockCount; // The frame's measurements, as of the new offset
    m_lastSearchedP1 = -1;            // The spacing of P1s either side would be of both offsets

    // The impulse responses before a large correction were spoilt by it (as in nextP1())
    if (std::abs(ppm) * m_fftSize > 5.0 * 32768.0)
    {
        m_firstPathSum = 0.0;
        m_spreadSum = 0.0;
        m_cirCount = 0;
    }
}

bool OFDMDemod::processSymbol()
{
    const int n = m_fftSize;
    const int ns = n + m_guardSamples;
    const int64_t start = framePosition(m_p1Length + (int64_t) m_l * ns);

    if (bufferEnd() < start + ns + n) {
        return false;
    }

    // Demodulated again after its impulse response was measured: its timing and frequency already were
    const bool repeat = m_repeatSymbol;
    m_repeatSymbol = false;

    // A timing jump (samples lost, as by an SDR that can't keep up, or a recording of one): the guard interval no
    // longer correlates with the end of the symbol. Two symbols in a row, so a burst of interference isn't mistaken
    // for one, and the correlation peaking elsewhere in the symbol, so a fade (which lowers it everywhere) isn't
    // either; or six in a row, in case the jump's peak is too weak to see. The rest of the frame is lost, and the
    // next P1 found by searching from here. Not with guard intervals too short to measure reliably (as 1/128 in 1K
    // is 8 samples)
    if ((m_guardSamples >= 64) && !repeat)
    {
        const double gi = guardCorrelation(start);

        if ((m_giAverage > 0.0) && (gi < 0.35 * m_giAverage))
        {
            if ((++m_giLow >= 2) && ((m_giLow >= 6) || guardMoved(start, 0.5 * m_giAverage)))
            {
                log("Timing lost: guard interval correlation %.2f, %.2f usually: searching for the next P1", gi, m_giAverage);
                m_giLow = 0;
                m_giAverage = 0.0;
                m_searchPos = std::max(m_bufferOffset, start - ns);
                m_status.m_frameKnown = false;
                m_status.m_state = SearchP1;
                return true;
            }
        }
        else
        {
            m_giLow = 0;
            m_giAverage = m_giAverage <= 0.0 ? gi : 0.98 * m_giAverage + 0.02 * gi;
        }
    }

    const int l = m_l;
    const bool p2 = l < m_np2;
    const bool fc = !p2 && m_frameClosing && (l == m_lf - 1);
    const Tables *t = p2 ? m_p2Tables : m_tables;

    if (l == 0) {
        updateWindowOffset();
    }

    const int64_t window = start + windowOffset();

    PROFILER_START()

    if (!repeat) {
        trackFrequency(start);
    }

    m_fft.configure(n, false);
    fft(window);
    PROFILER_STOP("DVB-T2 OFDM NCO and FFT")

    const int kMax = t->m_kMax;
    m_y.resize(kMax + 1);
    m_h.resize(kMax + 1);

    if (!m_notched.empty()) {
        m_notched.prepare(kMax, m_sampleRate / n, m_frequency * m_sampleRate);
    }

    if ((int) m_hold.size() != kMax + 1)
    {
        // From normal to extended carrier mode, when the first L1-pre says so: the same carriers, Kext further up
        std::vector<Cf> hold(kMax + 1, Cf(0.0f, 0.0f));
        std::vector<float> amplitude(kMax + 1, 0.0f);
        int shift = (kMax + 1 - (int) m_hold.size()) / 2;

        if ((shift == kExt(n)) && (m_holdAmplitude.size() == m_hold.size()))
        {
            for (size_t k = 0; k < m_hold.size(); k++)
            {
                hold[k + shift] = m_hold[k];
                amplitude[k + shift] = m_holdAmplitude[k];
            }
        }

        m_hold.swap(hold);
        m_holdAmplitude.swap(amplitude);
    }

    // As if the window were at the symbol's exact position, rather than the nearest sample, so the symbols' timing
    // is consistent for time interpolation, as the clock offset moves them a fraction of a sample at a time. A window
    // early by f samples delays the signal by f, which rotates bin b by -2 pi b f / N
    // The exact drift accumulates a symbol at a time, with the latest clock offset: this frame's measurements once
    // there are a few, as they are more up to date than the frame's estimate, which framePosition() uses. A change of
    // the estimate then moves later symbols, rather than all of them at once, which the clock measurement would
    // mistake for the symbols moving
    // Before a change of the offset, the old one
    const double change = (m_clockChangeDelta != 0.0) && (start < m_clockChangeSample) ? m_clockChangeDelta : 0.0;
    const double clockOffset = (m_clockCount >= 2 ? m_clockSum / m_clockCount : m_status.m_clockOffset) - change;

    if (l == 0) {
        m_exactDrift = m_p1Length * (m_status.m_clockOffset - change) * 1e-6;
    } else {
        m_exactDrift += ns * clockOffset * 1e-6;
    }

    m_previousTimingFraction = m_timingFraction;
    const int64_t nominal = m_p1Length + (int64_t) l * ns;
    m_timingFraction = m_exactDrift - (double) (framePosition(nominal) - m_frameStart - nominal);

    extractCarriers(kMax);
    Symbol symbol;
    symbol.m_frame = m_frameCount;
    symbol.m_l = l;
    symbol.m_p2 = p2;
    symbol.m_frameClosing = fc;

    if (p2)
    {
        // Pilots every 6 carriers in 32K SISO, else every 3
        int spacing = (n == 32768) && !m_miso ? 6 : 3;

        // The first P2 symbol after acquisition: its pilots' impulse response, unambiguous over N / 3 (or N / 6), more
        // than any guard interval. The symbol is demodulated again, with the window placed for it and, for a long
        // echo, time interpolation in the data symbols, rather than with the defaults until the frame's data symbols
        // have measured it, by when L1 may have failed, and the frame be lost
        if (!m_p2Measured && (l == 0))
        {
            m_p2Measured = true;
            const int pn = framePN(l);
            m_p2Estimates.assign(kMax + 1, Cf(0.0f, 0.0f));

            for (const Tables::Pilot& pilot : t->m_p2Pilots)
            {
                if (pilot.m_k % spacing == 0)
                {
                    const float value = (t->m_w[pilot.m_k] ^ pn) ? -pilot.m_amplitude : pilot.m_amplitude;
                    m_p2Estimates[pilot.m_k] = m_y[pilot.m_k] / value;
                }
            }

            const int offset = (int) (window - start);

            if (m_miso)
            {
                // The response of the pilots as received has each group's paths, the second's half the grid's
                // period (N / 3) away, which with the groups' delays differing can't be told from the first's (paths
                // at 0 and -200 samples in 2K at 1/4 as 0 and 141), nor, with one group, from an echo. So, as the
                // readings of a long echo in SISO, the ways the clusters of paths could be the groups' are tried, and
                // the one whose L1-pre cells are nearest their points used: the rough separation first (right when the
                // groups' delays are near, as usual), used if its cells are clean; then all the paths one group's
                // (the strongest cluster the first group's, or the second's image), and with two clusters, each one
                // group's
                std::vector<double> clusters;
                measureImpulseResponse(m_p2Estimates.data(), kMax, spacing, 0, offset, l, false, &m_cirReadings);
                cirClusters(kMax, spacing, 0, offset, clusters);

                if (!clusters.empty())
                {
                    const double period = (double) n / spacing;
                    const double expected = m_guardSamples - offset;
                    auto near = [&](double d) { return d - period * std::round((d - expected) / period); };
                    struct Hypothesis { double m_centre[2]; bool m_valid; };
                    std::vector<Hypothesis> hypotheses;
                    hypotheses.push_back({{0.0, 0.0}, false});
                    hypotheses.push_back({{clusters[0], clusters[0]}, true});
                    hypotheses.push_back({{near(clusters[0] - 0.5 * period), near(clusters[0] - 0.5 * period)}, true});

                    if (clusters.size() >= 2)
                    {
                        hypotheses.push_back({{clusters[0], near(clusters[1] - 0.5 * period)}, true});
                        hypotheses.push_back({{clusters[1], near(clusters[0] - 0.5 * period)}, true});
                    }

                    NoiseState saved;
                    saveNoise(saved);
                    const int preCells = std::min(l1PreCells / std::max(1, m_np2), (int) t->m_p2Data.size());
                    double bestError = -1.0;
                    size_t best = 0;

                    for (size_t h = 0; h < hypotheses.size(); h++)
                    {
                        if ((h == 1) && (bestError < 0.1 * preCells)) {
                            break;
                        }

                        m_misoCentre[0] = hypotheses[h].m_centre[0];
                        m_misoCentre[1] = hypotheses[h].m_centre[1];
                        m_misoCentreValid[0] = m_misoCentreValid[1] = hypotheses[h].m_valid;
                        estimateChannel(t->m_p2Pilots, spacing, 0, l, false, t->m_p2Pilots[0].m_amplitude);
                        equalise(t->m_p2Data, t->p2Deinterleave(l), t->m_p2Pilots, l);
                        const double error = l1PreError(preCells);

                        if ((bestError < 0.0) || (error < bestError))
                        {
                            bestError = error;
                            best = h;
                        }
                    }

                    restoreNoise(saved);
                    m_misoCentre[0] = hypotheses[best].m_centre[0];
                    m_misoCentre[1] = hypotheses[best].m_centre[1];
                    m_misoCentreValid[0] = m_misoCentreValid[1] = hypotheses[best].m_valid;
                }

                separateMISO(m_p2Estimates.data(), spacing, 0, kMax, true);
                misoImpulseResponse(spacing, 0, offset, l);

                if (m_cirCount > 0)
                {
                    m_repeatSymbol = true;
                    return true; // Again, from updateWindowOffset()
                }
            }
            else
            {
                measureImpulseResponse(m_p2Estimates.data(), kMax, spacing, 0, offset, l, false, &m_cirReadings);
            }

            if (!m_miso && !m_cirReadings.empty())
            {
                // The response repeats every N / 3 (or N / 6), so with a long echo it can be read more than one way
                // (paths at 0 and 450 samples in 2K as -233 and 0). The way that interpolates the channel between the
                // pilots right has the L1-pre's BPSK cells (the first of each P2 symbol) nearest their points
                size_t best = 0;

                if (m_cirReadings.size() > 1)
                {
                    NoiseState saved;
                    saveNoise(saved);
                    const int preCells = std::min(l1PreCells / std::max(1, m_np2), (int) t->m_p2Data.size());
                    double bestError = -1.0;

                    for (size_t r = 0; r < m_cirReadings.size(); r++)
                    {
                        // With the window where the reading would put it, as a long pre-echo read as a post-echo
                        // leaves the window past the end of its symbol, and the interference spoils both readings
                        const CIRReading& reading = m_cirReadings[r];
                        const int trialOffset = windowOffsetFor(reading.m_delay - m_guardSamples + offset, reading.m_spread);
                        const int step = trialOffset - offset;
                        fft(start + trialOffset);
                        extractCarriers(kMax);
                        m_centre = reading.m_delay - step + reading.m_spread / 2.0;
                        m_centreValid = true;
                        m_spread = reading.m_spread;
                        estimateChannel(t->m_p2Pilots, spacing, 0, l, false, t->m_p2Pilots[0].m_amplitude);
                        equalise(t->m_p2Data, t->p2Deinterleave(l), t->m_p2Pilots, l);
                        const double error = l1PreError(preCells);

                        if ((bestError < 0.0) || (error < bestError))
                        {
                            bestError = error;
                            best = r;
                        }
                    }

                    restoreNoise(saved);
                }

                useImpulseResponse(m_cirReadings[best], offset);
                m_repeatSymbol = true;
                return true; // Again, from updateWindowOffset()
            }
        }

        estimateChannel(t->m_p2Pilots, spacing, 0, l, false, t->m_p2Pilots[0].m_amplitude);
        equalise(t->m_p2Data, t->p2Deinterleave(l), t->m_p2Pilots, l);

        if (m_miso && (l == m_np2 - 1))
        {
            m_p2Channels[0] = m_h;
            m_p2Channels[1] = m_h2;
        }
    }
    else if (fc)
    {
        estimateChannel(t->m_fcPilots, t->m_dx, 0, l, false, t->m_fcPilots[0].m_amplitude);
        equalise(t->m_fcData, t->fcDeinterleave(l), t->m_fcPilots, l);
    }
    else
    {
        // After acquisition, the noise measured on the pilots is far too high (25 times, with a 0 dB echo) until the
        // channel estimate has its first Dy data symbols, and averaging it would take 20 more symbols to come down,
        // with the LLRs too small until then (a quarter of the first frame lost). So for 2 Dy data symbols, each
        // symbol's own estimate is used
        if (m_noiseSettling < 0)
        {
            m_noiseSettling = 2 * t->m_dy;
            m_carrierNoise.clear();
            m_carrierWeight.clear();
        }

        // In MISO, the P2 pilots' signs differ from the data symbols' on the same carriers, so they can't refresh the
        // held pilots as in SISO, which are then from the previous frame (and its timing), or none in the first (a
        // frame start lost a sixth of its blocks). So before the first data symbol (when L1 has given the pattern),
        // they're made from the last P2 symbol's two channels, as the data symbols' pilots would be: H1 + (-1)^j H2.
        // Not pilots, they aren't used for the noise (negative)
        // (In the first frame of extended mode, the P2 symbols were of the normal carriers, Kext further down)
        const std::vector<Cf>& p2H1 = m_p2Channels[0];
        const std::vector<Cf>& p2H2 = m_p2Channels[1];
        const int shift = ((int) m_hold.size() - (int) p2H1.size()) / 2;

        if (m_miso && (l == m_np2) && !p2H1.empty() && (p2H2.size() == p2H1.size()) && ((shift == 0) || (shift == t->m_kExt)))
        {
            const int dxGrid = t->m_dx;
            const float amplitude = t->m_pilots[0].empty() ? 1.0f : t->m_pilots[0][0].m_amplitude;

            for (int k = t->m_kExt % dxGrid, j = 0; k < (int) m_hold.size(); k += dxGrid, j++)
            {
                const int kp = k - shift;

                if ((kp >= 0) && (kp < (int) p2H1.size()))
                {
                    m_hold[k] = p2H1[kp] + p2H2[kp] * ((j & 1) ? -1.0f : 1.0f);
                    m_holdAmplitude[k] = -amplitude;
                }
            }
        }

        // Scattered pilots alone if frequency interpolation is enough, else held on every Dx carrier from earlier symbols
        const int phase = l % t->m_dy;
        const int period = t->m_dx * t->m_dy;
        const int spOffset = (t->m_kExt + t->m_dx * phase) % period;
        // Frequency interpolation alone, from this symbol's scattered pilots, is a little better in a single path
        // channel, as the estimate is fresh. But the pilots, period carriers apart, only represent delays over a span
        // of N / period, which echoes approaching the guard interval fill, so the estimate is then poor. Then use the
        // pilots of the last Dy symbols, on every Dx carrier, rotated to the same phase, as the standard needs anyway
        // for longer guard intervals
        const double span = (double) n / period;

        if (m_spread > 0.2 * span) {
            m_timeInterpolation = true;
        } else if (m_spread < 0.15 * span) {
            m_timeInterpolation = false;
        }

        const bool frequencyOnly = (period * m_guardSamples < n) && !m_timeInterpolation && !m_miso;
        const std::vector<Tables::Pilot>& pilots = t->m_pilots[phase];
        const std::vector<Tables::Pilot>& scattered = t->m_scattered[phase]; // Those of them spOffset + j period
        float amplitude = scattered.empty() ? 1.0f : scattered[0].m_amplitude;

        if (frequencyOnly) {
            estimateChannel(scattered, period, spOffset, l, false, amplitude);
        } else {
            estimateChannel(scattered, t->m_dx, t->m_kExt % t->m_dx, l, true, amplitude);
        }

        equalise(t->m_data[phase], t->dataDeinterleave(l), pilots, l);
        PROFILER_START()

        // Not while the guard interval correlation is low, which may be a timing jump, between this symbol and the
        // one before, nor between it and the next
        if (m_giLow > 0) {
            m_previousWindow = -1;
        } else {
            measureClock(window);
        }

        if (l % 2 == 0) {
            if (m_miso) {
                misoImpulseResponse(t->m_dx, t->m_kExt % t->m_dx, (int) (window - start), l);
            } else {
                measureImpulseResponse(m_hold.data(), (int) m_hold.size() - 1, t->m_dx, t->m_kExt % t->m_dx, (int) (window - start), l, true);
            }
        }

        PROFILER_STOP("DVB-T2 OFDM clock and impulse response")
    }

    if (p2 || fc) {
        m_previousWindow = -1;
    }

    symbol.m_cells = m_cells.data();
    symbol.m_llrScales = m_llrScales.data();
    symbol.m_nbCells = (int) m_cells.size();

    if (m_callback)
    {
        PROFILER_START()
        m_callback(symbol);
        PROFILER_STOP("DVB-T2 OFDM symbol output (frame builder, L1)")
    }

    m_l++;

    if ((l == m_np2 - 1) && !m_frameKnown)
    {
        log("L1-pre not decoded: searching for the next P1");
        m_status.m_frameKnown = false;
        m_searchPos = start + ns;
        m_status.m_state = SearchP1;
    }
    else if (m_l >= m_lf)
    {
        m_status.m_frames++;
        m_status.m_state = NextP1;
    }

    return true;
}

// The carriers, from the FFT's bins, with the symbol's timing offset from its window (m_timingFraction, usually a
// fraction of a sample, but it can be several) removed, and any notches undone. Carrier k is bin (k - kMax / 2) mod N:
// those below kMax / 2 at the top of the bins, the rest from bin 0
void OFDMDemod::extractCarriers(int kMax)
{
    const int n = m_fftSize;
    const int centre = kMax / 2;
    const Cf *bins = m_fft.m_engine->out();
    const Cf *top = bins + n - centre;

    if (m_timingFraction == 0.0)
    {
        std::copy(top, top + centre, m_y.begin());
        std::copy(bins, bins + (kMax + 1 - centre), m_y.begin() + centre);
    }
    else
    {
        // Carrier k's phasor, e^(j 2 pi f (k - kMax / 2) / N): one for each block of 64 carriers, stepped from block to
        // block, times one for the carrier within the block, in double, so as exact (rounded to float) as stepping
        // carrier by carrier, without each carrier's having to wait for the last's
        const double stepPhase = m_twoPi * m_timingFraction / n;
        Cd fine[64];

        for (int j = 0; j < 64; j++) {
            fine[j] = std::polar(1.0, stepPhase * j);
        }

        const Cd blockStep = std::polar(1.0, stepPhase * 64);
        Cd block = std::polar(1.0, -m_twoPi * m_timingFraction * (kMax / 2) / n);

        for (int k0 = 0; k0 <= kMax; k0 += 64)
        {
            const int end = std::min(kMax + 1, k0 + 64);

            for (int k = k0; k < end; k++)
            {
                const Cf bin = k < centre ? top[k] : bins[k - centre];
                m_y[k] = bin * Cf(block * fine[k - k0]);
            }

            block *= blockStep;
        }
    }

    // Undo notches of interferers, before the FFT
    if (!m_notched.empty()) {
        m_notched.compensate(m_y.data());
    }
}

// The FFT window starts after the nominal symbol start (from the P1), by default a quarter of the guard interval
// before its end, which allows for pre-echoes and timing error. Once paths have been measured, the window is placed
// so they all fall within the guard interval: starting a quarter of the guard interval before the first path's
// useful part (as the default), or less if the spread needs it, with any spread the guard interval can't hold
// split evenly. Then the window starts within the guard interval of every path, so there is no interference from
// the symbols either side
// Where updateWindowOffset() places the window for paths from firstPath (samples after the nominal symbol start)
// spreading over spread
int OFDMDemod::windowOffsetFor(double firstPath, double spread) const
{
    const int guard = m_guardSamples;
    const double backoff = std::min((double) std::max(4, guard / 4), (guard - spread) / 2.0);
    return std::max(0, std::min(guard, (int) std::lround(firstPath + guard - backoff)));
}

int OFDMDemod::windowOffset() const
{
    if ((m_windowOffset >= 0) && (m_windowGuard == m_guardSamples)) {
        return m_windowOffset;
    }

    return m_guardSamples - std::max(4, m_guardSamples / 4);
}

void OFDMDemod::updateWindowOffset()
{
    if (m_cirCount > 0)
    {
        const double firstPath = m_firstPathSum / m_cirCount;
        const double spread = m_spreadSum / m_cirCount;
        const int guard = m_guardSamples;
        const int offset = windowOffsetFor(firstPath, spread);
        const int previous = windowOffset();

        if (offset != previous)
        {
            // The held pilot estimates are as if taken with the window where it was: rotate them to the new position
            const int step = offset - previous;
            const int kMax = (int) m_hold.size() - 1;
            const Cd rotStep = std::polar(1.0, m_twoPi * step / m_fftSize);
            Cd rot = std::polar(1.0, -m_twoPi * step * (kMax / 2) / m_fftSize);

            for (int k = 0; k <= kMax; k++)
            {
                m_hold[k] *= Cf(rot);
                rot *= rotStep;
            }

            m_centre -= step;
            m_misoCentre[0] -= step;
            m_misoCentre[1] -= step;

            if (std::abs(step) > 1) {
                log("FFT window moved %d samples: first path %.1f, spread %.1f samples", step, firstPath, spread);
            }
        }

        // The pilots' errors measured so far may be mostly from estimating the channel with the window or interpolation
        // wrong for it, as in the first frame with a long echo, rather than noise or interference on their carriers
        if ((std::abs(offset - previous) > 1) || (std::abs(spread - m_spread) > std::max(4.0, 0.25 * std::max(spread, m_spread))))
        {
            m_carrierNoise.clear();
            m_carrierWeight.clear();
        }

        m_windowOffset = offset;
        m_windowGuard = guard;
        m_spread = spread;
    }

    m_firstPathSum = 0.0;
    m_spreadSum = 0.0;
    m_cirCount = 0;
}

// How well the impulse response, read with its first path at bin first and delay firstDelay (the bins after it in
// order, each resolution later; from pilots starting at carrier offset), predicts this symbol's continual pilots: their
// normalised correlation. They aren't on the Dx grid, so they tell apart the readings the grid can't. From the
// response's peaks, which are few
double OFDMDemod::continualPilotFit(const Cf *cir, int size, double resolution, int first, double firstDelay, int offset, int l) const
{
    const std::vector<int>& cps = m_tables->m_continual;
    const int pn = framePN(l);
    Cd zp(0.0, 0.0);
    double zz = 0.0, pp = 0.0;

    for (int k : cps)
    {
        if (k >= (int) m_y.size()) {
            continue;
        }

        Cd predicted(0.0, 0.0);

        for (size_t i = 0; i < m_cirBins.size(); i++)
        {
            const int b = m_cirBins[i];
            const float power = m_cirPower[b];

            if ((power < m_cirPower[(b + 1) % size]) || (power < m_cirPower[(b + size - 1) % size])) {
                continue; // Not a peak
            }

            const double delay = firstDelay + ((b - first + size) % size) * resolution;
            predicted += Cd(cir[b]) * std::polar(1.0, -m_twoPi * (k - offset) * delay / m_fftSize);
        }

        const Cd z = Cd(m_y[k]) * (m_tables->m_w[k] ^ pn ? -1.0 : 1.0);
        zp += z * std::conj(predicted);
        zz += std::norm(z);
        pp += std::norm(predicted);
    }

    return (zz > 0.0) && (pp > 0.0) ? std::norm(zp) / (zz * pp) : 0.0;
}

// The impulse response, from an inverse FFT of the pilots spacing carriers apart (Dx, held from the last Dy symbols,
// or a P2 symbol's), repeats every N / spacing samples, which is several times the guard interval. The paths within 20 dB of the
// strongest are significant; the largest gap between them (circularly) is where the response ends and starts again.
// The delay of a path in the response is from the window start to the start of the path's useful part: the guard
// interval less the window's offset, for a path at the nominal symbol start
void OFDMDemod::measureImpulseResponse(const Cf *estimates, int kMax, int spacing, int offset, int windowOffset, int l,
    bool resolveAmbiguity, std::vector<CIRReading> *readings)
{
    const int nbPilots = (kMax - offset) / spacing + 1;
    int size = 64;

    while (size < 2 * nbPilots) {
        size <<= 1;
    }

    m_cirFft.configure(size, true);
    Cf *in = m_cirFft.m_engine->in();

    for (int j = 0; j < nbPilots; j++)
    {
        const float window = (float) (0.5 - 0.5 * std::cos(m_twoPi * (j + 0.5) / nbPilots)); // Hann, for low sidelobes
        in[j] = estimates[offset + spacing * j] * window;
    }

    std::fill(in + nbPilots, in + size, Cf(0.0f, 0.0f));
    m_cirFft.m_engine->transform();
    const Cf *out = m_cirFft.m_engine->out();
    m_cirPower.resize(size);
    float peak = 0.0f;

    for (int b = 0; b < size; b++)
    {
        m_cirPower[b] = std::norm(out[b]);
        peak = std::max(peak, m_cirPower[b]);
    }

    if (peak <= 0.0f) {
        return;
    }

    m_cirResolution = (double) m_fftSize / ((double) spacing * size);

    // Paths within 20 dB of the strongest, and well above the noise: 30 times the median bin (mostly noise), which
    // noise alone exceeds with a probability of about 1e-9
    m_cirSorted = m_cirPower;
    std::nth_element(m_cirSorted.begin(), m_cirSorted.begin() + size / 2, m_cirSorted.end());
    const float threshold = std::max(0.01f * peak, 30.0f * m_cirSorted[size / 2]);
    m_cirBins.clear();

    for (int b = 0; b < size; b++)
    {
        if (m_cirPower[b] >= threshold) {
            m_cirBins.push_back(b);
        }
    }

    if (m_cirBins.empty()) // Not finite
    {
        if (readings) {
            readings->clear();
        }

        return;
    }

    const int nbBins = (int) m_cirBins.size();
    int first = m_cirBins[0];
    int largestGap = 0;

    for (int i = 0; i < nbBins; i++)
    {
        const int next = m_cirBins[(i + 1) % nbBins];
        const int gap = nbBins == 1 ? size : (next - m_cirBins[i] + size) % size;

        if (gap > largestGap)
        {
            largestGap = gap;
            first = next;
        }
    }

    const double period = (double) m_fftSize / spacing;
    const double resolution = period / size;
    double spread = (size - largestGap) * resolution;
    // Of the delays the repetition allows, the one nearest where the window expects the first path
    double delay = first * resolution;
    const double expected = m_guardSamples - windowOffset;
    delay -= period * std::round((delay - expected) / period);

    if (readings)
    {
        // Each with its first path a period either side too, if that's within a guard interval of where expected
        readings->clear();
        readings->push_back({delay, spread});

        for (int i = 0; i < nbBins; i++)
        {
            const int next = nbBins > 1 ? m_cirBins[(i + 1) % nbBins] : first;
            const double candidateSpread = nbBins > 1 ? (size - (next - m_cirBins[i] + size) % size) * resolution : spread;

            if ((next != first) && (candidateSpread > m_guardSamples)) {
                continue;
            }

            double candidateDelay = next * resolution;
            candidateDelay -= period * std::round((candidateDelay - expected) / period);

            for (int m = -1; m <= 1; m++)
            {
                const double d = candidateDelay + m * period;

                if (((next != first) || (m != 0)) && (std::abs(d - expected) <= m_guardSamples)) {
                    readings->push_back({d, candidateSpread});
                }
            }

            if (nbBins == 1) {
                break;
            }
        }

        return;
    }

    // The response repeats every N / Dx samples, so a long echo can also be read as one before the main path (+700 as
    // -665 in 8K PP2). If another gap leaves a spread the guard interval could hold, the readings (each a period
    // either side, too) are told apart by the continual pilots
    if (resolveAmbiguity && (nbBins > 1) && m_tables && !m_tables->m_continual.empty() && ((int) m_y.size() == kMax + 1))
    {
        double bestFit = -1.0;
        const int bestFirst = first;
        const double bestDelay = delay;
        const double bestSpread = spread;
        bool ambiguous = false;

        for (int i = 0; i < nbBins; i++)
        {
            const int next = m_cirBins[(i + 1) % nbBins];
            const int gap = (next - m_cirBins[i] + size) % size;
            const double candidateSpread = (size - gap) * resolution;

            if ((next != bestFirst) && (candidateSpread <= m_guardSamples)) {
                ambiguous = true;
            }
        }

        for (int i = 0; ambiguous && (i < nbBins); i++)
        {
            const int next = m_cirBins[(i + 1) % nbBins];
            const int gap = (next - m_cirBins[i] + size) % size;
            const double candidateSpread = (size - gap) * resolution;

            if ((next != bestFirst) && (candidateSpread > m_guardSamples)) {
                continue;
            }

            double candidateDelay = next * resolution;
            candidateDelay -= period * std::round((candidateDelay - expected) / period);

            for (int m = -1; m <= 1; m++)
            {
                const double fit = continualPilotFit(out, size, resolution, next, candidateDelay + m * period, offset, l);

                if (fit > bestFit)
                {
                    bestFit = fit;
                    first = next;
                    delay = candidateDelay + m * period;
                    spread = candidateSpread;
                }
            }
        }

        if (!ambiguous)
        {
            first = bestFirst;
            delay = bestDelay;
            spread = bestSpread;
        }
    }

    useImpulseResponse({delay, spread}, windowOffset);
}

void OFDMDemod::useImpulseResponse(const CIRReading& reading, int windowOffset)
{
    m_firstPathSum += reading.m_delay - m_guardSamples + windowOffset;
    m_centre = reading.m_delay + reading.m_spread / 2.0;
    m_centreValid = true;
    m_spreadSum += reading.m_spread;
    m_cirCount++;
}

// A sample clock offset e moves the symbols relative to the FFT windows, by Ns e a symbol, which rotates the phase
// of carrier k by 2 pi k Ns e / N. The windows move by whole samples, which is allowed for. Measured between the
// continual pilots of consecutive data symbols, from the phase difference of the upper and lower halves of the band
void OFDMDemod::measureClock(int64_t window)
{
    const std::vector<int>& cps = m_tables->m_continual;
    const int n = m_fftSize;
    const int ns = n + m_guardSamples;
    const int centre = m_tables->m_kMax / 2;

    if ((m_previousWindow >= 0) && (m_previousCP.size() == cps.size()))
    {
        Cd lower(0.0, 0.0), upper(0.0, 0.0);
        double kLower = 0.0, kUpper = 0.0;
        int nLower = 0, nUpper = 0;

        for (size_t i = 0; i < cps.size(); i++)
        {
            int k = cps[i];
            Cd z = Cd(m_y[k] * std::conj(m_previousCP[i]));
            float magnitude = (float) std::abs(z);

            if (magnitude <= 0.0f) {
                continue;
            }

            z /= magnitude;

            if (k < centre)
            {
                lower += z;
                kLower += k;
                nLower++;
            }
            else
            {
                upper += z;
                kUpper += k;
                nUpper++;
            }
        }

        if ((nLower > 0) && (nUpper > 0))
        {
            // Phase per carrier, from the symbol moving relative to the window: delay change of ns (1 + e) - step
            double perCarrier = std::arg(upper * std::conj(lower)) / (kUpper / nUpper - kLower / nLower);
            // The fractional timing correction removed part of the change
            double delayChange = -perCarrier * n / (2.0 * M_PI) + (m_timingFraction - m_previousTimingFraction);
            int64_t step = window - m_previousWindow;
            double e = (delayChange + step) / ns - 1.0;
            // As of the latest offset, if it has changed since
            const double change = (m_clockChangeDelta != 0.0) && (window < m_clockChangeSample) ? m_clockChangeDelta : 0.0;
            m_clockSum += e * 1e6 + change;
            m_clockCount++;
        }
    }

    m_previousCP.resize(cps.size());

    for (size_t i = 0; i < cps.size(); i++) {
        m_previousCP[i] = m_y[cps[i]];
    }

    m_previousWindow = window;
}

// A P1 of the profile to receive. In a signal with both, each profile's frames are in the other's FEF parts
bool OFDMDemod::profileWanted(const P1Signalling& p1) const
{
    switch (m_profile)
    {
    case Profile::Base: return p1.isT2Base();
    case Profile::Lite: return p1.isT2Lite();
    default: return p1.isT2Base() || (p1.isT2Lite() && !p1.hasFEF());
    }
}

int64_t OFDMDemod::frameLength() const
{
    return m_inFEF ? m_fefLength : m_p1Length + (int64_t) m_lf * (m_fftSize + m_guardSamples);
}

// Until the clock offset has been measured, the next P1 could be up to 100 ppm of the frame from where it's expected
int OFDMDemod::p1Range() const
{
    return m_clockFrames >= 2 ? m_nextP1Range : m_nextP1Range + (int) (frameLength() * 100e-6);
}

// The next P1 follows the frame directly. It may be a FEF part's (8.4), whose S1 isn't T2's, and then the next
// T2-frame's P1 is FEF_LENGTH after it. Its position tracks the sample clock: the frame's measurements of it, from the
// continual pilots, are used first, as at a large offset they're needed to find the P1 at all (20 ppm moves a 32K
// frame 45 samples), all of the first, then half of each
bool OFDMDemod::nextP1()
{
    const int64_t nominalLength = frameLength();
    const bool measured = m_clockCount >= 4;
    const double previousOffset = m_status.m_clockOffset;

    if (measured)
    {
        const double ppm = m_clockSum / m_clockCount;
        m_status.m_clockOffset = m_clockFrames == 0 ? ppm : 0.5 * m_status.m_clockOffset + 0.5 * ppm;

        // Interference between the carriers at a large offset (from about 5 ppm in 32K) spoils the frame's impulse
        // responses: placing the next frame's window from them could put it past the guard interval, losing that
        // frame too. It stays where it is until the offset has been corrected
        if (std::abs(ppm) * m_fftSize > 5.0 * 32768.0)
        {
            m_firstPathSum = 0.0;
            m_spreadSum = 0.0;
            m_cirCount = 0;
        }
    }

    const int64_t expected = framePosition(nominalLength);
    const int range = p1Range();

    if (bufferEnd() < expected + range + m_p1Length + 1)
    {
        m_status.m_clockOffset = previousOffset; // Again, when there are enough samples
        return false;
    }

    if (measured)
    {
        m_clockFrames++;
        m_status.m_clockFromP1s = false;
    }

    // Each path has its own correlation peak, and with a strong echo the largest may not be the path the frames have
    // been timed from, which would move the frame by the echo's delay, and the FFT window with it, until the impulse
    // response moved it back (a frame lost, with a 0 dB echo). So of the peaks nearly as large as the largest (within
    // 16 samples either side), the one nearest where the P1 is expected
    const int width = 2 * range + 1;
    m_p1Metrics.resize(width);
    m_p1C1.resize(width);
    m_p1C2.resize(width);
    double bestMetric = 0.0;

    for (int i = 0; i < width; i++)
    {
        m_p1Metrics[i] = p1Metric(expected - range + i, m_p1C1[i], m_p1C2[i]);
        bestMetric = std::max(bestMetric, m_p1Metrics[i]);
    }

    int chosen = -1;

    for (int i = 0; i < width; i++)
    {
        if (m_p1Metrics[i] < 0.75 * bestMetric) {
            continue;
        }

        bool peak = true;

        for (int j = std::max(0, i - 16); peak && (j <= std::min(width - 1, i + 16)); j++) {
            peak = m_p1Metrics[j] <= m_p1Metrics[i];
        }

        if (peak && ((chosen < 0) || (std::abs(i - range) < std::abs(chosen - range)))) {
            chosen = i;
        }
    }

    if (chosen < 0) {
        chosen = range;
    }

    int64_t best = expected - range + chosen;
    const Cd bestC1 = m_p1C1[chosen];
    const Cd bestC2 = m_p1C2[chosen];
    bestMetric = m_p1Metrics[chosen];

    if (bestMetric < m_p1Threshold)
    {
        // Earlier than expected if samples were lost, as by an SDR that can't keep up: from the last two symbols
        log("Next P1 not found (correlation %.2f): searching", bestMetric);
        m_searchPos = std::max(m_bufferOffset, framePosition(std::max<int64_t>(0, nominalLength - 2 * (int64_t) (m_fftSize + m_guardSamples))));
        m_inFEF = false;
        m_status.m_state = SearchP1;
        return true;
    }

    // A FEF part's P1: skip it, if its length is known, or else search for the next T2-frame's
    if (!m_inFEF)
    {
        P1Signalling p1;
        double frequency;

        if (decodeP1(best, -std::arg(bestC1 * bestC2) / (m_twoPi * 1024.0), p1, frequency) && (!p1.isT2() || (p1.isT2Lite() != m_lite)))
        {
            m_clockSum = 0.0;
            m_clockCount = 0;
            m_previousWindow = -1;
            m_status.m_fefParts++;

            if (m_fefLength <= 0)
            {
                log("FEF part (S1 %d), of unknown length: searching", p1.m_s1);
                m_searchPos = best + m_p1Length;
                m_status.m_state = SearchP1;
                return true;
            }

            // Its P1 is where the frame would have started
            m_previousFrameStart = m_frameStart;
            m_frameStart = best;

            if (m_clockChangeSample <= best) {
                m_clockChangeDelta = 0.0;
            }

            m_inFEF = true;
            return true;
        }
    }

    const bool afterFEF = m_inFEF;
    m_inFEF = false;

    // Clock offset, from the frame's length if the continual pilots didn't measure it
    if (!measured && !afterFEF && (m_previousFrameStart >= 0))
    {
        double ppm = 1e6 * (best - m_frameStart - nominalLength) / (double) nominalLength;
        m_status.m_clockOffset = 0.7 * m_status.m_clockOffset + 0.3 * ppm;
    }

    m_clockSum = 0.0;
    m_clockCount = 0;
    m_previousWindow = -1;

    m_previousFrameStart = m_frameStart;
    m_frameStart = best;

    // The new frame is all at the latest offset, unless it changes again after its start
    if (m_clockChangeSample <= best) {
        m_clockChangeDelta = 0.0;
    }
    m_status.m_p1Correlation = bestMetric;
    m_frameCount++;
    m_l = 0;
    m_lf = m_np2;
    m_frameKnown = false;
    m_status.m_state = Symbols;
    return true;
}

void OFDMDemod::getDisplayData(std::vector<Cf>& cells, int maxCells, std::vector<float>& responseDb, int points) const
{
    cells.clear();
    int step = std::max(1, (int) m_cells.size() / std::max(1, maxCells));

    for (size_t i = 0; i < m_cells.size(); i += step) {
        cells.push_back(m_cells[i]);
    }

    responseDb.clear();

    if (m_h.empty() || (points <= 0)) {
        return;
    }

    double mean = 0.0;

    for (const Cf& h : m_h) {
        mean += std::norm(h);
    }

    mean /= m_h.size();

    for (int i = 0; i < points; i++)
    {
        size_t k = (size_t) i * (m_h.size() - 1) / std::max(1, points - 1);
        responseDb.push_back((float) (10.0 * std::log10(std::max(1e-12, std::norm(m_h[k]) / std::max(mean, 1e-12)))));
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

    if ((m_cirResolution <= 0.0) || m_cirPower.empty() || (points <= 0)) {
        return;
    }

    impulseResponseDb(m_cirPower, m_cirResolution, points, response.m_powerDb, response.m_resolution);
    response.m_guard = m_guardSamples;
}

} // namespace dvbt2
