///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Integer half-band FIR based interpolator and decimator                        //
// This is the even/odd double buffer variant. Really useful only when SIMD is   //
// used                                                                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_INTHALFBANDFILTEREO2I_H_
#define INCLUDE_INTHALFBANDFILTEREO2I_H_

#include <stdint.h>

#if defined(USE_AVX2)
#include <immintrin.h>
#elif defined(USE_SSE4_1)
#include <smmintrin.h>
#elif defined(USE_NEON)
#include <arm_neon.h>
#endif

#include "hbfiltertraits.h"

template<uint32_t HBFilterOrder>
class IntHalfbandFilterEO2Intrisics
{
public:
    static void work(
            int ptrA,
            int ptrB,
            int32_t evenA[2][HBFilterOrder],
            int32_t evenB[2][HBFilterOrder],
            int32_t oddA[2][HBFilterOrder],
            int32_t oddB[2][HBFilterOrder],
            int32_t& iAcc, int32_t& qAcc)
    {
#if defined(USE_SSE4_1)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        const __m128i* h = (const __m128i*) HBFIRFilterTraits<HBFilterOrder>::hbCoeffs;
        __m128i sumI = _mm_setzero_si128();
        __m128i sumQ = _mm_setzero_si128();
        __m128i sa, sb;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 16; i++)
        {
            if ((ptrB % 2) == 0)
            {
                sa = _mm_loadu_si128((__m128i*) &(evenA[0][a]));
                sb = _mm_loadu_si128((__m128i*) &(evenB[0][b]));
                sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));

                sa = _mm_loadu_si128((__m128i*) &(evenA[1][a]));
                sb = _mm_loadu_si128((__m128i*) &(evenB[1][b]));
                sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            }
            else
            {
                sa = _mm_loadu_si128((__m128i*) &(oddA[0][a]));
                sb = _mm_loadu_si128((__m128i*) &(oddB[0][b]));
                sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));

                sa = _mm_loadu_si128((__m128i*) &(oddA[1][a]));
                sb = _mm_loadu_si128((__m128i*) &(oddB[1][b]));
                sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            }

            a += 4;
            b += 4;
            ++h;
        }

        // horizontal add of four 32 bit partial sums

        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 8));
        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 4));
        iAcc = _mm_cvtsi128_si32(sumI);

        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 8));
        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 4));
        qAcc = _mm_cvtsi128_si32(sumQ);

#elif defined(USE_NEON)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        int32x4_t sumI = vdupq_n_s32(0);
        int32x4_t sumQ = vdupq_n_s32(0);
        int32x4_t sa, sb, sh;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 16; i++)
        {
            sh = vld1q_s32(&HBFIRFilterTraits<HBFilterOrder>::hbCoeffs[4*i]);

            if ((ptrB % 2) == 0)
            {
                sa = vld1q_s32(&(evenA[0][a]));
                sb = vld1q_s32(&(evenB[0][b]));
                sumI = vmlaq_s32(sumI, vaddq_s32(sa, sb), sh);

                sa = vld1q_s32(&(evenA[1][a]));
                sb = vld1q_s32(&(evenB[1][b]));
                sumQ = vmlaq_s32(sumQ, vaddq_s32(sa, sb), sh);
            }
            else
            {
                sa = vld1q_s32(&(oddA[0][a]));
                sb = vld1q_s32(&(oddB[0][b]));
                sumI = vmlaq_s32(sumI, vaddq_s32(sa, sb), sh);

                sa = vld1q_s32(&(oddA[1][a]));
                sb = vld1q_s32(&(oddB[1][b]));
                sumQ = vmlaq_s32(sumQ, vaddq_s32(sa, sb), sh);
            }

            a += 4;
            b += 4;
        }

        int32x2_t sumI1 = vpadd_s32(vget_high_s32(sumI), vget_low_s32(sumI));
        int32x2_t sumI2 = vpadd_s32(sumI1, sumI1);
        iAcc = vget_lane_s32(sumI2, 0);

        int32x2_t sumQ1 = vpadd_s32(vget_high_s32(sumQ), vget_low_s32(sumQ));
        int32x2_t sumQ2 = vpadd_s32(sumQ1, sumQ1);
        qAcc = vget_lane_s32(sumQ2, 0);
#endif
    }
};

