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

#ifndef MODEMDVB_DVBT2LDPCKERNEL_H
#define MODEMDVB_DVBT2LDPCKERNEL_H

// The LDPC decoder, for a vector type V (see dvbt2ldpc_sse41.cpp, dvbt2ldpc_avx2.cpp, dvbt2ldpc_neon.cpp and
// dvbt2ldpc_generic.cpp, compiled for each instruction set). Included only by those files. As some are compiled for
// instruction sets the CPU mightn't have, only raw buffers here, made by LDPC (dvbt2fec.cpp): library code they
// instantiated (std::vector's, std::fill...) would be compiled for that instruction set too, and the linker could keep
// that copy for every caller. And all in an unnamed namespace, so each file's is its own

#include <cstddef>
#include <cstdint>

#include "dvbt2ldpc.h"

#if defined(DVBT2_LDPC_X86)
#include <emmintrin.h>
#endif

namespace dvbt2 {
namespace {

inline int minInt(int a, int b) { return a < b ? a : b; }
inline int maxInt(int a, int b) { return a > b ? a : b; }

#if defined(DVBT2_LDPC_X86)

// 16x16 byte transpose: r[i] holds row i, and on return column i
static inline void transpose16x16(__m128i r[16])
{
    __m128i b[16], c[16], d[16];

    for (int i = 0; i < 8; i++)
    {
        b[2 * i] = _mm_unpacklo_epi8(r[2 * i], r[2 * i + 1]);
        b[2 * i + 1] = _mm_unpackhi_epi8(r[2 * i], r[2 * i + 1]);
    }

    for (int i = 0; i < 4; i++)
    {
        c[4 * i] = _mm_unpacklo_epi16(b[4 * i], b[4 * i + 2]);
        c[4 * i + 1] = _mm_unpackhi_epi16(b[4 * i], b[4 * i + 2]);
        c[4 * i + 2] = _mm_unpacklo_epi16(b[4 * i + 1], b[4 * i + 3]);
        c[4 * i + 3] = _mm_unpackhi_epi16(b[4 * i + 1], b[4 * i + 3]);
    }

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            d[8 * i + 2 * j] = _mm_unpacklo_epi32(c[8 * i + j], c[8 * i + j + 4]);
            d[8 * i + 2 * j + 1] = _mm_unpackhi_epi32(c[8 * i + j], c[8 * i + j + 4]);
        }
    }

    for (int j = 0; j < 8; j++)
    {
        r[2 * j] = _mm_unpacklo_epi64(d[j], d[j + 8]);
        r[2 * j + 1] = _mm_unpackhi_epi64(d[j], d[j + 8]);
    }
}
#endif

// Layered decoding: each check's messages to its variables are stored, so the variables' LLRs can be updated in place,
// check by check. The vectors hold the same bit of W codewords. The check update is the sum-product's, with pairwise
// minimums corrected towards the exact box-plus, as ldpctool's MinSumC, which offset min-sum is 0.2 dB worse than
template <class V>
class LDPCKernelImpl
{
public:
    typedef typename V::T T;

    explicit LDPCKernelImpl(const LDPCKernelData& data) :
        m_n(data.m_n),
        m_nbChecks(data.m_nbChecks),
        m_start(data.m_checkStart),
        m_vars(data.m_vars),
        m_filler(data.m_filler),
        m_app(static_cast<T *>(data.m_app)),
        m_c2v(static_cast<T *>(data.m_c2v)),
        m_t(static_cast<T *>(data.m_t)),
        m_prefix(static_cast<T *>(data.m_prefix))
    {
    }

