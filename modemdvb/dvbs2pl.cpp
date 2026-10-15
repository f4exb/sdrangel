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
#include <limits>

#include "dvbs.h"
#include "dvbs2pl.h"

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define DVBS2PL_SSE2
#endif

namespace dvbs2 {

const Cf PLDecoder::m_pilotValue(0.70710678f, 0.70710678f);
const Cf PLDecoder::m_quarterTurns[4] = {Cf(1.0f, 0.0f), Cf(0.0f, 1.0f), Cf(-1.0f, 0.0f), Cf(0.0f, -1.0f)};

inline double PLDecoder::wrap(double a)
{
    while (a > m_pi) {
        a -= 2.0 * m_pi;
    }

    while (a <= -m_pi) {
        a += 2.0 * m_pi;
    }

    return a;
}

// The angle between neighbouring points of the outer ring: a decision's slip
double PLDecoder::outerRingStep(const std::vector<Cf>& points)
{
    float outer = 0.0f;

    for (const Cf& p : points) {
        outer = std::max(outer, std::abs(p));
    }

    std::vector<double> angles;

    for (const Cf& p : points)
    {
        if (std::abs(p) > 0.97f * outer) {
            angles.push_back(std::arg(p));
        }
    }

    if (angles.empty()) {
        return 2.0 * m_pi;
    }

    std::sort(angles.begin(), angles.end());
    double step = 2.0 * m_pi - (angles.back() - angles.front());

    for (size_t i = 1; i < angles.size(); i++) {
        step = std::min(step, angles[i] - angles[i - 1]);
    }

    return step;
}

PLDecoder::PLDecoder() :
    m_base(0),
    m_end(0),
    m_vlsnrScan(0),
    m_vlsnrPairs(0),
    m_scan(0),
    m_frameStart(0),
    m_lockStart(0),
    m_implausible(false),
    m_misses(0),
    m_suspect(false),
    m_frequencyKnown(false),
    m_frequency(0.0),
    m_frequencyBeforeLock(0.0),
    m_frequencyKnownBeforeLock(false),
    m_phaseNoiseBeforeLock(0.0),
    m_phaseNoiseFramesBeforeLock(0),
    m_coarseAverage(0.0),
    m_vlsnrCoarseAverage(0.0),
    m_vlsnrCoarseFrames(0),
    m_ddEndError(0.0),
    m_ddPhaseNoise(0.0),
    m_ddPhaseNoiseBeforeLock(0.0),
    m_ddDrift(0.0),
    m_ddCoarseSum(0.0),
    m_ddCoarseSquares(0.0),
    m_ddCoarseFrames(0),
    m_ddResidual(0.0),
    m_ddResidualValid(false),
    m_lockedFrames(0),
    m_sequence(0),
    m_gap(false),
    m_inverted(false),
    m_timeSlicing(false),
    m_modeStart(0),
    m_searchStart(0),
    m_slipSum(0),
    m_slipSymbols(0),
    m_slipFrames(0),
    m_clockCorrection(0.0),
    m_clockHold(false),
    m_slipFrom(0),
    m_tsnRepeats(0),
    m_esn0Average(0.0),
    m_phaseNoise(0.0),
    m_phaseNoiseFrames(0)
{
    // The header's symbols for 0, and the SOF and PLS code's known differences
    const uint8_t *sof = sofBits();
    const uint8_t *scrambling = plsScrambling();

    for (int n = 0; n < timeSlicingHeaderSymbols; n++) {
        m_zero[n] = headerSymbolZero(n);
    }

    m_sofDiff[0] = Cf(0.0f, 0.0f);

    for (int i = 1; i < sofSymbols; i++)
    {
        const Cf a = m_zero[i] * (sof[i] ? -1.0f : 1.0f), b = m_zero[i - 1] * (sof[i - 1] ? -1.0f : 1.0f);
        m_sofDiff[i] = a * std::conj(b);
    }

    // A pair is (a ^ s0, a ^ s1) when b7 = 0, so its difference doesn't depend on a
    for (int k = 0; k < 32; k++)
    {
        const float signs = (scrambling[2 * k] ? -1.0f : 1.0f) * (scrambling[2 * k + 1] ? -1.0f : 1.0f);
        m_plsDiff[k] = m_zero[sofSymbols + 2 * k + 1] * std::conj(m_zero[sofSymbols + 2 * k]) * signs;
    }

    // Time slicing's pairs are (c ^ s0, c ^ s1): their difference is known whatever c
    const uint8_t *timeSlicingScramble = timeSlicingScrambling();

    for (int k = 0; k < timeSlicingCodeBits; k++)
    {
        const float signs = (timeSlicingScramble[2 * k] ? -1.0f : 1.0f) * (timeSlicingScramble[2 * k + 1] ? -1.0f : 1.0f);
        m_timeSlicingDiff[k] = m_zero[sofSymbols + 2 * k + 1] * std::conj(m_zero[sofSymbols + 2 * k]) * signs;
    }

    m_codewords.resize(64);

    for (int v = 0; v < 64; v++)
    {
        uint8_t bits[32];
        plsCodeword(v, bits);
        m_codewords[v].resize(32);

        for (int i = 0; i < 32; i++) {
            m_codewords[v][i] = bits[i] ? -1.0f : 1.0f;
        }

        // Which Walsh function (row u of the Hadamard matrix, (-1)^popcount(u & i)) it is, and with which sign
        m_walsh[v] = -1;
        m_walshSign[v] = 1.0f;

        for (int u = 0; (u < 32) && (m_walsh[v] < 0); u++)
        {
            for (float sign : {1.0f, -1.0f})
            {
                bool same = true;

                for (int i = 0; same && (i < 32); i++)
                {
                    int parity = u & i;
                    parity ^= parity >> 4;
                    parity ^= parity >> 2;
                    parity ^= parity >> 1;
                    same = m_codewords[v][i] == sign * ((parity & 1) ? -1.0f : 1.0f);
                }

                if (same)
                {
                    m_walsh[v] = u;
                    m_walshSign[v] = sign;
                    break;
                }
            }
        }
    }

    // S2X's b0 = 1: its row added to each codeword
    for (int i = 0; i < 32; i++) {
        m_b0Signs[i] = ((plsB0Row >> (31 - i)) & 1) ? -1.0f : 1.0f;
    }

    for (int m = 0; m < nbModcods; m++)
    {
        Modulation modulation;
        CodeRate rate;
        m_slipAngle[m] = 0.0;
        m_nearestOf[m] = -1;

        if ((m > 0) && modcodInfo(m, modulation, rate))
        {
            constellation(modulation, rate, m_points[m], modcodFECType(m, FECType::Normal));
            m_slipAngle[m] = outerRingStep(m_points[m]);
        }
    }

    // VL-SNR's headers: each set's PLHEADER then index 0's VL-SNR header, and each index's row signs against index 0's
    uint8_t zeroBits[vlsnrHeaderSymbols];
    vlsnrHeaderBits(0, zeroBits);

    for (int set = 1; set <= 2; set++)
    {
        plHeaderSymbols(vlsnrSetModcod(set), 1, m_vlsnrKnown[set - 1]);
        vlsnrHeader(0, m_vlsnrKnown[set - 1] + headerSymbols);
    }

    for (int i = 0; i < vlsnrIndexes; i++)
    {
        uint8_t bits[vlsnrHeaderSymbols];
        vlsnrHeaderBits(i, bits);

        for (int r = 0; r < 16; r++) {
            m_vlsnrSigns[i][r] = bits[2 + 56 * r] != zeroBits[2 + 56 * r] ? -1.0f : 1.0f;
        }

        if (vlsnrModcod(i).m_valid || (i == vlsnrDummy))
        {
            VLSNRHypothesis h;
            h.m_set = vlsnrModcod(i).m_set;
            h.m_index = i;
            h.m_known.resize(vlsnrHeaderLength);
            plHeaderSymbols(vlsnrSetModcod(h.m_set), 1, h.m_known.data());
            vlsnrHeader(i, h.m_known.data() + headerSymbols);
            m_vlsnrHypotheses.push_back(h);
        }
    }

    // The search's known differences: the SOF's, and the VL-SNR header's but where a row's sign (the index's) or the
    // PLS code (the set's) would say
    {
        m_vlsnrDiffSpectrum.assign(m_vlsnrSearchFFT, 0.0);
        m_vlsnrPairs = 0;
        const Cf *k = m_vlsnrKnown[0];

        for (int n = 1; n < vlsnrHeaderLength; n++)
        {
            const int m = n - headerSymbols;   // In the VL-SNR header
            const bool sof = n < sofSymbols;
            const bool row = (m >= 3) && (m < vlsnrHeaderSymbols - 2) && ((m - 2) / 56 == (m - 3) / 56);
            const bool edge = (m == 1) || (m == vlsnrHeaderSymbols - 1);

            if (sof || row || edge)
            {
                m_vlsnrDiffSpectrum[n] = std::complex<double>(k[n] * std::conj(k[n - 1]));
                m_vlsnrPairs++;
            }
        }

        dvbs::fft(m_vlsnrDiffSpectrum);

        for (std::complex<double>& c : m_vlsnrDiffSpectrum) {
            c = std::conj(c);
        }
    }

    for (int k = 0; k < 2; k++)
    {
        m_metricMean[k] = 0.17;
        m_metricVar[k] = 0.06 * 0.06;
        m_metricCount[k] = 0;
    }

    m_dataPower.assign(nbModcods, 0.0);
    reset();
}

double PLDecoder::metricSigma(bool timeSlicing) const
{
    return std::max(m_minMetricSigma, std::sqrt(m_metricVar[timeSlicing]));
}

void PLDecoder::setScramblingCode(int n)
{
    m_scrambler.setCode(n);
}

void PLDecoder::reset()
{
    m_buffer.clear();
    m_diff.clear();
    m_base = 0;
    m_end = 0;
    m_scan = 1;
    m_vlsnrScan = 1;
    m_candidates.clear();
    m_misses = 0;
    m_slipSum = 0;
    m_slipSymbols = 0;
    m_slipFrames = 0;
    m_clockCorrection = 0.0;
    m_clockHold = false;
    m_slipFrom = 0;
    m_frequencyKnown = false;
    m_frequency = 0.0;
    m_lockedFrames = 0;
    m_sequence = 0;
    m_gap = true;   // The BBFRAME decoder's told, as after a lock's lost: the receiver restarts this without resetting it
    m_inverted = false; // Another signal (the receiver's reset on a retune) may not be
    m_timeSlicing = false;
    m_modeStart = 0;
    m_searchStart = 0;
    m_tsnRepeats = 0;
    m_esn0Average = 0.0;
    m_phaseNoise = 0.0;
    m_phaseNoiseFrames = 0;
    m_ddEndError = 0.0;
    m_ddPhaseNoise = 0.0;
    m_ddDrift = 0.0;
    m_ddCoarseSum = 0.0;
    m_ddCoarseSquares = 0.0;
    m_ddCoarseFrames = 0;
    m_ddResidualValid = false;
    m_coarseAverage = 0.0;
    m_vlsnrCoarseAverage = 0.0;
    m_vlsnrCoarseFrames = 0;
    m_dataPower.assign(nbModcods, 0.0);
    m_status = Status();
    m_display.clear();
}

// A complex multiply, written out (std::complex's can check for infinities)
inline Cf PLDecoder::mul(const Cf& a, const Cf& b)
{
    return Cf(a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real());
}

// The nearest of QPSK's points (+-1/sqrt 2 +-j/sqrt 2) and 8PSK's (every pi/4 from 0, unit amplitude), computed: the
// table's lookup is much of a decision's time, at tens of MS/s
inline Cf PLDecoder::nearestQPSK(const Cf& v)
{
    const float a = 0.70710678f;
    return Cf(v.real() >= 0.0f ? a : -a, v.imag() >= 0.0f ? a : -a);
}

inline Cf PLDecoder::nearestPSK8(const Cf& v)
{
    const float a = 0.70710678f;
    const float t = 0.41421356f;    // tan(pi / 8): on an axis within pi/8 of it, else a diagonal
    const float x = std::fabs(v.real()), y = std::fabs(v.imag());
    const float sx = v.real() >= 0.0f ? 1.0f : -1.0f, sy = v.imag() >= 0.0f ? 1.0f : -1.0f;
    return y < t * x ? Cf(sx, 0.0f) : x < t * y ? Cf(0.0f, sy) : Cf(sx * a, sy * a);
}

// The sum over n values, each turned by p, of the value times its nearest PSK point's conjugate (QPSK's or 8PSK's)
template <bool psk8>
Cf PLDecoder::pskErrorSum(const Cf *c, int n, const Cf& p)
{
    const float pr = p.real(), pi = p.imag();
    float er = 0.0f, ei = 0.0f;
    int i = 0;

#if defined(DVBS2PL_SSE2)
    // 4 at a time and without branches: noise makes which point it is unpredictable, so branching mispredicts often
    const __m128 vpr = _mm_set1_ps(pr), vpi = _mm_set1_ps(pi);
    const __m128 signMask = _mm_set1_ps(-0.0f), one = _mm_set1_ps(1.0f);
    const __m128 diagonal = _mm_set1_ps(0.70710678f), tanPi8 = _mm_set1_ps(0.41421356f);
    __m128 ser = _mm_setzero_ps(), sei = _mm_setzero_ps();

    for (; i + 4 <= n; i += 4)
    {
        const __m128 a = _mm_loadu_ps(reinterpret_cast<const float *>(c + i));
        const __m128 b = _mm_loadu_ps(reinterpret_cast<const float *>(c + i + 2));
        const __m128 cr = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 ci = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
        const __m128 vr = _mm_sub_ps(_mm_mul_ps(cr, vpr), _mm_mul_ps(ci, vpi));
        const __m128 vi = _mm_add_ps(_mm_mul_ps(cr, vpi), _mm_mul_ps(ci, vpr));
        __m128 magnitudeR = diagonal, magnitudeI = diagonal;

        if (psk8)
        {
            // On the real axis within pi/8 of it, on the imaginary within pi/8 of that, else on a diagonal
            const __m128 x = _mm_andnot_ps(signMask, vr), y = _mm_andnot_ps(signMask, vi);
            const __m128 onR = _mm_cmplt_ps(y, _mm_mul_ps(tanPi8, x));
            const __m128 onI = _mm_cmplt_ps(x, _mm_mul_ps(tanPi8, y));
            magnitudeR = _mm_andnot_ps(onI, _mm_or_ps(_mm_and_ps(onR, one), _mm_andnot_ps(onR, diagonal)));
            magnitudeI = _mm_andnot_ps(onR, _mm_or_ps(_mm_and_ps(onI, one), _mm_andnot_ps(onI, diagonal)));
        }

        const __m128 dr = _mm_or_ps(magnitudeR, _mm_and_ps(signMask, vr));
        const __m128 di = _mm_or_ps(magnitudeI, _mm_and_ps(signMask, vi));
        ser = _mm_add_ps(ser, _mm_add_ps(_mm_mul_ps(vr, dr), _mm_mul_ps(vi, di)));
        sei = _mm_add_ps(sei, _mm_sub_ps(_mm_mul_ps(vi, dr), _mm_mul_ps(vr, di)));
    }

    alignas(16) float sr[4], si[4];
    _mm_store_ps(sr, ser);
    _mm_store_ps(si, sei);
    er = (sr[0] + sr[1]) + (sr[2] + sr[3]);
    ei = (si[0] + si[1]) + (si[2] + si[3]);
#endif

    for (; i < n; i++)
    {
        const float vr = c[i].real() * pr - c[i].imag() * pi, vi = c[i].real() * pi + c[i].imag() * pr;
        const Cf d = psk8 ? nearestPSK8(Cf(vr, vi)) : nearestQPSK(Cf(vr, vi));
        er += vr * d.real() + vi * d.imag();
        ei += vi * d.real() - vr * d.imag();
    }

    return Cf(er, ei);
}

// n values turned by a phasor stepped a symbol at a time (and, if scrambling's given, descrambled by (-j)^R): 4 at a
// time from the step's powers, so each multiply doesn't wait for the last, as stepping symbol by symbol did
void PLDecoder::turnValues(const Cf *in, Cf *out, int n, Cf rot, const Cf& step, const uint8_t *scrambling)
{
    static const float descrambleRe[4] = {1.0f, 0.0f, -1.0f, 0.0f};    // (-j)^R
    static const float descrambleIm[4] = {0.0f, -1.0f, 0.0f, 1.0f};
    alignas(16) float powersRe[4], powersIm[4];
    Cf p(1.0f, 0.0f);

    for (int k = 0; k < 4; k++)
    {
        powersRe[k] = p.real();
        powersIm[k] = p.imag();
        p = mul(p, step);
    }

    const Cf step4 = p;
    int x = 0;

#if defined(DVBS2PL_SSE2)
    const __m128 qr = _mm_load_ps(powersRe), qi = _mm_load_ps(powersIm);

    for (; x + 4 <= n; x += 4)
    {
        const __m128 rr = _mm_set1_ps(rot.real()), ri = _mm_set1_ps(rot.imag());
        __m128 wr = _mm_sub_ps(_mm_mul_ps(rr, qr), _mm_mul_ps(ri, qi));
        __m128 wi = _mm_add_ps(_mm_mul_ps(rr, qi), _mm_mul_ps(ri, qr));

        if (scrambling)
        {
            const uint8_t *q = scrambling + x;
            const __m128 dr = _mm_setr_ps(descrambleRe[q[0]], descrambleRe[q[1]], descrambleRe[q[2]], descrambleRe[q[3]]);
            const __m128 di = _mm_setr_ps(descrambleIm[q[0]], descrambleIm[q[1]], descrambleIm[q[2]], descrambleIm[q[3]]);
            const __m128 tr = _mm_sub_ps(_mm_mul_ps(wr, dr), _mm_mul_ps(wi, di));
            wi = _mm_add_ps(_mm_mul_ps(wr, di), _mm_mul_ps(wi, dr));
            wr = tr;
        }

        const __m128 a = _mm_loadu_ps(reinterpret_cast<const float *>(in + x));
        const __m128 b = _mm_loadu_ps(reinterpret_cast<const float *>(in + x + 2));
        const __m128 cr = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 ci = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
        const __m128 outR = _mm_sub_ps(_mm_mul_ps(cr, wr), _mm_mul_ps(ci, wi));
        const __m128 outI = _mm_add_ps(_mm_mul_ps(cr, wi), _mm_mul_ps(ci, wr));
        _mm_storeu_ps(reinterpret_cast<float *>(out + x), _mm_unpacklo_ps(outR, outI));
        _mm_storeu_ps(reinterpret_cast<float *>(out + x + 2), _mm_unpackhi_ps(outR, outI));
        rot = mul(rot, step4);
    }
#endif

    for (; x < n; x += 4)
    {
        const int m = std::min(4, n - x);

        for (int k = 0; k < m; k++)
        {
            float wr = rot.real() * powersRe[k] - rot.imag() * powersIm[k];
            float wi = rot.real() * powersIm[k] + rot.imag() * powersRe[k];

            if (scrambling)
            {
                const uint8_t q = scrambling[x + k];
                const float tr = wr * descrambleRe[q] - wi * descrambleIm[q];
                wi = wr * descrambleIm[q] + wi * descrambleRe[q];
                wr = tr;
            }

            const float cr = in[x + k].real(), ci = in[x + k].imag();
            out[x + k] = Cf(cr * wr - ci * wi, cr * wi + ci * wr);
        }

        rot = mul(rot, step4);
    }
}

// For the decisions (the decision directed loop, the slots' phases, the smoother, MER), made when first wanted: or
// another MODCOD's, of the same points (each table's 512 KB, and a stall to make)
const NearestTable& PLDecoder::nearestPoints(int modcod)
{
    int& of = m_nearestOf[modcod];

    if (of < 0)
    {
        for (int m = 0; (m < nbModcods) && (of < 0); m++)
        {
            if ((m_nearestOf[m] == m) && (m_points[m] == m_points[modcod])) {
                of = m;
            }
        }

        if (of < 0)
        {
            m_nearestTables[modcod].build(m_points[modcod]);
            of = modcod;
        }
    }

    return m_nearestTables[of];
}

// A little phase noise (up to 0.004 radians a symbol: as at higher symbol rates, where an LNB's is less a symbol) leaves
// each slot turned a little from the phase interpolated between the pilots. Each slot's turn is tracked from its
// symbols' decisions, from slot to slot from where the pilots put it (none, at the start of each pilot interval);
// averaged with its neighbours' (so from 270 symbols, little affected by noise even near threshold, where decision
// errors only make it smaller), interpolated from slot centre to slot centre, and taken off. Cheap: more phase noise,
// and smoothPhase follows it symbol by symbol
void PLDecoder::refineSlotPhases(Cf *cells, int slots, int modcod, Modulation modulation)
{
    if (slots < 2) {
        return;
    }

    const std::vector<Cf>& points = m_points[modcod];
    const NearestTable& table = nearestPoints(modcod);
    const float invScale = m_dataPower[modcod] > 0.0 ? (float) (1.0 / std::sqrt(m_dataPower[modcod])) : 1.0f;

    m_slotUnwrapped.resize(slots);
    m_slotPhases.resize(slots);

    auto measure = [&](auto errorSum) {
        double previous = 0.0;

        for (int s = 0; s < slots; s++)
        {
            // Each turned by the last's phase first, so it's only out by the change since: a slot's own decisions
            // can't say how far it's turned near half the angle between points, as as many go to the next point as to
            // the right one
            if (s % pilotInterval == 0) {
                previous = 0.0;
            }

            const Cf e = errorSum(cells + s * slotSymbols, std::polar(invScale, (float) -previous));
            previous += std::norm(e) > 0.0f ? std::arg(e) : 0.0;
            m_slotUnwrapped[s] = previous;
        }
    };

    if (modulation == Modulation::QPSK) {
        measure([](const Cf *c, const Cf& p) { return pskErrorSum<false>(c, slotSymbols, p); });
    } else if (modulation == Modulation::PSK8) {
        measure([](const Cf *c, const Cf& p) { return pskErrorSum<true>(c, slotSymbols, p); });
    }
    else
    {
        measure([&](const Cf *c, const Cf& p) {
            Cf e(0.0f, 0.0f);

            for (int i = 0; i < slotSymbols; i++)
            {
                const Cf vs = c[i] * p;
                e += vs * std::conj(points[table.nearest(vs, points)]);
            }

            return e;
        });
    }

    for (int s = 0; s < slots; s++)
    {
        const int a = std::max(0, s - 1), b = std::min(slots - 1, s + 1);
        double sum = 0.0;

        for (int k = a; k <= b; k++) {
            sum += m_slotUnwrapped[k];
        }

        m_slotPhases[s] = sum / (b - a + 1);
    }

    // Taken off, interpolated between the slots' centres (and as the first and last before and after theirs): from
    // each centre to the next, a phasor stepped a symbol at a time
    const double centre = (slotSymbols - 1) / 2.0;

    for (int s = -1; s < slots; s++)
    {
        const int first = s < 0 ? 0 : s * slotSymbols + (int) std::ceil(centre);
        const int last = s + 1 < slots ? (s + 1) * slotSymbols + (int) std::ceil(centre) : slots * slotSymbols;
        const double p0 = m_slotPhases[std::max(0, s)];
        const double p1 = m_slotPhases[std::min(slots - 1, s + 1)];
        const double x0 = s < 0 ? first : s * slotSymbols + centre;
        const double slope = (s < 0) || (s + 1 >= slots) ? 0.0 : (p1 - p0) / slotSymbols;
        const Cf rot = std::polar(1.0f, (float) -(p0 + slope * (first - x0)));
        const Cf step = std::polar(1.0f, (float) -slope);
        turnValues(cells + first, cells + first, last - first, rot, step);
    }
}

// Phase noise faster than the pilots can follow (an LNB's, at a low symbol rate: their blocks are 1476 symbols apart)
// leaves the data turned from the phase interpolated between them, beyond 8PSK's decision boundaries at times. What's
// left is tracked symbol by symbol, as a random walk of variance q a symbol (measured from the pilot blocks' phases),
// by a Kalman filter through the whole frame, on the known symbols (the header, pilot blocks and next header) and each
// data symbol's decision, forwards and backwards, the two combined: so each symbol's phase is from those either side,
// weighted by how far the phase may have walked since: the mean of as many as the walk allows. Through the known
// symbols, each pass starts each run of data at the phase at its edge (a block's mean phase, with heavy phase noise,
// can be a point out from it). Only with more phase noise than m_smoothedPhaseNoise: it's serial (each decision from
// the last's phase), about 20 ns a symbol each way, too much at tens of MS/s, where there's less phase noise a symbol
// and refineSlotPhases is enough.
//
// Following quickly, a decision gone over a boundary can take those after it with it, a whole point round: a slip,
// until the next known symbols put it right. Each pass's is measured at the next block's first symbols, and in each
// run of data, the passes' difference in whole points says which (if either) is out where: one that's slipped is right
// before it slipped (forwards) or after (backwards), and out by its slip between
void PLDecoder::smoothPhase(Cf *cells, int modcod, double noise, double q)
{
    const int n = (int) m_trackValue.size();

    if (n <= 0) {
        return;
    }

    const std::vector<Cf>& points = m_points[modcod];
    const NearestTable& table = nearestPoints(modcod);
    const double dataPower = m_dataPower[modcod] > 0.0 ? m_dataPower[modcod] : 1.0;
    const float invScale = (float) (1.0 / std::sqrt(dataPower));
    const float knownNoise = (float) (noise / 2.0);             // A known symbol's phase error variance
    const float gate2 = 3.0f * 3.0f;                            // Standard deviations, squared
    const int entrySymbols = 8;                                 // Of a block, to see whether a pass has slipped
    const float qf = (float) q;
    // A slip's angle: the points' angle apart (the outer ring's, for APSK)
    const double unit = m_slipAngle[modcod];
    const double invUnit = 1.0 / unit;

    // Each point's inverse power (at unit energy), and a decision's phase error variance there: the noise's over twice
    // the point's power
    m_pointInvPower.resize(points.size());
    m_pointNoise.resize(points.size());

    for (size_t k = 0; k < points.size(); k++)
    {
        const double power = std::max(1e-6, (double) std::norm(points[k]));
        m_pointInvPower[k] = (float) (1.0 / power);
        m_pointNoise[k] = (float) (noise / dataPower / 2.0 / power);
    }

    m_trackForward.resize(n);
    m_trackForwardVariance.resize(n);
    m_trackForwardRot.resize(n);
    m_trackBackward.resize(n);
    m_trackBackwardVariance.resize(n);
    m_segmentSlips.clear();

    // A measurement of the phase left, from the prediction, updating it, and its phasor (e^-j phase: the changes are
    // small, so it's turned by the first terms of their cos and sin, and made exact every 32 symbols). A decision's is
    // gated: one gone over a boundary is further from the prediction than its spread says it should be (and the wrong
    // way), and would pull the estimate towards the next point, and the next decisions over too, so it's left out. Near
    // threshold, the spread's wider than the points' half angle, so none are
    auto update = [&](int i, float& phase, float& variance, Cf& rot) {
        if ((i & 31) == 0) {
            rot = std::polar(1.0f, -phase);
        }

        const Cf v = m_trackValue[i] * rot;
        float change = 0.0f;

        if (m_trackKnown[i])
        {
            const float k = variance / (variance + knownNoise);
            change = k * v.imag();
            variance *= 1.0f - k;
        }
        else
        {
            const Cf vs = v * invScale;
            const int nearest = table.nearest(vs, points);
            const float error = (vs * std::conj(points[nearest])).imag() * m_pointInvPower[nearest];
            const float r = m_pointNoise[nearest];

            if (error * error <= gate2 * (variance + r))
            {
                const float k = variance / (variance + r);
                change = k * error;
                variance *= 1.0f - k;
            }
        }

        phase += change;
        rot *= Cf(1.0f - 0.5f * change * change, -change + change * change * change / 6.0f);
    };
    // A pass's slip, in points, from the first symbols of the block it's entering (from i, in direction step)
    auto slip = [&](int i, int step, float phase) {
        Cf sum(0.0f, 0.0f);

        for (int k = 0; (k < entrySymbols) && (i >= 0) && (i < n) && m_trackKnown[i]; k++, i += step) {
            sum += m_trackValue[i];
        }

        return std::norm(sum) > 0.0f ? (int) std::lround(-wrap(std::arg(sum) - phase) * invUnit) : 0;
    };

    // Forwards, from the header's start, its phase unknown
    float phase = 0.0f, variance = 1.0f;
    Cf rot(1.0f, 0.0f);

    for (int i = 0; i < n; i++)
    {
        if ((i > 0) && m_trackKnown[i] && !m_trackKnown[i - 1]) {
            m_segmentSlips.push_back(std::make_pair(slip(i, 1, phase), 0));
        }

        variance += qf;
        update(i, phase, variance, rot);
        m_trackForward[i] = phase;
        m_trackForwardVariance[i] = variance;
        m_trackForwardRot[i] = rot;
    }

    // Backwards, from the next header's end: each symbol's estimate without it
    phase = 0.0f;
    variance = 1.0f;
    rot = Cf(1.0f, 0.0f);
    int segment = (int) m_segmentSlips.size();

    for (int i = n - 1; i >= 0; i--)
    {
        if ((i < n - 1) && m_trackKnown[i] && !m_trackKnown[i + 1] && (segment > 0)) {
            m_segmentSlips[--segment].second = slip(i, -1, phase);
        }

        variance += qf;
        m_trackBackward[i] = phase;
        m_trackBackwardVariance[i] = variance;
        update(i, phase, variance, rot);
    }

    // Combined, and taken off the data: the forward's phasor, turned by the smoothed phase's difference from it
    segment = -1;
    bool passed = false;    // Whether this run of data's had a point where both passes agree, or the backward's right
    int forwardSlip = 0, backwardSlip = 0;
    int cell = 0;

    for (int i = 0; i < n; i++)
    {
        if (m_trackKnown[i]) {
            continue;
        }

        if ((i == 0) || m_trackKnown[i - 1])
        {
            segment++;
            passed = false;
            forwardSlip = segment < (int) m_segmentSlips.size() ? m_segmentSlips[segment].first : 0;
            backwardSlip = segment < (int) m_segmentSlips.size() ? m_segmentSlips[segment].second : 0;
        }

        const float fv = m_trackForwardVariance[i], bv = m_trackBackwardVariance[i];
        // Each pass's phase may have gone whole turns round (an interpolation unwrapped a turn out), differently
        const float d = (float) wrap(m_trackBackward[i] - m_trackForward[i]);
        float turn;     // From the forward's phase

        if ((forwardSlip == 0) && (backwardSlip == 0))
        {
            turn = fv / (fv + bv) * d;
        }
        else
        {
            const int apart = (int) std::lround(d * invUnit);  // In points

            if (apart == 0)
            {
                turn = fv / (fv + bv) * d;
                passed = true;
            }
            else if ((backwardSlip != 0) && (apart == backwardSlip) && ((apart != -forwardSlip) || !passed))
            {
                turn = 0.0f;        // The backward's slipped here, not (yet) the forward
            }
            else if ((forwardSlip != 0) && (apart == -forwardSlip))
            {
                turn = d;           // The forward's slipped, not (now) the backward
                passed = true;
            }
            else if (apart == backwardSlip - forwardSlip)
            {
                turn = (float) (-forwardSlip * unit);  // Both have
                passed = true;
            }
            else
            {
                turn = fv / (fv + bv) * d;
            }
        }

        const Cf turnRot = std::fabs(turn) < 0.25f ? Cf(1.0f - 0.5f * turn * turn, -turn + turn * turn * turn / 6.0f)
            : std::polar(1.0f, -turn);
        cells[cell++] *= m_trackForwardRot[i] * turnRot;
    }
}

// The phase noise, as a random walk's variance a symbol: from how far each known block's phase is from that predicted
// by the last's and the frequency, less what the blocks' own noise accounts for. Averaged over frames
void PLDecoder::measurePhaseNoise(const double *centres, const double *phases, const int *lengths, int nbBlocks,
    double step, double noise)
{
    double excess = 0.0, symbols = 0.0;

    for (int k = 1; k < nbBlocks; k++)
    {
        const double distance = centres[k] - centres[k - 1];
        const double d = phases[k] - phases[k - 1] - step * distance;
        excess += d * d - noise / 2.0 * (1.0 / lengths[k] + 1.0 / lengths[k - 1]);
        symbols += distance;
    }

    if (symbols <= 0.0) {
        return;
    }

    const double q = std::max(0.0, excess / symbols);
    m_phaseNoise = m_phaseNoiseFrames == 0 ? q : m_phaseNoise + 0.2 * (q - m_phaseNoise);
    m_phaseNoiseFrames++;
    m_status.m_phaseNoise = std::sqrt(m_phaseNoise);
}

void PLDecoder::discontinuity()
{
    // Locked (as when samples were dropped, being behind), the frame being followed is cut, and where it is in the
    // buffer would be gone: searching again from the symbols after, as on a lock lost (the frequency kept, if trusted)
    if (m_status.m_state != State::Searching) {
        lost(false);
    }

    m_buffer.clear();
    m_diff.clear();
    m_base = m_end;
    m_scan = m_end + 1;
    m_vlsnrScan = m_scan;
    m_modeStart = m_scan;
    m_searchStart = m_scan;
    m_candidates.clear();
    m_gap = true;
}

void PLDecoder::append(const Cf *symbols, int nbSymbols)
{
    const size_t n = m_buffer.size();
    m_buffer.insert(m_buffer.end(), symbols, symbols + nbSymbols);

    if (m_inverted)
    {
        for (size_t i = n; i < m_buffer.size(); i++) {
            m_buffer[i] = std::conj(m_buffer[i]);
        }
    }

    m_diff.resize(m_buffer.size());

    for (size_t i = std::max((size_t) 1, n); i < m_buffer.size(); i++) {
        m_diff[i] = m_buffer[i] * std::conj(m_buffer[i - 1]);
    }

    if (n == 0 && !m_buffer.empty()) {
        m_diff[0] = Cf(0.0f, 0.0f);
    }

    m_end += nbSymbols;
}

// The symbols held conjugated (their differences too), and the carrier's frequency negated: in the searcher's terms it's
// as if the signal had been the right way up all along
void PLDecoder::invert()
{
    m_inverted = !m_inverted;
    m_status.m_inverted = m_inverted;

    for (Cf& s : m_buffer) {
        s = std::conj(s);
    }

    for (Cf& d : m_diff) {
        d = std::conj(d);
    }

    m_frequency = -m_frequency;
    m_frequencyBeforeLock = -m_frequencyBeforeLock;
    m_status.m_frequency = m_frequency;
    m_candidates.clear();
}

// Drop what's before keepFrom (and the symbol before it, for its difference), when it's worth moving the rest
void PLDecoder::trim(int64_t keepFrom)
{
    const int64_t drop = keepFrom - 1 - m_base;

    if ((drop > 0) && (drop > (int64_t) m_buffer.size() / 2))
    {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + drop);
        m_diff.erase(m_diff.begin(), m_diff.begin() + drop);
        m_base += drop;
    }
}

