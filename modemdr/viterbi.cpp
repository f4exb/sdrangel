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
#include <cstdlib>
#include <utility>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#define MODEMDR_VITERBI_SSE2 1
#include <emmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define MODEMDR_VITERBI_NEON 1
#include <arm_neon.h>
#endif

#include "viterbi.h"

namespace modemdr
{

namespace
{

unsigned int parity(unsigned int v)
{
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1U;
}

// Set in the environment to use the scalar pass, to check the two agree
bool scalarRequested()
{
    static const bool scalar = (std::getenv("MODEMDR_SCALAR") != nullptr) || (std::getenv("MODEMDAB_SCALAR") != nullptr);
    return scalar;
}

} // anonymous namespace

Viterbi::Viterbi(const std::vector<unsigned int>& generators) :
    m_outputs(static_cast<int>(std::min<std::size_t>(generators.size(), MaximumOutputs))),
    m_generators(generators.begin(), generators.begin() + m_outputs)
{
    for (int k = 0; k < MaximumOutputs; k++)
    {
        for (int j = 0; j < 32; j++) {
            m_signs[k][j] = (k < m_outputs) && ((registerOutputs(2U * j) >> k) & 1U) ? -1.0f : 1.0f;
        }
    }
}

const std::vector<unsigned int>& Viterbi::dabGenerators()
{
    static const std::vector<unsigned int> generators{0133, 0171, 0145, 0133};
    return generators;
}

const std::vector<unsigned int>& Viterbi::drmGenerators()
{
    static const std::vector<unsigned int> generators{0133, 0171, 0145, 0133, 0171, 0145};
    return generators;
}

unsigned int Viterbi::registerOutputs(unsigned int reg) const
{
    unsigned int out = 0;
    for (int k = 0; k < m_outputs; k++) {
        out |= parity(reg & m_generators[k]) << k;
    }
    return out;
}

// The trellis as butterflies: the predecessors of states j and j + 32 are states 2j and 2j + 1.
// The outputs of (input 1, state 2j) are those of (input 0, state 2j + 1), and flipping the oldest
// bit inverts them all: one branch metric m and its negation serve the four transitions.
//   new[j]      = max(old[2j] + m, old[2j + 1] - m)
//   new[j + 32] = max(old[2j] - m, old[2j + 1] + m)
void Viterbi::forward(const float *soft, int steps, std::uint64_t *decisions, std::uint64_t *close, float closeLimit) const
{
#ifdef MODEMDR_VITERBI_SSE2
    if (scalarRequested())
    {
        forwardScalar(soft, steps, decisions, close, closeLimit);
        return;
    }
    alignas(16) float a[States];
    alignas(16) float b[States];
    float *metrics = a;
    float *next = b;
    const bool closeWanted = (close != nullptr) && (closeLimit > 0.0f);
    const __m128 absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
    const __m128 limit = _mm_set1_ps(closeLimit);
    const int n = m_outputs;

    std::fill(a, a + States, -1.0e30f);
    a[0] = 0.0f;

    for (int t = 0; t < steps; t++)
    {
        __m128 s[MaximumOutputs];
        for (int k = 0; k < n; k++) {
            s[k] = _mm_set1_ps(soft[n * t + k]);
        }
        std::uint64_t stepDecisions = 0;
        std::uint64_t closeStates = 0;

        for (int block = 0; block < 8; block++)
        {
            const __m128 lo = _mm_load_ps(&metrics[8 * block]);
            const __m128 hi = _mm_load_ps(&metrics[8 * block + 4]);
            const __m128 even = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(2, 0, 2, 0));
            const __m128 odd = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 3, 1));
            const int j = 4 * block;
            __m128 m = _mm_mul_ps(s[0], _mm_load_ps(&m_signs[0][j]));
            for (int k = 1; k < n; k++) {
                m = _mm_add_ps(m, _mm_mul_ps(s[k], _mm_load_ps(&m_signs[k][j])));
            }