    int decode(int8_t *const *blocks, int nbBlocks, int maxIterations, int *unconverged, uint64_t quickBlocks)
    {
        const int nbChecks = m_nbChecks;
        const int *start = m_start;
        const int *vars = m_vars;
        const uint64_t laneMask = nbBlocks >= 64 ? ~0ULL : ((1ULL << nbBlocks) - 1);
        const uint64_t quick = quickBlocks & laneMask;
        const uint64_t others = laneMask & ~quick;

        load(blocks, nbBlocks);

        T *app = m_app;
        T *c2v = m_c2v;
        int iteration = 0;
        bool stuck = false;
        uint8_t counts[V::W], previous[V::W];
        int stable[V::W];

        for (int l = 0; l < V::W; l++)
        {
            previous[l] = 255;
            stable[l] = 0;
        }

        // The iterations the blocks not quick took (converged or stuck), -1 till they have: the quick ones are given a
        // few times as many (at threshold, many, as they can need)
        int othersDone = ((quick != 0) && (others == 0)) ? 0 : -1;

        for (; iteration < maxIterations; iteration++)
        {
            // A codeword with a few unsatisfied checks, the same number for a few iterations, is usually stuck
            // there, with errors BCH can correct, so continuing would only delay the others of the batch. Most
            // converge within a few iterations, so that's when to start checking
            if (iteration >= stuckStart)
            {
                unsatisfied(counts);
                bool converged = true;
                bool othersFinished = true;

                for (int l = 0; l < minInt(nbBlocks, V::W); l++)
                {
                    if (counts[l] == 0) {
                        continue;
                    }

                    converged = false;
                    stable[l] = (counts[l] <= stuckChecks) && (counts[l] == previous[l]) ? stable[l] + 1 : 0;
                    previous[l] = counts[l];
                    othersFinished = othersFinished && ((((quick >> l) & 1) != 0) || (stable[l] >= stuckIterations));
                }

                if (othersFinished && (othersDone < 0)) {
                    othersDone = iteration;
                }

                const bool giveUp = (othersDone >= 0) && (iteration >= maxInt(quickIterations, quickFactor * othersDone));
                stuck = !converged;

                for (int l = 0; l < minInt(nbBlocks, V::W); l++)
                {
                    if (counts[l] != 0) {
                        stuck = stuck && ((giveUp && (((quick >> l) & 1) != 0)) || (stable[l] >= stuckIterations));
                    }
                }

                if (converged || stuck) {
                    break;
                }
            }
            else if (satisfied(laneMask))
            {
                break;
            }
            else if ((othersDone < 0) && (quick != 0) && satisfied(others))
            {
                othersDone = iteration;
            }

            // The first iteration's messages from the checks are all 0, so aren't cleared first: each check's are
            // only read by its own update, before it writes them
            if (iteration == 0)
            {
                for (int c = 0; c < nbChecks; c++) {
                    updateCheck<true>(app, c2v + start[c], vars + start[c], start[c + 1] - start[c]);
                }
            }
            else
            {
                for (int c = 0; c < nbChecks; c++) {
                    updateCheck<false>(app, c2v + start[c], vars + start[c], start[c + 1] - start[c]);
                }
            }
        }

        // Codewords not satisfying the parity checks: those counted when found stuck, or at the limit, counted now
        int failed = 0;

        if (!stuck && (iteration == maxIterations)) {
            unsatisfied(counts);
        }

        if (stuck || (iteration == maxIterations))
        {
            for (int l = 0; l < minInt(nbBlocks, V::W); l++) {
                failed += counts[l] != 0;
            }
        }

        if (failed > 0) {
            iteration = maxIterations + 1;
        }

        if (unconverged) {
            *unconverged = failed;
        }

        store(blocks, nbBlocks);
        return iteration;
    }

private:
    // Variable to check messages: the variables' LLRs without this check's last messages (none in the first iteration:
    // the same as subtracting 0)
    template <bool first>
    inline void extrinsic(const T *app, const T *c2v, const int *vars, int degree, T *t) const
    {
        const T floor = V::set1(-127);

        for (int j = 0; j < degree; j++)
        {
            if constexpr (first) {
                (void) c2v;
                t[j] = V::max(app[vars[j]], floor);
            } else {
                t[j] = V::max(V::subs(app[vars[j]], c2v[j]), floor);
            }
        }
    }