template<>
class IntHalfbandFilterEO2Intrisics<48>
{
public:
    static void work(
            int ptrA,
            int ptrB,
            int32_t evenA[2][48],
            int32_t evenB[2][48],
            int32_t oddA[2][48],
            int32_t oddB[2][48],
            int32_t& iAcc, int32_t& qAcc)
    {
#if defined(USE_SSE4_1)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        const __m128i* h = (const __m128i*) HBFIRFilterTraits<48>::hbCoeffs;
        __m128i sumI = _mm_setzero_si128();
        __m128i sumQ = _mm_setzero_si128();
        __m128i sa, sb;

        if ((ptrB % 2) == 0)
        {
            // 0
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            h++;
            // 1
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+4]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+4]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            h++;
            // 2
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+8]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+8]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
        }
        else
        {
            // 0
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            h++;
            // 1
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+4]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+4]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            h++;
            // 2
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+8]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+8]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
        }

        // horizontal add of four 32 bit partial sums

        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 8));
        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 4));
        iAcc = _mm_cvtsi128_si32(sumI);

        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 8));
        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 4));
        qAcc = _mm_cvtsi128_si32(sumQ);

#elif defined(USE_NEON)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        int32x4_t sumI = vdupq_n_s32(0);
        int32x4_t sumQ = vdupq_n_s32(0);
        int32x4x3_t sh = vld3q_s32((int32_t const *) &HBFIRFilterTraits<48>::hbCoeffs[0]);
        int32x4x3_t sa, sb;

        if ((ptrB % 2) == 0)
        {
            sa = vld3q_s32((int32_t const *) &(evenA[0][a]));
            sb = vld3q_s32((int32_t const *) &(evenB[0][b]));
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[0], sb.val[0]), sh.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[1], sb.val[1]), sh.val[1]);
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[2], sb.val[2]), sh.val[2]);
            sa = vld3q_s32((int32_t const *) &(evenA[1][a]));
            sb = vld3q_s32((int32_t const *) &(evenB[1][b]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[0], sb.val[0]), sh.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[1], sb.val[1]), sh.val[1]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[2], sb.val[2]), sh.val[2]);
        }
        else
        {
            sa = vld3q_s32((int32_t const *) &(oddA[0][a]));
            sb = vld3q_s32((int32_t const *) &(oddB[0][b]));
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[0], sb.val[0]), sh.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[1], sb.val[1]), sh.val[1]);
            sumI = vmlaq_s32(sumI, vaddq_s32(sa.val[2], sb.val[2]), sh.val[2]);
            sa = vld3q_s32((int32_t const *) &(oddA[1][a]));
            sb = vld3q_s32((int32_t const *) &(oddB[1][b]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[0], sb.val[0]), sh.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[1], sb.val[1]), sh.val[1]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(sa.val[2], sb.val[2]), sh.val[2]);
        }

        int32x2_t sumI1 = vpadd_s32(vget_high_s32(sumI), vget_low_s32(sumI));
        int32x2_t sumI2 = vpadd_s32(sumI1, sumI1);
        iAcc = vget_lane_s32(sumI2, 0);

        int32x2_t sumQ1 = vpadd_s32(vget_high_s32(sumQ), vget_low_s32(sumQ));
        int32x2_t sumQ2 = vpadd_s32(sumQ1, sumQ1);
        qAcc = vget_lane_s32(sumQ2, 0);
#endif
    }
};


template<>
class IntHalfbandFilterEO2Intrisics<96>
{
public:
    static void work(
            int ptrA,
            int ptrB,
            int32_t evenA[2][96],
            int32_t evenB[2][96],
            int32_t oddA[2][96],
            int32_t oddB[2][96],
            int32_t& iAcc, int32_t& qAcc)
    {
#if defined(USE_AVX2)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        const __m256i* h = (const __m256i*) HBFIRFilterTraits<96>::hbCoeffs;
        __m256i sumI = _mm256_setzero_si256();
        __m256i sumQ = _mm256_setzero_si256();
        __m256i sa, sb;

        if ((ptrB % 2) == 0)
        {
            // I
            sa = _mm256_loadu_si256((__m256i*) &(evenA[0][a]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[0][b]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[0]));
            sa = _mm256_loadu_si256((__m256i*) &(evenA[0][a+8]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[0][b+8]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[1]));
            sa = _mm256_loadu_si256((__m256i*) &(evenA[0][a+16]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[0][b+16]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[2]));
            // Q
            sa = _mm256_loadu_si256((__m256i*) &(evenA[1][a]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[1][b]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[0]));
            sa = _mm256_loadu_si256((__m256i*) &(evenA[1][a+8]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[1][b+8]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[1]));
            sa = _mm256_loadu_si256((__m256i*) &(evenA[1][a+16]));
            sb = _mm256_loadu_si256((__m256i*) &(evenB[1][b+16]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[2]));
        }
        else
        {
            // I
            sa = _mm256_loadu_si256((__m256i*) &(oddA[0][a]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[0][b]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[0]));
            sa = _mm256_loadu_si256((__m256i*) &(oddA[0][a+8]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[0][b+8]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[1]));
            sa = _mm256_loadu_si256((__m256i*) &(oddA[0][a+16]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[0][b+16]));
            sumI = _mm256_add_epi32(sumI, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[2]));
            // Q
            sa = _mm256_loadu_si256((__m256i*) &(oddA[1][a]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[1][b]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[0]));
            sa = _mm256_loadu_si256((__m256i*) &(oddA[1][a+8]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[1][b+8]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[1]));
            sa = _mm256_loadu_si256((__m256i*) &(oddA[1][a+16]));
            sb = _mm256_loadu_si256((__m256i*) &(oddB[1][b+16]));
            sumQ = _mm256_add_epi32(sumQ, _mm256_mullo_epi32(_mm256_add_epi32(sa, sb), h[2]));
        }

