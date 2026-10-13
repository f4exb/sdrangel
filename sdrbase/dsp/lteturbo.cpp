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

#include "lteturbo.h"

// TS 36.212 Table 5.1.3-3: QPP interleaver parameters for the 188 block sizes
// K = 40..512 step 8, 528..1024 step 16, 1056..2048 step 32, 2112..6144 step 64
const uint16_t LTETurbo::m_qppF1[188] = {
    3, 7, 19, 7, 7, 11, 5, 11, 7, 41, 103, 15, 9, 17, 9, 21, 101, 21, 57, 23, 13,
    27, 11, 27, 85, 29, 33, 15, 17, 33, 103, 19, 19, 37, 19, 21, 21, 115, 193, 21,
    133, 81, 45, 23, 243, 151, 155, 25, 51, 47, 91, 29, 29, 247, 29, 89, 91, 157,
    55, 31, 17, 35, 227, 65, 19, 37, 41, 39, 185, 43, 21, 155, 79, 139, 23, 217,
    25, 17, 127, 25, 239, 17, 137, 215, 29, 15, 147, 29, 59, 65, 55, 31, 17, 171,
    67, 35, 19, 39, 19, 199, 21, 211, 21, 43, 149, 45, 49, 71, 13, 17, 25, 183, 55,
    127, 27, 29, 29, 57, 45, 31, 59, 185, 113, 31, 17, 171, 209, 253, 367, 265, 181,
    39, 27, 127, 143, 43, 29, 45, 157, 47, 13, 111, 443, 51, 51, 451, 257, 57, 313,
    271, 179, 331, 363, 375, 127, 31, 33, 43, 33, 477, 35, 233, 357, 337, 37, 71,
    71, 37, 39, 127, 39, 39, 31, 113, 41, 251, 43, 21, 43, 45, 45, 161, 89, 323, 47, 23, 47, 263
};

const uint16_t LTETurbo::m_qppF2[188] = {
    10, 12, 42, 16, 18, 20, 22, 24, 26, 84, 90, 32, 34, 108, 38, 120, 84, 44, 46,
    48, 50, 52, 36, 56, 58, 60, 62, 32, 198, 68, 210, 36, 74, 76, 78, 120, 82, 84,
    86, 44, 90, 46, 94, 48, 98, 40, 102, 52, 106, 72, 110, 168, 114, 58, 118, 180,
    122, 62, 84, 64, 66, 68, 420, 96, 74, 76, 234, 80, 82, 252, 86, 44, 120, 92,
    94, 48, 98, 80, 102, 52, 106, 48, 110, 112, 114, 58, 118, 60, 122, 124, 84,
    64, 66, 204, 140, 72, 74, 76, 78, 240, 82, 252, 86, 88, 60, 92, 846, 48, 28,
    80, 102, 104, 954, 96, 110, 112, 114, 116, 354, 120, 610, 124, 420, 64, 66,
    136, 420, 216, 444, 456, 468, 80, 164, 504, 172, 88, 300, 92, 188, 96, 28, 240,
    204, 104, 212, 192, 220, 336, 228, 232, 236, 120, 244, 248, 168, 64, 130, 264,
    134, 408, 138, 280, 142, 480, 146, 444, 120, 152, 462, 234, 158, 80, 96, 902,
    166, 336, 170, 86, 174, 176, 178, 120, 182, 184, 186, 94, 190, 480
};

int LTETurbo::qppIndex(int K)
{
    if ((K >= 40) && (K <= 512) && ((K % 8) == 0)) {
        return (K - 40) / 8;
    } else if ((K > 512) && (K <= 1024) && ((K % 16) == 0)) {
        return 60 + (K - 528) / 16;
    } else if ((K > 1024) && (K <= 2048) && ((K % 32) == 0)) {
        return 92 + (K - 1056) / 32;
    } else if ((K > 2048) && (K <= 6144) && ((K % 64) == 0)) {
        return 124 + (K - 2112) / 64;
    } else {
        return -1;
    }
}

bool LTETurbo::isValidBlockSize(int K)
{
    return qppIndex(K) >= 0;
}

bool LTETurbo::qppParameters(int K, int& f1, int& f2)
{
    int idx = qppIndex(K);

    if (idx < 0) {
        return false;
    }
    f1 = m_qppF1[idx];
    f2 = m_qppF2[idx];
    return true;
}