    // a box-plus b, approximately: the smaller magnitude, with the sign of the product, corrected by
    // log(1 + exp(-|a + b|)) - log(1 + exp(-|a - b|)) rounded to a unit
    static inline T boxplus(T a, T b)
    {
        const T floor = V::set1(-127);
        const T four = V::set1(4);
        const T one = V::set1(1);
        const T minusOne = V::set1(-1);
        T m = V::min(V::abs(a), V::abs(b));
        T x = V::sign(V::sign(m, a), b);
        T apb = V::abs(V::max(V::adds(a, b), floor));
        T amb = V::abs(V::max(V::subs(a, b), floor));
        T pc = V::and_(V::and_(V::cmpgt(four, apb), V::cmpgt(amb, V::adds(apb, apb))), one);
        T nc = V::and_(V::and_(V::cmpgt(four, amb), V::cmpgt(apb, V::adds(amb, amb))), minusOne);
        return V::adds(x, V::or_(pc, nc));
    }

    // Each variable's message is the box-plus of the others', from a prefix and a suffix. The messages are limited,
    // so the sums of them in the variables rarely saturate: without that, many codewords diverge
    template <bool first>
    inline void updateCheck(T *app, T *c2v, const int *vars, int degree)
    {
        const T floor = V::set1(-127);
        const T limit = V::set1((char) messageLimit);
        const T minusLimit = V::set1((char) -messageLimit);
        T *t = m_t;
        T *prefix = m_prefix;
        auto out = [&](int j, T message) {
            message = V::max(V::min(message, limit), minusLimit);
            c2v[j] = message;
            app[vars[j]] = V::max(V::adds(t[j], message), floor);
        };

        extrinsic<first>(app, c2v, vars, degree, t);
        T pre = t[0];

        for (int j = 1; j < degree - 1; j++)
        {
            prefix[j] = pre;
            pre = boxplus(pre, t[j]);
        }

        T suf = t[degree - 1];
        out(degree - 1, pre);

        for (int j = degree - 2; j > 0; j--)
        {
            T message = boxplus(prefix[j], suf);
            suf = boxplus(suf, t[j]);
            out(j, message);
        }

        out(0, suf);
    }

    // Number of unsatisfied parity checks of each lane, up to 255
    void unsatisfied(uint8_t *counts) const
    {
        const int nbChecks = m_nbChecks;
        const int *start = m_start;
        const int *vars = m_vars;
        const T *app = m_app;
        const T zero = V::zero();
        const T one = V::set1(1);
        T count = V::zero();

        for (int c = 0; c < nbChecks; c++)
        {
            T x = V::zero();

            for (int j = start[c]; j < start[c + 1]; j++) {
                x = V::xor_(x, app[vars[j]]);
            }

            count = V::addsu(count, V::and_(V::cmpgt(zero, x), one));
        }

        V::store(counts, count);
    }

