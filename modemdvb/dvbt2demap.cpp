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

// Demapping of many cells at once, 4 at a time with SSE2 or NEON (AArch64). The same arithmetic as demapCell()
// and the receiver's outlier limiting, cell by cell, so the same LLRs

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>

#include "dvbt2fec.h"
#include "dvbt2demap.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define DEMAP_NEON
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define DEMAP_SSE2
#endif

namespace dvbt2 {

float CellDemapper::normalisation(int eta)
{
    static const float values[] = {1.0f, 1.0f, std::sqrt(2.0f), 1.0f, std::sqrt(10.0f), 1.0f, std::sqrt(42.0f), 1.0f, std::sqrt(170.0f)};
    return values[eta];
}

#if defined(DEMAP_SSE2)

struct F4
{
    typedef __m128 T;
    static T load(const float *p) { return _mm_loadu_ps(p); }
    static void store(float *p, T a) { _mm_storeu_ps(p, a); }
    static T set1(float x) { return _mm_set1_ps(x); }
    static T add(T a, T b) { return _mm_add_ps(a, b); }
    static T sub(T a, T b) { return _mm_sub_ps(a, b); }
    static T mul(T a, T b) { return _mm_mul_ps(a, b); }
    static T div(T a, T b) { return _mm_div_ps(a, b); }
    static T min(T a, T b) { return _mm_min_ps(a, b); }
    static T max(T a, T b) { return _mm_max_ps(a, b); }
    static T abs(T a) { return _mm_andnot_ps(_mm_set1_ps(-0.0f), a); }
    static T greater(T a, T b) { return _mm_cmpgt_ps(a, b); }
    static T select(T mask, T a, T b) { return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b)); }

    // Exact for |a| < 2^31: truncate, then one less where that rounded up
    static T floor(T a)
    {
        T t = _mm_cvtepi32_ps(_mm_cvttps_epi32(a));
        return _mm_sub_ps(t, _mm_and_ps(_mm_cmpgt_ps(t, a), _mm_set1_ps(1.0f)));
    }

    // Round to nearest, ties to even (the default rounding mode, as std::nearbyint), clamped to +/-llrLimit
    static void toInt8(T a, int8_t out[4])
    {
        a = _mm_min_ps(_mm_max_ps(a, _mm_set1_ps(-llrLimit)), _mm_set1_ps(llrLimit));
        __m128i i = _mm_cvtps_epi32(a);
        i = _mm_packs_epi32(i, i);
        i = _mm_packs_epi16(i, i);
        int32_t bytes = _mm_cvtsi128_si32(i);
        std::memcpy(out, &bytes, 4);
    }
};

#elif defined(DEMAP_NEON)

struct F4
{
    typedef float32x4_t T;
    static T load(const float *p) { return vld1q_f32(p); }
    static void store(float *p, T a) { vst1q_f32(p, a); }
    static T set1(float x) { return vdupq_n_f32(x); }
    static T add(T a, T b) { return vaddq_f32(a, b); }
    static T sub(T a, T b) { return vsubq_f32(a, b); }
    static T mul(T a, T b) { return vmulq_f32(a, b); }
    static T div(T a, T b) { return vdivq_f32(a, b); }
    static T min(T a, T b) { return vminq_f32(a, b); }
    static T max(T a, T b) { return vmaxq_f32(a, b); }
    static T abs(T a) { return vabsq_f32(a); }
    static T greater(T a, T b) { return vreinterpretq_f32_u32(vcgtq_f32(a, b)); }
    static T select(T mask, T a, T b) { return vbslq_f32(vreinterpretq_u32_f32(mask), a, b); }
    static T floor(T a) { return vrndmq_f32(a); }

    static void toInt8(T a, int8_t out[4])
    {
        a = vminq_f32(vmaxq_f32(a, vdupq_n_f32(-llrLimit)), vdupq_n_f32(llrLimit));
        int32x4_t i = vcvtnq_s32_f32(a);
        int16x4_t h = vmovn_s32(i);
        int8x8_t b = vmovn_s16(vcombine_s16(h, h));
        int32_t bytes = vget_lane_s32(vreinterpret_s32_s8(b), 0);
        std::memcpy(out, &bytes, 4);
    }
};

