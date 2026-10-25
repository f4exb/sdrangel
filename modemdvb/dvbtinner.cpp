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
#include <QDebug>

#include "dvbtinner.h"

#if defined(USE_SSE2) || defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define INNER_SSE2
#endif

namespace dvbt {

const float InnerDecoder::m_llrScale = 16.0f;

InnerDecoder::InnerDecoder() :
    m_mode(Mode::Mode8K),
    m_constellation(Constellation::QAM64),
    m_codeRate(CodeRate::R2_3),
    m_hierarchy(Hierarchy::None),
    m_lowPriority(false),
    m_nbData(0),
    m_bitsPerCell(0),
    m_infoBitsPerSymbol(0),
    m_puncturePeriod(1),
    m_mer(0.0)
{
}

void InnerDecoder::getPuncturePattern(CodeRate codeRate, std::vector<PunctureEntry>& pattern, int& period)
{
    // Transmission order of the punctured X and Y bits (4.3.3, table 2)
    switch (codeRate)
    {
    case CodeRate::R1_2:
        pattern = {{0, 0}, {0, 1}};
        period = 1;
        break;
    case CodeRate::R2_3:
        pattern = {{0, 0}, {0, 1}, {1, 1}};
        period = 2;
        break;
    case CodeRate::R3_4:
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 0}};
        period = 3;
        break;
    case CodeRate::R5_6:
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 0}, {3, 1}, {4, 0}};
        period = 5;
        break;
    case CodeRate::R7_8:
    default:
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 0}, {5, 1}, {6, 0}};
        period = 7;
        break;
    }
}

float InnerDecoder::normalisation(Constellation constellation, Hierarchy hierarchy)
{
    const int a = alpha(hierarchy);

    if (constellation == Constellation::QPSK) {
        return std::sqrt(2.0f);
    } else if (constellation == Constellation::QAM16) {
        return std::sqrt(a == 1 ? 10.0f : a == 2 ? 20.0f : 52.0f);
    } else {
        return std::sqrt(a == 1 ? 42.0f : a == 2 ? 60.0f : 108.0f);
    }
}

void InnerDecoder::configure(Mode mode, Constellation constellation, CodeRate codeRate, Hierarchy hierarchy, bool lowPriority)
{
    switch (constellation)
    {
    case Constellation::QPSK:
    case Constellation::QAM16:
    case Constellation::QAM64:
        break;
    default:
        qWarning() << "InnerDecoder::configure: Invalid constellation"
                   << static_cast<int>(constellation);
        return;
    }

    const Tables& tables = Tables::get(mode);

    m_mode = mode;
    m_constellation = constellation;
    m_codeRate = codeRate;
    m_hierarchy = constellation == Constellation::QPSK ? Hierarchy::None : hierarchy; // QPSK can't be hierarchical
    m_lowPriority = (m_hierarchy != Hierarchy::None) && lowPriority;
    m_nbData = tables.m_nbData;
    m_bitsPerCell = bitsPerCell(constellation);
    getPuncturePattern(codeRate, m_puncture, m_puncturePeriod);
    const int streamBits = m_lowPriority ? lpBitsPerSymbol() : hpBitsPerSymbol();
    m_infoBitsPerSymbol = streamBits / (int) m_puncture.size() * m_puncturePeriod;
    m_softBits.resize(m_nbData * m_bitsPerCell);
    m_quantized.resize(m_nbData * m_bitsPerCell);
    m_mer = 0.0;

    // Symbol interleaver: even symbols carry word q on cell H(q), odd symbols carry word H(q) on cell q (4.3.4.2)
    const std::vector<int>& h = tables.m_symbolInterleaver;
    std::vector<int> hInv(m_nbData);

    for (int q = 0; q < m_nbData; q++) {
        hInv[h[q]] = q;
    }

    // Bit interleavers H_e(w) = (w + offset_e) mod 126 (4.3.4.1)
    static const int bitInterleaverOffsets[6] = {0, 63, 105, 42, 21, 84};
    const int v = m_bitsPerCell;

    const bool hierarchical = m_hierarchy != Hierarchy::None;

    for (int parity = 0; parity < 2; parity++)
    {
        m_permutation[parity].resize(m_nbData * v);
        m_softIndex[parity].clear();
        m_llrIndex[parity].clear();

        for (int c = 0; c < m_nbData; c++)
        {
            int w = parity == 0 ? hInv[c] : h[c];
            int block = w / 126;
            int j = w % 126;

            for (int e = 0; e < v; e++)
            {
                // Bit e of the word comes from sub-stream e. Non-hierarchical, it takes input bits in the order
                // 0, 2, 4, 1, 3, 5 of each group of v. Hierarchical, sub-streams 0 and 1 take the high priority
                // stream's bits alternately, and 2 to v - 1 the low priority stream's, in the order 2, 4, 3, 5 of each
                // group of v - 2 (x''di to b[di mod (v-2)] div ((v-2)/2) + 2 [di mod ((v-2)/2)] + 2, 4.3.4.1)
                int n = 126 * block + (j + bitInterleaverOffsets[e]) % 126;
                int x;
                bool lp = false;

                if (!hierarchical)
                {
                    int order = (e % 2 == 0) ? e / 2 : v / 2 + e / 2;
                    x = v * n + order;
                }
                else if (e < 2)
                {
                    x = 2 * n + e;
                }
                else
                {
                    const int lpBits = v - 2;
                    const int half = lpBits / 2;
                    int r = 0;

                    for (int rr = 0; rr < lpBits; rr++)
                    {
                        if ((rr / half) + 2 * (rr % half) + 2 == e) {
                            r = rr;
                        }
                    }

                    x = lpBits * n + r;
                    lp = true;
                }

                m_permutation[parity][c * v + e] = x;

                // The stream decoded: position of punctured bit x in the X/Y pairs
                if (lp == m_lowPriority)
                {
                    const PunctureEntry& entry = m_puncture[x % m_puncture.size()];
                    m_softIndex[parity].push_back(c * v + e);
                    m_llrIndex[parity].push_back(2 * ((int) (x / m_puncture.size()) * m_puncturePeriod + entry.m_bit) + entry.m_y);
                }
            }
        }
    }
}

