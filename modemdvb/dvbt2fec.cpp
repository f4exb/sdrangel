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
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <QDebug>

#include "dvbt2fec.h"
#include "dvbt2ldpc.h"

#include "cpufeatures.h"

namespace dvbt2 {

// BCH generator polynomial factors (table 7), as the powers of x with non-zero coefficients
const std::vector<std::vector<int>> BCH::m_bchNormalPolys = {
    {0, 2, 3, 5, 16},
    {0, 1, 4, 5, 6, 8, 16},
    {0, 2, 3, 4, 5, 7, 8, 9, 10, 11, 16},
    {0, 2, 4, 6, 9, 11, 12, 14, 16},
    {0, 1, 2, 3, 5, 8, 9, 10, 11, 12, 16},
    {0, 2, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 16},
    {0, 2, 5, 6, 8, 9, 10, 11, 13, 15, 16},
    {0, 1, 2, 5, 6, 8, 9, 12, 13, 14, 16},
    {0, 5, 7, 9, 10, 11, 16},
    {0, 1, 2, 5, 7, 8, 10, 12, 13, 14, 16},
    {0, 2, 3, 5, 9, 11, 12, 13, 16},
    {0, 1, 5, 6, 7, 9, 11, 12, 16},
};

// EN 302 307-2 table 7: the medium FECFRAME's (32 400 bits), GF(2^15). Checked: g1 primitive, each g_i's root
// alpha^(2i - 1)
const std::vector<std::vector<int>> BCH::m_bchMediumPolys = {
    {0, 2, 3, 5, 15},
    {0, 1, 4, 7, 10, 11, 15},
    {0, 2, 4, 6, 8, 10, 12, 13, 15},
    {0, 2, 3, 5, 6, 8, 10, 11, 15},
    {0, 1, 2, 4, 6, 7, 10, 12, 15},
    {0, 4, 6, 7, 12, 13, 15},
    {0, 2, 4, 5, 7, 11, 12, 14, 15},
    {0, 2, 4, 6, 8, 9, 11, 14, 15},
    {0, 1, 2, 4, 5, 7, 9, 11, 12, 13, 15},
    {0, 1, 2, 3, 4, 7, 10, 11, 12, 13, 15},
    {0, 1, 2, 4, 9, 11, 15},
    {0, 2, 4, 8, 10, 11, 13, 14, 15},
};

const std::vector<std::vector<int>> BCH::m_bchShortPolys = {
    {0, 1, 3, 5, 14},
    {0, 6, 8, 11, 14},
    {0, 1, 2, 6, 9, 10, 14},
    {0, 4, 7, 8, 10, 12, 14},
    {0, 2, 4, 6, 8, 9, 11, 13, 14},
    {0, 3, 7, 8, 9, 13, 14},
    {0, 2, 5, 6, 7, 10, 11, 13, 14},
    {0, 5, 8, 9, 10, 11, 14},
    {0, 1, 2, 3, 9, 10, 14},
    {0, 3, 6, 9, 11, 12, 14},
    {0, 4, 11, 12, 14},
    {0, 1, 2, 3, 5, 6, 7, 8, 10, 13, 14},
};

BCH::BCH(FECType type, int t) :
    m_m(type == FECType::Normal ? 16 : type == FECType::Medium ? 15 : 14),
    m_t(t),
    m_parityBits(m_m * t),
    m_parityBytes((m_parityBits + 7) / 8)
{
    if ((t < 1) || (t > m_maxT)) { // The tables' polynomials, and the remainder's and decoder's sizes
        throw std::invalid_argument("BCH: t out of range");
    }

    const std::vector<std::vector<int>>& polys = type == FECType::Normal ? m_bchNormalPolys
        : type == FECType::Medium ? m_bchMediumPolys : m_bchShortPolys;

    // Generator polynomial: product of the first t polynomials. g[i] is the coefficient of x^i
    std::vector<uint8_t> g(1, 1);

    for (int j = 0; j < t; j++)
    {
        std::vector<uint8_t> product(g.size() + m_m, 0);

        for (size_t a = 0; a < g.size(); a++)
        {
            if (g[a])
            {
                for (int b : polys[j]) {
                    product[a + b] ^= 1;
                }
            }
        }

        g = product;
    }

    // Byte at a time division: remainder of byte(x) x^P mod g(x), left aligned, the MSB of the first word the
    // coefficient of x^(P-1). That's the slice for the last of 8 bytes
    m_slices.assign(8 * 256 * m_remWords, 0);
    uint64_t *last = &m_slices[7 * 256 * m_remWords];

    for (int v = 0; v < 256; v++)
    {
        std::vector<uint8_t> reg(m_parityBits, 0); // reg[i]: coefficient of x^i

        for (int b = 7; b >= 0; b--)
        {
            uint8_t feedback = reg[m_parityBits - 1] ^ ((v >> b) & 1);

            for (int i = m_parityBits - 1; i > 0; i--) {
                reg[i] = reg[i - 1] ^ (feedback & g[i]);
            }

            reg[0] = feedback & g[0];
        }

        for (int i = 0; i < m_parityBits; i++)
        {
            if (reg[i])
            {
                const int j = m_parityBits - 1 - i; // From the MSB
                last[v * m_remWords + j / 64] |= 1ULL << (63 - j % 64);
            }
        }
    }

    // A byte followed by 7 - k zero bytes: its remainder, then a zero byte at a time
    for (int k = 6; k >= 0; k--)
    {
        const uint64_t *next = &m_slices[(k + 1) * 256 * m_remWords];
        uint64_t *slice = &m_slices[k * 256 * m_remWords];

        for (int v = 0; v < 256; v++)
        {
            const uint64_t *r = &next[v * m_remWords];
            const uint64_t *row = &last[(r[0] >> 56) * m_remWords];
            slice[v * m_remWords] = ((r[0] << 8) | (r[1] >> 56)) ^ row[0];
            slice[v * m_remWords + 1] = ((r[1] << 8) | (r[2] >> 56)) ^ row[1];
            slice[v * m_remWords + 2] = (r[2] << 8) ^ row[2];
        }
    }

    // GF(2^m) from the first polynomial, which is primitive
    const int q = (1 << m_m) - 1;
    int primitive = 0;

    for (int b : polys[0]) {
        primitive |= 1 << b;
    }

    m_exp.resize(2 * q);
    m_log.assign(q + 1, 0);
    int x = 1;

    for (int i = 0; i < q; i++)
    {
        m_exp[i] = x;
        m_exp[i + q] = x;
        m_log[x] = i;
        x <<= 1;

        if (x & (1 << m_m)) {
            x ^= primitive;
        }
    }
}

// The bytes MSB first
inline uint64_t BCH::loadBigEndian64(const uint8_t *p)
{
    uint64_t v;
    std::memcpy(&v, p, 8);
#if defined(_MSC_VER)
    return _byteswap_uint64(v);
#elif defined(__GNUC__)
    return __builtin_bswap64(v);
#else
    v = 0;

    for (int i = 0; i < 8; i++) {
        v = (v << 8) | p[i];
    }

    return v;
#endif
}

// 8 bytes at a time: the register's top 64 bits plus the next 8 bytes, as each byte would be at its place in them,
// with the rest of the register shifted up 64 (linear, so the same as a byte at a time)
void BCH::remainder(const uint8_t *data, int nbBytes, uint8_t *rem) const
{
    const uint64_t *slices = m_slices.data();
    const uint64_t *last = &slices[7 * 256 * m_remWords];
    uint64_t r0 = 0, r1 = 0, r2 = 0;
    int i = 0;

    for (; i + 8 <= nbBytes; i += 8)
    {
        const uint64_t x = r0 ^ loadBigEndian64(data + i);
        r0 = r1;
        r1 = r2;
        r2 = 0;

        for (int k = 0; k < 8; k++)
        {
            const uint64_t *row = &slices[(k * 256 + ((x >> (56 - 8 * k)) & 0xff)) * m_remWords];
            r0 ^= row[0];
            r1 ^= row[1];
            r2 ^= row[2];
        }
    }

    for (; i < nbBytes; i++)
    {
        const uint64_t *row = &last[((r0 >> 56) ^ data[i]) * m_remWords];
        r0 = ((r0 << 8) | (r1 >> 56)) ^ row[0];
        r1 = ((r1 << 8) | (r2 >> 56)) ^ row[1];
        r2 = (r2 << 8) ^ row[2];
    }

    const uint64_t words[m_remWords] = {r0, r1, r2};

    for (int j = 0; j < m_parityBytes; j++) {
        rem[j] = (uint8_t) (words[j / 8] >> (56 - 8 * (j % 8)));
    }
}

void BCH::encode(const uint8_t *msg, int msgBytes, uint8_t *parity) const
{
    remainder(msg, msgBytes, parity);
}

// Leading zeros don't change the remainder: the message shifted along to end on a byte
void BCH::encodeBits(const uint8_t *msg, int msgBits, uint8_t *parity) const
{
    const int pad = (8 - msgBits % 8) % 8;

    if (pad == 0)
    {
        remainder(msg, msgBits / 8, parity);
        return;
    }

    std::vector<uint8_t> shifted((msgBits + pad) / 8, 0);

    for (int i = 0; i < msgBits; i++)
    {
        if ((msg[i / 8] >> (7 - i % 8)) & 1) {
            shifted[(i + pad) / 8] |= 0x80 >> ((i + pad) % 8);
        }
    }

    remainder(shifted.data(), (int) shifted.size(), parity);
}

int BCH::decode(uint8_t *codeword, int codewordBytes) const
{
    uint8_t rem[8 * m_remWords];
    remainder(codeword, codewordBytes, rem);
    bool zero = true;

    for (int i = 0; i < m_parityBytes; i++) {
        zero = zero && !rem[i];
    }

    if (zero) {
        return 0;
    }

    // rem(x) = c(x) x^P mod g(x), so the syndromes are S_j = c(a^j) = rem(a^j) a^(-jP)
    const int q = (1 << m_m) - 1;
    // On the stack, as threads decode at once, and small (t is at most 12)
    int s[2 * m_maxT + 1] = {};

    // rem(a^j) for each j, a set bit at a time: a^(j bitIndex), its log stepping by bitIndex (less than q)
    for (int i = 0; i < m_parityBits; i++)
    {
        if ((rem[i / 8] >> (7 - (i % 8))) & 1)
        {
            const int bitIndex = m_parityBits - 1 - i; // Power of x
            int power = bitIndex;

            for (int j = 1; j <= 2 * m_t; j++)
            {
                s[j] ^= m_exp[power];
                power += bitIndex;

                if (power >= q) {
                    power -= q;
                }
            }
        }
    }

    for (int j = 1; j <= 2 * m_t; j++)
    {
        int shift = (int) ((q - ((int64_t) j * m_parityBits) % q) % q);
        s[j] = mul(s[j], m_exp[shift]);
    }

    // Berlekamp-Massey
    const int size = 2 * m_t + 2;
    int c[2 * m_maxT + 2] = {}, b[2 * m_maxT + 2] = {}, tmp[2 * m_maxT + 2] = {};
    c[0] = 1;
    b[0] = 1;
    int l = 0, m = 1, bValue = 1;

    for (int n = 0; n < 2 * m_t; n++)
    {
        int d = s[n + 1];

        for (int i = 1; i <= l; i++) {
            d ^= mul(c[i], s[n + 1 - i]);
        }

        if (d == 0)
        {
            m++;
        }
        else
        {
            int coef = mul(d, m_exp[(q - m_log[bValue]) % q]);
            std::copy(c, c + size, tmp);

            for (int i = 0; i + m < size; i++) {
                c[i + m] ^= mul(coef, b[i]);
            }

            if (2 * l <= n)
            {
                l = n + 1 - l;
                std::copy(tmp, tmp + size, b);
                bValue = d;
                m = 1;
            }
            else
            {
                m++;
            }
        }
    }

    // Lambda (c, of degree at most l) needs l roots: one of lower degree hasn't that many, so the search can't succeed
    if ((l > m_t) || ((l > 0) && (c[l] == 0))) {
        return -1;
    }

    // Chien search over the shortened code: error at power e if Lambda(a^-e) = 0. Each non-zero term c_k a^-ke as its
    // log, less k each step. Lambda has at most l roots, so the search stops at the l'th
    const int nbBits = codewordBytes * 8;
    int logs[m_maxT] = {}, steps[m_maxT] = {}, positions[m_maxT] = {};
    int terms = 0, found = 0;

    for (int k = 1; k <= l; k++)
    {
        if (c[k])
        {
            logs[terms] = m_log[c[k]];
            steps[terms] = k;
            terms++;
        }
    }

    for (int e = 0; (e < nbBits) && (found < l); e++)
    {
        int sum = c[0];

        for (int i = 0; i < terms; i++)
        {
            sum ^= m_exp[logs[i]];
            logs[i] -= steps[i];

            if (logs[i] < 0) {
                logs[i] += q;
            }
        }

        if (sum == 0) {
            positions[found++] = nbBits - 1 - e;
        }
    }

    if (found != l) {
        return -1;
    }

    for (int i = 0; i < found; i++) {
        codeword[positions[i] / 8] ^= 0x80 >> (positions[i] % 8);
    }

    return l;
}

// A code's parity check matrix: the variables (codeword bits) of each check, information bits first, then parity.
// It doesn't change, so the decoders of a code (each thread's) share it
struct LDPCGraph
{
    int m_n = 0;
    int m_k = 0;
    std::vector<int> m_checkStart;  //!< Index into m_vars of each check's variables, and one past the last
    std::vector<int> m_vars;
    int m_maxDegree = 0;
    std::vector<int8_t> m_filler;   //!< m_n of 127: a codeword of zeros, for the kernel's lanes without a block
};

struct LDPC::Private
{
    std::shared_ptr<const LDPCGraph> m_graph;
    const LDPCKernel *m_kernel = nullptr;
    size_t m_bytes = 0;
};

// A thread's buffers for decoding: its messages, a vector for each of the graph's edges (about 7 MB for a normal
// frame's code with AVX2), and the variables'. Only needed during LDPC::decode(), so each thread's decoders share them,
// whatever their codes, rather than each keeping its own
class LDPCScratch
{
public:
    //! bytes, aligned to 64 (not cleared)
    uint8_t *get(size_t bytes)
    {
        if (bytes > m_bytes)
        {
            m_memory.reset(); // Before the next, so they aren't both held
            m_bytes = 0;
            m_memory.reset(new uint8_t[bytes + 63]);
            m_bytes = bytes;
        }

        const uintptr_t p = reinterpret_cast<uintptr_t>(m_memory.get());
        return reinterpret_cast<uint8_t *>((p + 63) & ~(uintptr_t) 63);
    }