#else

// One lane, for other processors
struct F4
{
    struct T { float v[4]; };
    static T load(const float *p) { T r; for (int i = 0; i < 4; i++) r.v[i] = p[i]; return r; }
    static void store(float *p, T a) { for (int i = 0; i < 4; i++) p[i] = a.v[i]; }
    static T set1(float x) { T r; for (int i = 0; i < 4; i++) r.v[i] = x; return r; }
    static T add(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] += b.v[i]; return a; }
    static T sub(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] -= b.v[i]; return a; }
    static T mul(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] *= b.v[i]; return a; }
    static T div(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] /= b.v[i]; return a; }
    static T min(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] = std::min(a.v[i], b.v[i]); return a; }
    static T max(T a, T b) { for (int i = 0; i < 4; i++) a.v[i] = std::max(a.v[i], b.v[i]); return a; }
    static T abs(T a) { for (int i = 0; i < 4; i++) a.v[i] = std::fabs(a.v[i]); return a; }
    static T greater(T a, T b) { T r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] > b.v[i] ? 1.0f : 0.0f; return r; }
    static T select(T mask, T a, T b) { for (int i = 0; i < 4; i++) a.v[i] = mask.v[i] != 0.0f ? a.v[i] : b.v[i]; return a; }
    static T floor(T a) { for (int i = 0; i < 4; i++) a.v[i] = std::floor(a.v[i]); return a; }

    static void toInt8(T a, int8_t out[4])
    {
        for (int i = 0; i < 4; i++) {
            out[i] = (int8_t) std::max(-llrLimit, std::min(llrLimit, std::nearbyint(a.v[i])));
        }
    }
};

#endif

