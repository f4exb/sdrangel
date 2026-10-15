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
#include <cstdlib>

#include "cpufeatures.h"
#include "viterbi27.h"
#include "viterbi27kernel.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define VITERBI27_NEON
#endif

namespace dvbt {

int Viterbi27::parity(int v)
{
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1;
}

// Encoder X (bit 0) and Y (bit 1) outputs for each state, with input 0. From state j + 32 they're those from j
// inverted (s5 is in both polynomials), as are those with input 1
struct EncoderOutputs
{
    alignas(32) uint8_t m_xy[64];

    EncoderOutputs()
    {
        for (int s = 0; s < 64; s++) {
            m_xy[s] = Viterbi27::parity(s & Viterbi27::m_maskX) | (Viterbi27::parity(s & Viterbi27::m_maskY) << 1);
        }
    }
};

const EncoderOutputs Viterbi27::m_encoderOutputs;

// Branch costs for 8-bit path metrics: a soft bit costs |llr| / 2 (up to 63) when the bit sent differs from its sign,
// so a branch costs up to 126. A bit's costs for encoder outputs 0..3 (X = bit 0, Y = bit 1) are in bytes 0..3, so a
// step's costs are the sum of X's and Y's words. Halving the LLRs cost 0.01 dB against 16-bit metrics at 64-QAM 2/3
// (quartering them, 0.025 dB)
struct BranchCosts
{
    uint32_t m_x[256];
    uint32_t m_y[256];

