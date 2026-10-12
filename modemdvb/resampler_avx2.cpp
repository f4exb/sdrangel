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

// The resampler's filter, with AVX2 and FMA. Compiled with them enabled, so only used if the CPU has them

#include <immintrin.h>

#include "resamplerkernel.h"

namespace dvbt {

// acc holds re, im of 4 taps: their sums, re and im in the low two floats
static inline __m128 sumTaps(__m256 acc)
{
    __m128 sum = _mm_add_ps(_mm256_castps256_ps128(acc), _mm256_extractf128_ps(acc, 1));
    return _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
}

// In an unnamed namespace, as is resampleRun<DotAVX2>, so neither can be confused with another file's
namespace {

struct DotAVX2
{
    // 4 complex taps at a time
    static void dot(const float *x, const float *h, int taps, float *out)
    {
        __m256 acc = _mm256_setzero_ps();

        for (int n = 0; n < 2 * taps; n += 8) {
            acc = _mm256_fmadd_ps(_mm256_loadu_ps(x + n), _mm256_loadu_ps(h + n), acc);
        }

        _mm_storel_pi(reinterpret_cast<__m64 *>(out), sumTaps(acc));
    }
};

} // namespace

// The first input sample and the filter phase of the output at time, as resampleRun()
static inline void position(double time, int phases, int half, int& first, int& phase)
{
    int i0 = (int) time;
    phase = (int) ((time - i0) * phases + 0.5);

    if (phase == phases)
    {
        i0++;
        phase = 0;
    }

    first = i0 - half;
}

// Four outputs at a time, each summed as DotAVX2::dot does, so the same, but with four chains of multiply-adds in
// flight rather than one, which would wait for each other
int resampleAVX2(const ResamplerCf *samples, int offset, int available, const float *pairs, int taps, int phases,
    double& time, double step, ResamplerCf *out, int maxOut)
{
    const int half = taps / 2 - 1;
    int n = 0;
    double t = time;

    while (n + 4 <= maxOut)
    {
        int first0, first1, first2, first3, phase0, phase1, phase2, phase3;
        double next = t;
        position(next, phases, half, first0, phase0);
        next += step;
        position(next, phases, half, first1, phase1);
        next += step;
        position(next, phases, half, first2, phase2);
        next += step;
        position(next, phases, half, first3, phase3);
        next += step;

        // The last's first tap is the furthest on
        if (first3 + taps > available) {
            break;
        }

        const float *x0 = reinterpret_cast<const float *>(samples + (first0 - offset));
        const float *x1 = reinterpret_cast<const float *>(samples + (first1 - offset));
        const float *x2 = reinterpret_cast<const float *>(samples + (first2 - offset));
        const float *x3 = reinterpret_cast<const float *>(samples + (first3 - offset));
        const float *h0 = pairs + 2 * phase0 * taps;
        const float *h1 = pairs + 2 * phase1 * taps;
        const float *h2 = pairs + 2 * phase2 * taps;
        const float *h3 = pairs + 2 * phase3 * taps;
        __m256 acc0 = _mm256_setzero_ps(), acc1 = _mm256_setzero_ps(), acc2 = _mm256_setzero_ps(), acc3 = _mm256_setzero_ps();

        for (int i = 0; i < 2 * taps; i += 8)
        {
            acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(x0 + i), _mm256_loadu_ps(h0 + i), acc0);
            acc1 = _mm256_fmadd_ps(_mm256_loadu_ps(x1 + i), _mm256_loadu_ps(h1 + i), acc1);
            acc2 = _mm256_fmadd_ps(_mm256_loadu_ps(x2 + i), _mm256_loadu_ps(h2 + i), acc2);
            acc3 = _mm256_fmadd_ps(_mm256_loadu_ps(x3 + i), _mm256_loadu_ps(h3 + i), acc3);
        }

        float *o = reinterpret_cast<float *>(out + n);
        _mm_storel_pi(reinterpret_cast<__m64 *>(o), sumTaps(acc0));
        _mm_storel_pi(reinterpret_cast<__m64 *>(o + 2), sumTaps(acc1));
        _mm_storel_pi(reinterpret_cast<__m64 *>(o + 4), sumTaps(acc2));
        _mm_storel_pi(reinterpret_cast<__m64 *>(o + 6), sumTaps(acc3));
        n += 4;
        t = next;
    }

    // The rest, one at a time
    time = t;
    return n + resampleRun<DotAVX2>(samples, offset, available, pairs, taps, phases, time, step, out + n, maxOut - n);
}

} // namespace dvbt