    // All parity checks satisfied by the hard decisions, in the lanes used
    bool satisfied(uint64_t laneMask) const
    {
        const int nbChecks = m_nbChecks;
        const int *start = m_start;
        const int *vars = m_vars;
        const T *app = m_app;
        T failed = V::zero();

        for (int c = 0; c < nbChecks; c++)
        {
            T x = V::zero();

            for (int j = start[c]; j < start[c + 1]; j++) {
                x = V::xor_(x, app[vars[j]]);
            }

            failed = V::or_(failed, x);

            if (((c & 255) == 255) && (V::movemask(failed) & laneMask)) {
                return false;
            }
        }

        return (V::movemask(failed) & laneMask) == 0;
    }

#if defined(DVBT2_LDPC_X86)
    // Interleave the blocks' LLRs, 16 lanes and 16 bits at a time
    void load(int8_t *const *blocks, int nbBlocks)
    {
        int8_t *app8 = reinterpret_cast<int8_t *>(m_app);
        const int n = m_n;
        __m128i r[16];

        for (int base = 0; base < V::W; base += 16)
        {
            const int8_t *rows[16];

            for (int l = 0; l < 16; l++) {
                rows[l] = base + l < nbBlocks ? blocks[base + l] : m_filler;
            }

            int v = 0;

            for (; v + 16 <= n; v += 16)
            {
                for (int l = 0; l < 16; l++) {
                    r[l] = _mm_loadu_si128(reinterpret_cast<const __m128i *>(rows[l] + v));
                }

                transpose16x16(r);

                for (int k = 0; k < 16; k++) {
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(app8 + (size_t) (v + k) * V::W + base), r[k]);
                }
            }

            for (; v < n; v++)
            {
                for (int l = 0; l < 16; l++) {
                    app8[(size_t) v * V::W + base + l] = rows[l][v];
                }
            }
        }
    }

    void store(int8_t *const *blocks, int nbBlocks) const
    {
        const int8_t *app8 = reinterpret_cast<const int8_t *>(m_app);
        const int n = m_n;
        __m128i r[16];

        for (int base = 0; base < minInt(nbBlocks, V::W); base += 16)
        {
            const int nbLanes = minInt(16, nbBlocks - base);
            int v = 0;

            for (; v + 16 <= n; v += 16)
            {
                for (int k = 0; k < 16; k++) {
                    r[k] = _mm_loadu_si128(reinterpret_cast<const __m128i *>(app8 + (size_t) (v + k) * V::W + base));
                }

                transpose16x16(r);

                for (int l = 0; l < nbLanes; l++) {
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(blocks[base + l] + v), r[l]);
                }
            }

            for (; v < n; v++)
            {
                for (int l = 0; l < nbLanes; l++) {
                    blocks[base + l][v] = app8[(size_t) v * V::W + base + l];
                }
            }
        }
    }

#else
    // Interleave the blocks' LLRs
    void load(int8_t *const *blocks, int nbBlocks)
    {
        int8_t *app8 = reinterpret_cast<int8_t *>(m_app);
        const int n = m_n;

        for (int l = 0; l < V::W; l++)
        {
            const int8_t *row = l < nbBlocks ? blocks[l] : m_filler;

            for (int v = 0; v < n; v++) {
                app8[(size_t) v * V::W + l] = row[v];
            }
        }
    }

    void store(int8_t *const *blocks, int nbBlocks) const
    {
        const int8_t *app8 = reinterpret_cast<const int8_t *>(m_app);
        const int n = m_n;

        for (int l = 0; l < minInt(nbBlocks, V::W); l++)
        {
            for (int v = 0; v < n; v++) {
                blocks[l][v] = app8[(size_t) v * V::W + l];
            }
        }
    }
#endif

    static const int messageLimit = 31;
    static const int stuckStart = 8;        //!< Iterations before looking for stuck codewords
    static const int stuckChecks = 24;      //!< A codeword with up to this many unsatisfied checks...
    static const int stuckIterations = 4;   //!< ...unchanged for this many iterations is stuck
    static const int quickIterations = 10;  //!< A quick block not converged by then...
    static const int quickFactor = 3;       //!< ...nor this many times the iterations the others took is given up

    const int m_n;
    const int m_nbChecks;
    const int *const m_start;
    const int *const m_vars;
    const int8_t *const m_filler;   //!< For lanes without a block: a codeword of zeros
    T *const m_app;                 //!< Each variable's LLR
    T *const m_c2v;                 //!< Each edge's message from its check
    T *const m_t;                   //!< Variable to check messages of the check being updated
    T *const m_prefix;
};

//! For LDPCKernel::m_decode
template <class V>
int decodeLDPC(const LDPCKernelData& data, int8_t *const *blocks, int nbBlocks, int maxIterations, int *unconverged,
    uint64_t quickBlocks)
{
    LDPCKernelImpl<V> kernel(data);
    return kernel.decode(blocks, nbBlocks, maxIterations, unconverged, quickBlocks);
}

} // namespace
} // namespace dvbt2

#endif // MODEMDVB_DVBT2LDPCKERNEL_H