// Nearest odd integer, limited to +/-maxPoint
inline float InnerDecoder::nearestPoint(float x, float maxPoint)
{
    x = std::max(-maxPoint, std::min(maxPoint, x));
    float h = x * 0.5f + 8.0f; // Positive, so truncation is floor
    return 2.0f * (float) ((int) h - 8) + 1.0f;
}

// Scale, round and saturate to +/-127
void InnerDecoder::quantize(const float *in, int8_t *out, int n)
{
    int i = 0;
#ifdef INNER_SSE2
    const __m128 scale = _mm_set1_ps(m_llrScale);
    const __m128 maxValue = _mm_set1_ps(127.0f);
    const __m128 minValue = _mm_set1_ps(-127.0f);

    for (; i + 16 <= n; i += 16)
    {
        __m128i q[4];

        for (int j = 0; j < 4; j++)
        {
            __m128 v = _mm_mul_ps(_mm_loadu_ps(in + i + 4 * j), scale);
            v = _mm_max_ps(minValue, _mm_min_ps(maxValue, v));
            q[j] = _mm_cvtps_epi32(v);
        }

        __m128i lo = _mm_packs_epi32(q[0], q[1]);
        __m128i hi = _mm_packs_epi32(q[2], q[3]);
        _mm_storeu_si128((__m128i *) (out + i), _mm_packs_epi16(lo, hi));
    }
#endif
    for (; i < n; i++)
    {
        float v = std::max(-127.0f, std::min(127.0f, in[i] * m_llrScale));
        out[i] = (int8_t) (v >= 0.0f ? v + 0.5f : v - 0.5f);
    }
}

