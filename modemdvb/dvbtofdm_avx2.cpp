///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

// DVB-T OFDM demodulation's kernels, with AVX2 and FMA. Compiled with them enabled, so only used if the CPU has
// them. The convolutions work 32 floats (16 carriers) a block, in four or eight accumulators, enough to keep the
// FMAs busy. No library templates (std::complex, std::min) are used: code instantiated here, compiled for AVX2,
// could be shared with other files that run on any CPU

#include <cmath>

#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "dvbtofdmkernel.h"

namespace dvbt {

int convolveAVX2(const float *x, const float *wc, int half, bool symmetric, int f, int end, float *y)
{
    for (; f + 32 <= end; f += 32)
    {
        const float *xc = x + f;
        const __m256 w0 = _mm256_set1_ps(wc[0]);
        __m256 a0 = _mm256_mul_ps(w0, _mm256_loadu_ps(xc));
        __m256 a1 = _mm256_mul_ps(w0, _mm256_loadu_ps(xc + 8));
        __m256 a2 = _mm256_mul_ps(w0, _mm256_loadu_ps(xc + 16));
        __m256 a3 = _mm256_mul_ps(w0, _mm256_loadu_ps(xc + 24));

        if (symmetric)
        {
            for (int i = 1; i <= half; i++)
            {
                const __m256 w = _mm256_broadcast_ss(&wc[i]);
                const float *lo = xc - 2 * i;
                const float *hi = xc + 2 * i;
                a0 = _mm256_fmadd_ps(w, _mm256_add_ps(_mm256_loadu_ps(lo), _mm256_loadu_ps(hi)), a0);
                a1 = _mm256_fmadd_ps(w, _mm256_add_ps(_mm256_loadu_ps(lo + 8), _mm256_loadu_ps(hi + 8)), a1);
                a2 = _mm256_fmadd_ps(w, _mm256_add_ps(_mm256_loadu_ps(lo + 16), _mm256_loadu_ps(hi + 16)), a2);
                a3 = _mm256_fmadd_ps(w, _mm256_add_ps(_mm256_loadu_ps(lo + 24), _mm256_loadu_ps(hi + 24)), a3);
            }
        }
        else
        {
            for (int i = -half; i <= half; i++)
            {
                if (i == 0) {
                    continue;
                }

                const __m256 w = _mm256_broadcast_ss(&wc[i]);
                const float *xi = xc + 2 * i;
                a0 = _mm256_fmadd_ps(w, _mm256_loadu_ps(xi), a0);
                a1 = _mm256_fmadd_ps(w, _mm256_loadu_ps(xi + 8), a1);
                a2 = _mm256_fmadd_ps(w, _mm256_loadu_ps(xi + 16), a2);
                a3 = _mm256_fmadd_ps(w, _mm256_loadu_ps(xi + 24), a3);
            }
        }

        _mm256_storeu_ps(y + f, a0);
        _mm256_storeu_ps(y + f + 8, a1);
        _mm256_storeu_ps(y + f + 16, a2);
        _mm256_storeu_ps(y + f + 24, a3);
    }

    return f;
}

int convolve2AVX2(const float *x, const float *wc1, const float *wc2, int half, int f, int end, float *y1, float *y2)
{
    for (; f + 32 <= end; f += 32)
    {
        const float *xc = x + f;
        __m256 a0 = _mm256_setzero_ps(), a1 = a0, a2 = a0, a3 = a0;
        __m256 b0 = a0, b1 = a0, b2 = a0, b3 = a0;

        for (int i = -half; i <= half; i++)
        {
            const __m256 w1 = _mm256_broadcast_ss(&wc1[i]);
            const __m256 w2 = _mm256_broadcast_ss(&wc2[i]);
            const float *xi = xc + 2 * i;
            const __m256 x0 = _mm256_loadu_ps(xi);
            const __m256 x1 = _mm256_loadu_ps(xi + 8);
            const __m256 x2 = _mm256_loadu_ps(xi + 16);
            const __m256 x3 = _mm256_loadu_ps(xi + 24);
            a0 = _mm256_fmadd_ps(w1, x0, a0);
            a1 = _mm256_fmadd_ps(w1, x1, a1);
            a2 = _mm256_fmadd_ps(w1, x2, a2);
            a3 = _mm256_fmadd_ps(w1, x3, a3);
            b0 = _mm256_fmadd_ps(w2, x0, b0);
            b1 = _mm256_fmadd_ps(w2, x1, b1);
            b2 = _mm256_fmadd_ps(w2, x2, b2);
            b3 = _mm256_fmadd_ps(w2, x3, b3);
        }

        _mm256_storeu_ps(y1 + f, a0);
        _mm256_storeu_ps(y1 + f + 8, a1);
        _mm256_storeu_ps(y1 + f + 16, a2);
        _mm256_storeu_ps(y1 + f + 24, a3);
        _mm256_storeu_ps(y2 + f, b0);
        _mm256_storeu_ps(y2 + f + 8, b1);
        _mm256_storeu_ps(y2 + f + 16, b2);
        _mm256_storeu_ps(y2 + f + 24, b3);
    }

    return f;
}

// Four complex products at once
static inline __m256 complexMultiply(__m256 a, __m256 b)
{
    const __m256 swapped = _mm256_permute_ps(a, 0xb1);  // Imaginary, real
    return _mm256_fmaddsub_ps(a, _mm256_moveldup_ps(b), _mm256_mul_ps(swapped, _mm256_movehdup_ps(b)));
}

// One, (ar + j ai)(br + j bi), as std::complex<float> would
static inline void complexMultiply(float ar, float ai, float br, float bi, float& yr, float& yi)
{
    yr = ar * br - ai * bi;
    yi = ar * bi + ai * br;
}

// Eight carriers (interleaved complex) are loaded as carriers 0, 1 | 4, 5 and 2, 3 | 6, 7, so a horizontal add of
// the two gives a value per carrier in order, without moving data across the 128-bit halves, which is slow on
// efficiency cores
static inline __m256 load2(const float *lo, const float *hi)
{
    return _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(lo)), _mm_loadu_ps(hi), 1);
}