void PLDecoder::correlate(int64_t index, bool timeSlicing, Cf& sof, Cf& pls) const
{
    const Cf *d = &m_diff[(size_t) (index - m_base)];
    float sr = 0.0f, si = 0.0f, pr = 0.0f, pi_ = 0.0f;

    for (int i = 1; i < sofSymbols; i++)
    {
        const Cf& a = d[i];
        const Cf& b = m_sofDiff[i];
        sr += a.real() * b.real() + a.imag() * b.imag();
        si += a.imag() * b.real() - a.real() * b.imag();
    }

    // Time slicing's first 32 pairs of its 77, so either kind's search costs the same a symbol (it's run at every symbol
    // searched, while the header's decoded only at candidates), and finds a header as well as S2's does
    const Cf *diffs = timeSlicing ? m_timeSlicingDiff : m_plsDiff;

    for (int k = 0; k < 32; k++)
    {
        const Cf& a = d[sofSymbols + 2 * k + 1];
        const Cf& b = diffs[k];
        pr += a.real() * b.real() + a.imag() * b.imag();
        pi_ += a.imag() * b.real() - a.real() * b.imag();
    }

    sof = Cf(sr, si);
    pls = Cf(pr, pi_);
}

// Each position in a lane, its sums in correlate()'s order and with its operations, so they're the same: about a
// quarter of the time
void PLDecoder::correlate4(int64_t index, bool timeSlicing, Cf sof[4], Cf pls[4]) const
{
#if defined(DVBS2PL_SSE2)
    const float *d = reinterpret_cast<const float *>(&m_diff[(size_t) (index - m_base)]);
    __m128 sr = _mm_setzero_ps(), si = _mm_setzero_ps(), pr = _mm_setzero_ps(), pi_ = _mm_setzero_ps();

    // The i'th difference of each position: d[i] to d[i + 3], split into their real and imaginary parts
    auto load = [d](int i, __m128& re, __m128& im) {
        const __m128 a = _mm_loadu_ps(d + 2 * i), b = _mm_loadu_ps(d + 2 * i + 4);
        re = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
        im = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
    };

    for (int i = 1; i < sofSymbols; i++)
    {
        __m128 ar, ai;
        load(i, ar, ai);
        const __m128 br = _mm_set1_ps(m_sofDiff[i].real()), bi = _mm_set1_ps(m_sofDiff[i].imag());
        sr = _mm_add_ps(sr, _mm_add_ps(_mm_mul_ps(ar, br), _mm_mul_ps(ai, bi)));
        si = _mm_add_ps(si, _mm_sub_ps(_mm_mul_ps(ai, br), _mm_mul_ps(ar, bi)));
    }

    const Cf *diffs = timeSlicing ? m_timeSlicingDiff : m_plsDiff;

    for (int k = 0; k < 32; k++)
    {
        __m128 ar, ai;
        load(sofSymbols + 2 * k + 1, ar, ai);
        const __m128 br = _mm_set1_ps(diffs[k].real()), bi = _mm_set1_ps(diffs[k].imag());
        pr = _mm_add_ps(pr, _mm_add_ps(_mm_mul_ps(ar, br), _mm_mul_ps(ai, bi)));
        pi_ = _mm_add_ps(pi_, _mm_sub_ps(_mm_mul_ps(ai, br), _mm_mul_ps(ar, bi)));
    }

    alignas(16) float r[4][4];
    _mm_store_ps(r[0], sr);
    _mm_store_ps(r[1], si);
    _mm_store_ps(r[2], pr);
    _mm_store_ps(r[3], pi_);

    for (int j = 0; j < 4; j++)
    {
        sof[j] = Cf(r[0][j], r[1][j]);
        pls[j] = Cf(r[2][j], r[3][j]);
    }
#else
    for (int j = 0; j < 4; j++) {
        correlate(index + j, timeSlicing, sof[j], pls[j]);
    }
#endif
}

