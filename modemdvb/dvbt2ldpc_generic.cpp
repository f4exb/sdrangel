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

// LDPC decoder for 16 codewords at a time, in portable C++, for CPUs without SSE4.1 or NEON. The same arithmetic
// as the SIMD versions (saturating int8), so it gives the same results, but is several times slower. Compilers may
// vectorise the loops

#include <algorithm>

#include "dvbt2ldpc.h"

// Load and store without SSE2's transposes, as this may be built for any CPU
#undef DVBT2_LDPC_X86
#include "dvbt2ldpckernel.h"

namespace dvbt2 {
namespace {

// Plain loops over the lanes, which compilers can vectorise
#define DVBT2_LANES(expr) T r; for (int i = 0; i < W; i++) { r.v[i] = (int8_t) (expr); } return r;

struct VectorGeneric
{
    static const int W = 16;
    struct T { int8_t v[W]; };

    static int clamp(int x) { return x > 127 ? 127 : x < -128 ? -128 : x; }

    static T set1(char x) { DVBT2_LANES(x) }
    static T zero() { DVBT2_LANES(0) }
    static T adds(const T& a, const T& b) { DVBT2_LANES(clamp(a.v[i] + b.v[i])) }
    static T subs(const T& a, const T& b) { DVBT2_LANES(clamp(a.v[i] - b.v[i])) }
    static T addsu(const T& a, const T& b) { DVBT2_LANES(std::min((int) (uint8_t) a.v[i] + (uint8_t) b.v[i], 255)) }
    // As _mm_abs_epi8: -128 stays -128
    static T abs(const T& a) { DVBT2_LANES(a.v[i] < 0 ? -a.v[i] : a.v[i]) }
    static T min(const T& a, const T& b) { DVBT2_LANES(a.v[i] < b.v[i] ? a.v[i] : b.v[i]) }
    static T max(const T& a, const T& b) { DVBT2_LANES(a.v[i] > b.v[i] ? a.v[i] : b.v[i]) }
    static T cmpgt(const T& a, const T& b) { DVBT2_LANES(a.v[i] > b.v[i] ? -1 : 0) }
    // As _mm_sign_epi8: a, negated where b is negative, zero where b is zero
    static T sign(const T& a, const T& b) { DVBT2_LANES(b.v[i] < 0 ? -a.v[i] : b.v[i] == 0 ? 0 : a.v[i]) }
    static T xor_(const T& a, const T& b) { DVBT2_LANES(a.v[i] ^ b.v[i]) }
    static T and_(const T& a, const T& b) { DVBT2_LANES(a.v[i] & b.v[i]) }
    static T or_(const T& a, const T& b) { DVBT2_LANES(a.v[i] | b.v[i]) }

    static void store(uint8_t *p, const T& a)
    {
        for (int i = 0; i < W; i++) {
            p[i] = (uint8_t) a.v[i];
        }
    }

    static uint64_t movemask(const T& a)
    {
        uint64_t mask = 0;

        for (int i = 0; i < W; i++) {
            mask |= (uint64_t) (a.v[i] < 0) << i;
        }

        return mask;
    }
};

#undef DVBT2_LANES

} // namespace

const LDPCKernel ldpcKernelGeneric = {VectorGeneric::W, decodeLDPC<VectorGeneric>};

} // namespace dvbt2