    BranchCosts()
    {
        for (int v = -128; v < 128; v++)
        {
            uint32_t q = std::min(63, (std::abs(v) + 1) >> 1);
            uint32_t if0 = v < 0 ? q : 0; // Cost if 0 was sent
            uint32_t if1 = v > 0 ? q : 0;
            m_x[v & 255] = if0 | (if1 << 8) | (if0 << 16) | (if1 << 24);
            m_y[v & 255] = if0 | (if0 << 8) | (if1 << 16) | (if1 << 24);
        }
    }
};

const BranchCosts Viterbi27::m_branchCosts;

Viterbi27::Viterbi27()
{
    reset();
}

void Viterbi27::reset()
{
    std::fill(m_metrics, m_metrics + 64, 0);
    m_nbDecisions = 0;
}

void Viterbi27::encode(const uint8_t *bits, int nbBits, uint8_t& state, std::vector<uint8_t>& out)
{
    size_t base = out.size();
    out.resize(base + 2 * nbBits);
    uint8_t *o = out.data() + base;
    unsigned int s = state; // A local, as the output (bytes) could alias the state

    for (int i = 0; i < nbBits; i++)
    {
        int u = bits[i] & 1;
        int xy = m_encoderOutputs.m_xy[s] ^ (u ? 3 : 0);
        o[2 * i] = xy & 1;
        o[2 * i + 1] = xy >> 1;
        s = ((s << 1) | u) & 63;
    }

    state = (uint8_t) s;
}

#ifdef VITERBI27_NEON

// As the SSE4.1 kernel (viterbi27_sse41.cpp), with table lookups for its byte shuffles, zips for its unpacks, a
// sum of weighted bits for its movemask, and a horizontal minimum. File-local, not Viterbi27's members: their
// signatures have NEON types, which viterbi27.h would need arm_neon.h for
static inline uint64_t movemask(uint8x16_t x, uint8x16_t weights)
{
    const uint8x16_t bits = vandq_u8(x, weights);
    return vaddv_u8(vget_low_u8(bits)) | ((uint64_t) vaddv_u8(vget_high_u8(bits)) << 8);
}

static inline void step8NEON(uint8x16_t m[4], uint32_t cost, const uint8x16_t index[2],
    const uint8x16_t indexInverse[2], uint8x16_t weights, uint64_t *decisions)
{
    const uint8x16_t w = vreinterpretq_u8_u32(vdupq_n_u32(cost));
    uint64_t dec = 0;
    uint8x16_t nm[4];

    for (int g = 0; g < 2; g++)
    {
        const uint8x16_t c = vqtbl1q_u8(w, index[g]);       // From j, input 0, and from j + 32, input 1
        const uint8x16_t cInverse = vqtbl1q_u8(w, indexInverse[g]);
        const uint8x16_t a = m[g];
        const uint8x16_t b = m[g + 2];
        const uint8x16_t e0 = vqaddq_u8(a, c);
        const uint8x16_t e1 = vqaddq_u8(b, cInverse);
        const uint8x16_t o0 = vqaddq_u8(a, cInverse);
        const uint8x16_t o1 = vqaddq_u8(b, c);
        const uint8x16_t even = vminq_u8(e0, e1);
        const uint8x16_t odd = vminq_u8(o0, o1);
        const uint8x16_t de = vceqq_u8(even, e0);
        const uint8x16_t dd = vceqq_u8(odd, o0);
        const uint64_t dlo = movemask(vzip1q_u8(de, dd), weights);
        const uint64_t dhi = movemask(vzip2q_u8(de, dd), weights);
        dec |= (dlo | (dhi << 16)) << (32 * g);
        // New states 32g..32g+31
        nm[2 * g] = vzip1q_u8(even, odd);
        nm[2 * g + 1] = vzip2q_u8(even, odd);
    }

    for (int g = 0; g < 4; g++) {
        m[g] = nm[g];
    }

    *decisions = ~dec;
}

static void forwardNEON(uint8_t *metrics, const uint8_t *xy, const uint32_t *costs, int nbBits, uint64_t *decisions)
{
    static const uint8_t weightBytes[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t weights = vld1q_u8(weightBytes);
    uint8x16_t m[4], index[2], indexInverse[2];

    for (int g = 0; g < 4; g++) {
        m[g] = vld1q_u8(metrics + 16 * g);
    }

    for (int g = 0; g < 2; g++)
    {
        index[g] = vld1q_u8(xy + 16 * g);
        indexInverse[g] = veorq_u8(index[g], vdupq_n_u8(3));
    }

    int i = 0;

    for (; i + 1 < nbBits; i += 2)
    {
        const uint8x16_t norm = vdupq_n_u8(vminvq_u8(vminq_u8(vminq_u8(m[0], m[1]), vminq_u8(m[2], m[3]))));

        step8NEON(m, costs[i], index, indexInverse, weights, &decisions[i]);
        step8NEON(m, costs[i + 1], index, indexInverse, weights, &decisions[i + 1]);

        for (int g = 0; g < 4; g++) {
            m[g] = vqsubq_u8(m[g], norm);
        }
    }

    // A last step on its own is normalised too, so the next call starts within the bound
    if (i < nbBits)
    {
        const uint8x16_t norm = vdupq_n_u8(vminvq_u8(vminq_u8(vminq_u8(m[0], m[1]), vminq_u8(m[2], m[3]))));

        step8NEON(m, costs[i], index, indexInverse, weights, &decisions[i]);

        for (int g = 0; g < 4; g++) {
            m[g] = vqsubq_u8(m[g], norm);
        }
    }

    for (int g = 0; g < 4; g++) {
        vst1q_u8(metrics + 16 * g, m[g]);
    }
}

#endif

// 8-bit path metrics, smallest best: 32 states a vector with AVX2, 16 with SSE4.1 or NEON, twice as many as 16-bit
// ones
void Viterbi27::forward(const int8_t *llrs, int nbBits)
{
    if (m_costs.size() < (size_t) nbBits) {
        m_costs.resize(nbBits);
    }

    for (int i = 0; i < nbBits; i++) {
        m_costs[i] = m_branchCosts.m_x[(uint8_t) llrs[2 * i]] + m_branchCosts.m_y[(uint8_t) llrs[2 * i + 1]];
    }

    const size_t start = m_nbDecisions;

    if (m_decisions.size() < start + nbBits) {
        m_decisions.resize(start + nbBits);
    }

    uint64_t *decisions = m_decisions.data() + start;
    m_nbDecisions += nbBits;

#if defined(VITERBI27_NEON)
    forwardNEON(m_metrics, m_encoderOutputs.m_xy, m_costs.data(), nbBits, decisions);
#else
#if defined(MODEMDVB_X86)
    // The SSE4.1 kernel is faster on efficiency cores: the AVX2 one's permutes across the halves of its registers
    // are slow there
    if (modemdvb::cpuHasAVX2() && !modemdvb::onEfficiencyCore())
    {
        viterbi27Forward8AVX2(m_metrics, m_encoderOutputs.m_xy, m_costs.data(), nbBits, decisions);
        return;
    }

    if (modemdvb::cpuHasSSE41())
    {
        viterbi27Forward8SSE41(m_metrics, m_encoderOutputs.m_xy, m_costs.data(), nbBits, decisions);
        return;
    }
#endif

    forwardScalar(nbBits, decisions);
#endif
}

// The 8-bit kernels' arithmetic, for CPUs without them: saturating adds, and the smallest metric at the start of
// each pair of steps (or of a last step on its own) subtracted at its end
void Viterbi27::forwardScalar(int nbBits, uint64_t *decisions)
{
    uint8_t newMetrics[64];
    int norm = 0;

    for (int i = 0; i < nbBits; i++)
    {
        const uint32_t w = m_costs[i];
        uint64_t dec = 0;

        if ((i & 1) == 0) {
            norm = *std::min_element(m_metrics, m_metrics + 64);
        }

        const int subtract = ((i & 1) || (i == nbBits - 1)) ? norm : 0;

        for (int j = 0; j < 32; j++)
        {
            const int k = m_encoderOutputs.m_xy[j];
            const int c = (w >> (8 * k)) & 255;
            const int cInverse = (w >> (8 * (k ^ 3))) & 255;
            const int e0 = std::min(255, m_metrics[j] + c);
            const int e1 = std::min(255, m_metrics[j + 32] + cInverse);
            const int o0 = std::min(255, m_metrics[j] + cInverse);
            const int o1 = std::min(255, m_metrics[j + 32] + c);
            newMetrics[2 * j] = (uint8_t) (std::min(e0, e1) - subtract);
            newMetrics[2 * j + 1] = (uint8_t) (std::min(o0, o1) - subtract);
            dec |= (uint64_t) (e1 < e0) << (2 * j);
            dec |= (uint64_t) (o1 < o0) << (2 * j + 1);
        }

        std::copy(newMetrics, newMetrics + 64, m_metrics);
        decisions[i] = dec;
    }
}

void Viterbi27::decode(const int8_t *llrs, int nbBits, std::vector<uint8_t>& out)
{
    forward(llrs, nbBits);

    int pending = m_nbDecisions;
    int nbOut = pending - m_tracebackLength;

    if (nbOut <= 0) {
        return;
    }

    uint32_t state = (uint32_t) (std::min_element(m_metrics, m_metrics + 64) - m_metrics);
    const uint64_t *decisions = m_decisions.data();

    for (int t = pending - 1; t >= nbOut; t--) {
        state = (state >> 1) | ((uint32_t) (decisions[t] >> state) & 1) << 5;
    }

    size_t base = out.size();
    out.resize(base + nbOut);
    uint8_t *bits = out.data() + base;

    for (int t = nbOut - 1; t >= 0; t--)
    {
        bits[t] = (uint8_t) (state & 1);
        state = (state >> 1) | ((uint32_t) (decisions[t] >> state) & 1) << 5;
    }

    // Keep the steps not yet output, at the start
    std::copy(m_decisions.begin() + nbOut, m_decisions.begin() + pending, m_decisions.begin());
    m_nbDecisions = pending - nbOut;
}

} // namespace dvbt