Cf PLDecoder::aligned(const Cf& sof, const Cf& pls) const
{
    return sof + ((sof * std::conj(pls)).real() >= 0.0f ? pls : -pls);
}

// The carrier's frequency from a header's differences: the PLS code's sign (b7) aligned with the SOF's
double PLDecoder::localFrequency(const Cf& sof, const Cf& pls) const
{
    return std::arg(aligned(sof, pls)) / (2.0 * m_pi);
}

PLDecoder::Header PLDecoder::decodeHeader(int64_t index, double frequency, HeaderKind kind) const
{
    Header header;
    Cf z[timeSlicingHeaderSymbols];

    // Turned back by the frequency: a phasor stepped a symbol at a time (in double, so its error over the header is
    // negligible), as cos and sin of each symbol's angle cost as much as the rest of a decode. S2's slot; time slicing's
    // second only when it's tried
    const std::complex<double> step = std::polar(1.0, -2.0 * m_pi * frequency);
    const std::complex<double> phasor = turnBack(index, headerSymbols, std::complex<double>(1.0, 0.0), step, nullptr, z);

    // The phase, from the SOF
    const uint8_t *sof = sofBits();
    Cf c(0.0f, 0.0f);

    for (int n = 0; n < sofSymbols; n++) {
        c += z[n] * std::conj(m_zero[n]) * (sof[n] ? -1.0f : 1.0f);
    }

    const float magnitude = std::abs(c);

    if (magnitude <= 0.0f) {
        return header;
    }

    const Cf rot = std::conj(c) / magnitude;
    const uint8_t *scrambling = plsScrambling();
    // In phase with the SOF, S2's PLS code; in quadrature, S2X's (b0 = 1), a quarter turn on
    float r[2][64];

    for (int i = 0; i < 64; i++)
    {
        const Cf v = z[sofSymbols + i] * rot * std::conj(m_zero[sofSymbols + i]);
        const float sign = scrambling[i] ? -1.0f : 1.0f;
        r[0][i] = v.real() * sign;
        r[1][i] = v.imag() * sign;
    }

    // Each value of b0 and b7 combines the pairs differently: then the (32, 6) code's codewords, b0's row's signs taken
    // off first
    double best = -1e30, second = -1e30;
    int bestValue = 0, bestB0 = 0, bestB7 = 0;

    for (int hypothesis = 0; hypothesis < 4; hypothesis++)
    {
        const int b0 = hypothesis >> 1, b7 = hypothesis & 1;
        const float *rb = r[b0];
        float a[32];

        for (int k = 0; k < 32; k++) {
            a[k] = (rb[2 * k] + (b7 ? -rb[2 * k + 1] : rb[2 * k + 1])) * (b0 ? m_b0Signs[k] : 1.0f);
        }

        // Correlations with every Walsh function at once
        float walsh[32];
        std::copy(a, a + 32, walsh);

        for (int half = 1; half < 32; half <<= 1)
        {
            for (int i = 0; i < 32; i += 2 * half)
            {
                for (int j = i; j < i + half; j++)
                {
                    const float x = walsh[j], y = walsh[j + half];
                    walsh[j] = x + y;
                    walsh[j + half] = x - y;
                }
            }
        }

        for (int v = 0; v < 64; v++)
        {
            float corr;

            if (m_walsh[v] >= 0)
            {
                corr = m_walshSign[v] * walsh[m_walsh[v]];
            }
            else
            {
                const std::vector<float>& w = m_codewords[v];
                corr = 0.0f;

                for (int k = 0; k < 32; k++) {
                    corr += a[k] * w[k];
                }
            }

            if (corr > best)
            {
                second = best;
                best = corr;
                bestValue = v;
                bestB0 = b0;
                bestB7 = b7;
            }
            else if (corr > second)
            {
                second = corr;
            }
        }
    }

    // b0..b7: S2's b1..b5 the MODCOD, b6 b7 the TYPE; S2X's (b0 = 1) b1..b6 the MODCOD, which says the frame size,
    // b7 the pilots
    auto fromPlsCode = [](int code, Header& h) {
        if (code & 128)
        {
            FECType type = FECType::Normal;
            h.m_modcod = s2xModcodBase + ((code >> 1) & 63);
            fixedFECType(h.m_modcod, type);
            h.m_type = frameType(type, (code & 1) != 0);
        }
        else
        {
            h.m_modcod = (code >> 2) & 31;
            h.m_type = code & 3;
        }
    };

    fromPlsCode((bestB0 << 7) | (bestValue << 1) | bestB7, header);
    header.m_margin = best > 0.0 ? (best - second) / best : 0.0;
    // A frame of known length: a dummy, S2's MODCODs (no short frames at 9/10), S2X's and its reserved ones of table 17b
    header.m_valid = (frameLength(header) > 0) && (header.m_margin >= m_minMargin);
    header.m_score = magnitude + best;  // The SOF's 26 symbols and the PLS code's 64, each with unit weight
    header.m_frequency = frequency;

    // Time slicing's (EN 302 307-1 Annex M), if wanted, or when S2's isn't clear (its PLS code correlating, symbol for
    // symbol, well under the SOF, as a real one's doesn't, whatever the SNR): then the one correlating better
    if ((kind == HeaderKind::TimeSlicing) || ((kind == HeaderKind::Any) && (!header.m_valid || (header.m_margin < m_searchMargin)
        || (best / 64.0 < m_timeSlicingTest * magnitude / sofSymbols))))
    {
        float t[2 * timeSlicingCodeBits];
        const uint8_t *timeSlicingScramble = timeSlicingScrambling();
        double power = 0.0, pairs = 0.0;
        turnBack(index + headerSymbols, timeSlicingHeaderSymbols - headerSymbols, phasor, step, nullptr, z + headerSymbols);

        for (int i = 0; i < 2 * timeSlicingCodeBits; i++)
        {
            t[i] = (z[sofSymbols + i] * rot * std::conj(m_zero[sofSymbols + i])).real();
            power += t[i] * t[i];
        }

        // First, cheaply, its repetition: each coded bit's two symbols, descrambled, agree. Their products' sum over its
        // half power is about 0.56 for a real header near threshold, 0 +/- 0.11 for noise or an S2 header's symbols
        for (int k = 0; k < timeSlicingCodeBits; k++)
        {
            const float a = timeSlicingScramble[2 * k] ? -t[2 * k] : t[2 * k];
            const float b = timeSlicingScramble[2 * k + 1] ? -t[2 * k + 1] : t[2 * k + 1];
            pairs += a * b;
        }

        if ((kind != HeaderKind::TimeSlicing) && (pairs < m_timeSlicingPairs * power / 2.0)) {
            return header;
        }

        int word = 0;
        float timeSlicingBest = 0.0f, timeSlicingSecond = 0.0f;
        decodeTimeSlicing(t, word, timeSlicingBest, timeSlicingSecond);

        // Either kind: time slicing's only if it correlates as a real header's does, with the SOF and against the
        // symbols' own rms (the best of its 65536 codewords with noise, or with an S2 header's symbols, reaches about
        // 0.38 of their rms, a real header near threshold 0.7), and better than S2's
        const double perSymbol = timeSlicingBest / (2.0 * timeSlicingCodeBits);
        const double rms = std::sqrt(power / (2.0 * timeSlicingCodeBits));

        if ((kind == HeaderKind::TimeSlicing)
            || ((perSymbol >= m_timeSlicingTest * magnitude / sofSymbols) && (perSymbol >= m_timeSlicingNoise * rms)
                && (perSymbol > best / 64.0)))
        {
            Header ts;
            ts.m_timeSlicing = true;
            ts.m_tsn = word & 255;
            fromPlsCode(word >> 8, ts);
            ts.m_margin = timeSlicingBest > 0.0f ? (timeSlicingBest - timeSlicingSecond) / timeSlicingBest : 0.0;
            ts.m_valid = (frameLength(ts) > 0) && (ts.m_margin >= m_minMargin);
            // Scaled to S2's header length, so the frequencies' scores compare
            ts.m_score = (magnitude + timeSlicingBest) * headerSymbols / timeSlicingHeaderSymbols;
            ts.m_frequency = frequency;
            return ts;
        }
    }

    return header;
}