void LTETurbo::interleaver(int K, std::vector<int>& pi)
{
    int f1, f2;

    pi.resize(K);
    if (!qppParameters(K, f1, f2)) {
        return;
    }
    for (int i = 0; i < K; i++)
    {
        // Use 64-bit to avoid overflow of f2*i*i
        int64_t ii = i;
        pi[i] = (int) ((f1 * ii + f2 * ii * ii) % K);
    }
}

// Encode one constituent RSC, including termination.
void LTETurboEncoder::rscEncode(const std::vector<uint8_t>& in, const int *pi,
    std::vector<uint8_t>& parity, uint8_t tailSys[3], uint8_t tailParity[3])
{
    int K = (int) in.size();
    int s = 0;

    parity.resize(K);
    for (int k = 0; k < K; k++)
    {
        int u = pi ? in[pi[k]] : in[k];
        int a = LTETurbo::rscFeedback(s, u);
        parity[k] = LTETurbo::rscParity(s, a);
        s = LTETurbo::rscNext(s, a);
    }
    // Termination: input is chosen so the register input is 0
    for (int k = 0; k < 3; k++)
    {
        int u = ((s >> 1) & 1) ^ (s & 1);
        tailSys[k] = u;
        tailParity[k] = LTETurbo::rscParity(s, 0);
        s = LTETurbo::rscNext(s, 0);
    }
}

void LTETurboEncoder::encode(const std::vector<uint8_t>& in,
    std::vector<uint8_t>& d0, std::vector<uint8_t>& d1, std::vector<uint8_t>& d2)
{
    int K = (int) in.size();
    std::vector<int> pi;
    std::vector<uint8_t> z1, z2;
    uint8_t x1t[3], z1t[3], x2t[3], z2t[3];

    LTETurbo::interleaver(K, pi);
    rscEncode(in, nullptr, z1, x1t, z1t);
    rscEncode(in, pi.data(), z2, x2t, z2t);

    d0.resize(K + 4);
    d1.resize(K + 4);
    d2.resize(K + 4);
    for (int k = 0; k < K; k++)
    {
        d0[k] = in[k];
        d1[k] = z1[k];
        d2[k] = z2[k];
    }
    // Tail bits multiplexing, TS 36.212 5.1.3.2.2
    d0[K] = x1t[0]; d0[K+1] = z1t[1]; d0[K+2] = x2t[0]; d0[K+3] = z2t[1];
    d1[K] = z1t[0]; d1[K+1] = x1t[2]; d1[K+2] = z2t[0]; d1[K+3] = x2t[2];
    d2[K] = x1t[1]; d2[K+1] = z1t[2]; d2[K+2] = x2t[1]; d2[K+3] = z2t[2];
}

LTETurboDecoder::LTETurboDecoder() :
    m_K(0)
{
}

void LTETurboDecoder::setBlockSize(int K)
{
    if (K != m_K)
    {
        m_K = K;
        LTETurbo::interleaver(K, m_pi);
        m_alpha.resize((K + 4) * 8);
        m_gammaSys.resize(K + 3);
        m_gammaPar.resize(K + 3);
    }
}

LTETurboDecoder::Trellis::Trellis()
{
    int count[8] = {0};

    for (int s = 0; s < 8; s++)
    {
        for (int u = 0; u < 2; u++)
        {
            int a = LTETurbo::rscFeedback(s, u);
            int ns = LTETurbo::rscNext(s, a);
            m_next[s][u] = ns;
            m_gamma[s][u] = 2 * u + LTETurbo::rscParity(s, a);
            m_prev[ns][count[ns]] = s;
            m_prevGamma[ns][count[ns]] = m_gamma[s][u];
            count[ns]++;
        }
    }
}

const LTETurboDecoder::Trellis& LTETurboDecoder::trellis()
{
    static const Trellis trellis;
    return trellis;
}