void InnerDecoder::decode(int symbolIndex, const Cf *cells, const float *csi, std::vector<int8_t>& llrs)
{
    const int v = m_bitsPerCell;
    const int nbBits = m_nbData * v;
    const float scale = normalisation(m_constellation, m_hierarchy);
    const float maxPoint = (float) ((1 << (v / 2)) - 1);
    // Non-uniform constellations' points are alpha - 1 further from the axes (4.3.5)
    const float offset = (float) (alpha(m_hierarchy) - 1);
    float *soft = m_softBits.data();

    // Soft bits, in cell order. Gray mapping (4.3.5): y0/y1 are the signs of I/Q (1 = negative),
    // y2/y3 and y4/y5 the magnitudes
    if (m_constellation == Constellation::QPSK)
    {
        for (int c = 0; c < m_nbData; c++, soft += 2)
        {
            float w = csi[c] * scale;
            soft[0] = cells[c].real() * w;
            soft[1] = cells[c].imag() * w;
        }
    }
    else if (m_constellation == Constellation::QAM16)
    {
        for (int c = 0; c < m_nbData; c++, soft += 4)
        {
            float xi = cells[c].real() * scale;
            float xq = cells[c].imag() * scale;
            float w = csi[c];
            soft[0] = xi * w;
            soft[1] = xq * w;
            soft[2] = (std::fabs(xi) - 2.0f - offset) * w;
            soft[3] = (std::fabs(xq) - 2.0f - offset) * w;
        }
    }
    else
    {
        for (int c = 0; c < m_nbData; c++, soft += 6)
        {
            float xi = cells[c].real() * scale;
            float xq = cells[c].imag() * scale;
            float w = csi[c];
            float ai = std::fabs(xi) - 4.0f - offset;
            float aq = std::fabs(xq) - 4.0f - offset;
            soft[0] = xi * w;
            soft[1] = xq * w;
            soft[2] = ai * w;
            soft[3] = aq * w;
            soft[4] = (std::fabs(ai) - 2.0f) * w;
            soft[5] = (std::fabs(aq) - 2.0f) * w;
        }
    }

    // MER, from a quarter of the cells, of those received (not erased, with CSI 0)
    double errorPower = 0.0;
    int measured = 0;

    for (int c = 0; c < m_nbData; c += 4)
    {
        if (csi[c] <= 0.0f) {
            continue;
        }

        measured++;
        // Error from the nearest point, of the magnitudes less the offset (an odd integer, at least 1)
        float ai = std::fabs(cells[c].real() * scale) - offset;
        float aq = std::fabs(cells[c].imag() * scale) - offset;
        float ei = ai - std::max(1.0f, nearestPoint(ai, maxPoint));
        float eq = aq - std::max(1.0f, nearestPoint(aq, maxPoint));
        errorPower += ei * ei + eq * eq;
    }

    if (measured > 0)
    {
        double mer = 10.0 * std::log10((scale * scale) * measured / std::max(errorPower, 1e-9));
        m_mer = m_mer == 0.0 ? mer : 0.9 * m_mer + 0.1 * mer;
    }

    quantize(m_softBits.data(), m_quantized.data(), nbBits);

    // De-interleave and de-puncture into X/Y pairs, erased bits being 0. Of the stream decoded
    const std::vector<int>& softIndex = m_softIndex[symbolIndex & 1];
    const std::vector<int>& llrIndex = m_llrIndex[symbolIndex & 1];
    size_t base = llrs.size();
    llrs.resize(base + 2 * m_infoBitsPerSymbol, 0);
    int8_t *out = &llrs[base];
    const int8_t *q = m_quantized.data();
    const int nbStream = (int) llrIndex.size();

    if (nbStream == nbBits)
    {
        for (int i = 0; i < nbBits; i++) {
            out[llrIndex[i]] = q[i];
        }
    }
    else
    {
        for (int i = 0; i < nbStream; i++) {
            out[llrIndex[i]] = q[softIndex[i]];
        }
    }
}

void InnerDecoder::encode(int symbolIndex, const uint8_t *bits, Cf *cells, const uint8_t *lpBits) const
{
    const std::vector<int>& perm = m_permutation[symbolIndex & 1];
    const int v = m_bitsPerCell;
    const float scale = normalisation(m_constellation, m_hierarchy);
    const float offset = (float) (alpha(m_hierarchy) - 1);
    const bool hierarchical = m_hierarchy != Hierarchy::None;

    for (int c = 0; c < m_nbData; c++)
    {
        const int *p = &perm[c * v];
        int y[6] = {0, 0, 0, 0, 0, 0};

        for (int e = 0; e < v; e++) {
            y[e] = (hierarchical && (e >= 2) ? lpBits : bits)[p[e]];
        }

        float mi = 1.0f, mq = 1.0f;

        if (m_constellation == Constellation::QAM16)
        {
            mi = (y[2] ? 1.0f : 3.0f) + offset;
            mq = (y[3] ? 1.0f : 3.0f) + offset;
        }
        else if (m_constellation == Constellation::QAM64)
        {
            static const float mag[4] = {7.0f, 5.0f, 1.0f, 3.0f}; // Index (y2 << 1) | y4
            mi = mag[(y[2] << 1) | y[4]] + offset;
            mq = mag[(y[3] << 1) | y[5]] + offset;
        }

        cells[c] = Cf(y[0] ? -mi : mi, y[1] ? -mq : mq) / scale;
    }
}

void InnerDecoder::puncture(CodeRate codeRate, const uint8_t *xy, int nbInfoBits, std::vector<uint8_t>& out)
{
    std::vector<PunctureEntry> pattern;
    int period;
    getPuncturePattern(codeRate, pattern, period);

    for (int i = 0; i + period <= nbInfoBits; i += period)
    {
        for (const PunctureEntry& entry : pattern) {
            out.push_back(xy[2 * (i + entry.m_bit) + entry.m_y]);
        }
    }
}

} // namespace dvbt