void PLDecoder::decodeTimeSlicing(const float *r, int& word, float& best, float& second) const
{
    // Each coded bit's two symbols, descrambled and added: positive for a 0
    const uint8_t *scrambling = timeSlicingScrambling();
    float soft[timeSlicingCodeBits];

    for (int k = 0; k < timeSlicingCodeBits; k++) {
        soft[k] = (scrambling[2 * k] ? -r[2 * k] : r[2 * k]) + (scrambling[2 * k + 1] ? -r[2 * k + 1] : r[2 * k + 1]);
    }

    // Each input bit's branch metric, by the bit and the state it leaves
    float branch[16][2][16];

    for (int j = 0, k = 0; j < 16; j++)
    {
        const int n = timeSlicingHasP4(j) ? 5 : 4;

        for (int u = 0; u < 2; u++)
        {
            for (int state = 0; state < 16; state++)
            {
                const int p = timeSlicingParity(u, state);
                float m = 0.0f;

                for (int i = 0; i < n; i++) {
                    m += ((p >> (4 - i)) & 1) ? -soft[k + i] : soft[k + i];
                }

                branch[j][u][state] = m;
            }
        }

        k += n;
    }

    // Tail biting: from each state in turn, the paths that end where they started. Each state keeps its two best paths
    // (a list Viterbi decoder), so the best two words are found, the second for the margin
    struct Path { float m_metric; int m_word; };
    static const float none = -1e30f;
    best = none;
    second = none;
    word = 0;

    for (int start = 0; start < 16; start++)
    {
        Path paths[16][2], next[16][2];

        for (int state = 0; state < 16; state++) {
            paths[state][0] = paths[state][1] = Path{none, 0};
        }

        paths[start][0] = Path{0.0f, 0};

        for (int j = 0; j < 16; j++)
        {
            for (int state = 0; state < 16; state++) {
                next[state][0] = next[state][1] = Path{none, 0};
            }

            for (int state = 0; state < 16; state++)
            {
                for (int e = 0; e < 2; e++)
                {
                    const Path& from = paths[state][e];

                    if (from.m_metric <= none) {
                        continue;
                    }

                    for (int u = 0; u < 2; u++)
                    {
                        const int to = (u << 3) | (state >> 1);
                        const Path p{from.m_metric + branch[j][u][state], (from.m_word << 1) | u};

                        if (p.m_metric > next[to][0].m_metric)
                        {
                            next[to][1] = next[to][0];
                            next[to][0] = p;
                        }
                        else if (p.m_metric > next[to][1].m_metric)
                        {
                            next[to][1] = p;
                        }
                    }
                }
            }

            std::copy(&next[0][0], &next[0][0] + 32, &paths[0][0]);
        }

        for (int e = 0; e < 2; e++)
        {
            const Path& p = paths[start][e];

            if (p.m_metric > best)
            {
                second = best;
                best = p.m_metric;
                word = p.m_word;
            }
            else if (p.m_metric > second)
            {
                second = p.m_metric;
            }
        }
    }
}

int PLDecoder::frameLength(const Header& header)
{
    const int n = plFrameSymbols(header.m_modcod, (header.m_type & 2) ? FECType::Short : FECType::Normal, (header.m_type & 1) != 0);
    return (n > 0) && header.m_timeSlicing ? n + timeSlicingHeaderSymbols - headerSymbols : n;
}

std::complex<double> PLDecoder::turnBack(int64_t index, int length, std::complex<double> phasor,
    const std::complex<double>& step, const Cf *known, Cf *out) const
{
    if (known)
    {
        for (int i = 0; i < length; i++)
        {
            out[i] = at(index + i) * Cf((float) phasor.real(), (float) phasor.imag()) * std::conj(known[i]);
            phasor *= step;
        }
    }
    else
    {
        for (int i = 0; i < length; i++)
        {
            out[i] = at(index + i) * Cf((float) phasor.real(), (float) phasor.imag());
            phasor *= step;
        }
    }

    return phasor;
}

void PLDecoder::knownHeader(const Header& header, Cf *symbols)
{
    if (header.m_timeSlicing) {
        timeSlicingHeader(header.m_modcod, header.m_type, header.m_tsn, symbols);
    } else {
        plHeaderSymbols(header.m_modcod, header.m_type, symbols);
    }
}

PLDecoder::Header PLDecoder::searchHeader(int64_t index, double frequency) const
{
    Header best = decodeHeader(index, frequency);

    for (int k = 1; k <= m_searchSteps; k++)
    {
        for (int sign = -1; sign <= 1; sign += 2)
        {
            const Header h = decodeHeader(index, frequency + sign * k * m_searchStep);

            if (h.m_score > best.m_score) {
                best = h;
            }
        }
    }

    return best;
}

double PLDecoder::takeFrequencyCorrection()
{
    // Only from a lock that's lasted, so not from a false one's
    if ((m_status.m_state != State::Locked) || (m_lockedFrames < m_trustedFrames) || (m_misses > 0)
        || (std::fabs(m_frequency) < m_maxFrequency)) {
        return 0.0;
    }

    // In the front end's terms: of its input, not of the symbols conjugated
    const double f = m_inverted ? -m_frequency : m_frequency;
    lost();
    m_buffer.clear();
    m_diff.clear();
    m_base = m_end;
    m_scan = m_end + 1;
    m_vlsnrScan = m_scan;
    m_frequency = 0.0;
    m_status.m_frequency = 0.0;     // Now the NCO's: the status adds the two
    return f;
}

double PLDecoder::takeClockCorrection()
{
    const double ppm = m_clockCorrection;
    m_clockCorrection = 0.0;

    if (ppm != 0.0) {
        m_clockHold = true;
    }

    return ppm;
}

void PLDecoder::clockCorrected()
{
    m_clockHold = false;
    m_slipFrom = m_end;
    m_slipSum = 0;
    m_slipSymbols = 0;
    m_slipFrames = 0;
}

// VL-SNR's search starts after m_vlsnrSearchDelay, finds a header within a set 1 frame (its FFT's block later) and is
// confirmed by the next, a frame on: and another frame, for a header missed at its lowest SNRs. Time slicing's metric
// (near threshold, S2's misses its headers) starts after m_modeSymbols, then a header and the next with its longest
// frames. About 200 000 symbols: 6 s at 33 kS/s
int64_t PLDecoder::acquisitionSymbols()
{
    const int64_t vlsnr = m_vlsnrSearchDelay + 3 * (int64_t) vlsnrFrameSymbols(1) + m_vlsnrSearchFFT + vlsnrHeaderLength + m_maxSlip;
    const int64_t longest = plFrameSymbols(1, FECType::Normal, true) + timeSlicingHeaderSymbols - headerSymbols;
    const int64_t timeSlicing = m_modeSymbols + 2 * longest + timeSlicingHeaderSymbols;
    return std::max(vlsnr, timeSlicing);
}

bool PLDecoder::vlsnrCandidatePending() const
{
    return std::any_of(m_candidates.begin(), m_candidates.end(), [](const Candidate& c) { return c.m_vlsnr; });
}

void PLDecoder::lost(bool falseLock)
{
    if (falseLock) {
        m_status.m_falseLocks++;
    } else if (m_status.m_state == State::Locked) {
        m_status.m_syncLosses++;
    }

    // A lock that didn't last was probably false: its frames' frequencies and phase noise weren't the carrier's
    if (m_lockedFrames < m_trustedFrames)
    {
        m_frequency = m_frequencyBeforeLock;
        m_frequencyKnown = m_frequencyKnownBeforeLock;
        m_phaseNoise = m_phaseNoiseBeforeLock;
        m_phaseNoiseFrames = m_phaseNoiseFramesBeforeLock;
        m_ddPhaseNoise = m_ddPhaseNoiseBeforeLock;
        m_status.m_frequency = m_frequency;
        m_status.m_phaseNoise = std::sqrt(m_phaseNoise);
    }

    m_status.m_state = State::Searching;
    m_candidates.clear();
    m_modeStart = m_end;
    m_searchStart = m_end;
    m_misses = 0;
    m_lockedFrames = 0;
    m_gap = true;
}

void PLDecoder::getConstellation(std::vector<Cf>& cells, int maxCells) const
{
    const int n = std::min(maxCells, (int) m_display.size());
    cells.assign(m_display.begin(), m_display.begin() + n);
}

void PLDecoder::process(const Cf *symbols, int nbSymbols)
{
    append(symbols, nbSymbols);

    while (true)
    {
        if (m_status.m_state != State::Locked)
        {
            // Searching long without a lock: the other kind of header's metric. A header's decoded either way, but the
            // metric of the other kind's pairs is half noise, and near threshold misses it
            if (m_scan - m_modeStart > m_modeSymbols)
            {
                m_timeSlicing = !m_timeSlicing;
                m_modeStart = m_scan;
            }

            // Candidates whose next header has arrived, oldest first: the first confirmed is locked to
            bool confirmed = false;

            // Only once the search has passed the next header (both searches: VL-SNR's goes a block at a time), so that
            // every header before it is a candidate too (a header's decoded from time slicing's two slots, too; VL-SNR's
            // headers are 990 symbols, and a slip or two either side's tried)
            auto ready = [this](const Candidate& c) {
                return (c.m_next + (c.m_vlsnr ? vlsnrHeaderLength + m_maxSlip : timeSlicingHeaderSymbols) <= m_end)
                    && (c.m_next < std::min(m_scan, m_vlsnrScan));
            };

            while (!m_candidates.empty() && ready(m_candidates.front()))
            {
                const Candidate c = m_candidates.front();
                m_candidates.pop_front();
                bool found, inverted = false;

                if (c.m_vlsnr)
                {
                    // Its next frame's headers, coherently at the frequency it was found at
                    int64_t next = c.m_next;
                    Header nextHeader;
                    found = findVLSNRHeader(next, c.m_frequency, vlsnrSet(c.m_header.m_modcod, c.m_header.m_type), nextHeader);
                    // A conjugated pi/2 BPSK header's the header half a cycle a symbol out, which the coherent search
                    // finds (its pilots and data don't match): the spectrum's inverted
                    inverted = found && (std::fabs(std::remainder(c.m_frequency, 1.0)) > m_invertedFrequency);
                }
                else
                {
                    Cf sof, pls;
                    correlate(c.m_next, c.m_timeSlicing, sof, pls);
                    const double m = metric(sof, pls);
                    const double threshold = metricThreshold(c.m_timeSlicing, m_verifySigmas);
                    const Header nextHeader = decodeHeader(c.m_next, c.m_frequency, kindOf(c.m_header));
                    // The same MODCOD and TYPE (as in CCM), or decoded with confidence (VCM, ACM)
                    const bool consistent = (nextHeader.m_timeSlicing == c.m_header.m_timeSlicing)
                        && (((nextHeader.m_modcod == c.m_header.m_modcod) && (nextHeader.m_type == c.m_header.m_type))
                            || (nextHeader.m_margin >= 2.0 * m_searchMargin));
                    // Frames that repeat (the same packets, so the same symbols, as a still picture's) have data that
                    // looks like a header in the same place in each, so passes the next header's checks. The real
                    // header's metric is well above such data's, and it recurs too, so one much stronger waiting goes
                    // first (of the same search: VL-SNR's metric's another kind)
                    const bool stronger = std::any_of(m_candidates.begin(), m_candidates.end(), [&c](const Candidate& d) {
                        return !d.m_vlsnr && (d.m_metric > m_strongerCandidate * c.m_metric);
                    });
                    found = !stronger && (m > threshold) && nextHeader.m_valid && (nextHeader.m_margin >= m_searchMargin) && consistent;
                    // Confirmed far out of any frequency the front end can leave: the spectrum's inverted
                    inverted = found && (std::fabs(std::remainder(c.m_frequency, 1.0)) > m_invertedFrequency);
                }

                // The spectrum inverted: the symbols are conjugated, and searched again from the candidate's header
                if (inverted)
                {
                    invert();
                    m_scan = std::max(m_base + 1, c.m_index - 3);
                    m_vlsnrScan = m_scan;
                    confirmed = true;
                    break;
                }

                if (found)
                {
                    m_status.m_state = State::Locked;
                    m_slipSum = 0;
                    m_slipSymbols = 0;
                    m_slipFrames = 0;
                    m_frameStart = c.m_index;
                    m_lockStart = c.m_index;
                    m_frameHeader = c.m_header;
                    m_misses = 0;
                    m_lockedFrames = 0;
                    m_frequencyBeforeLock = m_frequency;
                    m_frequencyKnownBeforeLock = m_frequencyKnown;
                    m_phaseNoiseBeforeLock = m_phaseNoise;
                    m_phaseNoiseFramesBeforeLock = m_phaseNoiseFrames;
                    m_ddPhaseNoiseBeforeLock = m_ddPhaseNoise;
                    m_frequency = c.m_frequency;
                    m_coarseAverage = 0.0;
                    m_vlsnrCoarseAverage = 0.0;
                    m_vlsnrCoarseFrames = 0;
                    // The decision directed loop's, of the last lock (its drift, and its headers' estimates averaged
                    // for aliases, weren't this one's); its phase noise kept, as the pilots' is
                    m_ddEndError = 0.0;
                    m_ddResidualValid = false;
                    m_ddDrift = 0.0;
                    m_ddCoarseSum = 0.0;
                    m_ddCoarseSquares = 0.0;
                    m_ddCoarseFrames = 0;
                    m_frequencyKnown = true;
                    m_candidates.clear();
                    confirmed = true;

                    // Time slicing's or not: the metric then that of its headers. Its slices not yet seen repeating
                    m_timeSlicing = c.m_header.m_timeSlicing;
                    m_status.m_timeSlicing = m_timeSlicing;
                    m_tsnRepeats = 0;
                }
            }

            if (confirmed) {
                continue;
            }

            // Correlated 4 positions at a time, each as correlate() would: most need only that and the metric
            int64_t batch = 0;
            bool batched = false;
            Cf batchSof[4], batchPls[4];

            auto correlation = [&](int64_t index, Cf& sof, Cf& pls) {
                if (!batched || (index < batch) || (index >= batch + 4))
                {
                    correlate4(index, m_timeSlicing, batchSof, batchPls);
                    batch = index;
                    batched = true;
                }

                sof = batchSof[index - batch];
                pls = batchPls[index - batch];
            };

            for (; m_scan + timeSlicingHeaderSymbols + 3 <= m_end; m_scan++)
            {
                Cf sof, pls;
                correlation(m_scan, sof, pls);
                const double m = metric(sof, pls);
                bool candidate = false;

                // Under the threshold at the least spread (as most are), it's under the threshold: the spread's square
                // root isn't needed
                if ((m > m_metricMean[m_timeSlicing] + m_candidateSigmas * m_minMetricSigma)
                    && (m > metricThreshold(m_timeSlicing, m_candidateSigmas)))
                {
                    // The best of the next few, the peak being a symbol wide
                    int64_t bestIndex = m_scan;
                    double bestMetric = m;
                    Cf bestSof = sof, bestPls = pls;

                    for (int64_t k = m_scan + 1; k <= m_scan + 3; k++)
                    {
                        Cf s, p;
                        correlation(k, s, p);
                        const double mk = metric(s, p);

                        if (mk > bestMetric)
                        {
                            bestMetric = mk;
                            bestIndex = k;
                            bestSof = s;
                            bestPls = p;
                        }
                    }

                    // At the frequency known, from the last lock; but if the header's own differs (a strong header's
                    // is accurate), around that too, in case the last lock was false
                    const double local = localFrequency(bestSof, bestPls);
                    Header header = m_frequencyKnown ? decodeHeader(bestIndex, m_frequency) : searchHeader(bestIndex, local);

                    if (m_frequencyKnown && (std::fabs(std::remainder(local - m_frequency, 1.0)) > m_knownTolerance))
                    {
                        const Header h = searchHeader(bestIndex, local);

                        if (h.m_valid && (h.m_margin >= m_searchMargin) && (h.m_score > header.m_score)) {
                            header = h;
                        }
                    }

                    if (header.m_valid && (header.m_margin >= m_searchMargin))
                    {
                        Candidate c;
                        c.m_index = bestIndex;
                        c.m_header = header;
                        c.m_frequency = header.m_frequency;
                        c.m_metric = bestMetric;
                        c.m_timeSlicing = m_timeSlicing;
                        c.m_vlsnr = false;
                        c.m_next = bestIndex + frameLength(header);
                        m_candidates.push_back(c);

                        if (m_candidates.size() > m_maxCandidates) {
                            m_candidates.pop_front();
                        }

                        m_scan = bestIndex + 3;
                        candidate = true;
                    }
                }

                // The metric's statistics where there's (almost always) no header: everywhere but a candidate's, not
                // only under the threshold, so ones that are wrong recover (taken to nothing by zero samples, as a
                // device gives while it retunes, every symbol was over it, a header searched for at each, and they
                // were never updated again)
                if (!candidate)
                {
                    const int k = m_timeSlicing;
                    const double alpha = m_metricCount[k] < 4096 ? 1.0 / (double) (m_metricCount[k] + 1) : 1.0 / 4096.0;
                    const double delta = m - m_metricMean[k];
                    m_metricMean[k] += alpha * delta;
                    m_metricVar[k] += alpha * (delta * delta - m_metricVar[k]);
                    m_metricCount[k]++;
                }
            }

            // VL-SNR's search, a block at a time, once S2's has searched a while
            m_vlsnrScan = std::max(m_vlsnrScan, m_base + 1);

            if (m_scan - m_searchStart < m_vlsnrSearchDelay) {
                m_vlsnrScan = std::max(m_vlsnrScan, m_scan);
            }

            while (m_vlsnrScan + m_vlsnrSearchFFT <= m_end) {
                searchVLSNR();
            }

            // Candidates waiting are in order of their next headers too, but for frame sizes that differ: any ready?
            if (std::any_of(m_candidates.begin(), m_candidates.end(), ready))
            {
                std::sort(m_candidates.begin(), m_candidates.end(), [](const Candidate& a, const Candidate& b) { return a.m_next < b.m_next; });
                continue;
            }

            int64_t keep = std::min(m_scan, m_vlsnrScan);

            for (const Candidate& c : m_candidates) {
                keep = std::min(keep, c.m_index);
            }

            trim(keep);
            return;
        }

        // Locked: a frame at a time, once the next header has arrived
        int64_t next = m_frameStart + frameLength(m_frameHeader);
        // After a VL-SNR frame, the next one's headers, a symbol either side
        const bool afterVLSNR = vlsnrSet(m_frameHeader.m_modcod, m_frameHeader.m_type) && !m_frameHeader.m_timeSlicing;

        if (next + (afterVLSNR ? vlsnrHeaderLength + m_maxSlip : timeSlicingHeaderSymbols) > m_end)
        {
            // Until the lock's trusted, from its start, to search again from if it's false
            trim(m_lockedFrames < m_trustedFrames ? std::min(m_frameStart, m_lockStart) : m_frameStart);
            return;
        }

        Cf sof, pls;
        correlate(next, m_timeSlicing, sof, pls);
        const double m = metric(sof, pls);
        Header nextHeader = decodeHeader(next, m_frequency, kindOf(m_frameHeader));
        // Time slicing's metric has only 32 of its header's 77 pairs (as cheap as S2's, to search with): its whole code
        // decoded clearly, as this frame's MODCOD and TYPE, says the header's there too
        bool present = (m > metricThreshold(m_timeSlicing, m_confirmSigmas))
            || (m_frameHeader.m_timeSlicing && nextHeader.m_valid && (nextHeader.m_margin >= m_searchMargin)
                && (nextHeader.m_modcod == m_frameHeader.m_modcod) && (nextHeader.m_type == m_frameHeader.m_type));

        // After a VL-SNR frame, the next VL-SNR frame's headers, 990 symbols, correlated coherently: below the SNR the
        // PLHEADER can be found at alone (about -3 dB), S2's metric passes its threshold by chance now and then, and the
        // PLS code decodes as any MODCOD, whose length would lose the lock. S2's header only if they aren't there (an
        // S2 frame next, in VCM)
        const int64_t expected = next;

        if (afterVLSNR && findVLSNRHeader(next, m_frequency, vlsnrSet(m_frameHeader.m_modcod, m_frameHeader.m_type), nextHeader)) {
            present = true;
        }

        // The symbol clock, from the slips: enough of them one way, their rate's the clock's error. Not while a
        // correction's on its way, nor from frames made before it
        if (afterVLSNR && present && nextHeader.m_valid && !m_clockHold && (m_frameStart >= m_slipFrom))
        {
            m_slipSum += (int) (next - expected);
            m_slipSymbols += next - m_frameStart;
            m_slipFrames++;

            if (std::abs(m_slipSum) >= m_slipCorrection) {
                m_clockCorrection -= 1e6 * m_slipSum / (double) m_slipSymbols;  // Headers early: the signal's faster
            }

            if ((std::abs(m_slipSum) >= m_slipCorrection) || (m_slipFrames >= m_maxSlipFrames))
            {
                m_slipSum = 0;
                m_slipSymbols = 0;
                m_slipFrames = 0;
            }
        }

        // Where samples were lost (the input overflowing), the frame they cut, and those after till the lock's lost,
        // are noise, which the LDPC decoder would spend its every iteration on, holding up the frames decoded with
        // them. Without a header either side, a frame's probably one of those (or at low SNR, a header's missed)
        m_suspect = m_misses > 0;

        if (!present || !nextHeader.m_valid)
        {
            m_status.m_headerMisses++;
            m_misses++;
            nextHeader.m_valid = false;
            m_suspect = true;
        }
        else
        {
            m_misses = 0;
        }

        processFrame(m_frameStart, m_frameHeader, next, nextHeader);

        if (m_implausible || (m_misses >= m_maxMisses))
        {
            // A lock that hadn't lasted was probably false: searched again from just after where it started, as the
            // real headers it passed over (the signal's, at low SNR, where noise passes a header's checks now and then)
            // weren't searched for meanwhile
            const bool untrusted = m_lockedFrames < m_trustedFrames;
            lost(m_implausible);
            m_scan = untrusted ? m_lockStart + 1 : next - headerSymbols;
            m_vlsnrScan = m_scan;
            continue;
        }

        // Without its header, the next frame is taken to be like this one (as in CCM), but for its time slice, unknown
        // unless frames have been of the same one for a while (a single slice)
        m_frameStart = next;

        if (nextHeader.m_valid)
        {
            m_tsnRepeats = (nextHeader.m_tsn >= 0) && (nextHeader.m_tsn == m_frameHeader.m_tsn) ? m_tsnRepeats + 1 : 0;
            m_frameHeader = nextHeader;
        }
        else if (m_frameHeader.m_timeSlicing && (m_tsnRepeats < m_sameTsnFrames))
        {
            m_frameHeader.m_tsn = -1;
        }
    }
}