            const __m128 evenPlus = _mm_add_ps(even, m);
            const __m128 evenMinus = _mm_sub_ps(even, m);
            const __m128 oddPlus = _mm_add_ps(odd, m);
            const __m128 oddMinus = _mm_sub_ps(odd, m);

            _mm_store_ps(&next[j], _mm_max_ps(evenPlus, oddMinus));
            _mm_store_ps(&next[j + 32], _mm_max_ps(evenMinus, oddPlus));
            if (closeWanted)
            {
                const __m128 lowDelta = _mm_and_ps(_mm_sub_ps(evenPlus, oddMinus), absMask);
                const __m128 highDelta = _mm_and_ps(_mm_sub_ps(evenMinus, oddPlus), absMask);
                closeStates |= (static_cast<std::uint64_t>(_mm_movemask_ps(_mm_cmplt_ps(lowDelta, limit))) << j)
                    | (static_cast<std::uint64_t>(_mm_movemask_ps(_mm_cmplt_ps(highDelta, limit))) << (j + 32));
            }
            const std::uint64_t low = static_cast<std::uint64_t>(_mm_movemask_ps(_mm_cmpgt_ps(oddMinus, evenPlus)));
            const std::uint64_t high = static_cast<std::uint64_t>(_mm_movemask_ps(_mm_cmpgt_ps(oddPlus, evenMinus)));
            stepDecisions |= (low << j) | (high << (j + 32));
        }
        decisions[t] = stepDecisions;
        if (closeWanted) {
            close[t] = closeStates;
        }

        // Keep the metrics near zero
        if ((t & 31) == 31)
        {
            const __m128 reference = _mm_set1_ps(next[0]);
            for (int i = 0; i < States; i += 4) {
                _mm_store_ps(&next[i], _mm_sub_ps(_mm_load_ps(&next[i]), reference));
            }
        }
        std::swap(metrics, next);
    }
#elif defined(MODEMDR_VITERBI_NEON)
    // As the SSE2 pass: the same operations in the same order, so the same decisions
    if (scalarRequested())
    {
        forwardScalar(soft, steps, decisions, close, closeLimit);
        return;
    }
    alignas(16) float a[States];
    alignas(16) float b[States];
    float *metrics = a;
    float *next = b;
    const bool closeWanted = (close != nullptr) && (closeLimit > 0.0f);
    const float32x4_t limit = vdupq_n_f32(closeLimit);
    static const std::uint32_t laneBitValues[4] = {1U, 2U, 4U, 8U};
    const uint32x4_t laneBits = vld1q_u32(laneBitValues);
    // A comparison's lanes as bits, lane i in bit i (SSE2's movemask)
    const auto mask = [&laneBits](uint32x4_t compared) {
        return static_cast<std::uint64_t>(vaddvq_u32(vandq_u32(compared, laneBits)));
    };
    const int n = m_outputs;

    std::fill(a, a + States, -1.0e30f);
    a[0] = 0.0f;

    for (int t = 0; t < steps; t++)
    {
        float32x4_t s[MaximumOutputs];
        for (int k = 0; k < n; k++) {
            s[k] = vdupq_n_f32(soft[n * t + k]);
        }
        std::uint64_t stepDecisions = 0;
        std::uint64_t closeStates = 0;

        for (int block = 0; block < 8; block++)
        {
            const float32x4x2_t pairs = vld2q_f32(&metrics[8 * block]);
            const float32x4_t even = pairs.val[0];
            const float32x4_t odd = pairs.val[1];
            const int j = 4 * block;
            // Products with signs of +-1 are exact: fused or not, the sums are the same
            float32x4_t m = vmulq_f32(s[0], vld1q_f32(&m_signs[0][j]));
            for (int k = 1; k < n; k++) {
                m = vaddq_f32(m, vmulq_f32(s[k], vld1q_f32(&m_signs[k][j])));
            }

            const float32x4_t evenPlus = vaddq_f32(even, m);
            const float32x4_t evenMinus = vsubq_f32(even, m);
            const float32x4_t oddPlus = vaddq_f32(odd, m);
            const float32x4_t oddMinus = vsubq_f32(odd, m);

            vst1q_f32(&next[j], vmaxq_f32(evenPlus, oddMinus));
            vst1q_f32(&next[j + 32], vmaxq_f32(evenMinus, oddPlus));
            if (closeWanted)
            {
                closeStates |= (mask(vcltq_f32(vabdq_f32(evenPlus, oddMinus), limit)) << j)
                    | (mask(vcltq_f32(vabdq_f32(evenMinus, oddPlus), limit)) << (j + 32));
            }
            stepDecisions |= (mask(vcgtq_f32(oddMinus, evenPlus)) << j) | (mask(vcgtq_f32(oddPlus, evenMinus)) << (j + 32));
        }
        decisions[t] = stepDecisions;
        if (closeWanted) {
            close[t] = closeStates;
        }

        // Keep the metrics near zero
        if ((t & 31) == 31)
        {
            const float32x4_t reference = vdupq_n_f32(next[0]);
            for (int i = 0; i < States; i += 4) {
                vst1q_f32(&next[i], vsubq_f32(vld1q_f32(&next[i]), reference));
            }
        }
        std::swap(metrics, next);
    }
