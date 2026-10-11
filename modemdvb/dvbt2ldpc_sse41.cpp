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

// LDPC decoder for 16 codewords at a time, with SSE4.1. Compiled with SSE4.1 enabled

#include <smmintrin.h>

#include "dvbt2ldpckernel.h"

namespace dvbt2 {
namespace {

struct VectorSSE41
{
    typedef __m128i T;
    static const int W = 16;

    static T zero() { return _mm_setzero_si128(); }
    static T set1(char x) { return _mm_set1_epi8(x); }
    static T adds(T a, T b) { return _mm_adds_epi8(a, b); }
    static T subs(T a, T b) { return _mm_subs_epi8(a, b); }
    static T addsu(T a, T b) { return _mm_adds_epu8(a, b); }
    static T abs(T a) { return _mm_abs_epi8(a); }
    static T min(T a, T b) { return _mm_min_epi8(a, b); }
    static T max(T a, T b) { return _mm_max_epi8(a, b); }
    static T cmpgt(T a, T b) { return _mm_cmpgt_epi8(a, b); }
    static T sign(T a, T b) { return _mm_sign_epi8(a, b); }
    static T xor_(T a, T b) { return _mm_xor_si128(a, b); }
    static T and_(T a, T b) { return _mm_and_si128(a, b); }
    static T or_(T a, T b) { return _mm_or_si128(a, b); }
    static void store(uint8_t *p, T a) { _mm_storeu_si128(reinterpret_cast<T *>(p), a); }
    static uint64_t movemask(T a) { return (uint32_t) _mm_movemask_epi8(a); }
};

} // namespace

const LDPCKernel ldpcKernelSSE41 = {VectorSSE41::W, decodeLDPC<VectorSSE41>};

} // namespace dvbt2