// Max-log-MAP BCJR for one constituent decoder. Outputs a posteriori LLRs for the K information bits.
// Branch metric for bit b with LLR L is +L/2 if b = 0, -L/2 if b = 1.
// Metrics are normalised relative to state 0, which is reachable at every step.
void LTETurboDecoder::bcjr(const float *sys, const float *apriori, const float *parity,
    const float *tailSys, const float *tailParity, float *llrOut)
{
    const int K = m_K;
    const Trellis& t = trellis();
    float *alpha = m_alpha.data();

    for (int k = 0; k < K; k++)
    {
        m_gammaSys[k] = 0.5f * (sys[k] + apriori[k]);
        m_gammaPar[k] = 0.5f * parity[k];
    }
    for (int k = 0; k < 3; k++)
    {
        m_gammaSys[K + k] = 0.5f * tailSys[k];
        m_gammaPar[K + k] = 0.5f * tailParity[k];
    }

    // Forward recursion
    alpha[0] = 0.0f;
    for (int s = 1; s < 8; s++) {
        alpha[s] = m_minusInf;
    }
    for (int k = 0; k < K; k++)
    {
        const float *a = &alpha[k * 8];
        float *an = &alpha[(k + 1) * 8];
        const float gs = m_gammaSys[k];
        const float gp = m_gammaPar[k];
        const float g[4] = {gs + gp, gs - gp, -gs + gp, -gs - gp};

        for (int ns = 0; ns < 8; ns++)
        {
            an[ns] = std::max(a[t.m_prev[ns][0]] + g[t.m_prevGamma[ns][0]],
                a[t.m_prev[ns][1]] + g[t.m_prevGamma[ns][1]]);
        }
        const float norm = an[0];
        for (int s = 0; s < 8; s++) {
            an[s] -= norm;
        }
    }
    // Termination: register input is forced to 0, so systematic input depends on state
    for (int k = K; k < K + 3; k++)
    {
        const float *a = &alpha[k * 8];
        float *an = &alpha[(k + 1) * 8];
        const float gs = m_gammaSys[k];
        const float gp = m_gammaPar[k];
        const float g[4] = {gs + gp, gs - gp, -gs + gp, -gs - gp};

        for (int s = 0; s < 8; s++) {
            an[s] = m_minusInf;
        }
        for (int s = 0; s < 8; s++)
        {
            const int u = LTETurbo::rscFeedback(s, 0);
            an[t.m_next[s][u]] = std::max(an[t.m_next[s][u]], a[s] + g[t.m_gamma[s][u]]);
        }
        const float norm = an[0];
        for (int s = 0; s < 8; s++) {
            an[s] -= norm;
        }
    }

    // Backward recursion, ending in state 0
    float beta[8], betaPrev[8];
    beta[0] = 0.0f;
    for (int s = 1; s < 8; s++) {
        beta[s] = m_minusInf;
    }
    for (int k = K + 2; k >= K; k--)
    {
        const float gs = m_gammaSys[k];
        const float gp = m_gammaPar[k];
        const float g[4] = {gs + gp, gs - gp, -gs + gp, -gs - gp};

        for (int s = 0; s < 8; s++)
        {
            const int u = LTETurbo::rscFeedback(s, 0);
            betaPrev[s] = g[t.m_gamma[s][u]] + beta[t.m_next[s][u]];
        }
        const float norm = betaPrev[0];
        for (int s = 0; s < 8; s++) {
            beta[s] = betaPrev[s] - norm;
        }
    }
    for (int k = K - 1; k >= 0; k--)
    {
        const float *a = &alpha[k * 8];
        const float gs = m_gammaSys[k];
        const float gp = m_gammaPar[k];
        const float g[4] = {gs + gp, gs - gp, -gs + gp, -gs - gp};
        float m0 = m_minusInf;
        float m1 = m_minusInf;

        for (int s = 0; s < 8; s++)
        {
            const float b0 = g[t.m_gamma[s][0]] + beta[t.m_next[s][0]];
            const float b1 = g[t.m_gamma[s][1]] + beta[t.m_next[s][1]];
            betaPrev[s] = std::max(b0, b1);
            m0 = std::max(m0, a[s] + b0);
            m1 = std::max(m1, a[s] + b1);
        }
        llrOut[k] = m0 - m1;
        const float norm = betaPrev[0];
        for (int s = 0; s < 8; s++) {
            beta[s] = betaPrev[s] - norm;
        }
    }
}