void PLDecoder::modulationOff(int64_t frameStart, int64_t blockStart, int length, const Cf *known, double frequency,
    std::vector<Cf>& u) const
{
    // A phasor stepped a symbol at a time (in double: a block's at most a header, 90 symbols)
    u.resize(length);
    const std::complex<double> rot = std::polar(1.0, -2.0 * m_pi * frequency * (double) (blockStart - frameStart));
    turnBack(blockStart, length, rot, std::polar(1.0, -2.0 * m_pi * frequency), known, u.data());
}

// A dummy frame's known symbols (all of them) at the frequency so far: their Es/N0 from the coherent mean of each
// short run (so a frequency a little out doesn't matter) against their power. A new lock's false if it's too low
bool PLDecoder::checkDummy(int64_t start, const Header& header)
{
    // A time slicing header's TSN not known, only its SOF is
    const int length = headerLength(header);
    const int knownLength = header.m_timeSlicing && (header.m_tsn < 0) ? sofSymbols : length;
    const int total = length + dummySlots * slotSymbols;
    const std::vector<uint8_t>& scrambling = m_scrambler.sequence();
    Cf known[timeSlicingHeaderSymbols];
    knownHeader(header, known);
    const std::complex<double> step = std::polar(1.0, -2.0 * m_pi * m_frequency);
    std::complex<double> phasor(1.0, 0.0), run(0.0, 0.0);
    const int runSymbols = 30;
    double coherent = 0.0, power = 0.0;
    int used = 0;   // Symbols known: the runs are of these, so one isn't stretched over those skipped

    for (int n = 0; n < total; n++)
    {
        if ((n >= knownLength) && (n < length))
        {
            phasor *= step;
            continue;
        }

        const Cf k = n < length ? known[n] : m_pilotValue * m_quarterTurns[scrambling[(size_t) (n - length)]];
        const std::complex<double> v = std::complex<double>(at(start + n) * std::conj(k)) * phasor;
        run += v;
        power += std::norm(v);
        phasor *= step;

        if (++used % runSymbols == 0)
        {
            coherent += std::norm(run) / runSymbols;
            run = 0.0;
        }
    }

    const int counted = total - (length - knownLength);
    const int runs = counted / runSymbols;
    const double signal = coherent / (runs * runSymbols);   // |mean|^2, plus noise / run length
    const double noise = power / counted - signal;
    const double esn0 = noise > 0.0 ? 10.0 * std::log10(std::max(1e-10, signal) / noise) : 50.0;
    return esn0 >= m_minLockEsN0;
}

// A dummy frame's data is unmodulated (I = Q = 1 / sqrt 2), scrambled, so all its symbols are known: the frequency from
// them, as from a frame's known blocks, so it's right for the frames that follow (a transmitter with nothing to send
// sends only these, and the first frames after would be lost, their headers decoded at a frequency out by the error of
// the spectrum's estimate). Luise and Reggiannini's estimator over the frame, then the phase step from its first half
// to its second, which over 3330 known symbols is well within the step's unambiguous range
void PLDecoder::trackDummy(int64_t start, const Header& header, const Header& nextHeader)
{
    // The header too, unless it's time slicing's whose TSN isn't known: then the data's slots alone
    const int length = headerLength(header);
    const int first = header.m_timeSlicing && (header.m_tsn < 0) ? length : 0;
    const int total = length + dummySlots * slotSymbols - first;
    const std::vector<uint8_t>& scrambling = m_scrambler.sequence();
    Cf known[timeSlicingHeaderSymbols];

    if (first == 0) {
        knownHeader(header, known);
    }

    m_dummy.resize(total);
    const std::complex<double> step = std::polar(1.0, -2.0 * m_pi * m_frequency);
    std::complex<double> phasor = std::polar(1.0, -2.0 * m_pi * m_frequency * first);

    for (int n = 0; n < total; n++)
    {
        const int x = first + n;
        const Cf k = x < length ? known[x] : m_pilotValue * m_quarterTurns[scrambling[(size_t) (x - length)]];
        m_dummy[n] = at(start + x) * Cf((float) phasor.real(), (float) phasor.imag()) * std::conj(k);
        phasor *= step;
    }

    std::complex<double> sum(0.0, 0.0);

    for (int lag = 1; lag <= m_lrLags; lag++)
    {
        for (int i = lag; i < total; i++) {
            sum += std::complex<double>(m_dummy[i] * std::conj(m_dummy[i - lag]));
        }
    }

    const double coarse = std::arg(sum) / (m_pi * (m_lrLags + 1));
    const int half = total / 2;
    const std::complex<double> halfStep = std::polar(1.0, -2.0 * m_pi * coarse);
    std::complex<double> a(0.0, 0.0), b(0.0, 0.0), turn(1.0, 0.0);

    for (int n = 0; n < total; n++)
    {
        (n < half ? a : b) += std::complex<double>(m_dummy[n]) * turn;
        turn *= halfStep;
    }

    const double refined = m_frequency + coarse + std::arg(b * std::conj(a)) / (2.0 * m_pi * half);

    if (nextHeader.m_valid && (std::abs(a) > 0.0) && (std::abs(b) > 0.0))
    {
        const bool settled = m_lockedFrames > m_settledFrames;
        m_frequency = !settled ? refined : m_frequency + 0.3 * (refined - m_frequency);
        m_status.m_frequency = m_frequency;
    }
}