template <bool axes>
void CellDemapper::demap(const float *re, const float *im, float *scales, float *scalesQ, int nbCells, int eta, float outlierThreshold,
    const int *bitMap, int8_t *llrs, float *errors)
{
    const int m = eta / 2;
    const float norm = normalisation(eta);
    const float norm2 = norm * norm;
    const float maxLevel = (float) ((1 << m) - 1);
    const F4::T vNorm = F4::set1(norm);
    const F4::T vNorm2 = F4::set1(norm2);
    const F4::T vMax = F4::set1(maxLevel);
    const F4::T vMin = F4::set1(-maxLevel);
    const F4::T vHalf = F4::set1(0.5f);
    const F4::T vTwo = F4::set1(2.0f);
    const F4::T vOne = F4::set1(1.0f);
    const F4::T vQuarter = F4::set1(0.25f);
    const F4::T vThreshold = F4::set1(outlierThreshold);
    const F4::T vInfinity = F4::set1(std::numeric_limits<float>::infinity());
    const F4::T vZero = F4::set1(0.0f);
    int8_t bytes[4];
    int c = 0;

    // Whole groups of 4 cells, then the rest as a padded group
    for (; c < nbCells; c += 4)
    {
        const int n = std::min(4, nbCells - c);
        float r4[4] = {0.0f, 0.0f, 0.0f, 0.0f}, i4[4] = {0.0f, 0.0f, 0.0f, 0.0f}, s4[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float q4[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        std::copy(re + c, re + c + n, r4);
        std::copy(im + c, im + c + n, i4);
        std::copy(scales + c, scales + c + n, s4);

        if (axes) {
            std::copy(scalesQ + c, scalesQ + c + n, q4);
        }

        const F4::T x = F4::mul(F4::load(r4), vNorm);  // Unnormalised units
        const F4::T y = F4::mul(F4::load(i4), vNorm);
        // A scale that isn't finite (NaN, or infinite, from no noise at all) is no information: as the clamps of NaN
        // differ between processors, its LLRs would too
        F4::T scale = F4::load(s4);
        scale = F4::select(F4::greater(vInfinity, F4::abs(scale)), scale, vZero);
        F4::T scaleQ = scale;

        if (axes)
        {
            scaleQ = F4::load(q4);
            scaleQ = F4::select(F4::greater(vInfinity, F4::abs(scaleQ)), scaleQ, vZero);
        }

        // Error power to the nearest point, normalised. Clamping before the floor makes no difference to the level,
        // and keeps the floor exact however far out the cell is
        auto axisError = [&](F4::T v) {
            F4::T half = F4::max(vMin, F4::min(vMax, F4::mul(v, vHalf)));
            F4::T level = F4::add(F4::mul(vTwo, F4::floor(half)), vOne);
            level = F4::max(vMin, F4::min(vMax, level));
            return F4::sub(v, level);
        };
        const F4::T ei = axisError(x), eq = axisError(y);
        const F4::T ei2 = F4::mul(ei, ei), eq2 = F4::mul(eq, eq);
        const F4::T error = F4::div(F4::add(ei2, eq2), vNorm2);
        float e4[4];
        F4::store(e4, error);
        std::copy(e4, e4 + n, errors + c);

        // A cell much further from the nearest point than noise would put it is less reliable than it appears. With
        // a scale for each axis, each axis's error is relative to its own noise, and both are reduced alike
        if (outlierThreshold > 0.0f)
        {
            if (axes)
            {
                const F4::T relative = F4::mul(F4::div(F4::add(F4::mul(ei2, scale), F4::mul(eq2, scaleQ)), vNorm2), vQuarter);
                const F4::T over = F4::greater(relative, vThreshold);
                const F4::T factor = F4::div(vThreshold, relative);
                scale = F4::select(over, F4::mul(scale, factor), scale);
                scaleQ = F4::select(over, F4::mul(scaleQ, factor), scaleQ);
                F4::store(q4, scaleQ);
                std::copy(q4, q4 + n, scalesQ + c);
            }
            else
            {
                const F4::T relative = F4::mul(F4::mul(error, scale), vQuarter); // Over the noise per dimension
                const F4::T limited = F4::mul(scale, F4::div(vThreshold, relative));
                scale = F4::select(F4::greater(relative, vThreshold), limited, scale);
            }

            F4::store(s4, scale);
            std::copy(s4, s4 + n, scales + c);
        }

        // LLRs of each axis's bits: the sign, then |d - threshold| recursively (table 14's Gray mapping)
        const F4::T sI = F4::div(scale, vNorm2);
        const F4::T sQ = axes ? F4::div(scaleQ, vNorm2) : sI;

        for (int axis = 0; axis < 2; axis++)
        {
            const F4::T s = axis == 0 ? sI : sQ;
            F4::T v = axis == 0 ? x : y;
            F4::T d = F4::abs(v);
            float threshold = (float) (1 << (m - 1));
            F4::toInt8(F4::mul(s, v), bytes);

            for (int j = 0; j < n; j++) {
                llrs[bitMap[(c + j) * eta + axis]] = bytes[j];
            }

            for (int k = 1; k < m; k++)
            {
                const F4::T t = F4::set1(threshold);
                F4::toInt8(F4::mul(s, F4::sub(d, t)), bytes);

                for (int j = 0; j < n; j++) {
                    llrs[bitMap[(c + j) * eta + 2 * k + axis]] = bytes[j];
                }

                d = F4::abs(F4::sub(d, t));
                threshold *= 0.5f;
            }
        }
    }
}

void demapCells(const float *re, const float *im, float *scales, int nbCells, int eta, float outlierThreshold,
    const int *bitMap, int8_t *llrs, float *errors)
{
    CellDemapper::demap<false>(re, im, scales, nullptr, nbCells, eta, outlierThreshold, bitMap, llrs, errors);
}

void demapCellsAxes(const float *re, const float *im, float *scalesI, float *scalesQ, int nbCells, int eta,
    float outlierThreshold, const int *bitMap, int8_t *llrs, float *errors)
{
    CellDemapper::demap<true>(re, im, scalesI, scalesQ, nbCells, eta, outlierThreshold, bitMap, llrs, errors);
}

} // namespace dvbt2