int LTETurboDecoder::decode(const std::vector<float>& d0, const std::vector<float>& d1, const std::vector<float>& d2,
    std::vector<uint8_t>& out, int maxIterations, EarlyStop earlyStop)
{
    const int K = m_K;
    const float extrinsicScale = 0.75f; // Compensates for max-log approximation optimism
    std::vector<float> sys1(K), sys2(K), par1(K), par2(K);
    std::vector<float> apriori1(K, 0.0f), apriori2(K), llr1(K), llr2(K);
    float tailSys1[3], tailPar1[3], tailSys2[3], tailPar2[3];

    out.resize(K);
    if ((K == 0) || ((int) d0.size() < K + 4) || ((int) d1.size() < K + 4) || ((int) d2.size() < K + 4)) {
        return -1;
    }

    for (int k = 0; k < K; k++)
    {
        sys1[k] = d0[k];
        sys2[k] = d0[m_pi[k]];
        par1[k] = d1[k];
        par2[k] = d2[k];
    }
    // Tail bits demultiplexing, inverse of TS 36.212 5.1.3.2.2
    tailSys1[0] = d0[K];   tailPar1[0] = d1[K];
    tailSys1[1] = d2[K];   tailPar1[1] = d0[K+1];
    tailSys1[2] = d1[K+1]; tailPar1[2] = d2[K+1];
    tailSys2[0] = d0[K+2]; tailPar2[0] = d1[K+2];
    tailSys2[1] = d2[K+2]; tailPar2[1] = d0[K+3];
    tailSys2[2] = d1[K+3]; tailPar2[2] = d2[K+3];

    for (int iter = 1; iter <= maxIterations; iter++)
    {
        bcjr(sys1.data(), apriori1.data(), par1.data(), tailSys1, tailPar1, llr1.data());
        for (int k = 0; k < K; k++)
        {
            int j = m_pi[k];
            apriori2[k] = extrinsicScale * (llr1[j] - sys1[j] - apriori1[j]);
        }
        bcjr(sys2.data(), apriori2.data(), par2.data(), tailSys2, tailPar2, llr2.data());
        for (int k = 0; k < K; k++)
        {
            int j = m_pi[k];
            apriori1[j] = extrinsicScale * (llr2[k] - sys2[k] - apriori2[k]);
            out[j] = llr2[k] < 0.0f ? 1 : 0;
        }
        if (earlyStop && earlyStop(out)) {
            return iter;
        }
    }

    return earlyStop ? -maxIterations : maxIterations;
}

void LTERateMatcher::bufferMap(int D, std::vector<int>& stream, std::vector<int>& index)
{
    // Inter-column permutation pattern for sub-block interleaver, TS 36.212 Table 5.1.4-1
    static const int P[32] = {
        0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30,
        1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31
    };
    const int R = (D + 31) / 32;
    const int Kpi = R * 32;
    const int ND = Kpi - D;

    stream.resize(3 * Kpi);
    index.resize(3 * Kpi);

    for (int k = 0; k < Kpi; k++)
    {
        int y01 = P[k / R] + 32 * (k % R);
        int y2 = (P[k / R] + 32 * (k % R) + 1) % Kpi;

        // Systematic
        stream[k] = y01 < ND ? -1 : 0;
        index[k] = y01 - ND;
        // Interlaced parity streams
        stream[Kpi + 2 * k] = y01 < ND ? -1 : 1;
        index[Kpi + 2 * k] = y01 - ND;
        stream[Kpi + 2 * k + 1] = y2 < ND ? -1 : 2;
        index[Kpi + 2 * k + 1] = y2 - ND;
    }
}

int LTERateMatcher::startPosition(int D, int rv)
{
    const int R = (D + 31) / 32;
    const int Ncb = 3 * R * 32;
    const int c = (Ncb + 8 * R - 1) / (8 * R);
    return R * (2 * c * rv + 2);
}

void LTERateMatcher::match(const std::vector<uint8_t>& d0, const std::vector<uint8_t>& d1, const std::vector<uint8_t>& d2,
    int E, int rv, std::vector<uint8_t>& e)
{
    const int D = (int) d0.size();
    std::vector<int> stream, index;
    const std::vector<uint8_t> *d[3] = {&d0, &d1, &d2};

    bufferMap(D, stream, index);
    const int Ncb = (int) stream.size();
    const int k0 = startPosition(D, rv);

    e.resize(E);
    for (int k = 0, j = 0; k < E; j++)
    {
        int pos = (k0 + j) % Ncb;
        if (stream[pos] >= 0) {
            e[k++] = (*d[stream[pos]])[index[pos]];
        }
    }
}

void LTERateMatcher::dematch(const std::vector<float>& e, int D, int rv,
    std::vector<float>& d0, std::vector<float>& d1, std::vector<float>& d2)
{
    const int E = (int) e.size();
    std::vector<int> stream, index;
    std::vector<float> *d[3] = {&d0, &d1, &d2};

    bufferMap(D, stream, index);
    const int Ncb = (int) stream.size();
    const int k0 = startPosition(D, rv);

    d0.assign(D, 0.0f);
    d1.assign(D, 0.0f);
    d2.assign(D, 0.0f);
    for (int k = 0, j = 0; k < E; j++)
    {
        int pos = (k0 + j) % Ncb;
        if (stream[pos] >= 0) {
            (*d[stream[pos]])[index[pos]] += e[k++];
        }
    }
}