    static thread_local LDPCScratch m_scratch;  //!< The calling thread's

private:
    std::unique_ptr<uint8_t[]> m_memory;
    size_t m_bytes = 0;
};

thread_local LDPCScratch LDPCScratch::m_scratch;

size_t LDPC::align64(size_t bytes)
{
    return (bytes + 63) & ~(size_t) 63;
}

// DVBT2_LDPC in the environment can choose the decoder, for testing: generic, or on x86, sse41
const LDPCKernel& LDPC::chooseLDPCKernel()
{
    const char *env = getenv("DVBT2_LDPC");
    const std::string choice = env ? env : "";

    if (choice == "generic") {
        return ldpcKernelGeneric;
    }

#if defined(DVBT2_LDPC_X86)
    if ((choice != "sse41") && modemdvb::cpuHasAVX2()) {
        return ldpcKernelAVX2;
    }

    if (modemdvb::cpuHasSSE41()) {
        return ldpcKernelSSE41;
    }
#elif defined(DVBT2_LDPC_NEON)
    return ldpcKernelNEON;
#endif

    return ldpcKernelGeneric;
}

// Calls f(j, addresses, count) for each information bit j, with the parity addresses (6.1.2) it is accumulated at:
// those in the table for the first bit of each group of 360, then x + m * Qldpc mod (n - k) for the m'th
template <class F>
void LDPC::forEachInfoBit(const LDPCTable& table, F f)
{
    const int checks = table.m_n - table.m_k;
    const int q = checks / 360;
    const uint16_t *row = table.m_rows;
    int addresses[32];

    for (int group = 0; group < table.m_k / 360; group++)
    {
        const int degree = *row++;

        for (int d = 0; d < degree; d++) {
            addresses[d] = row[d];
        }

        for (int m = 0; m < 360; m++)
        {
            f(group * 360 + m, addresses, degree);

            for (int d = 0; d < degree; d++)
            {
                addresses[d] += q;

                if (addresses[d] >= checks) {
                    addresses[d] -= checks;
                }
            }
        }

        row += degree;
    }
}

// Each check's variables: the information bits whose parity addresses (6.1.2) include it, then parity bits p_(i-1)
// and p_i, the accumulation
void LDPC::buildGraph(const LDPCTable& table, LDPCGraph& graph)
{
    const int n = table.m_n;
    const int k = table.m_k;
    const int checks = n - k;
    std::vector<int> degree(checks, 0);

    forEachInfoBit(table, [&](int, const int *addresses, int count) {
        for (int d = 0; d < count; d++) {
            degree[addresses[d]]++;
        }
    });

    graph.m_n = n;
    graph.m_k = k;
    graph.m_checkStart.resize(checks + 1);
    graph.m_checkStart[0] = 0;
    graph.m_maxDegree = 0;

    for (int i = 0; i < checks; i++)
    {
        const int d = degree[i] + (i > 0 ? 2 : 1);
        graph.m_checkStart[i + 1] = graph.m_checkStart[i] + d;
        graph.m_maxDegree = std::max(graph.m_maxDegree, d);
    }

    graph.m_vars.resize(graph.m_checkStart[checks]);
    std::vector<int> fill(graph.m_checkStart.begin(), graph.m_checkStart.end() - 1);

    forEachInfoBit(table, [&](int j, const int *addresses, int count) {
        for (int d = 0; d < count; d++) {
            graph.m_vars[fill[addresses[d]]++] = j;
        }
    });

    for (int i = 0; i < checks; i++)
    {
        if (i > 0) {
            graph.m_vars[fill[i]++] = k + i - 1;
        }

        graph.m_vars[fill[i]++] = k + i;
    }

    graph.m_filler.assign(n, 127);
}

// The code's graph: made by the first of its decoders, and shared with the others while any is left. Under a mutex, so
// threads wanting the same new code wait for the one making it, rather than each making its own
std::shared_ptr<const LDPCGraph> LDPC::sharedGraph(const LDPCTable& table)
{
    static std::mutex mutex;
    static std::map<const LDPCTable *, std::weak_ptr<const LDPCGraph>> graphs;
    std::lock_guard<std::mutex> lock(mutex);
    std::weak_ptr<const LDPCGraph>& entry = graphs[&table];
    std::shared_ptr<const LDPCGraph> graph = entry.lock();

    if (!graph)
    {
        std::shared_ptr<LDPCGraph> made(new LDPCGraph());
        buildGraph(table, *made);
        graph = made;
        entry = graph;
    }

    return graph;
}

LDPC::LDPC(FECType type, CodeRate rate, CodeFamily family) :
    m_private(new Private())
{
    m_table = ldpcTable(type, rate, family);

    if (!m_table) { // Not a valid combination: use the family's rate 1/2 of the frame size
        m_table = ldpcTable(type, CodeRate::R1_2, family);
    }

    if (!m_table) { // There's none (the medium frame's)
        throw std::invalid_argument("LDPC: no such code");
    }

    m_n = m_table->m_n;
    m_k = m_table->m_k;

    m_private->m_graph = sharedGraph(*m_table);
    m_private->m_kernel = &chooseLDPCKernel();
    const LDPCGraph& graph = *m_private->m_graph;
    m_private->m_bytes = (graph.m_vars.size() + graph.m_checkStart.size()) * sizeof(int) + graph.m_filler.size();
}

LDPC::~LDPC()
{
}

int LDPC::lanes() const
{
    return m_private->m_kernel->m_lanes;
}

size_t LDPC::memoryBytes() const
{
    return m_private->m_bytes;
}

// 6.1.2: accumulate each information bit at the parity addresses of the tables, then p_i ^= p_(i-1)
void LDPC::encode(const uint8_t *bits, uint8_t *parity) const
{
    std::fill(parity, parity + m_n - m_k, 0);

    forEachInfoBit(*m_table, [&](int j, const int *addresses, int count) {
        if (bits[j])
        {
            for (int d = 0; d < count; d++) {
                parity[addresses[d]] ^= 1;
            }
        }
    });

    for (int i = 1; i < m_n - m_k; i++) {
        parity[i] ^= parity[i - 1];
    }
}

int LDPC::decode(int8_t *const *blocks, int nbBlocks, int maxIterations, int *unconverged, uint64_t quickBlocks)
{
    const LDPCGraph& graph = *m_private->m_graph;
    const LDPCKernel& kernel = *m_private->m_kernel;
    const size_t vectorBytes = (size_t) kernel.m_lanes;
    const size_t appBytes = align64((size_t) graph.m_n * vectorBytes);
    const size_t c2vBytes = align64(graph.m_vars.size() * vectorBytes);
    const size_t tBytes = align64((size_t) graph.m_maxDegree * vectorBytes);
    uint8_t *memory = LDPCScratch::m_scratch.get(appBytes + c2vBytes + 2 * tBytes);

    LDPCKernelData data;
    data.m_n = graph.m_n;
    data.m_nbChecks = (int) graph.m_checkStart.size() - 1;
    data.m_checkStart = graph.m_checkStart.data();
    data.m_vars = graph.m_vars.data();
    data.m_filler = graph.m_filler.data();
    data.m_app = memory;
    data.m_c2v = memory + appBytes;
    data.m_t = memory + appBytes + c2vBytes;
    data.m_prefix = memory + appBytes + c2vBytes + tBytes;
    return kernel.m_decode(data, blocks, nbBlocks, maxIterations, unconverged, quickBlocks);
}

// Demultiplexer tables 13(a), (b) and (c), and I.3 for T2-Lite's 1/3 and 2/5: output bit for input bit number mod Nsubstreams
const int *CellMapper::demuxTable(CodeRate rate, Constellation constellation, FECType type)
{
    static const int qpskLite[2] = {0, 1};
    static const int r13_16[8] = {6, 0, 3, 4, 5, 2, 1, 7};
    static const int r13_64[12] = {4, 2, 0, 5, 6, 1, 3, 7, 8, 9, 10, 11};
    static const int r13_256[8] = {4, 0, 1, 2, 5, 3, 6, 7};
    static const int r25_16[8] = {7, 5, 4, 0, 3, 1, 2, 6};
    static const int r25_64[12] = {4, 0, 1, 6, 2, 3, 5, 8, 7, 10, 9, 11};
    static const int r25_256[8] = {4, 0, 5, 1, 2, 3, 6, 7};

    if ((rate == CodeRate::R1_3) || (rate == CodeRate::R2_5))
    {
        const bool third = rate == CodeRate::R1_3;

        switch (constellation)
        {
        case Constellation::QPSK: return qpskLite;
        case Constellation::QAM16: return third ? r13_16 : r25_16;
        case Constellation::QAM64: return third ? r13_64 : r25_64;
        default: return third ? r13_256 : r25_256;
        }
    }

    static const int qpsk[2] = {0, 1};
    static const int a16[8] = {7, 1, 4, 2, 5, 3, 6, 0};
    static const int a64[12] = {11, 7, 3, 10, 6, 2, 9, 5, 1, 8, 4, 0};
    static const int a256N[16] = {15, 1, 13, 3, 8, 11, 9, 5, 10, 6, 4, 7, 12, 2, 14, 0};
    static const int a256S[8] = {7, 3, 1, 5, 2, 6, 4, 0};
    static const int b16N[8] = {0, 5, 1, 2, 4, 7, 3, 6};
    static const int b64N[12] = {2, 7, 6, 9, 0, 3, 1, 8, 4, 11, 5, 10};
    static const int b256N[16] = {2, 11, 3, 4, 0, 9, 1, 8, 10, 13, 7, 14, 6, 15, 5, 12};
    static const int c256N[16] = {7, 2, 9, 0, 4, 6, 13, 3, 14, 10, 15, 5, 8, 12, 11, 1};
    const bool normal = type == FECType::Normal;

    switch (constellation)
    {
    case Constellation::QPSK:
        return qpsk;
    case Constellation::QAM16:
        return (rate == CodeRate::R3_5) && normal ? b16N : a16;
    case Constellation::QAM64:
        return (rate == CodeRate::R3_5) && normal ? b64N : a64;
    default:
        if (!normal) {
            return a256S;
        }

        return rate == CodeRate::R3_5 ? b256N : rate == CodeRate::R2_3 ? c256N : a256N;
    }
}

void cellBitMap(FECType type, CodeRate rate, Constellation constellation, std::vector<int>& map)
{
    switch (constellation)
    {
    case Constellation::QPSK:
    case Constellation::QAM16:
    case Constellation::QAM64:
    case Constellation::QAM256:
        break;
    default:
        qWarning() << "cellBitMap: Invalid constellation" << static_cast<int>(constellation);
        map.clear();
        return;
    }

    const int n = nldpc(type);
    const int k = kldpc(type, rate);
    const int qldpc = (n - k) / 360;
    const int eta = bitsPerCell(constellation);
    const bool split = !((constellation == Constellation::QPSK) || ((constellation == Constellation::QAM256) && (type == FECType::Short)));
    const int nsub = split ? 2 * eta : eta;
    const int *demux = CellMapper::demuxTable(rate, constellation, type);

    // Column twist (table 10)
    static const int t16N[8] = {0, 0, 2, 4, 4, 5, 7, 7};
    static const int t16S[8] = {0, 0, 0, 1, 7, 20, 20, 21};
    static const int t64N[12] = {0, 0, 2, 2, 3, 4, 4, 5, 5, 7, 8, 9};
    static const int t64S[12] = {0, 0, 0, 2, 2, 2, 3, 3, 3, 6, 7, 7};
    static const int t256N[16] = {0, 2, 2, 2, 2, 3, 7, 15, 16, 20, 22, 22, 27, 27, 28, 32};
    static const int t256S[8] = {0, 0, 0, 1, 7, 20, 20, 21};
    const int *twist = nullptr;
    int nc = 0;

    switch (constellation)
    {
    case Constellation::QAM16:
        twist = type == FECType::Normal ? t16N : t16S;
        nc = 8;
        break;
    case Constellation::QAM64:
        twist = type == FECType::Normal ? t64N : t64S;
        nc = 12;
        break;
    case Constellation::QAM256:
        twist = type == FECType::Normal ? t256N : t256S;
        nc = type == FECType::Normal ? 16 : 8;
        break;
    default:
        break;
    }

    const int nr = nc ? n / nc : n;
    map.resize(n);

    for (int di = 0; di < n; di++)
    {
        // Bit interleaver output index di to the codeword bit it came from
        int lambda;

        if (twist)
        {
            int row = di / nc;
            int col = di % nc;
            int u = col * nr + ((row - twist[col]) % nr + nr) % nr;
            lambda = u < k ? u : k + qldpc * ((u - k) % 360) + (u - k) / 360;
        }
        else if ((rate == CodeRate::R1_3) || (rate == CodeRate::R2_5))
        {
            // T2-Lite's QPSK at 1/3 and 2/5 has parity interleaving only (table I.2)
            lambda = di < k ? di : k + qldpc * ((di - k) % 360) + (di - k) / 360;
        }
        else
        {
            lambda = di;
        }

        // Demultiplexer: to cell and bit within the cell
        int e = demux[di % nsub];
        int word = di / nsub;
        int cell = split ? 2 * word + (e >= eta) : word;
        int bit = e % eta;
        map[cell * eta + bit] = lambda;
    }
}

float CellMapper::normalisation(int eta)
{
    static const float values[] = {1.0f, 1.0f, std::sqrt(2.0f), 1.0f, std::sqrt(10.0f), 1.0f, std::sqrt(42.0f), 1.0f, std::sqrt(170.0f)};
    return values[eta];
}

// Level of an axis with m bits (sign first), Gray mapped as table 14
int CellMapper::axisLevel(const uint8_t *bits, int stride, int m)
{
    // Invert the demapping recursion: d_k = |d_(k-1) - T_(k-1)|, bit k set when d_k < T_k
    int t = 1 << (m - 1);
    int levels = 2 * t;

    for (int level = -(levels - 1); level <= levels - 1; level += 2)
    {
        bool match = (bits[0] != 0) == (level < 0);
        int d = std::abs(level);
        int threshold = t;

        for (int k = 1; (k < m) && match; k++)
        {
            match = (bits[k * stride] != 0) == (d < threshold);
            d = std::abs(d - threshold);
            threshold /= 2;
        }

        if (match) {
            return level;
        }
    }

    return 0;
}

Cf mapCell(const uint8_t *bits, int eta)
{
    if (eta == 1) {
        return Cf(bits[0] ? -1.0f : 1.0f, 0.0f);
    }

    int m = eta / 2;
    float scale = 1.0f / CellMapper::normalisation(eta);
    return Cf(CellMapper::axisLevel(bits, 2, m) * scale, CellMapper::axisLevel(bits + 1, 2, m) * scale);
}

inline int8_t CellMapper::quantise(float value)
{
    value = std::nearbyint(value);
    return (int8_t) std::max(-llrLimit, std::min(llrLimit, value));
}

// LLRs (times 2) for the m bits of one axis, at llrs[0], llrs[stride]... value is in units of the unnormalised constellation
inline void CellMapper::demapAxis(float value, int m, float scale, int8_t *llrs, int stride)
{
    llrs[0] = quantise(scale * value);
    float d = std::fabs(value);
    float threshold = (float) (1 << (m - 1));

    for (int k = 1; k < m; k++)
    {
        llrs[k * stride] = quantise(scale * (d - threshold));
        d = std::fabs(d - threshold);
        threshold *= 0.5f;
    }
}

void demapCell(Cf cell, int eta, float llrScale, int8_t *llrs)
{
    if (eta == 1)
    {
        llrs[0] = CellMapper::quantise(llrScale * cell.real());
        return;
    }

    // In unnormalised units, the LLR (times 2) is 4 x metric / variance, and the variance scales with norm^2
    float norm = CellMapper::normalisation(eta);
    float scale = llrScale / (norm * norm);
    int m = eta / 2;
    CellMapper::demapAxis(cell.real() * norm, m, scale, llrs, 2);
    CellMapper::demapAxis(cell.imag() * norm, m, scale, llrs + 1, 2);
}

void demapCellAxes(Cf cell, int eta, float scaleI, float scaleQ, int8_t *llrs)
{
    float norm = CellMapper::normalisation(eta);
    int m = eta / 2;
    CellMapper::demapAxis(cell.real() * norm, m, scaleI / (norm * norm), llrs, 2);
    CellMapper::demapAxis(cell.imag() * norm, m, scaleQ / (norm * norm), llrs + 1, 2);
}

float rotationAngle(Constellation constellation)
{
    const float degrees = (float) (M_PI / 180.0);

    switch (constellation)
    {
    case Constellation::QPSK: return 29.0f * degrees;
    case Constellation::QAM16: return 16.8f * degrees;
    case Constellation::QAM64: return 8.6f * degrees;
    default: return std::atan(1.0f / 16.0f);
    }
}

CellInterleaver::CellInterleaver(int ncells) :
    m_ncells(ncells)
{
    m_nd = 0;

    while ((1 << m_nd) < ncells) {
        m_nd++;
    }

    m_l0.resize(ncells);
    unsigned int r = 0; // Bits nd-2..0

    for (int i = 0, q = 0; (i < (1 << m_nd)) && (q < ncells); i++)
    {
        if (i < 2) {
            r = 0;
        } else if (i == 2) {
            r = 1;
        }
        else
        {
            unsigned int bit;

            switch (m_nd)
            {
            case 11: bit = (r ^ (r >> 3)) & 1; break;
            case 12: bit = (r ^ (r >> 2)) & 1; break;
            case 13: bit = (r ^ (r >> 1) ^ (r >> 4) ^ (r >> 6)) & 1; break;
            case 14: bit = (r ^ (r >> 1) ^ (r >> 4) ^ (r >> 5) ^ (r >> 9) ^ (r >> 11)) & 1; break;
            default: bit = (r ^ (r >> 1) ^ (r >> 2) ^ (r >> 12)) & 1; break;
            }

            r = (r >> 1) | (bit << (m_nd - 2));
        }

        int value = m_nd > 0 ? (int) (((i & 1) << (m_nd - 1)) | r) : (int) r;

        if (value < ncells) {
            m_l0[q++] = value;
        }
    }

    // Every block's shift a TI-block can have, now, so shift() needn't change anything, and threads can ask at once
    int k = 0;
    m_shifts.resize(m_maxBlocks);

    for (int b = 0; b < m_maxBlocks; b++) {
        m_shifts[b] = nextShift(k);
    }

    m_nextK = k;
}

// The bit reversed counter's next value (from k, advanced), skipping those too large
int CellInterleaver::nextShift(int& k) const
{
    int reversed;

    do
    {
        reversed = 0;

        for (int j = 0; j < m_nd; j++) {
            reversed |= ((k >> j) & 1) << (m_nd - 1 - j);
        }

        k++;
    }
    while (reversed >= m_ncells);

    return reversed;
}

int CellInterleaver::shift(int r) const
{
    if (r < (int) m_shifts.size()) {
        return m_shifts[r];
    }

    // Beyond any TI-block's: worked out again each time
    int k = m_nextK;
    int reversed = 0;

    for (int i = (int) m_shifts.size(); i <= r; i++) {
        reversed = nextShift(k);
    }

    return reversed;
}

void timeInterleaver(int ncells, int nfec, std::vector<int>& read)
{
    const int nr = ncells / 5;
    const int nc = 5 * nfec;
    read.resize((size_t) ncells * nfec);

    for (int i = 0; i < ncells * nfec; i++)
    {
        int row = i / nc;
        int col = i % nc;
        int r = col / 5;
        int q = (col % 5) * nr + row;
        read[i] = r * ncells + q;
    }
}

// L1 signalling (7.3)
const int L1Codec::m_preGroups[9] = {7, 3, 6, 5, 2, 4, 1, 8, 0};
const int L1Codec::m_preParity[36] = {27, 13, 29, 32, 5, 0, 11, 21, 33, 20, 25, 28, 18, 35, 8, 3, 9, 31,
    22, 24, 7, 14, 17, 4, 2, 26, 16, 34, 19, 10, 12, 23, 1, 6, 30, 15};
const int L1Codec::m_postGroups[3][20] = {
    {18, 17, 16, 15, 14, 13, 12, 11, 4, 10, 9, 8, 3, 2, 7, 6, 5, 1, 19, 0},
    {18, 17, 16, 15, 14, 13, 12, 11, 4, 10, 9, 8, 7, 3, 2, 1, 6, 5, 19, 0},
    {18, 17, 16, 4, 15, 14, 13, 12, 3, 11, 10, 9, 2, 8, 7, 1, 6, 5, 19, 0},
};
const int L1Codec::m_postParity[3][25] = {
    {6, 4, 18, 9, 13, 8, 15, 20, 5, 17, 2, 24, 10, 22, 12, 3, 16, 23, 1, 14, 0, 21, 19, 7, 11},
    {6, 4, 13, 9, 18, 8, 15, 20, 5, 17, 2, 22, 24, 7, 12, 1, 16, 23, 14, 0, 21, 10, 19, 11, 3},
    {6, 15, 13, 10, 3, 17, 21, 8, 5, 19, 2, 23, 16, 24, 7, 18, 1, 12, 20, 0, 4, 14, 9, 11, 22},
};

int L1Codec::postTableIndex(L1Constellation constellation)
{
    return constellation == L1Constellation::QAM16 ? 1 : constellation == L1Constellation::QAM64 ? 2 : 0;
}

L1Codec::L1Codec() :
    m_preLDPC(FECType::Short, CodeRate::R1_4),
    m_postLDPC(FECType::Short, CodeRate::R1_2),
    m_bch(FECType::Short, 12),
    m_postBlock()
{
    layout(m_preKsig, m_preKbch, m_preKldpc, 16200 - m_preKldpc, m_preNpunc, m_preGroups, m_preParity, m_preBlock);
}

L1Codec::~L1Codec()
{
}

// Shortening (7.3.2.2) and puncturing (7.3.2.5): which codeword bits carry information and which are sent
void L1Codec::layout(int ksig, int kbchL1, int kldpcL1, int nparity, int npunc, const int *groupOrder,
    const int *parityOrder, Block& block)
{
    const int ngroup = kldpcL1 / 360;
    const int qldpc = nparity / 360;
    std::vector<uint8_t> padded(kbchL1, 0);

    auto groupEnd = [kbchL1](int g) { return std::min(360 * (g + 1), kbchL1); };
    int npad = ksig < 360 ? ngroup - 1 : (kbchL1 - ksig) / 360;

    for (int j = 0; j < npad; j++)
    {
        for (int i = 360 * groupOrder[j]; i < groupEnd(groupOrder[j]); i++) {
            padded[i] = 1;
        }
    }

    int partialGroup = npad == ngroup - 1 ? groupOrder[ngroup - 1] : groupOrder[npad];
    int partialBits = npad == ngroup - 1 ? 360 - ksig : kbchL1 - ksig - 360 * npad;

    for (int i = groupEnd(partialGroup) - partialBits; i < groupEnd(partialGroup); i++) {
        padded[i] = 1;
    }

    std::vector<uint8_t> punctured(nparity, 0);
    int puncGroups = npunc / 360;

    for (int j = 0; j < puncGroups; j++)
    {
        for (int s = 0; s < 360; s++) {
            punctured[parityOrder[j] + qldpc * s] = 1;
        }
    }

    if (puncGroups < qldpc)
    {
        for (int s = 0; s < npunc - 360 * puncGroups; s++) {
            punctured[parityOrder[puncGroups] + qldpc * s] = 1;
        }
    }

    block.m_ksig = ksig;
    block.m_npunc = npunc;
    block.m_info.clear();
    block.m_transmitted.clear();

    for (int i = 0; i < kbchL1; i++)
    {
        if (!padded[i]) {
            block.m_info.push_back(i);
        }
    }

    block.m_transmitted = block.m_info;

    for (int i = kbchL1; i < kldpcL1; i++) {
        block.m_transmitted.push_back(i);
    }

    for (int i = 0; i < nparity; i++)
    {
        if (!punctured[i]) {
            block.m_transmitted.push_back(kldpcL1 + i);
        }
    }
}

void L1Codec::packBits(const uint8_t *bits, int nbBits, uint8_t *bytes)
{
    for (int i = 0; i < nbBits / 8; i++)
    {
        uint8_t v = 0;

        for (int b = 0; b < 8; b++) {
            v = (uint8_t) ((v << 1) | (bits[8 * i + b] & 1));
        }

        bytes[i] = v;
    }
}

// Encode one block: info bits at the info positions of an all zero codeword, then BCH and LDPC
void L1Codec::encodeL1Block(const uint8_t *info, int ksig, const std::vector<int>& infoPositions, int kbchL1, int kldpcL1,
    const BCH& bch, const LDPC& ldpc, std::vector<uint8_t>& codeword)
{
    codeword.assign(ldpc.n(), 0);

    for (int i = 0; i < ksig; i++) {
        codeword[infoPositions[i]] = info[i];
    }

    std::vector<uint8_t> msg(kbchL1 / 8), parity(bch.parityBits() / 8);
    packBits(codeword.data(), kbchL1, msg.data());
    bch.encode(msg.data(), (int) msg.size(), parity.data());

    for (int i = 0; i < bch.parityBits(); i++) {
        codeword[kbchL1 + i] = (parity[i / 8] >> (7 - (i % 8))) & 1;
    }

    ldpc.encode(codeword.data(), codeword.data() + kldpcL1);
}

void L1Codec::encodePre(const uint8_t *bits, std::vector<Cf>& cells)
{
    Block block;
    layout(m_preKsig, m_preKbch, m_preKldpc, 16200 - m_preKldpc, m_preNpunc, m_preGroups, m_preParity, block);
    std::vector<uint8_t> codeword;
    encodeL1Block(bits, m_preKsig, block.m_info, m_preKbch, m_preKldpc, m_bch, m_preLDPC, codeword);
    cells.resize(block.m_transmitted.size());

    for (size_t i = 0; i < cells.size(); i++) {
        cells[i] = Cf(codeword[block.m_transmitted[i]] ? -1.0f : 1.0f, 0.0f);
    }
}

bool L1Codec::decodeBlock(const Block& block, LDPC& ldpc, const BCH& bch, int kbchL1, const int8_t *llrs, std::vector<uint8_t>& bits)
{
    std::vector<int8_t> codeword(ldpc.n(), 0); // Punctured bits are unknown

    for (int i = 0; i < kbchL1; i++) {
        codeword[i] = 127; // Padding is known to be zero
    }

    for (size_t i = 0; i < block.m_transmitted.size(); i++) {
        codeword[block.m_transmitted[i]] = llrs[i];
    }

    int8_t *blocks[1] = {codeword.data()};
    ldpc.decode(blocks, 1, 50);

    std::vector<uint8_t> hard(ldpc.k());

    for (int i = 0; i < ldpc.k(); i++) {
        hard[i] = codeword[i] < 0 ? 1 : 0;
    }

    std::vector<uint8_t> packed(ldpc.k() / 8);
    packBits(hard.data(), ldpc.k(), packed.data());

    if (bch.decode(packed.data(), (int) packed.size()) < 0) {
        return false;
    }

    bits.resize(block.m_ksig);

    for (int i = 0; i < block.m_ksig; i++)
    {
        int p = block.m_info[i];
        bits[i] = (packed[p / 8] >> (7 - (p % 8))) & 1;
    }

    return true;
}

bool L1Codec::decodePre(const Cf *cells, const float *llrScales, L1Pre& pre)
{
    const Block& block = m_preBlock;
    std::vector<int8_t> llrs(block.m_transmitted.size());

    for (size_t i = 0; i < llrs.size(); i++) {
        llrs[i] = CellMapper::quantise(llrScales[i] * cells[i].real());
    }

    std::vector<uint8_t> bits;

    if (!decodeBlock(block, m_preLDPC, m_bch, m_preKbch, llrs.data(), bits)) {
        return false;
    }

    return pre.decode(bits.data());
}

// L1-post block sizes (7.3.1.2)
struct PostLayout
{
    int m_blocks;
    int m_ksig;
    int m_kpost;
    int m_npost;
    int m_npunc;
};

PostLayout L1Codec::postLayout(int infoBits, L1Constellation constellation, int fftSize)
{
    PostLayout l;
    const int eta = bitsPerCell(constellation);
    const int np2 = numP2(fftSize);
    const int kpostExPad = infoBits + 32;
    l.m_blocks = (kpostExPad + m_postKbch - 1) / m_postKbch;
    int perBlock = (kpostExPad + l.m_blocks - 1) / l.m_blocks;
    l.m_kpost = perBlock * l.m_blocks;
    l.m_ksig = perBlock;
    int npuncTemp = (6 * (m_postKbch - l.m_ksig)) / 5;
    int npostTemp = l.m_ksig + m_l1BchParity + 9000 - npuncTemp;
    int mult = np2 == 1 ? 2 * eta : eta * np2;
    l.m_npost = ((npostTemp + mult - 1) / mult) * mult;
    l.m_npunc = npuncTemp - (l.m_npost - npostTemp);
    return l;
}

int L1Codec::postCells(int infoBits, L1Constellation constellation, int fftSize)
{
    PostLayout l = postLayout(infoBits, constellation, fftSize);
    return l.m_blocks * l.m_npost / bitsPerCell(constellation);
}

// L1-post bit interleaver (7.3.2.7) and demultiplexer (7.3.3.1): for each bit of the cells, its index in the block
void L1Codec::postCellBitMap(L1Constellation constellation, int npost, std::vector<int>& map)
{
    const int eta = bitsPerCell(constellation);
    map.resize(npost);

    if (eta <= 2)
    {
        for (int i = 0; i < npost; i++) {
            map[i] = i;
        }

        return;
    }

    static const int demux16[8] = {7, 1, 4, 2, 5, 3, 6, 0};
    static const int demux64[12] = {11, 7, 3, 10, 6, 2, 9, 5, 1, 8, 4, 0};
    const int *demux = eta == 4 ? demux16 : demux64;
    const int nc = 2 * eta;
    const int nr = npost / nc;

    for (int di = 0; di < npost; di++)
    {
        int index = (di % nc) * nr + di / nc; // Written column-wise, read row-wise
        int e = demux[di % nc];
        int cell = 2 * (di / nc) + (e >= eta);
        map[cell * eta + e % eta] = index;
    }
}

void L1Codec::encodePost(const std::vector<uint8_t>& bits, L1Pre& pre, std::vector<Cf>& cells)
{
    const int infoBits = (int) bits.size() - 32;
    const int fftSize = pre.fftSize();
    PostLayout l = postLayout(infoBits, pre.m_l1Mod, fftSize);
    const int eta = bitsPerCell(pre.m_l1Mod);
    const int t = postTableIndex(pre.m_l1Mod);
    std::vector<uint8_t> padded(bits);
    padded.resize(l.m_kpost, 0); // L1_PADDING
    Block block;
    layout(l.m_ksig, m_postKbch, m_postKldpc, 9000, l.m_npunc, m_postGroups[t], m_postParity[t], block);
    std::vector<int> map;
    postCellBitMap(pre.m_l1Mod, l.m_npost, map);
    cells.clear();

    for (int b = 0; b < l.m_blocks; b++)
    {
        std::vector<uint8_t> info(padded.begin() + b * l.m_ksig, padded.begin() + (b + 1) * l.m_ksig);

        if (pre.m_l1PostScrambled) {
            scramble(info.data(), l.m_ksig);
        }

        std::vector<uint8_t> codeword;
        encodeL1Block(info.data(), l.m_ksig, block.m_info, m_postKbch, m_postKldpc, m_bch, m_postLDPC, codeword);
        std::vector<uint8_t> cellBits(l.m_npost);

        for (int i = 0; i < l.m_npost; i++) {
            cellBits[i] = codeword[block.m_transmitted[map[i]]];
        }

        for (int i = 0; i < l.m_npost; i += eta) {
            cells.push_back(mapCell(&cellBits[i], eta));
        }
    }

    pre.m_l1PostSize = (int) cells.size();
}

bool L1Codec::decodePost(const Cf *cells, const float *llrScales, const L1Pre& pre, L1Post& post)
{
    const int eta = bitsPerCell(pre.m_l1Mod);
    const int kpostExPad = pre.m_l1PostInfoSize + 32;
    const int blocks = (kpostExPad + m_postKbch - 1) / m_postKbch;

    if ((blocks < 1) || (pre.m_l1PostSize % blocks)) {
        return false;
    }

    const int ksig = (kpostExPad + blocks - 1) / blocks;
    const int cellsPerBlock = pre.m_l1PostSize / blocks;
    const int npost = cellsPerBlock * eta;
    const int npunc = 9000 - (npost - ksig - m_l1BchParity);
    // Npost is a multiple of 2 eta, or of eta NP2 (7.3.1.2 step 3), so of the bit interleaver's columns
    const int np2 = numP2(pre.fftSize());
    const int mult = np2 == 1 ? 2 * eta : eta * np2;

    if ((npunc < 0) || (npunc >= 9000) || (ksig > m_postKbch) || (npost % mult)) {
        return false;
    }

    // The same every frame, until the signalling changes
    if ((m_postKey[0] != ksig) || (m_postKey[1] != npunc) || (m_postKey[2] != npost) || (m_postKey[3] != (int) pre.m_l1Mod))
    {
        const int t = postTableIndex(pre.m_l1Mod);
        layout(ksig, m_postKbch, m_postKldpc, 9000, npunc, m_postGroups[t], m_postParity[t], m_postBlock);
        postCellBitMap(pre.m_l1Mod, npost, m_postMap);
        m_postKey[0] = ksig;
        m_postKey[1] = npunc;
        m_postKey[2] = npost;
        m_postKey[3] = (int) pre.m_l1Mod;
    }

    const Block& block = m_postBlock;
    const std::vector<int>& map = m_postMap;
    std::vector<uint8_t> allBits;

    for (int b = 0; b < blocks; b++)
    {
        std::vector<int8_t> cellLlrs(npost), llrs(npost);

        for (int c = 0; c < cellsPerBlock; c++)
        {
            int index = b * cellsPerBlock + c;
            demapCell(cells[index], eta, llrScales[index], &cellLlrs[c * eta]);
        }

        for (int i = 0; i < npost; i++) {
            llrs[map[i]] = cellLlrs[i];
        }

        std::vector<uint8_t> bits;

        if (!decodeBlock(block, m_postLDPC, m_bch, m_postKbch, llrs.data(), bits)) {
            return false;
        }

        if (pre.m_l1PostScrambled) {
            scramble(bits.data(), ksig);
        }

        allBits.insert(allBits.end(), bits.begin(), bits.end());
    }

    return post.decode(allBits.data(), pre);
}

BBFrameEncoder::BBFrameEncoder(FECType type, CodeRate rate, bool highEfficiency, CodeFamily family) :
    m_kbch(dvbt2::kbch(type, rate, family)),
    m_hem(highEfficiency),
    m_syncd(0),
    m_crc(0)
{
}

void BBFrameEncoder::setOptions(bool nullPacketDeletion, int issyBytes)
{
    m_npd = nullPacketDeletion;
    m_issyBytes = m_hem ? 0 : issyBytes;
}

bool BBFrameEncoder::addPacket(const uint8_t *packet, std::vector<uint8_t>& frame)
{
    // Null packets deleted are counted, in the DNP byte after the next packet sent (up to 255: one more is sent)
    if (m_npd && ((((packet[1] & 0x1f) << 8) | packet[2]) == 0x1fff) && (m_dnp < 255))
    {
        m_dnp++;
        return false;
    }

    // Transmitted user packet: normal mode puts the CRC-8 of the previous one in place of the sync byte. After the
    // packet, the ISSY bytes and DNP, which that CRC-8 covers
    const int upLength = (m_hem ? 187 : 188) + m_issyBytes + (m_npd ? 1 : 0);

    if (!m_hem) {
        m_data.push_back(m_crc);
    }

    const size_t first = m_data.size();
    m_data.insert(m_data.end(), packet + 1, packet + 188);

    for (int i = m_issyBytes - 1; i >= 0; i--) {
        m_data.push_back((uint8_t) (m_issyCount >> (8 * i)));
    }

    m_issyCount++;

    if (m_npd)
    {
        m_data.push_back((uint8_t) m_dnp);
        m_dnp = 0;
    }

    m_crc = crc8(&m_data[first], (int) (m_data.size() - first));

    const int dataBytes = (m_kbch - 80) / 8;

    if ((int) m_data.size() < dataBytes) {
        return false;
    }

    // m_syncd is the offset of the first packet start in m_data
    frame.assign(m_kbch / 8, 0);
    frame[0] = m_matype1 | (m_issyBytes > 0 ? 0x08 : 0) | (m_npd ? 0x04 : 0); // By default TS, single input stream, CCM
    frame[1] = m_matype2;
    int upl = m_hem ? 0 : upLength * 8;
    frame[2] = (uint8_t) (upl >> 8);
    frame[3] = (uint8_t) upl;
    int dfl = dataBytes * 8;
    frame[4] = (uint8_t) (dfl >> 8);
    frame[5] = (uint8_t) dfl;
    frame[6] = m_hem ? 0 : 0x47;
    int syncd = m_syncd < dataBytes ? m_syncd * 8 : 65535;
    frame[7] = (uint8_t) (syncd >> 8);
    frame[8] = (uint8_t) syncd;
    frame[9] = crc8(frame.data(), 9) ^ (m_hem ? 1 : 0);
    std::copy(m_data.begin(), m_data.begin() + dataBytes, frame.begin() + 10);
    m_data.erase(m_data.begin(), m_data.begin() + dataBytes);

    // Offset of the next packet start in the remaining data
    int consumed = dataBytes - m_syncd;
    m_syncd = (upLength - (consumed % upLength)) % upLength;

    scrambleBytes(frame.data(), (int) frame.size());
    return true;
}

void BBFrameDecoder::reset()
{
    m_stats = Stats(); // The receivers' status copies them: a signal's before aren't this one's
    m_synced = false;
    m_packetBits = 0;
    m_packetBad = false;
    m_heldValid = false; // Of the stream before
}

void BBFrameDecoder::discontinuity()
{
    m_synced = false;
    m_packetBits = 0;
    releaseHeld(); // What comes next won't have its CRC-8
}

void BBFrameDecoder::flush()
{
    releaseHeld();
}

void BBFrameDecoder::releaseHeld()
{
    if (m_heldValid)
    {
        m_heldValid = false;

        if (m_callback) {
            m_callback(m_held);
        }
    }
}

// In normal mode, each packet's sync byte was replaced by the CRC-8 of the packet before (5.1.7). That one is held
// until this one arrives, and flagged as errored if it fails: unless this packet's start is from a BBFRAME that BCH
// didn't correct, when the CRC-8 itself may be wrong (and the held packet, if from that BBFRAME, is already flagged)
void BBFrameDecoder::outputPacket(bool bad)
{
    if (!m_hem && m_heldValid && m_lastCrcValid && (m_packet[0] != m_lastCrc))
    {
        m_stats.m_crcErrors++;

        if (!m_firstByteBad) {
            m_held[1] |= 0x80; // transport_error_indicator
        }
    }

    releaseHeld();

    // Null packets deleted before this one. Not if BCH didn't correct the packet, as their count (up to 255) can't be
    // trusted: they'd be output unflagged, hundreds for a few errored packets
    if (m_npd && !bad)
    {
        int dnp = m_packet.back();
        uint8_t null[188];
        null[0] = 0x47;
        null[1] = 0x1f;
        null[2] = 0xff;
        null[3] = 0x10;
        std::fill(null + 4, null + 188, 0xff);

        for (int i = 0; i < dnp; i++)
        {
            m_stats.m_nullPackets++;

            if (m_callback) {
                m_callback(null);
            }
        }
    }

    uint8_t packet[188];
    packet[0] = 0x47;
    const int offset = m_hem ? 0 : 1; // Normal mode starts with the CRC-8 of the previous packet

    std::copy(m_packet.begin() + offset, m_packet.begin() + offset + 187, packet + 1);
    // Of the whole transmitted packet but the previous one's CRC-8: with the ISSY bytes and DNP after the TS bytes
    // (EN 302 307 5.1.4 and annex D, EN 302 755 5.1.8)
    m_lastCrc = crc8(m_packet.data() + offset, (int) m_packet.size() - offset);
    m_lastCrcValid = true;

    if (bad) {
        packet[1] |= 0x80; // transport_error_indicator
    }

    m_stats.m_packets++;

    if (m_hem)
    {
        if (m_callback) {
            m_callback(packet);
        }
    }
    else
    {
        std::copy(packet, packet + 188, m_held);
        m_heldValid = true;
    }
}

const char *BBFrameDecoder::streamTypeName(int type)
{
    switch (type)
    {
    case 0: return "Generic packetized";
    case 1: return "Generic continuous";
    case 2: return "GSE";
    case 3: return "Transport stream";
    default: return "";
    }
}

bool BBFrameDecoder::parseHeader(const uint8_t *frame, Header& header)
{
    header.m_matype1 = frame[0];
    header.m_matype2 = frame[1];
    header.m_upl = (frame[2] << 8) | frame[3];
    header.m_dfl = (frame[4] << 8) | frame[5];
    header.m_sync = frame[6];
    header.m_syncd = (frame[7] << 8) | frame[8];
    header.m_mode = crc8(frame, 9) ^ frame[9];
    return header.m_mode <= 1;
}

void BBFrameDecoder::process(const uint8_t *frame, int kbch, bool bad)
{
    m_stats.m_frames++;

    if (kbch < 80) // Not even a BBHEADER
    {
        m_stats.m_headerErrors++;
        m_synced = false;
        return;
    }

    uint8_t mode = crc8(frame, 9) ^ frame[9];

    if (mode > (m_hemAllowed ? 1 : 0))
    {
        // Can't trust the header
        m_stats.m_headerErrors++;
        m_synced = false;
        return;
    }

    m_stats.m_streamType = frame[0] >> 6;

    if (m_stats.m_streamType != 3)
    {
        // A generic stream (as GSE, carrying IP): no transport stream packets to decode
        m_stats.m_genericFrames++;
        m_synced = false;
        return;
    }

    bool hem = mode == 1;
    bool npd = (frame[0] >> 2) & 1;
    bool issyi = (frame[0] >> 3) & 1;
    int dfl = (frame[4] << 8) | frame[5];
    int syncd = (frame[7] << 8) | frame[8];
    int upl = hem ? (187 + (npd ? 1 : 0)) * 8 : (frame[2] << 8) | frame[3];

    // In normal mode, the bytes after a packet's 188 (and DNP): ISSY's 0, 2 or 3. A header with a CRC that happens to
    // be right can have any UPL
    const int issyBytes = hem ? 0 : (upl / 8 - 188 - (npd ? 1 : 0));

    if ((dfl > kbch - 80) || (dfl % 8) || (upl % 8) || (upl < (hem ? 187 : 188) * 8)
        || ((issyBytes != 0) && (issyBytes != 2) && (issyBytes != 3)))
    {
        m_stats.m_headerErrors++;
        m_synced = false;
        return;
    }

    if ((hem != m_hem) || (npd != m_npd) || (upl != m_upl))
    {
        m_hem = hem;
        m_npd = npd;
        m_upl = upl;
        m_issyBytes = issyBytes;
        (void) issyi;
        m_synced = false;
    }

    const uint8_t *data = frame + 10;
    const int dataBytes = dfl / 8;
    const int upBytes = upl / 8;
    // SYNCD: the bit of the data field the first packet starting in it does at (65535 if none does). Usable if a
    // whole byte within it
    const bool syncdValid = (syncd != 65535) && !(syncd % 8) && (syncd / 8 < dataBytes);
    int start = 0;

    if (!m_synced)
    {
        if (!syncdValid) {
            return;
        }

        start = syncd / 8;
        m_synced = true;
        m_packetBits = 0;
        m_packetBad = false;
        m_lastCrcValid = false; // The packet before the first isn't known, so neither is its CRC-8
        releaseHeld();
        m_packet.resize(upBytes);
    }
    else if ((syncd != 65535) && (syncd / 8 != (upBytes - m_packetBits / 8) % upBytes))
    {
        // The packet boundary isn't where it should be: resynchronise, if it's somewhere it can be
        if (!syncdValid)
        {
            m_synced = false;
            return;
        }

        start = syncd / 8;
        m_packetBits = 0;
        m_packetBad = false;
        m_lastCrcValid = false;
        releaseHeld();
    }

    // As much of a packet as there is at a time
    for (int i = start; i < dataBytes;)
    {
        const int n = std::min(dataBytes - i, (upl - m_packetBits) / 8);

        if (m_packetBits == 0) {
            m_firstByteBad = bad;
        }

        std::memcpy(&m_packet[m_packetBits / 8], data + i, n);
        i += n;
        m_packetBits += 8 * n;
        m_packetBad = m_packetBad || bad;

        if (m_packetBits == upl)
        {
            outputPacket(m_packetBad);
            m_packetBits = 0;
            m_packetBad = false;
        }
    }
}

} // namespace dvbt2
