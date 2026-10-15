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

#ifndef INCLUDE_LTETURBO_H
#define INCLUDE_LTETURBO_H

#include <cstdint>
#include <functional>
#include <vector>

#include "export.h"

// LTE turbo code as specified in 3GPP TS 36.212 section 5.1.3.2:
// two 8-state RSC encoders (g0 = 13, g1 = 15 octal) with a QPP interleaver
// and trellis termination, giving three output streams d0, d1, d2 of K+4 bits.
//
// Soft values throughout are LLRs with the convention: positive means bit 0.

class SDRBASE_API LTETurbo
{
public:
    // Whether K is one of the 188 block sizes in TS 36.212 Table 5.1.3-3
    static bool isValidBlockSize(int K);
    // QPP interleaver parameters for block size K. Returns false if K is invalid.
    static bool qppParameters(int K, int& f1, int& f2);
    // QPP interleaver: output bit i of the interleaver is input bit pi[i]
    static void interleaver(int K, std::vector<int>& pi);

    // RSC trellis. State S = r0*4 + r1*2 + r2, where r0 holds the most recent register input.
    // Feedback g0 = 1 + D^2 + D^3, feedforward g1 = 1 + D + D^3.
    static int rscFeedback(int s, int u) { return u ^ ((s >> 1) & 1) ^ (s & 1); } // Register input a for state s and systematic input u
    static int rscParity(int s, int a) { return a ^ ((s >> 2) & 1) ^ (s & 1); }
    static int rscNext(int s, int a) { return (a << 2) | (s >> 1); }

private:
    // TS 36.212 Table 5.1.3-3: QPP interleaver parameters for the 188 block sizes
    static const uint16_t m_qppF1[188];
    static const uint16_t m_qppF2[188];

    static int qppIndex(int K);
};

class SDRBASE_API LTETurboEncoder
{
public:
    // Encode K bits (one bit per byte, 0 or 1) into d0, d1, d2 (each K+4 bits)
    static void encode(const std::vector<uint8_t>& in,
        std::vector<uint8_t>& d0, std::vector<uint8_t>& d1, std::vector<uint8_t>& d2);

private:
    static void rscEncode(const std::vector<uint8_t>& in, const int *pi,
        std::vector<uint8_t>& parity, uint8_t tailSys[3], uint8_t tailParity[3]);
};

class SDRBASE_API LTETurboDecoder
{
public:
    // Called after each iteration with the hard decisions. Return true to stop early.
    typedef std::function<bool(const std::vector<uint8_t>& bits)> EarlyStop;

    LTETurboDecoder();

    void setBlockSize(int K);
    int getBlockSize() const { return m_K; }

    // Max-log-MAP decoding of d0, d1, d2 LLRs (each K+4 values).
    // Writes K hard bits to out. Returns number of iterations performed,
    // or a negative value if earlyStop never returned true.
    int decode(const std::vector<float>& d0, const std::vector<float>& d1, const std::vector<float>& d2,
        std::vector<uint8_t>& out, int maxIterations, EarlyStop earlyStop = nullptr);

private:
    // Trellis branches, so they needn't be calculated in the inner loops
    struct Trellis
    {
        int m_next[8][2];       // Next state for state and systematic input
        int m_gamma[8][2];      // Branch metric index (2*input + parity) for state and systematic input
        int m_prev[8][2];       // The two states leading to a state
        int m_prevGamma[8][2];  // Branch metric index for those transitions

        Trellis();
    };

    static constexpr float m_minusInf = -1.0e30f;

    static const Trellis& trellis();

    void bcjr(const float *sys, const float *apriori, const float *parity,
        const float *tailSys, const float *tailParity, float *llrOut);

    int m_K;
    std::vector<int> m_pi;
    std::vector<float> m_alpha;
    std::vector<float> m_gammaSys; // Systematic+apriori contribution per step
    std::vector<float> m_gammaPar; // Parity contribution per step
};

// Circular buffer rate matching for turbo coded transport channels, TS 36.212 section 5.1.4.1.
// No soft buffer limitation is applied (Ncb = Kw).
class SDRBASE_API LTERateMatcher
{
public:
    // Select E bits from d0, d1, d2 (each D bits) for redundancy version rv
    static void match(const std::vector<uint8_t>& d0, const std::vector<uint8_t>& d1, const std::vector<uint8_t>& d2,
        int E, int rv, std::vector<uint8_t>& e);
    // Reverse of match: accumulate E LLRs in to d0, d1, d2 (each resized to D, zeroed first)
    static void dematch(const std::vector<float>& e, int D, int rv,
        std::vector<float>& d0, std::vector<float>& d1, std::vector<float>& d2);

private:
    // For each position in the circular buffer, the stream (0..2) and index of the bit it holds,
    // or stream -1 for <NULL> filler bits
    static void bufferMap(int D, std::vector<int>& stream, std::vector<int>& index);
    static int startPosition(int D, int rv);
};

#endif // INCLUDE_LTETURBO_H
