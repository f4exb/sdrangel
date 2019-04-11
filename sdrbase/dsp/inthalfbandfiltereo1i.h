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

#ifndef SDRBASE_DSP_INTHALFBANDFILTEREO1I_H_
#define SDRBASE_DSP_INTHALFBANDFILTEREO1I_H_

#include <stdint.h>

#if defined(USE_SSE4_1)
#include <smmintrin.h>
#endif

#include "hbfiltertraits.h"

template<uint32_t HBFilterOrder>
class IntHalfbandFilterEO1Intrisics
{
public:
    static void work(
            int ptr,
            int32_t even[2][HBFilterOrder],
            int32_t odd[2][HBFilterOrder],
            int32_t& iAcc, int32_t& qAcc)
    {
#if defined(USE_SSE4_1)
        int a = ptr/2 + HBFIRFilterTraits<HBFilterOrder>::hbOrder/2; // tip pointer
        int b = ptr/2 + 1; // tail pointer
        const __m128i* h = (const __m128i*) HBFIRFilterTraits<HBFilterOrder>::hbCoeffs;
        __m128i sumI = _mm_setzero_si128();
        __m128i sumQ = _mm_setzero_si128();
        __m128i sa, sb;
        a -= 3;

        for (int i = 0; i < HBFIRFilterTraits<HBFilterOrder>::hbOrder / 16; i++)
        {
            if ((ptr % 2) == 0)
            {
                sa = _mm_shuffle_epi32(_mm_loadu_si128((__m128i*) &(even[0][a])), _MM_SHUFFLE(0,1,2,3));
                sb = _mm_loadu_si128((__m128i*) &(even[0][b]));
                sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));

                sa = _mm_shuffle_epi32(_mm_loadu_si128((__m128i*) &(even[1][a])), _MM_SHUFFLE(0,1,2,3));
                sb = _mm_loadu_si128((__m128i*) &(even[1][b]));
                sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            }
            else
            {
                sa = _mm_shuffle_epi32(_mm_loadu_si128((__m128i*) &(odd[0][a])), _MM_SHUFFLE(0,1,2,3));
                sb = _mm_loadu_si128((__m128i*) &(odd[0][b]));
                sumI = _mm_add_epi32(sumI, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));

                sa = _mm_shuffle_epi32(_mm_loadu_si128((__m128i*) &(odd[1][a])), _MM_SHUFFLE(0,1,2,3));
                sb = _mm_loadu_si128((__m128i*) &(odd[1][b]));
                sumQ = _mm_add_epi32(sumQ, _mm_mullo_epi32(_mm_add_epi32(sa, sb), *h));
            }

            a -= 4;
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
#endif
    }
};

#endif /* SDRBASE_DSP_INTHALFBANDFILTEREO1I_H_ */