        // horizontal add

        __m128i vloI = _mm256_castsi256_si128(sumI);
        vloI = _mm_add_epi32(vloI, _mm_srli_si128(vloI, 8));
        vloI = _mm_add_epi32(vloI, _mm_srli_si128(vloI, 4));
        iAcc = _mm_cvtsi128_si32(vloI);
        __m128i vhiI = _mm256_extracti128_si256(sumI, 1);
        vhiI = _mm_add_epi32(vhiI, _mm_srli_si128(vhiI, 8));
        vhiI = _mm_add_epi32(vhiI, _mm_srli_si128(vhiI, 4));
        iAcc += _mm_cvtsi128_si32(vhiI);

        __m128i vloQ = _mm256_castsi256_si128(sumQ);
        vloQ = _mm_add_epi32(vloQ, _mm_srli_si128(vloQ, 8));
        vloQ = _mm_add_epi32(vloQ, _mm_srli_si128(vloQ, 4));
        qAcc = _mm_cvtsi128_si32(vloQ);
        __m128i vhiQ = _mm256_extracti128_si256(sumQ, 1);
        vhiQ = _mm_add_epi32(vhiQ, _mm_srli_si128(vhiQ, 8));
        vhiQ = _mm_add_epi32(vhiQ, _mm_srli_si128(vhiQ, 4));
        qAcc += _mm_cvtsi128_si32(vhiQ);

#elif defined(USE_SSE4_1)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer
        const __m128i* h = (const __m128i*) HBFIRFilterTraits<96>::hbCoeffs;
        __m128i sumI = _mm_setzero_si128();
        __m128i sumQ = _mm_setzero_si128();
        __m128i sa, sb;

        if ((ptrB % 2) == 0)
        {
            // I
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[0]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+4]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[1]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+8]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[2]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+12]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+12]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[3]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+16]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+16]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[4]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[0][a+20]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[0][b+20]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[5]));
            // Q
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[0]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+4]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[1]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+8]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[2]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+12]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+12]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[3]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+16]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+16]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[4]));
            sa = _mm_loadu_si128((__m128i*) &(evenA[1][a+20]));
            sb = _mm_loadu_si128((__m128i*) &(evenB[1][b+20]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[5]));
        }
        else
        {
            // I
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[0]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+4]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[1]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+8]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[2]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+12]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+12]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[3]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+16]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+16]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[4]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[0][a+20]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[0][b+20]));
            sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[5]));
            // Q
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[0]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+4]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+4]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[1]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+8]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+8]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[2]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+12]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+12]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[3]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+16]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+16]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[4]));
            sa = _mm_loadu_si128((__m128i*) &(oddA[1][a+20]));
            sb = _mm_loadu_si128((__m128i*) &(oddB[1][b+20]));
            sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), h[5]));
        }

        // horizontal add of four 32 bit partial sums

        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 8));
        sumI = _mm_add_epi32(sumI, _mm_srli_si128(sumI, 4));
        iAcc = _mm_cvtsi128_si32(sumI);

        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 8));
        sumQ = _mm_add_epi32(sumQ, _mm_srli_si128(sumQ, 4));
        qAcc = _mm_cvtsi128_si32(sumQ);

