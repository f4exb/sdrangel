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

// The Viterbi decoder's add-compare-select with 8-bit path metrics and AVX2: 32 states a vector. Compiled with AVX2
// enabled, so only used if the CPU has it

#include <immintrin.h>

#include "viterbi27kernel.h"

namespace dvbt {

// All 64 states in two vectors: states 0..31 (a) and their butterfly partners 32..63 (b)
static inline void step8(__m256i& a, __m256i& b, uint32_t cost, const __m256i index, const __m256i indexInverse,
    uint64_t *decisions)
{
    const __m256i w = _mm256_set1_epi32((int) cost);
    const __m256i c = _mm256_shuffle_epi8(w, index);            // From j, input 0, and from j + 32, input 1
    const __m256i cInverse = _mm256_shuffle_epi8(w, indexInverse);
    const __m256i e0 = _mm256_adds_epu8(a, c);
    const __m256i e1 = _mm256_adds_epu8(b, cInverse);
    const __m256i o0 = _mm256_adds_epu8(a, cInverse);
    const __m256i o1 = _mm256_adds_epu8(b, c);
    const __m256i even = _mm256_min_epu8(e0, e1);
    const __m256i odd = _mm256_min_epu8(o0, o1);
    const __m256i de = _mm256_cmpeq_epi8(even, e0);
    const __m256i dd = _mm256_cmpeq_epi8(odd, o0);

    // States 2j and 2j + 1 interleaved, within each 128-bit half: [0..15 | 32..47] and [16..31 | 48..63]
    const __m256i lo = _mm256_unpacklo_epi8(even, odd);
    const __m256i hi = _mm256_unpackhi_epi8(even, odd);
    a = _mm256_permute2x128_si256(lo, hi, 0x20);
    b = _mm256_permute2x128_si256(lo, hi, 0x31);

    // Decisions likewise (set where the state came from j), then the middle 16-bit quarters swapped
    const uint64_t dlo = (uint32_t) _mm256_movemask_epi8(_mm256_unpacklo_epi8(de, dd));
    const uint64_t dhi = (uint32_t) _mm256_movemask_epi8(_mm256_unpackhi_epi8(de, dd));
    const uint64_t x = ~(dlo | (dhi << 32));
    *decisions = (x & 0xffff00000000ffffULL) | ((x >> 16) & 0x00000000ffff0000ULL) | ((x << 16) & 0x0000ffff00000000ULL);
}

// The smallest metric, in every byte
static inline __m256i smallest8(const __m256i a, const __m256i b)
{
    const __m256i ab = _mm256_min_epu8(a, b);
    __m128i smallest = _mm_min_epu8(_mm256_castsi256_si128(ab), _mm256_extracti128_si256(ab, 1));
    smallest = _mm_min_epu8(smallest, _mm_srli_si128(smallest, 8));
    smallest = _mm_minpos_epu16(_mm_cvtepu8_epi16(smallest));
    return _mm256_broadcastb_epi8(smallest);
}

// The smallest metric at the start of each pair of steps is subtracted at the end, as the metrics can't get smaller,
// so finding it overlaps the steps
void viterbi27Forward8AVX2(uint8_t *metrics, const uint8_t *outputs, const uint32_t *costs, int nbBits,
    uint64_t *decisions)
{
    __m256i a = _mm256_loadu_si256((const __m256i *) metrics);
    __m256i b = _mm256_loadu_si256((const __m256i *) (metrics + 32));
    const __m256i index = _mm256_loadu_si256((const __m256i *) outputs);
    const __m256i indexInverse = _mm256_xor_si256(index, _mm256_set1_epi8(3));
    int i = 0;

    for (; i + 1 < nbBits; i += 2)
    {
        const __m256i norm = smallest8(a, b);

        step8(a, b, costs[i], index, indexInverse, &decisions[i]);
        step8(a, b, costs[i + 1], index, indexInverse, &decisions[i + 1]);
        a = _mm256_subs_epu8(a, norm);
        b = _mm256_subs_epu8(b, norm);
    }

    // A last step on its own is normalised too, so the next call starts within the bound
    if (i < nbBits)
    {
        const __m256i norm = smallest8(a, b);

        step8(a, b, costs[i], index, indexInverse, &decisions[i]);
        a = _mm256_subs_epu8(a, norm);
        b = _mm256_subs_epu8(b, norm);
    }

    _mm256_storeu_si256((__m256i *) metrics, a);
    _mm256_storeu_si256((__m256i *) (metrics + 32), b);
}

} // namespace dvbt
