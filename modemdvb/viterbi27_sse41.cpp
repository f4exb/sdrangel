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

// The Viterbi decoder's add-compare-select with 8-bit path metrics and SSE4.1 (for its byte shuffle, unsigned byte
// minimum and minpos): 16 states a vector. Compiled with SSE4.1 enabled, so only used if the CPU has it

#include <smmintrin.h>

#include "viterbi27kernel.h"

namespace dvbt {

// States 0..31 (a) in m[0..1], their butterfly partners 32..63 (b) in m[2..3]
static inline void step8(__m128i m[4], uint32_t cost, const __m128i index[2], const __m128i indexInverse[2],
    uint64_t *decisions)
{
    const __m128i w = _mm_set1_epi32((int) cost);
    uint64_t dec = 0;
    __m128i nm[4];

    for (int g = 0; g < 2; g++)
    {
        const __m128i c = _mm_shuffle_epi8(w, index[g]);   // From j, input 0, and from j + 32, input 1
        const __m128i cInverse = _mm_shuffle_epi8(w, indexInverse[g]);
        const __m128i a = m[g];
        const __m128i b = m[g + 2];
        const __m128i e0 = _mm_adds_epu8(a, c);
        const __m128i e1 = _mm_adds_epu8(b, cInverse);
        const __m128i o0 = _mm_adds_epu8(a, cInverse);
        const __m128i o1 = _mm_adds_epu8(b, c);
        const __m128i even = _mm_min_epu8(e0, e1);
        const __m128i odd = _mm_min_epu8(o0, o1);
        const __m128i de = _mm_cmpeq_epi8(even, e0);
        const __m128i dd = _mm_cmpeq_epi8(odd, o0);
        const uint64_t dlo = (uint32_t) _mm_movemask_epi8(_mm_unpacklo_epi8(de, dd));
        const uint64_t dhi = (uint32_t) _mm_movemask_epi8(_mm_unpackhi_epi8(de, dd));
        dec |= (dlo | (dhi << 16)) << (32 * g);
        // New states 32g..32g+31
        nm[2 * g] = _mm_unpacklo_epi8(even, odd);
        nm[2 * g + 1] = _mm_unpackhi_epi8(even, odd);
    }

    for (int g = 0; g < 4; g++) {
        m[g] = nm[g];
    }

    *decisions = ~dec;
}

// The smallest metric, in every byte
static inline __m128i smallest8(const __m128i m[4])
{
    __m128i smallest = _mm_min_epu8(_mm_min_epu8(m[0], m[1]), _mm_min_epu8(m[2], m[3]));
    smallest = _mm_min_epu8(smallest, _mm_srli_si128(smallest, 8));
    smallest = _mm_minpos_epu16(_mm_cvtepu8_epi16(smallest));
    return _mm_shuffle_epi8(smallest, _mm_setzero_si128());
}

// The smallest metric at the start of each pair of steps is subtracted at the end, as the metrics can't get smaller,
// so finding it overlaps the steps
void viterbi27Forward8SSE41(uint8_t *metrics, const uint8_t *outputs, const uint32_t *costs, int nbBits,
    uint64_t *decisions)
{
    __m128i m[4], index[2], indexInverse[2];

    for (int g = 0; g < 4; g++) {
        m[g] = _mm_loadu_si128((const __m128i *) (metrics + 16 * g));
    }

    for (int g = 0; g < 2; g++)
    {
        index[g] = _mm_loadu_si128((const __m128i *) (outputs + 16 * g));
        indexInverse[g] = _mm_xor_si128(index[g], _mm_set1_epi8(3));
    }

    int i = 0;

    for (; i + 1 < nbBits; i += 2)
    {
        const __m128i norm = smallest8(m);

        step8(m, costs[i], index, indexInverse, &decisions[i]);
        step8(m, costs[i + 1], index, indexInverse, &decisions[i + 1]);

        for (int g = 0; g < 4; g++) {
            m[g] = _mm_subs_epu8(m[g], norm);
        }
    }

    // A last step on its own is normalised too, so the next call starts within the bound
    if (i < nbBits)
    {
        const __m128i norm = smallest8(m);

        step8(m, costs[i], index, indexInverse, &decisions[i]);

        for (int g = 0; g < 4; g++) {
            m[g] = _mm_subs_epu8(m[g], norm);
        }
    }

    for (int g = 0; g < 4; g++) {
        _mm_storeu_si128((__m128i *) (metrics + 16 * g), m[g]);
    }
}

} // namespace dvbt