static inline void store2(float *lo, float *hi, __m256 v)
{
    _mm_storeu_ps(lo, _mm256_castps256_ps128(v));
    _mm_storeu_ps(hi, _mm256_extractf128_ps(v, 1));
}

static inline __m256 select(__m256 mask, __m256 a, __m256 b) // a where mask is set, else b
{
    return _mm256_or_ps(_mm256_and_ps(mask, a), _mm256_andnot_ps(mask, b));
}

void rotateAVX2(const float *x, float *y, int n, const float *table, double phase, double blockStepRe,
    double blockStepIm)
{
    static const int block = 64;
    double baseRe = std::cos(phase), baseIm = std::sin(phase); // As std::polar(1.0, phase)

    for (int b = 0; b < n; b += block)
    {
        const int m = n - b < block ? n - b : block;
        const float br = (float) baseRe, bi = (float) baseIm;
        const __m256 bv = _mm256_setr_ps(br, bi, br, bi, br, bi, br, bi);
        int i = 0;

        for (; i + 4 <= m; i += 4)
        {
            const __m256 phasors = complexMultiply(_mm256_loadu_ps(table + 2 * i), bv);
            _mm256_storeu_ps(y + 2 * (b + i), complexMultiply(_mm256_loadu_ps(x + 2 * (b + i)), phasors));
        }

        for (; i < m; i++)
        {
            float pr, pi;
            complexMultiply(table[2 * i], table[2 * i + 1], br, bi, pr, pi);
            complexMultiply(x[2 * (b + i)], x[2 * (b + i) + 1], pr, pi, y[2 * (b + i)], y[2 * (b + i) + 1]);
        }

        // base *= blockStep, as std::complex<double>
        const double re = baseRe * blockStepRe - baseIm * blockStepIm;
        baseIm = baseRe * blockStepIm + baseIm * blockStepRe;
        baseRe = re;
    }
}

