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

// LDPC decoder for 16 codewords at a time, with ARM NEON (always present on AArch64; on 32-bit ARM, when the
// compiler targets it)

#include "dvbt2ldpc.h"

#if defined(DVBT2_LDPC_NEON)

#include <arm_neon.h>

#include "dvbt2ldpckernel.h"

namespace dvbt2 {
namespace {

struct VectorNEON
{
    typedef int8x16_t T;
    static const int W = 16;

    static T zero() { return vdupq_n_s8(0); }
    static T set1(char x) { return vdupq_n_s8((int8_t) x); }
    static T adds(T a, T b) { return vqaddq_s8(a, b); }
    static T subs(T a, T b) { return vqsubq_s8(a, b); }
    static T addsu(T a, T b) { return vreinterpretq_s8_u8(vqaddq_u8(vreinterpretq_u8_s8(a), vreinterpretq_u8_s8(b))); }
    static T abs(T a) { return vabsq_s8(a); }
    static T min(T a, T b) { return vminq_s8(a, b); }
    static T max(T a, T b) { return vmaxq_s8(a, b); }
    static T cmpgt(T a, T b) { return vreinterpretq_s8_u8(vcgtq_s8(a, b)); }

    // As SSSE3's _mm_sign_epi8: a, negated where b is negative, zero where b is zero
    static T sign(T a, T b)
    {
        const uint8x16_t negative = vcltq_s8(b, vdupq_n_s8(0));
        const uint8x16_t isZero = vceqq_s8(b, vdupq_n_s8(0));
        const T r = vbslq_s8(negative, vnegq_s8(a), a);
        return vbicq_s8(r, vreinterpretq_s8_u8(isZero));
    }

    static T xor_(T a, T b) { return veorq_s8(a, b); }
    static T and_(T a, T b) { return vandq_s8(a, b); }
    static T or_(T a, T b) { return vorrq_s8(a, b); }
    static void store(uint8_t *p, T a) { vst1q_u8(p, vreinterpretq_u8_s8(a)); }

    // The sign bit of each lane, as SSE2's _mm_movemask_epi8. NEON has no equivalent, but this is only used now and
    // then, to see if decoding has finished
    static uint64_t movemask(T a)
    {
        uint8_t bits[16];
        vst1q_u8(bits, vshrq_n_u8(vreinterpretq_u8_s8(a), 7));
        uint64_t mask = 0;

        for (int i = 0; i < 16; i++) {
            mask |= (uint64_t) bits[i] << i;
        }

        return mask;
    }
};

} // namespace

const LDPCKernel ldpcKernelNEON = {VectorNEON::W, decodeLDPC<VectorNEON>};

} // namespace dvbt2

#endif // DVBT2_LDPC_NEON