// The pilot blocks are about 740 symbols apart (S2's 36 symbol blocks, with 32 to 36 between), so the phase is
// interpolated between them as for S2's frames; at the low SNRs VL-SNR's for, each block's phase is the mean of those
// either side too (their phasors turned by the frequency's step), as far as the noise wants. The VL-SNR header's index
// is decided from its 16 rows' correlations (each row inverted or not by the index's sequence), its symbols at the
// phase interpolated between the PLHEADER and the first pilot block; then it's a known block too
void PLDecoder::processVLSNR(int64_t start, const Header& header, int64_t next, const Header& nextHeader)
{
    const int set = vlsnrSet(header.m_modcod, header.m_type);
    const std::vector<uint8_t>& mask = vlsnrPilots(set);
    const std::vector<uint8_t>& scrambling = m_scrambler.sequence();
    const int length = headerLength(header);
    const int64_t vlStart = start + length;
    const int64_t bodyStart = vlStart + vlsnrHeaderSymbols;
    const uint8_t *sof = sofBits();

    // The known blocks, in order (their symbols in members, kept: these were 3 lists of a vector each a frame)
    typedef VLSNRBlock Block;
    std::vector<Block>& blocks = m_vlsnrBlocks;
    blocks.clear();
    Block first{};
    first.m_start = start;
    first.m_known = m_vlsnrFirstKnown;

    if (header.m_timeSlicing && (header.m_tsn < 0))
    {
        first.m_length = sofSymbols;

        for (int n = 0; n < sofSymbols; n++) {
            m_vlsnrFirstKnown[n] = m_zero[n] * (sof[n] ? -1.0f : 1.0f);
        }
    }
    else
    {
        knownHeader(header, m_vlsnrFirstKnown);
        first.m_length = length;
    }

    blocks.push_back(first);

    // The pilot blocks' symbols, in order, scrambled from offset (0: the scrambling starts after the VL-SNR header; its
    // length: after the PLHEADER)
    for (int h = 0; h < 2; h++)
    {
        const int offset = h ? vlsnrHeaderSymbols : 0;
        std::vector<Cf>& known = m_vlsnrPilotKnown[h];
        known.clear();

        for (size_t i = 0; i < mask.size(); i++)
        {
            if (mask[i]) {
                known.push_back(m_pilotValue * m_quarterTurns[scrambling[i + offset]]);
            }
        }
    }

    // The pilot blocks, with those symbols
    auto addPilots = [&](std::vector<Block>& list, int h) {
        const Cf *known = m_vlsnrPilotKnown[h].data();

        for (size_t k = 0; k < mask.size(); k++)
        {
            if (mask[k] && ((k == 0) || !mask[k - 1]))
            {
                Block b{};
                b.m_start = bodyStart + (int64_t) k;
                b.m_length = 0;
                b.m_known = known;

                for (size_t i = k; (i < mask.size()) && mask[i]; i++) {
                    b.m_length++;
                }

                known += b.m_length;
                list.push_back(b);
            }
        }
    };

    // Which offset: the pilots' coherent power each way, at the frequency so far
    std::vector<Cf>& u = m_blockSymbols;
    int offset = 0;
    {
        double power[2] = {0.0, 0.0};

        for (int h = 0; h < 2; h++)
        {
            std::vector<Block>& trial = m_vlsnrTrial;
            trial.clear();
            addPilots(trial, h);

            for (const Block& b : trial)
            {
                modulationOff(start, b.m_start, b.m_length, b.m_known, m_frequency, u);
                Cf c(0.0f, 0.0f);

                for (const Cf& v : u) {
                    c += v;
                }

                power[h] += std::norm(c);
            }
        }

        offset = power[1] > power[0] ? vlsnrHeaderSymbols : 0;
    }

    addPilots(blocks, offset != 0 ? 1 : 0);
    Block last{};
    last.m_start = next;
    last.m_known = m_vlsnrLastKnown;

    if (nextHeader.m_valid)
    {
        last.m_length = headerLength(nextHeader);
        knownHeader(nextHeader, m_vlsnrLastKnown);
    }
    else
    {
        last.m_length = sofSymbols;

        for (int n = 0; n < sofSymbols; n++) {
            m_vlsnrLastKnown[n] = m_zero[n] * (sof[n] ? -1.0f : 1.0f);
        }
    }

    blocks.push_back(last);

    // Frequency: Luise and Reggiannini's estimator on the known symbols, and the VL-SNR header's rows (each's sign
    // unknown till the index is, but the same along it), each lag's correlation the mean of its pairs. (Summed, the
    // short blocks weighted the short lags, which biased it a third low: at 100 kS/s, the first frame's frequency
    // was then out enough for the pilots' step to alias a whole 1 / 738, and the VL-SNR header, at that frequency,
    // read as another index)
    std::complex<double> lagSums[m_lrLags + 1];
    int lagCounts[m_lrLags + 1] = {0};

    auto correlateLags = [&](int64_t blockStart, int length, const Cf *known) {
        modulationOff(start, blockStart, length, known, m_frequency, u);

        for (int lag = 1; (lag <= m_lrLags) && (lag < length); lag++)
        {
            for (int i = lag; i < length; i++) {
                lagSums[lag] += std::complex<double>(u[i] * std::conj(u[i - lag]));
            }

            lagCounts[lag] += length - lag;
        }
    };

    for (const Block& b : blocks) {
        correlateLags(b.m_start, b.m_length, b.m_known);
    }

    for (int row = 0; row < 16; row++) {
        correlateLags(vlStart + 2 + 56 * row, 56, &m_vlsnrKnown[0][headerSymbols + 2 + 56 * row]);
    }

    std::complex<double> sum(0.0, 0.0);

    for (int lag = 1; lag <= m_lrLags; lag++)
    {
        if (lagCounts[lag] > 0) {
            sum += lagSums[lag] / (double) lagCounts[lag];
        }
    }

    const double coarse = std::arg(sum) / (m_pi * (m_lrLags + 1));
    const bool settled = m_lockedFrames > m_settledFrames;
    double frequency = m_frequency;

    if (!settled || (std::fabs(coarse) > m_coarseLimit))
    {
        frequency += coarse;
        m_vlsnrCoarseAverage = 0.0;
        m_vlsnrCoarseFrames = 0;
    }
    else if (nextHeader.m_valid)
    {
        // Settled, the frequency's from the pilots' steps, which alias at whole 1 / 738s: the estimates' average says
        // if it's slipped to one (a single frame's, at VL-SNR's SNRs, too noisy to)
        const double weight = std::max(0.1, 1.0 / (double) (m_vlsnrCoarseFrames + 1));
        m_vlsnrCoarseAverage += weight * (coarse - m_vlsnrCoarseAverage);
        m_vlsnrCoarseFrames++;
        const double cycles = m_vlsnrCoarseAverage * m_vlsnrPilotPeriod;

        if ((std::fabs(cycles) > 0.6) && (m_vlsnrCoarseFrames >= 3))
        {
            const double aliases = std::round(cycles) / m_vlsnrPilotPeriod;
            frequency += aliases;
            m_frequency += aliases;
            m_vlsnrCoarseAverage -= aliases;
        }
    }

    auto centre = [start](const Block& b) { return (double) (b.m_start - start) + (b.m_length - 1) / 2.0; };
    double gain = 1.0, step = 0.0;
    bool stepKnown = false;

    // Each block's phasor, the mean step between pilot blocks (all nearly the same distance apart), each block's phase
    // unwrapped from the last's by it; then each smoothed with its neighbours' (w either side) at low SNR
    auto phases = [&](int w) {
        double gainSum = 0.0, known = 0.0;

        for (Block& b : blocks)
        {
            modulationOff(start, b.m_start, b.m_length, b.m_known, frequency, u);
            Cf c(0.0f, 0.0f);

            for (const Cf& v : u) {
                c += v;
            }

            b.m_phasor = c;
            gainSum += std::abs(c);
            known += b.m_length;
        }

        gain = std::max(1e-6, gainSum / known);
        std::complex<double> z(0.0, 0.0);
        double distance = 0.0;
        int steps = 0;

        // From pilot block to pilot block
        auto isPilot = [&](const Block& b) { return (b.m_start >= bodyStart) && (b.m_start < next); };

        for (size_t k = 1; k < blocks.size(); k++)
        {
            if (isPilot(blocks[k]) && isPilot(blocks[k - 1]))
            {
                z += std::complex<double>(blocks[k].m_phasor) * std::conj(std::complex<double>(blocks[k - 1].m_phasor));
                distance += centre(blocks[k]) - centre(blocks[k - 1]);
                steps++;
            }
        }

        stepKnown = (steps > 0) && (std::abs(z) > 0.0);
        step = stepKnown ? std::arg(z) / (distance / steps) : 0.0;
        std::vector<Cf>& smoothed = m_vlsnrSmoothed;
        smoothed.resize(blocks.size());

        for (size_t k = 0; k < blocks.size(); k++)
        {
            std::complex<double> s(0.0, 0.0);
            const size_t from = k >= (size_t) w ? k - w : 0;
            const size_t to = std::min(blocks.size() - 1, k + w);

            for (size_t j = from; j <= to; j++) {
                s += std::complex<double>(blocks[j].m_phasor) * std::polar(1.0, -step * (centre(blocks[j]) - centre(blocks[k])));
            }

            smoothed[k] = Cf((float) s.real(), (float) s.imag());
        }

        for (size_t k = 0; k < blocks.size(); k++)
        {
            Block& b = blocks[k];
            const double phase = std::arg(smoothed[k]);

            if (k == 0)
            {
                b.m_phase = phase;
            }
            else
            {
                const double predicted = blocks[k - 1].m_phase + step * (centre(b) - centre(blocks[k - 1]));
                b.m_phase = predicted + wrap(phase - predicted);
            }
        }
    };

    phases(0);

    // The known symbols' noise, relative to their power
    auto knownNoise = [&]() {
        double noise = 0.0, known = 0.0;

        for (const Block& b : blocks)
        {
            modulationOff(start, b.m_start, b.m_length, b.m_known, frequency, u);
            const Cf rot = std::polar(1.0f / (float) gain, (float) -b.m_phase);

            for (const Cf& v : u) {
                noise += std::norm(v * rot - Cf(1.0f, 0.0f));
            }

            known += b.m_length;
        }

        return std::max(1e-6, noise / known);
    };

    double noise = knownNoise();
    // Below 3 dB, each block's phase from its neighbours' too: two either side below -3 dB
    const double esn0Raw = -10.0 * std::log10(noise);
    const int smoothing = esn0Raw > 3.0 ? 0 : esn0Raw > -3.0 ? 1 : 2;

    if (smoothing > 0) {
        phases(smoothing);
    }

    if (nextHeader.m_valid && (stepKnown || !settled))
    {
        const double refined = frequency + step / (2.0 * m_pi);
        m_frequency = !settled ? refined : m_frequency + 0.3 * (refined - m_frequency);
        m_status.m_frequency = m_frequency;
    }

    // A point's phase: interpolated between the blocks either side (by their centres)
    const double invGain = 1.0 / gain;
    const double stepAngle = -2.0 * m_pi * frequency;

    auto rotationAt = [&](size_t a, double x) {
        const Block& ba = blocks[a];
        const Block& bb = blocks[std::min(a + 1, blocks.size() - 1)];
        const double ca = centre(ba), cb = centre(bb);
        const double slope = cb > ca ? (bb.m_phase - ba.m_phase) / (cb - ca) : 0.0;
        return std::make_pair(std::polar(invGain, wrap(stepAngle * x - (ba.m_phase + slope * (x - ca)))),
            std::polar(1.0, stepAngle - slope));
    };

    // The VL-SNR header: its symbols, between the PLHEADER and the first pilot block, along index 0's; each row's
    // correlation; the index (of the PLHEADER's set: another's frame would be laid out and decoded wrongly) whose
    // sequence's signs sum them largest
    const Cf *zero = m_vlsnrKnown[0] + headerSymbols;
    double rows[16] = {0.0};
    {
        auto r = rotationAt(0, (double) (vlStart - start));
        std::complex<double> rot = r.first;

        for (int n = 0; n < vlsnrHeaderSymbols; n++)
        {
            const std::complex<double> v = std::complex<double>(at(vlStart + n)) * rot * std::conj(std::complex<double>(zero[n]));
            rot *= r.second;

            if ((n >= 2) && (n < vlsnrHeaderSymbols - 2)) {
                rows[(n - 2) / 56] += v.real();
            }
        }
    }

    int index = -1;
    double best = -1e30, second = -1e30;

    for (int i = 0; i < vlsnrIndexes; i++)
    {
        if (vlsnrModcod(i).m_set != set) {
            continue;
        }

        double c = 0.0;

        for (int row = 0; row < 16; row++) {
            c += rows[row] * m_vlsnrSigns[i][row];
        }

        if (c > best)
        {
            second = best;
            best = c;
            index = i;
        }
        else if (c > second)
        {
            second = c;
        }
    }

    // How clear: the best's correlation in standard deviations of its noise (896 symbols' real parts), and over the
    // next best's. A new lock whose header's not clear is false (its next header passed the checks by chance)
    const double sigma = std::sqrt(896.0 * noise / 2.0);
    const double clarity = best / sigma;
    const double margin = (best - second) / sigma;

    if ((m_lockedFrames < m_trustedFrames) && ((clarity < 8.0) || (margin < 4.0)))
    {
        m_implausible = true;
        return;
    }

    m_status.m_modcod = header.m_modcod;
    m_status.m_type = header.m_type;
    m_status.m_vlsnrScramblingThroughHeader = offset != 0;
    const VLSNRModcod& vl = vlsnrModcod(index);

    // Frames skipped that may have carried data (unlike a dummy): the next one's after a gap, so a packet the BBFRAME
    // decoder had begun isn't continued from it (a lost frame of a whole number of packets' data leaves the SYNCD
    // agreeing)
    if (margin < 2.0)
    {
        // Not clear which: nothing to decode, though the frame's length is known
        m_status.m_vlsnr = -1;
        m_status.m_reservedFrames++;
        m_gap = true;
        return;
    }

    if (index == vlsnrDummy)
    {
        m_status.m_vlsnr = index;
        m_status.m_vlsnrSeen |= 1u << index;
        m_status.m_dummyFrames++;
        return;
    }

    // An unassigned index's (perhaps an assigned one's misread): its length's known, but there's nothing to decode
    if (!vl.m_valid)
    {
        m_status.m_vlsnr = -1;
        m_status.m_reservedFrames++;
        m_gap = true;
        return;
    }

    m_status.m_vlsnr = index;
    m_status.m_vlsnrSeen |= 1u << index;

    // Now a known block too, the second, and the blocks' phases again with it
    {
        Block b{};
        b.m_start = vlStart;
        b.m_length = vlsnrHeaderSymbols;
        b.m_known = m_vlsnrHeaderKnown;
        vlsnrHeader(index, m_vlsnrHeaderKnown);
        blocks.insert(blocks.begin() + 1, b);
        phases(smoothing);
        noise = knownNoise();
    }

    const double esn0 = -10.0 * std::log10(noise);

    // The data: each run between known blocks turned by the phase interpolated across it, descrambled (QPSK's by
    // j^-R, pi/2 BPSK's by (-1)^R and its quarter turns, onto the real axis)
    XFECFrame& frame = m_frame;
    frame.m_modcod = header.m_modcod;
    frame.m_type = header.m_type;
    frame.m_modulation = vl.m_modulation;
    frame.m_rate = vl.m_rate;
    frame.m_fecType = vl.m_type;
    frame.m_sequence = m_sequence;
    frame.m_gap = m_gap;
    m_gap = false;
    frame.m_suspect = m_suspect;
    frame.m_timeSlicing = header.m_timeSlicing;
    frame.m_tsn = header.m_tsn;
    frame.m_vlsnr = index;
    frame.m_cells.resize((size_t) vlsnrDataSymbols(set));
    const bool qpsk = vl.m_modulation == Modulation::QPSK;
    size_t d = 0;
    size_t block = 1;   // The VL-SNR header: the data's first run's after it
    double error = 0.0;

    for (size_t k = 0; k < mask.size();)
    {
        if (mask[k])
        {
            k++;
            continue;
        }

        // A run of data, between blocks block and block + 1
        while ((block + 1 < blocks.size()) && (blocks[block + 1].m_start < bodyStart + (int64_t) k)) {
            block++;
        }

        size_t end = k;

        while ((end < mask.size()) && !mask[end]) {
            end++;
        }

        auto r = rotationAt(block, (double) (bodyStart + (int64_t) k - start));
        std::complex<double> rot = r.first;
        const Cf *y = &at(bodyStart + (int64_t) k);

        for (size_t i = k; i < end; i++, d++)
        {
            Cf v = y[i - k] * Cf((float) rot.real(), (float) rot.imag());
            rot *= r.second;

            if (qpsk)
            {
                v *= std::conj(m_quarterTurns[scrambling[i + offset]]);
                error += std::norm(v - nearestQPSK(v));
            }
            else
            {
                v *= std::conj(headerSymbolZero((int) d)) * ((scrambling[i + offset] & 1) ? -1.0f : 1.0f);
                error += std::norm(v - Cf(v.real() < 0.0f ? -1.0f : 1.0f, 0.0f));
            }

            frame.m_cells[d] = v;
        }

        k = end;
    }

    const double dataError = error / std::max<size_t>(1, d);
    const double mer = -10.0 * std::log10(std::max(1e-10, dataError));
    frame.m_noise = (float) (std::max(noise, dataError) / 2.0);
    m_esn0Average = m_status.m_frames == 0 ? esn0 : 0.9 * m_esn0Average + 0.1 * esn0;
    m_status.m_esn0 = m_esn0Average;
    m_status.m_mer = m_status.m_frames == 0 ? mer : 0.9 * m_status.m_mer + 0.1 * mer;
    m_status.m_frames++;
    m_status.m_vlsnrFrames++;
    m_status.m_tsn = header.m_tsn;

    const int display = std::min((int) frame.m_cells.size(), 4096);
    const int stride = std::max(1, (int) frame.m_cells.size() / display);
    m_display.clear();

    for (int i = 0; i < (int) frame.m_cells.size() && (int) m_display.size() < display; i += stride) {
        m_display.push_back(frame.m_cells[i]);
    }

    if (frame.m_timeSlicing && (frame.m_tsn < 0))
    {
        m_gap = true;
        return;
    }

    if (m_callback) {
        m_callback(frame);
    }
}

bool PLDecoder::findVLSNRHeader(int64_t& next, double frequency, int currentSet, Header& header) const
{
    double best = 0.0;
    int bestSet = 0, bestPlhSet = 0, bestDelta = 0;

    for (int delta = -m_maxSlip; delta <= m_maxSlip; delta++)
    {
        int set, vlIndex, plhSet;
        const double score = vlsnrHeaderScore(next + delta, frequency, set, vlIndex, plhSet);

        if (score > best)
        {
            best = score;
            bestSet = set;
            bestPlhSet = plhSet;
            bestDelta = delta;
        }
    }

    if ((best <= m_vlsnrPresentScore) || ((bestSet != currentSet) && (bestPlhSet != bestSet))) {
        return false;
    }

    next += bestDelta;
    header = Header();
    header.m_valid = true;
    header.m_modcod = vlsnrSetModcod(bestSet);
    header.m_type = 1;
    header.m_margin = 1.0;
    header.m_score = best;
    header.m_frequency = frequency;
    return true;
}

void PLDecoder::searchVLSNR()
{
    const int64_t p0 = m_vlsnrScan;
    std::vector<std::complex<double>>& x = m_vlsnrFFT;
    std::vector<double>& prefix = m_vlsnrPower;
    x.resize(m_vlsnrSearchFFT);
    prefix.resize(m_vlsnrSearchFFT + 1);
    prefix[0] = 0.0;

    for (int i = 0; i < m_vlsnrSearchFFT; i++)
    {
        x[i] = std::complex<double>(m_diff[(size_t) (p0 + i - m_base)]);
        prefix[i + 1] = prefix[i] + std::norm(x[i]);
    }

    // The correlation at each position: the inverse FFT of the differences' spectrum times the known's conjugate
    dvbs::fft(x);

    for (int i = 0; i < m_vlsnrSearchFFT; i++) {
        x[i] *= m_vlsnrDiffSpectrum[i];
    }

    dvbs::fft(x, true);
    // In standard deviations of noise's: its power, the differences' over the window, as many as are known
    auto sigmas = [&](int q) {
        const double power = (prefix[q + vlsnrHeaderLength] - prefix[q]) * m_vlsnrPairs / vlsnrHeaderLength;
        return power > 0.0 ? std::abs(x[q]) / m_vlsnrSearchFFT / std::sqrt(power) : 0.0;
    };

    for (int q = 0; q < m_vlsnrSearchBlock; q++)
    {
        const double z = sigmas(q);

        if (z <= m_vlsnrSearchSigmas) {
            continue;
        }

        // The peak's a symbol wide: the best of the next two
        bool peak = true;

        for (int k = q + 1; k <= std::min(q + 2, m_vlsnrSearchFFT - vlsnrHeaderLength); k++) {
            peak = peak && (sigmas(k) <= z);
        }

        if (!peak) {
            continue;
        }

        const int64_t index = p0 + q;
        Header header;
        double score;

        if (verifyVLSNR(index, header, score))
        {
            Candidate c;
            c.m_index = index;
            c.m_header = header;
            c.m_frequency = header.m_frequency;
            c.m_metric = score;
            c.m_timeSlicing = false;
            c.m_vlsnr = true;
            c.m_next = index + frameLength(header);
            m_candidates.push_back(c);

            if (m_candidates.size() > m_maxCandidates) {
                m_candidates.pop_front();
            }
        }

        q += 2;
    }

    m_vlsnrScan += m_vlsnrSearchBlock;
}