void equaliseAVX2(const float *y, const float *h, float derotateRe, float derotateIm, int n, float *e, float *power)
{
    const __m256 dv = _mm256_setr_ps(derotateRe, derotateIm, derotateRe, derotateIm, derotateRe, derotateIm,
        derotateRe, derotateIm);
    const __m256 conjugate = _mm256_castsi256_ps(_mm256_setr_epi32(0, (int) 0x80000000, 0, (int) 0x80000000, 0,
        (int) 0x80000000, 0, (int) 0x80000000));
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 zero = _mm256_setzero_ps();
    int i = 0;

    for (; i + 8 <= n; i += 8)
    {
        const float *yi = y + 2 * i;
        const float *hi = h + 2 * i;
        const __m256 h0 = load2(hi, hi + 8);        // Carriers i, i+1 | i+4, i+5
        const __m256 h1 = load2(hi + 4, hi + 12);   // i+2, i+3 | i+6, i+7
        const __m256 v0 = complexMultiply(complexMultiply(load2(yi, yi + 8), _mm256_xor_ps(h0, conjugate)), dv);
        const __m256 v1 = complexMultiply(complexMultiply(load2(yi + 4, yi + 12), _mm256_xor_ps(h1, conjugate)), dv);
        // |h|^2 in order. Duplicating each, low then high, gives the layout of v0 and v1
        const __m256 p = _mm256_hadd_ps(_mm256_mul_ps(h0, h0), _mm256_mul_ps(h1, h1));
        const __m256 inverse = _mm256_and_ps(_mm256_div_ps(one, p), _mm256_cmp_ps(p, zero, _CMP_GT_OQ));
        float *ei = e + 2 * i;
        store2(ei, ei + 8, _mm256_mul_ps(v0, _mm256_unpacklo_ps(inverse, inverse)));
        store2(ei + 4, ei + 12, _mm256_mul_ps(v1, _mm256_unpackhi_ps(inverse, inverse)));
        _mm256_storeu_ps(power + i, p);
    }

    for (; i < n; i++)
    {
        const float p = h[2 * i] * h[2 * i] + h[2 * i + 1] * h[2 * i + 1];
        float ur, ui, vr, vi;
        complexMultiply(y[2 * i], y[2 * i + 1], h[2 * i], -h[2 * i + 1], ur, ui);
        complexMultiply(ur, ui, derotateRe, derotateIm, vr, vi);
        const float inverse = p > 0.0f ? 1.0f / p : 0.0f;
        e[2 * i] = vr * inverse;
        e[2 * i + 1] = vi * inverse;
        power[i] = p;
    }
}

int updateNoiseAVX2(const float *e, const float *power, const float *mask, int n, float scale, float maxPoint,
    float invScale2, float alpha, float *noise, float& total, float& count)
{
    const __m256 sv = _mm256_set1_ps(scale);
    const __m256 maxv = _mm256_set1_ps(maxPoint);
    const __m256 minv = _mm256_set1_ps(-maxPoint);
    const __m256 half = _mm256_set1_ps(0.5f);
    const __m256 four = _mm256_set1_ps(4.0f);
    const __m256i fouri = _mm256_set1_epi32(4);
    const __m256 two = _mm256_set1_ps(2.0f);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 zero = _mm256_setzero_ps();
    const __m256 av = _mm256_set1_ps(alpha);
    const __m256 is2 = _mm256_set1_ps(invScale2);
    __m256 totals = zero, counts = zero;

    // The nearest point, 2 floor(x / 2) + 1, clamped. Adding 4 makes it positive, so truncation floors
    auto squaredError = [&](__m256 x) {
        x = _mm256_mul_ps(x, sv);
        const __m256 c = _mm256_max_ps(minv, _mm256_min_ps(maxv, x));
        const __m256i f = _mm256_sub_epi32(_mm256_cvttps_epi32(_mm256_fmadd_ps(c, half, four)), fouri);
        const __m256 err = _mm256_sub_ps(x, _mm256_fmadd_ps(two, _mm256_cvtepi32_ps(f), one));
        return _mm256_mul_ps(err, err);
    };

    int k = 0;

    for (; k + 8 <= n; k += 8)
    {
        const float *ek = e + 2 * k;
        const __m256 a = squaredError(load2(ek, ek + 8));
        const __m256 b = squaredError(load2(ek + 4, ek + 12));
        const __m256 measured = _mm256_mul_ps(_mm256_mul_ps(_mm256_hadd_ps(a, b), is2), _mm256_loadu_ps(power + k));
        const __m256 old = _mm256_loadu_ps(noise + k);
        const __m256 averaged = _mm256_fmadd_ps(av, _mm256_sub_ps(measured, old), old);
        const __m256 updated = select(_mm256_cmp_ps(old, zero, _CMP_LT_OQ), measured, averaged);
        const __m256 result = select(_mm256_cmp_ps(_mm256_loadu_ps(mask + k), zero, _CMP_GT_OQ), updated, old);
        _mm256_storeu_ps(noise + k, result);
        totals = _mm256_add_ps(totals, _mm256_max_ps(result, zero));
        counts = _mm256_add_ps(counts, _mm256_and_ps(_mm256_cmp_ps(result, zero, _CMP_GE_OQ), one));
    }

    alignas(32) float t[8], c[8];
    _mm256_store_ps(t, totals);
    _mm256_store_ps(c, counts);

    for (int i = 0; i < 8; i++)
    {
        total += t[i];
        count += c[i];
    }

    return k;
}