#else
    forwardScalar(soft, steps, decisions, close, closeLimit);
#endif
}

void Viterbi::forwardScalar(const float *soft, int steps, std::uint64_t *decisions, std::uint64_t *close, float closeLimit) const
{
    // The same butterflies as the SSE2 and NEON passes, in plain arrays that compilers vectorize
    float a[States];
    float b[States];
    float *metrics = a;
    float *next = b;
    float m[32];
    const bool closeWanted = (close != nullptr) && (closeLimit > 0.0f);
    const int n = m_outputs;

    std::fill(a, a + States, -1.0e30f);
    a[0] = 0.0f;

    for (int t = 0; t < steps; t++)
    {
        const float *s = soft + n * t;

        for (int j = 0; j < 32; j++)
        {
            float sum = s[0] * m_signs[0][j];
            for (int k = 1; k < n; k++) {
                sum += s[k] * m_signs[k][j];
            }
            m[j] = sum;
        }

        std::uint64_t stepDecisions = 0;
        std::uint64_t closeStates = 0;
        for (int j = 0; j < 32; j++)
        {
            const float evenPlus = metrics[2 * j] + m[j];
            const float evenMinus = metrics[2 * j] - m[j];
            const float oddPlus = metrics[2 * j + 1] + m[j];
            const float oddMinus = metrics[2 * j + 1] - m[j];
            const bool low = oddMinus > evenPlus;
            const bool high = oddPlus > evenMinus;
            next[j] = low ? oddMinus : evenPlus;
            next[j + 32] = high ? oddPlus : evenMinus;
            if (closeWanted)
            {
                closeStates |= (static_cast<std::uint64_t>(std::fabs(evenPlus - oddMinus) < closeLimit) << j)
                    | (static_cast<std::uint64_t>(std::fabs(evenMinus - oddPlus) < closeLimit) << (j + 32));
            }
            stepDecisions |= (static_cast<std::uint64_t>(low) << j) | (static_cast<std::uint64_t>(high) << (j + 32));
        }
        decisions[t] = stepDecisions;
        if (closeWanted) {
            close[t] = closeStates;
        }

        if ((t & 31) == 31)
        {
            const float reference = next[0];
            for (int i = 0; i < States; i++) {
                next[i] -= reference;
            }
        }
        std::swap(metrics, next);
    }
}

void Viterbi::traceback(const std::uint64_t *decisions, int steps, int informationBits, std::uint8_t *bits, std::uint8_t *states)
{
    int state = 0;

    for (int t = steps - 1; t >= 0; t--)
    {
        if (states) {
            states[t] = static_cast<std::uint8_t>(state);
        }
        if (t < informationBits) {
            bits[t] = static_cast<std::uint8_t>(state >> 5);
        }
        const int d = static_cast<int>((decisions[t] >> state) & 1U);
        state = ((state & 31) << 1) | d;
    }
}

} // namespace modemdr