bool PLDecoder::verifyVLSNR(int64_t index, Header& header, double& score)
{
    std::vector<std::complex<double>>& z = m_vlsnrVerify;
    double power = 0.0;

    for (int n = 0; n < vlsnrHeaderLength; n++) {
        power += std::norm(at(index + n));
    }

    score = 0.0;
    double frequency = 0.0;
    int set = 0;

    for (const VLSNRHypothesis& h : m_vlsnrHypotheses)
    {
        z.assign(m_vlsnrSearchFFT, 0.0);

        for (int n = 0; n < vlsnrHeaderLength; n++) {
            z[n] = std::complex<double>(at(index + n) * std::conj(h.m_known[n]));
        }

        dvbs::fft(z);
        int peak = 0;
        double peakPower = 0.0;

        for (int k = 0; k < m_vlsnrSearchFFT; k++)
        {
            if (std::norm(z[k]) > peakPower)
            {
                peakPower = std::norm(z[k]);
                peak = k;
            }
        }

        const double s = power > 0.0 ? std::sqrt(peakPower / power) : 0.0;

        if (s > score)
        {
            // The peak between bins, from its neighbours' magnitudes
            const double a = std::abs(z[(peak + m_vlsnrSearchFFT - 1) % m_vlsnrSearchFFT]);
            const double b = std::abs(z[peak]);
            const double c = std::abs(z[(peak + 1) % m_vlsnrSearchFFT]);
            const double d = a - 2.0 * b + c;
            const double offset = d < 0.0 ? 0.5 * (a - c) / d : 0.0;
            score = s;
            frequency = std::remainder((peak + offset) / m_vlsnrSearchFFT, 1.0);
            set = h.m_set;
        }
    }

    if (score <= m_vlsnrPresentScore) {
        return false;
    }

    header = Header();
    header.m_valid = true;
    header.m_modcod = vlsnrSetModcod(set);
    header.m_type = 1;
    header.m_margin = 1.0;
    header.m_score = score;
    header.m_frequency = frequency;
    return true;
}

double PLDecoder::vlsnrHeaderScore(int64_t index, double frequency, int& set, int& vlIndex, int& plhSet) const
{
    std::complex<double> plh[2] = {0.0, 0.0}, rows[16], edges(0.0, 0.0);
    double power = 0.0;
    std::complex<double> rot(1.0, 0.0);
    const std::complex<double> step = std::polar(1.0, -2.0 * m_pi * frequency);

    for (int n = 0; n < vlsnrHeaderLength; n++)
    {
        const std::complex<double> v = std::complex<double>(at(index + n)) * rot;
        rot *= step;
        power += std::norm(v);

        if (n < headerSymbols)
        {
            plh[0] += v * std::conj(std::complex<double>(m_vlsnrKnown[0][n]));
            plh[1] += v * std::conj(std::complex<double>(m_vlsnrKnown[1][n]));
            continue;
        }

        const int m = n - headerSymbols;
        const std::complex<double> c = v * std::conj(std::complex<double>(m_vlsnrKnown[0][n]));

        if ((m >= 2) && (m < vlsnrHeaderSymbols - 2)) {
            rows[(m - 2) / 56] += c;
        } else {
            edges += c;     // The zeros either end, the same for every index
        }
    }

    double best = 0.0;
    set = 1;
    vlIndex = 0;
    plhSet = std::abs(plh[0]) >= std::abs(plh[1]) ? 1 : 2;

    for (int i = 0; i < vlsnrIndexes; i++)
    {
        const int s = vlsnrModcod(i).m_set;
        std::complex<double> c = plh[s - 1] + edges;

        for (int r = 0; r < 16; r++) {
            c += (double) m_vlsnrSigns[i][r] * rows[r];
        }

        if (std::abs(c) > best)
        {
            best = std::abs(c);
            set = s;
            vlIndex = i;
        }
    }

    // Noise's correlation's standard deviation is sqrt(power) (the signal's included: a little high)
    return power > 0.0 ? best / std::sqrt(power) : 0.0;
}