int smoothNoiseAVX2(const float *noise, int n, float floor, float *smoothed, float *inverse)
{
    const __m256 floorv = _mm256_set1_ps(floor);
    const __m256 zero = _mm256_setzero_ps();
    const __m256 one = _mm256_set1_ps(1.0f);
    int j = 0;

    for (; j + 8 <= n; j += 8)
    {
        __m256 s = zero, c = zero;

        for (int d = -2; d <= 2; d++)
        {
            const __m256 x = _mm256_loadu_ps(noise + j + d);
            s = _mm256_add_ps(s, _mm256_max_ps(x, zero));
            c = _mm256_add_ps(c, _mm256_and_ps(_mm256_cmp_ps(x, zero, _CMP_GE_OQ), one));
        }

        // None measured gives the floor
        const __m256 sm = _mm256_max_ps(floorv, _mm256_div_ps(s, _mm256_max_ps(c, one)));
        _mm256_storeu_ps(smoothed + j, sm);
        _mm256_storeu_ps(inverse + j, _mm256_div_ps(one, sm));
    }

    return j;
}

void windowAVX2(const float *x, const float *window, int n, float *out)
{
    int j = 0;

    for (; j + 4 <= n; j += 4)
    {
        const __m128 w = _mm_loadu_ps(window + j); // Each duplicated, for the real and imaginary parts
        const __m256 wv = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_unpacklo_ps(w, w)), _mm_unpackhi_ps(w, w), 1);
        _mm256_storeu_ps(out + 2 * j, _mm256_mul_ps(_mm256_loadu_ps(x + 2 * j), wv));
    }

    for (; j < n; j++)
    {
        out[2 * j] = x[2 * j] * window[j];
        out[2 * j + 1] = x[2 * j + 1] * window[j];
    }
}

float powerAVX2(const float *x, int n, float *power)
{
    __m256 peaks = _mm256_setzero_ps();
    int b = 0;

    for (; b + 8 <= n; b += 8)
    {
        const float *xb = x + 2 * b;
        const __m256 a = load2(xb, xb + 8);
        const __m256 c = load2(xb + 4, xb + 12);
        const __m256 p = _mm256_hadd_ps(_mm256_mul_ps(a, a), _mm256_mul_ps(c, c));
        _mm256_storeu_ps(power + b, p);
        peaks = _mm256_max_ps(peaks, p);
    }

    alignas(32) float m[8];
    _mm256_store_ps(m, peaks);
    float peak = 0.0f;

    for (int i = 0; i < 8; i++) {
        peak = m[i] > peak ? m[i] : peak;
    }

    for (; b < n; b++)
    {
        power[b] = x[2 * b] * x[2 * b] + x[2 * b + 1] * x[2 * b + 1];
        peak = power[b] > peak ? power[b] : peak;
    }

    return peak;
}

static inline int lowestBit(unsigned int x)
{
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, x);
    return (int) index;
#else
    return __builtin_ctz(x);
#endif
}

int findAboveAVX2(const float *values, int n, float threshold, int *indices)
{
    const __m256 tv = _mm256_set1_ps(threshold);
    int count = 0;
    int b = 0;

    for (; b + 8 <= n; b += 8)
    {
        unsigned int mask = (unsigned int) _mm256_movemask_ps(_mm256_cmp_ps(_mm256_loadu_ps(values + b), tv, _CMP_GE_OQ));

        while (mask)
        {
            indices[count++] = b + lowestBit(mask);
            mask &= mask - 1;
        }
    }

    for (; b < n; b++)
    {
        if (values[b] >= threshold) {
            indices[count++] = b;
        }
    }

    return count;
}

} // namespace dvbt