#elif defined(USE_NEON)
        int a = ptrA/2; // tip pointer
        int b = ptrB/2 + 1; // tail pointer

        int32x4_t sumI = vdupq_n_s32(0);
        int32x4_t sumQ = vdupq_n_s32(0);

        int32x4x3_t sh = vld3q_s32((int32_t const *) &HBFIRFilterTraits<96>::hbCoeffs[0]);
        int32x4x4_t s4a, s4b, s4h;
        int32x4x2_t s2a, s2b, s2h;

        if ((ptrB % 2) == 0)
        {
            s4h = vld4q_s32((int32_t const *) &HBFIRFilterTraits<96>::hbCoeffs[0]);

            s4a = vld4q_s32((int32_t const *) &(evenA[0][a]));
            s4b = vld4q_s32((int32_t const *) &(evenB[0][b]));
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[0], s4b.val[0]), s4h.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[1], s4b.val[1]), s4h.val[1]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[2], s4b.val[2]), s4h.val[2]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[3], s4b.val[3]), s4h.val[3]);

            s4a = vld4q_s32((int32_t const *) &(evenA[1][a]));
            s4b = vld4q_s32((int32_t const *) &(evenB[1][b]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[0], s4b.val[0]), s4h.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[1], s4b.val[1]), s4h.val[1]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[2], s4b.val[2]), s4h.val[2]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[3], s4b.val[3]), s4h.val[3]);

            s2h = vld4q_s32((int32_t const *) &HBFIRFilterTraits<96>::hbCoeffs[16]);

            s2a = vld2q_s32((int32_t const *) &(evenA[0][a+16]));
            s2b = vld2q_s32((int32_t const *) &(evenB[0][b+16]));
            sumI = vmlaq_s32(sumI, vaddq_s32(s2a.val[0], s2b.val[0]), s2h.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s2a.val[1], s2b.val[1]), s2h.val[1]);

            s2a = vld2q_s32((int32_t const *) &(evenA[1][a+16]));
            s2b = vld2q_s32((int32_t const *) &(evenB[1][b+16]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s2a.val[0], s2b.val[0]), s2h.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s2a.val[1], s2b.val[1]), s2h.val[1]);
        }
        else
        {
            s4h = vld4q_s32((int32_t const *) &HBFIRFilterTraits<96>::hbCoeffs[0]);

            s4a = vld4q_s32((int32_t const *) &(oddA[0][a]));
            s4b = vld4q_s32((int32_t const *) &(oddB[0][b]));
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[0], s4b.val[0]), s4h.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[1], s4b.val[1]), s4h.val[1]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[2], s4b.val[2]), s4h.val[2]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s4a.val[3], s4b.val[3]), s4h.val[3]);

            s4a = vld4q_s32((int32_t const *) &(oddA[1][a]));
            s4b = vld4q_s32((int32_t const *) &(oddB[1][b]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[0], s4b.val[0]), s4h.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[1], s4b.val[1]), s4h.val[1]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[2], s4b.val[2]), s4h.val[2]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s4a.val[3], s4b.val[3]), s4h.val[3]);

            s2h = vld4q_s32((int32_t const *) &HBFIRFilterTraits<96>::hbCoeffs[16]);

            s2a = vld2q_s32((int32_t const *) &(oddA[0][a+16]));
            s2b = vld2q_s32((int32_t const *) &(oddB[0][b+16]));
            sumI = vmlaq_s32(sumI, vaddq_s32(s2a.val[0], s2b.val[0]), s2h.val[0]);
            sumI = vmlaq_s32(sumI, vaddq_s32(s2a.val[1], s2b.val[1]), s2h.val[1]);

            s2a = vld2q_s32((int32_t const *) &(oddA[1][a+16]));
            s2b = vld2q_s32((int32_t const *) &(oddB[1][b+16]));
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s2a.val[0], s2b.val[0]), s2h.val[0]);
            sumQ = vmlaq_s32(sumQ, vaddq_s32(s2a.val[1], s2b.val[1]), s2h.val[1]);
        }

        int32x2_t sumI1 = vpadd_s32(vget_high_s32(sumI), vget_low_s32(sumI));
        int32x2_t sumI2 = vpadd_s32(sumI1, sumI1);
        iAcc = vget_lane_s32(sumI2, 0);

        int32x2_t sumQ1 = vpadd_s32(vget_high_s32(sumQ), vget_low_s32(sumQ));
        int32x2_t sumQ2 = vpadd_s32(sumQ1, sumQ1);
        qAcc = vget_lane_s32(sumQ2, 0);
#endif
    }
};


#endif /* INCLUDE_INTHALFBANDFILTEREO2I_H_ */