void PLDecoder::processFrame(int64_t start, const Header& header, int64_t next, const Header& nextHeader)
{
    m_sequence++;
    m_lockedFrames++;
    m_implausible = false;

    if (vlsnrSet(header.m_modcod, header.m_type))
    {
        processVLSNR(start, header, next, nextHeader);
        return;
    }

    m_status.m_vlsnr = -1;

    if (header.m_modcod == dummyModcod)
    {
        if ((m_lockedFrames < m_trustedFrames) && !checkDummy(start, header))
        {
            m_implausible = true;
            return;
        }

        m_status.m_modcod = header.m_modcod;
        m_status.m_type = header.m_type;
        m_status.m_dummyFrames++;
        trackDummy(start, header, nextHeader);
        return;
    }

    Modulation modulation;
    CodeRate rate;

    if (!modcodInfo(header.m_modcod, modulation, rate))
    {
        // An S2X reserved PLS code's (EN 302 307-2 table 17b): its length's known, so the lock's kept, but there's
        // nothing to decode. Perhaps a data frame's PLS code misread: a gap, as for VL-SNR's unclear frames
        m_status.m_modcod = header.m_modcod;
        m_status.m_type = header.m_type;
        m_status.m_reservedFrames++;
        m_gap = true;
        return;
    }

    const FECType fecType = (header.m_type & 2) ? FECType::Short : FECType::Normal;
    const bool pilots = (header.m_type & 1) != 0;
    const int s = slotCount(modulation, fecType);
    const int nbPilots = pilotBlocks(s, pilots);
    const std::vector<uint8_t>& scrambling = m_scrambler.sequence();
    const int length = headerLength(header);
    const int64_t dataStart = start + length;
    const uint8_t *sof = sofBits();

    // The known blocks, in order: the header, the pilot blocks, the next header (or its SOF). Time slicing's header's
    // whose TSN isn't known, its SOF. Each's fields are all set before they're used
    typedef KnownBlock Block;
    std::vector<Block>& blocks = m_knownBlocks;
    blocks.resize(nbPilots + 2);
    blocks[0].m_start = start;

    if (header.m_timeSlicing && (header.m_tsn < 0))
    {
        blocks[0].m_length = sofSymbols;

        for (int n = 0; n < sofSymbols; n++) {
            blocks[0].m_known[n] = m_zero[n] * (sof[n] ? -1.0f : 1.0f);
        }
    }
    else
    {
        blocks[0].m_length = length;
        knownHeader(header, blocks[0].m_known);
    }


    for (int k = 0; k < nbPilots; k++)
    {
        Block& b = blocks[k + 1];
        b.m_start = dataStart + (int64_t) (k + 1) * pilotInterval * slotSymbols + (int64_t) k * pilotSymbols;
        b.m_length = pilotSymbols;

        for (int i = 0; i < pilotSymbols; i++) {
            b.m_known[i] = m_pilotValue * m_quarterTurns[scrambling[(size_t) (b.m_start + i - dataStart)]];
        }
    }

    Block& last = blocks.back();
    last.m_start = next;

    if (nextHeader.m_valid)
    {
        last.m_length = headerLength(nextHeader);
        knownHeader(nextHeader, last.m_known);
    }
    else
    {
        last.m_length = sofSymbols;

        for (int n = 0; n < sofSymbols; n++) {
            last.m_known[n] = m_zero[n] * (sof[n] ? -1.0f : 1.0f);
        }
    }

    // Frequency: Luise and Reggiannini's estimator on the known symbols with the modulation taken off, at the frequency
    // so far
    auto modulationOff = [&](const Block& b, double frequency, std::vector<Cf>& u) {
        PLDecoder::modulationOff(start, b.m_start, b.m_length, b.m_known, frequency, u);
    };

    std::vector<Cf>& u = m_blockSymbols;
    std::complex<double> sum(0.0, 0.0);

    for (const Block& b : blocks)
    {
        modulationOff(b, m_frequency, u);

        for (int lag = 1; lag <= m_lrLags; lag++)
        {
            for (int i = lag; i < b.m_length; i++) {
                sum += std::complex<double>(u[i] * std::conj(u[i - lag]));
            }
        }
    }

    // This frame's symbols are taken at this frequency (the pilots' phases, interpolated, follow what's left). At first,
    // the estimator's; settled, the frequency tracked from frame to frame (the estimator's noise, at low SNR, would have
    // the pilots' phases wrap), unless the estimator says it's well out
    const double coarse = std::arg(sum) / (m_pi * (m_lrLags + 1));
    const bool settled = m_lockedFrames > m_settledFrames;
    double frequency = m_frequency;

    if (!settled || ((std::fabs(coarse) > m_coarseLimit) && (nbPilots > 0)))
    {
        frequency += coarse;
        m_coarseAverage = 0.0;
    }
    else if (nbPilots == 0)
    {
        // Tracked from the headers' phases, frame to frame, and the data's decisions (the estimator, from the headers'
        // 180 symbols alone, is far too noisy near threshold, often over the limit)
    }
    else if (nextHeader.m_valid)
    {
        // Tracked from the pilots' steps, the frequency could have slipped to an alias, a whole number of 1 / 1476 out,
        // where the steps look right but each segment's turned a whole cycle: the estimator's average says so
        // A running mean at first, so an alias the first frames slipped to is found in a few
        const double weight = std::max(0.1, 1.0 / (double) (m_lockedFrames - m_settledFrames));
        m_coarseAverage += weight * (coarse - m_coarseAverage);
        const double cycles = m_coarseAverage * m_pilotPeriod;
        const double aliases = (std::fabs(cycles) > 0.6) && (m_lockedFrames >= m_settledFrames + 3) ? std::round(cycles) : 0.0;

        if (aliases != 0.0)
        {
            frequency += aliases / m_pilotPeriod;
            m_frequency += aliases / m_pilotPeriod;
            m_coarseAverage -= aliases / m_pilotPeriod;
        }
    }

    double refined = frequency;

    // Each block's phasor and gain
    double gainSum = 0.0, known = 0.0;

    for (Block& b : blocks)
    {
        modulationOff(b, frequency, u);
        Cf c(0.0f, 0.0f);

        for (const Cf& v : u) {
            c += v;
        }

        b.m_phasor = c;
        gainSum += std::abs(c);
        known += b.m_length;
    }

    const double gain = std::max(1e-6, gainSum / known);
    auto centre = [start](const Block& b) { return (double) (b.m_start - start) + (b.m_length - 1) / 2.0; };

    // The mean phase step from pilot block to pilot block (all 1476 symbols apart), from the sum of each's phasor times the
    // last's conjugate: no unwrapping, so a noisy block doesn't throw it, and unambiguous within +/- pi (3.4e-4 cycles a
    // symbol). It refines the frequency, and predicts each block's phase for unwrapping
    double step = 0.0;          // Radians a symbol
    bool stepKnown = false;

    if (nbPilots >= 2)
    {
        std::complex<double> sum(0.0, 0.0);

        for (int k = 2; k <= nbPilots; k++) {
            sum += std::complex<double>(blocks[k].m_phasor) * std::conj(std::complex<double>(blocks[k - 1].m_phasor));
        }

        if (std::abs(sum) > 0.0)
        {
            step = std::arg(sum) / (centre(blocks[2]) - centre(blocks[1]));
            stepKnown = true;
            refined += step / (2.0 * m_pi);
        }
    }

    for (size_t k = 0; k < blocks.size(); k++)
    {
        Block& b = blocks[k];
        const double phase = std::arg(b.m_phasor);

        if (k == 0)
        {
            b.m_phase = phase;
        }
        else
        {
            const double predicted = blocks[k - 1].m_phase + step * (centre(b) - centre(blocks[k - 1]));
            b.m_phase = predicted + wrap(phase - predicted);
        }
    }

    // From frames whose next header's there (not, perhaps, after a false lock)
    if (nextHeader.m_valid && (stepKnown || !settled || (nbPilots == 0)))
    {
        m_frequency = !settled ? refined : m_frequency + 0.3 * (refined - m_frequency);
        m_status.m_frequency = m_frequency;
    }

    double noise = 0.0;

    for (const Block& b : blocks)
    {
        modulationOff(b, frequency, u);
        const Cf rot = std::polar(1.0f / (float) gain, (float) -b.m_phase);

        for (const Cf& v : u) {
            noise += std::norm(v * rot - Cf(1.0f, 0.0f));
        }
    }

    noise = std::max(1e-6, noise / known); // Total, relative to the symbols' power
    const double measuredNoise = noise;

    if (nbPilots > 0)
    {
        // The known symbols' error includes the phase's walk within their blocks (each block's phase is their mean):
        // for a block of L, q L / 6 on average. With heavy phase noise, as much as the noise itself at 20 dB
        double walk = 0.0;

        for (const Block& b : blocks) {
            walk += m_phaseNoise * b.m_length * b.m_length / 6.0;
        }

        noise = std::max(0.25 * noise, noise - walk / known);
    }

    const double esn0 = -10.0 * std::log10(noise);

    // A new lock's frames with known symbols too noisy for any MODCOD: false (its next header passed the checks by
    // chance), dropped at once rather than after its next headers are missed, and not decoded
    if ((m_lockedFrames < m_trustedFrames) && (esn0 < m_minLockEsN0))
    {
        m_implausible = true;
        return;
    }

    m_status.m_modcod = header.m_modcod;
    m_status.m_type = header.m_type;

    // The phase noise, from a frame whose next header's there (a missing one's block is noise, its phase random)
    if ((nbPilots > 0) && nextHeader.m_valid)
    {
        m_blockCentres.resize(blocks.size());
        m_blockPhases.resize(blocks.size());
        m_blockLengths.resize(blocks.size());

        for (size_t k = 0; k < blocks.size(); k++)
        {
            m_blockCentres[k] = centre(blocks[k]);
            m_blockPhases[k] = blocks[k].m_phase;
            m_blockLengths[k] = blocks[k].m_length;
        }

        measurePhaseNoise(m_blockCentres.data(), m_blockPhases.data(), m_blockLengths.data(), (int) blocks.size(), step,
            measuredNoise);
    }

    m_esn0Average = m_status.m_frames == 0 ? esn0 : 0.9 * m_esn0Average + 0.1 * esn0;
    m_status.m_esn0 = m_esn0Average;

    // The data: each symbol's phase interpolated between the blocks either side (or decision directed without pilots),
    // normalised, descrambled
    XFECFrame& frame = m_frame;
    frame.m_modcod = header.m_modcod;
    frame.m_type = header.m_type;
    frame.m_modulation = modulation;
    frame.m_rate = rate;
    frame.m_fecType = fecType;
    frame.m_sequence = m_sequence;
    frame.m_gap = m_gap;
    m_gap = false;
    frame.m_suspect = m_suspect;
    frame.m_timeSlicing = header.m_timeSlicing;
    frame.m_tsn = header.m_tsn;
    frame.m_vlsnr = -1;
    frame.m_cells.resize((size_t) s * slotSymbols);
    const std::vector<Cf>& points = m_points[header.m_modcod];
    const float invGain = (float) (1.0 / gain);
    const double stepAngle = -2.0 * m_pi * frequency;
    Cf* cells = frame.m_cells.data();

    if (nbPilots > 0)
    {
        const bool smooth = m_phaseNoise > m_smoothedPhaseNoise * m_smoothedPhaseNoise;

        for (int slot = 0; slot < s; slot++)
        {
            const int segment = slot / pilotInterval;
            const Block& a = blocks[std::min(segment, (int) blocks.size() - 2)];
            const Block& b = blocks[std::min(segment + 1, (int) blocks.size() - 1)];
            const double ca = (double) (a.m_start - start) + (a.m_length - 1) / 2.0;
            const double cb = (double) (b.m_start - start) + (b.m_length - 1) / 2.0;
            const double slope = (b.m_phase - a.m_phase) / (cb - ca);
            const int64_t slotStart = dataStart + (int64_t) slot * slotSymbols + (int64_t) segment * pilotSymbols;
            const double x0 = (double) (slotStart - start);
            // The carrier's and interpolated phase at the slot's start, then a phasor step a symbol
            const double angle0 = wrap(stepAngle * x0 - (a.m_phase + slope * (x0 - ca)));
            const double step = stepAngle - slope;
            const Cf rot = std::polar(invGain, (float) angle0);
            const Cf stepRot((float) std::cos(step), (float) std::sin(step));
            turnValues(&at(slotStart), cells + slot * slotSymbols, slotSymbols, rot, stepRot,
                &scrambling[(size_t) (slotStart - dataStart)]);
        }

        if (!smooth)
        {
            refineSlotPhases(cells, s, header.m_modcod, modulation);
        }
        else
        {
            // The frame for the smoother: the known blocks' symbols (their modulation off) and the data, at the
            // carrier's and interpolated phase, in order
            m_trackValue.clear();
            m_trackKnown.clear();
            int cell = 0;

            for (size_t k = 0; k < blocks.size(); k++)
            {
                const Block& block = blocks[k];
                modulationOff(block, frequency, u);
                const double c = centre(block);
                // The interpolation's line before the block's centre, then after (beyond the first and last, theirs)
                const Block& a0 = blocks[k > 0 ? k - 1 : 0];
                const Block& a1 = blocks[k > 0 ? k : 1];
                const Block& b0 = blocks[k + 1 < blocks.size() ? k : blocks.size() - 2];
                const Block& b1 = blocks[k + 1 < blocks.size() ? k + 1 : blocks.size() - 1];
                const double before = (a1.m_phase - a0.m_phase) / (centre(a1) - centre(a0));
                const double after = (b1.m_phase - b0.m_phase) / (centre(b1) - centre(b0));

                for (int i = 0; i < block.m_length; i++)
                {
                    const double x = (double) (block.m_start + i - start);
                    const double ref = block.m_phase + (x < c ? before : after) * (x - c);
                    m_trackValue.push_back(u[i] * std::polar(invGain, (float) -ref));
                    m_trackKnown.push_back(1);
                }

                if (k + 1 < blocks.size())
                {
                    const int segmentCells = std::min(pilotInterval, s - (int) k * pilotInterval) * slotSymbols;

                    for (int i = 0; i < segmentCells; i++, cell++)
                    {
                        m_trackValue.push_back(frame.m_cells[cell]);
                        m_trackKnown.push_back(0);
                    }
                }
            }

            smoothPhase(cells, header.m_modcod, noise, m_phaseNoise);
        }
    }
    else
    {
        // Decision directed: a second order loop from the header's phase, each data symbol's phase error from its
        // nearest point (weighted by the point's power: an APSK inner ring's angle is noisier, the more so the further
        // in it is)
        const Cf *y = &at(dataStart);
        const double scale = m_dataPower[header.m_modcod] > 0.0 ? std::sqrt(m_dataPower[header.m_modcod]) : 1.0;
        const float invScale = (float) (1.0 / scale);
        const NearestTable& nearestTable = nearestPoints(header.m_modcod);
        const Block& last = blocks.back();
        const double span = centre(last) - centre(blocks[0]);
        const int nbData = s * slotSymbols;

        // A pass through the frame's data from the header's phase, turning at a rate (radians a symbol, on top of the
        // frequency): the cells taken off, the loop's phase and rate at the end, and the data's error power to the
        // points decided. The loop's updated a block of symbols at a time, from their summed errors: a symbol at a
        // time, each decision waiting on the last's correction, the chain was its time, at tens of MS/s; and the
        // block's far shorter than the loop's time constant (its gain a block well under 1, at the widest). The
        // carrier's turn is a phasor stepped a symbol at a time, recomputed exactly every block. Given up once the
        // error power's over limit (it only grows): the pass is then only that
        struct Pass { double m_endPhase; double m_endRate; double m_errorPower; };
        auto loopPassWith = [&](auto decide, double bandwidth, double rate, Cf *out, float limit) {
            const double zeta = 0.707;
            const double theta = bandwidth / (zeta + 1.0 / (4.0 * zeta));
            const double d = 1.0 + 2.0 * zeta * theta + theta * theta;
            const double kp = 4.0 * zeta * theta / d, ki = 4.0 * theta * theta / d;
            double phase = blocks[0].m_phase + rate * (length - centre(blocks[0]));
            double loopFrequency = rate;
            float errorPower = 0.0f;

            for (int i = 0; i < nbData; i += m_ddBlockSymbols)
            {
                const int n = std::min(m_ddBlockSymbols, nbData - i);
                // The phase at each symbol of the block: the loop's now, turning at the carrier's and the loop's rate
                Cf rot = std::polar(invGain, (float) wrap(stepAngle * (double) (length + i) - phase));
                const Cf step = std::polar(1.0f, (float) (stepAngle - loopFrequency));
                float error = 0.0f;

                for (int j = 0; j < n; j++)
                {
                    const Cf v = y[i + j] * rot * std::conj(m_quarterTurns[scrambling[(size_t) (i + j)]]);
                    out[i + j] = v;
                    const Cf vs = v * invScale;
                    const Cf p = decide(vs);
                    error += (vs * std::conj(p)).imag();
                    errorPower += std::norm(vs - p);
                    rot *= step;
                }

                // As each symbol's would have, but for the block's corrections to each other
                loopFrequency += ki * error;
                phase += n * loopFrequency + kp * error;

                if (errorPower > limit) {
                    break;
                }
            }

            Pass pass;
            pass.m_endPhase = phase;
            pass.m_endRate = loopFrequency;
            pass.m_errorPower = errorPower;
            return pass;
        };

        // The nearest point: PSK's directly (at the serial loop's tens of MS/s, the table's lookup is most of its time),
        // the others' from the table
        auto loopPass = [&](double bandwidth, double rate, Cf *out, float limit) {
            if (modulation == Modulation::QPSK) {
                return loopPassWith([](const Cf& v) { return nearestQPSK(v); }, bandwidth, rate, out, limit);
            }

            if (modulation == Modulation::PSK8) {
                return loopPassWith([](const Cf& v) { return nearestPSK8(v); }, bandwidth, rate, out, limit);
            }

            return loopPassWith([&](const Cf& v) { return points[nearestTable.nearest(v, points)]; }, bandwidth, rate,
                out, limit);
        };

        // The loop's bandwidth: as the Kalman filter's gain for a random walk phase (variance q a symbol) measured with
        // noise (the decisions' phase error's, r a symbol, about half the noise's power), sqrt(q / r), less for the
        // decisions' errors near threshold. In white noise, narrow (the frequency's known well); with phase noise (an
        // LNB's, at a low symbol rate), wide enough to follow it
        const double r = std::max(1e-3, noise / 2.0);
        const double bandwidth = std::min(m_ddMaxBandwidth, std::max(m_ddMinBandwidth,
            m_ddBandwidthGain * std::sqrt(std::max(0.0, m_ddPhaseNoise) / r)));

        // The headers give the frequency only to within a turn a frame. At lock it's known only to a few turns a frame
        // (from the headers' search, and the first frame's Luise and Reggiannini estimate, from its headers alone), and
        // near threshold the loop can't pull in a turn's ramp (decisions too noisy for its detector to hold against
        // it): it would settle a whole number of turns a frame out, the headers agreeing and every frame lost. So for a
        // new lock's first frames, the loop's run from each rate the headers allow (their phase step, plus whole turns)
        // about the frequency so far, and the one whose data's nearest the points decided is taken: the right rate's
        // followed (a drifting carrier's phase curves through the frame, as no fixed rate can), a wrong one's ridden
        // over the points, the data turned from them. (Neither the loop's corrections nor its end tell: from a rate a
        // turn out, it rides over the points, correcting nothing.) The loop then starts at it, and later frames
        // follow the frequency
        double rate = 0.0;      // Radians a symbol, the loop's start, on top of the frequency
        bool aliasChosen = false;

        if (nextHeader.m_valid && (m_lockedFrames <= m_aliasSearchFrames))
        {
            const double fraction = wrap(last.m_phase - blocks[0].m_phase) / span;
            double bestError = 1e30;
            int bestK = 0;
            // At first, as far as the headers' search could be out (half its step, and some); then, the second frame
            // confirming, a few
            const int aliases = m_lockedFrames <= 1 ? (int) std::ceil(m_aliasSearchUncertainty * span) : m_aliasConfirmTurns;

            // Outwards from the headers' rate (the right one's usually near it), each pass given up once its error's
            // over the best's so far: the one chosen is the same as from whole passes in order (the least error, the
            // first of any equal), for a fraction of the time (a long frame's search was up to 123 passes)
            for (int j = 0; j <= 2 * aliases; j++)
            {
                const int k = (j & 1) ? -(j + 1) / 2 : j / 2;
                const double candidate = fraction + 2.0 * m_pi * k / span;
                const double error = loopPass(bandwidth, candidate, cells, (float) bestError).m_errorPower;

                if ((error < bestError) || ((error == bestError) && (k < bestK)))
                {
                    bestError = error;
                    bestK = k;
                    rate = candidate;
                }
            }

            aliasChosen = true;
            m_ddEndError = 0.0;
            m_ddCoarseSum = 0.0;
            m_ddCoarseSquares = 0.0;
            m_ddCoarseFrames = 0;
        }

        const Pass pass = loopPass(bandwidth, rate, cells, std::numeric_limits<float>::infinity());

        if (nextHeader.m_valid)
        {
            // The frequency for the next frame, from the phase's advance header to header: the loop's, put right by
            // the next header's phase (which is absolute, so a slip doesn't count), when the loop ends near it (it
            // followed the carrier, even drifting more than half a turn a frame); else within half a turn of the rate
            // the frame started at (the loop, wandering far, would choose the turns wrong). Far less noisy than the
            // loop's rate at the end (an error each frame the next would have to pull in again). The alias chosen is
            // taken whole; then the frequency and its drift a frame are tracked (as a drifting LNB's, at a low symbol
            // rate, can be turns a frame)
            const double loopEnd = pass.m_endPhase + pass.m_endRate * (centre(last) - (length + nbData - 1));
            const double endError = wrap(last.m_phase - loopEnd);
            const double advance = std::fabs(endError) < m_ddTrustedEnd ? loopEnd + endError - blocks[0].m_phase
                : rate * span + wrap(last.m_phase - blocks[0].m_phase - rate * span);
            const double frequencyError = advance / span / (2.0 * m_pi);

            if (aliasChosen)
            {
                m_frequency += frequencyError;
                m_ddDrift = 0.0;
            }
            else if (settled)
            {
                m_ddDrift += m_ddDriftGain * frequencyError;
                m_frequency += m_ddFrequencyGain * frequencyError + m_ddDrift;
            }

            m_status.m_frequency = m_frequency;

            // The phase noise, from the headers' phase steps: each frame's (its residual from the frequency), less what
            // the update after the last took (a fraction of its), is the phase's random walk across the frame (variance
            // 2 q span, as successive frames' are differenced) and three headers' phase errors (6 times each's)
            const double residual = wrap(last.m_phase - blocks[0].m_phase - rate * span);

            if (m_ddResidualValid && !aliasChosen)
            {
                const double x = wrap(residual - (1.0 - m_ddFrequencyGain) * m_ddResidual);
                const double headerNoise = noise / (2.0 * length);
                m_ddPhaseNoise += m_ddPhaseNoiseWeight * ((x * x - 6.0 * headerNoise) / (2.0 * span) - m_ddPhaseNoise);
                m_status.m_phaseNoise = std::sqrt(std::max(0.0, m_ddPhaseNoise));
            }

            m_ddResidual = residual;
            m_ddResidualValid = settled;

            // A short frame's data, near threshold, may say too little of its phase for the search to choose the
            // turns (16APSK 8/15 at 8 dB: about as near its points turned or not), and the loop can then sit in an
            // alias, its end agreeing with the next header's. The headers' own Luise and Reggiannini estimate (no
            // decisions: noisy, but a short frame's turn is large) says so, averaged over frames: when it's most of a
            // turn out, and well over its standard error, the frequency's moved the whole turns. (Over a long frame's,
            // the average is far too noisy to say: there the search's enough)
            if (settled && !aliasChosen)
            {
                m_ddCoarseSum += coarse;
                m_ddCoarseSquares += coarse * coarse;
                m_ddCoarseFrames++;

                if (m_ddCoarseFrames >= m_coarseAliasFrames)
                {
                    const double mean = m_ddCoarseSum / m_ddCoarseFrames;
                    const double variance = std::max(0.0, m_ddCoarseSquares / m_ddCoarseFrames - mean * mean);
                    const double turns = mean * span;
                    const double error = std::sqrt(variance / m_ddCoarseFrames) * span;

                    if ((std::fabs(turns) > m_coarseAliasTurns) && (std::fabs(turns) > m_coarseAliasSigmas * error))
                    {
                        m_frequency += std::round(turns) / span;
                        m_status.m_frequency = m_frequency;
                        m_ddDrift = 0.0;
                        m_ddCoarseSum = 0.0;
                        m_ddCoarseSquares = 0.0;
                        m_ddCoarseFrames = 0;
                    }
                }
            }

            // How far the loop's end is from the next header's phase: large, frame after frame, the frequency's a
            // whole number of turns a frame out (an alias the search missed, or a jump), and the lock's dropped, to be
            // found again
            m_ddEndError += m_aliasErrorWeight * (std::fabs(endError) - m_ddEndError);

            if ((m_lockedFrames > m_aliasSearchFrames + m_aliasErrorFrames) && (m_ddEndError > m_aliasLostError)) {
                m_misses = m_maxMisses;   // As the headers lost, the frequency kept
            }
        }
        else
        {
            m_ddResidualValid = false;
        }
    }

    // APSK's scale: the data's power less the noise's, relative to the pilots' (1 at unit energy, less with the outer
    // ring at the pilots' amplitude)
    double scaleCorrection = 1.0;

    if ((modulation != Modulation::QPSK) && (modulation != Modulation::PSK8))
    {
        double power = 0.0;

        for (int i = 0; i < s * slotSymbols; i++) {
            power += std::norm(cells[i]);
        }

        const double signal = std::max(0.1, power / (s * slotSymbols) - noise);
        double& average = m_dataPower[header.m_modcod];
        average = average > 0.0 ? 0.8 * average + 0.2 * signal : signal;
        scaleCorrection = 1.0 / std::sqrt(average);

        const float k = (float) scaleCorrection;

        for (int i = 0; i < s * slotSymbols; i++) {
            cells[i] *= k;
        }
    }

    // MER of the data, from every 8th symbol's nearest point
    double error = 0.0;
    int count = 0;
    const NearestTable& nearestTable = nearestPoints(header.m_modcod);

    for (int i = 0; i < s * slotSymbols; i += 8, count++) {
        error += std::norm(cells[i] - points[nearestTable.nearest(cells[i], points)]);
    }

    const double dataError = error / std::max(1, count);
    const double mer = -10.0 * std::log10(std::max(1e-10, dataError));

    // The noise for the LLRs: the known symbols', unless the data's error is more. The known symbols have none of the
    // phase noise between pilot blocks, which the data has, so with phase noise (as at a low symbol rate from an LNB)
    // and a high SNR, LLRs from theirs alone are far too confident in the data's turned cells, and LDPC fails more the
    // higher the SNR. Near threshold the data's error is less (decisions are wrong), so the known symbols' is used
    frame.m_noise = (float) (std::max(noise * scaleCorrection * scaleCorrection, dataError) / 2.0);
    m_status.m_mer = m_status.m_frames == 0 ? mer : 0.9 * m_status.m_mer + 0.1 * mer;
    m_status.m_frames++;
    m_status.m_modcodsSeen[header.m_modcod >> 6] |= 1ull << (header.m_modcod & 63);
    m_status.m_tsn = header.m_tsn;
    m_status.m_vlsnr = -1;

    const int display = std::min((int) frame.m_cells.size(), 4096);
    const int stride = std::max(1, (int) frame.m_cells.size() / display);
    m_display.clear();

    for (int i = 0; i < (int) frame.m_cells.size() && (int) m_display.size() < display; i += stride) {
        m_display.push_back(frame.m_cells[i]);
    }

    // A time slice's frame without its header can't be told from another's: not decoded (as if lost)
    if (frame.m_timeSlicing && (frame.m_tsn < 0))
    {
        m_gap = true;
        return;
    }

    if (m_callback) {
        m_callback(frame);
    }
}

} // namespace dvbs2
