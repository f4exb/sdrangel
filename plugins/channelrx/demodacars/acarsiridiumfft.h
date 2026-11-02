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

#ifndef INCLUDE_ACARSIRIDIUMFFT_H
#define INCLUDE_ACARSIRIDIUMFFT_H

#include <complex>
#include <cmath>
#include <vector>

#if defined(USE_SSE4_1)
#include <smmintrin.h>
#endif

// A small radix 2 transform, shared by the Iridium channelizer and the burst receiver.
//
// Both only ever need power of two sizes, and both are in a plugin that should not drag
// a transform library in behind it, so this is deliberately plain. It is not fast by the
// standards of a real FFT library; it is fast enough, and the sizes are small.

class AcarsIridiumFft
{
public:
    typedef std::complex<double> Cd;

    //! Prepares for transforms of the given size, which must be a power of two
    void configure(int size)
    {
        if (m_size == size) {
            return;
        }

        m_size = size;
        m_twiddle.resize(size / 2);
        m_twiddleInverse.resize(size / 2);

        for (int k = 0; k < (size / 2); k++)
        {
            const double angle = -2.0 * M_PI * (double) k / (double) size;

            m_twiddle[k] = Cd(std::cos(angle), std::sin(angle));
            m_twiddleInverse[k] = std::conj(m_twiddle[k]);
        }
    }

    int size() const { return m_size; }

    //! In place transform. inverse runs the same butterflies with the conjugate twiddles
    //! and no scaling, which is what a filter bank wants
    void transform(std::vector<Cd>& data, bool inverse = false) const
    {
        const int n = m_size;

        for (int i = 1, j = 0; i < n; i++)
        {
            int bit = n >> 1;

            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }

            j ^= bit;

            if (i < j) {
                std::swap(data[i], data[j]);
            }
        }

        // Both tables are precomputed: conjugating inside the butterfly meant doing it
        // once per butterfly, which for a bank running tens of thousands of transforms a
        // second is a great deal of work to repeat for a table that never changes
        const Cd *twiddle = inverse ? m_twiddleInverse.data() : m_twiddle.data();

        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            const int half = len / 2;

            for (int i = 0; i < n; i += len)
            {
                Cd *lower = &data[i];
                Cd *upper = &data[i + half];

                for (int k = 0; k < half; k++)
                {
#if defined(USE_SSE4_1)
                    // A complex double fills one 128 bit register. The product of two of
                    // them is the classic shuffle and alternating add: ac - bd in the
                    // real half and bc + ad in the imaginary half, which addsub does in
                    // one instruction instead of the sign juggling it would otherwise take
                    const __m128d x = _mm_loadu_pd((const double *) &upper[k]);
                    const __m128d y = _mm_loadu_pd((const double *) &twiddle[k * step]);
                    const __m128d t1 = _mm_mul_pd(x, _mm_unpacklo_pd(y, y));
                    const __m128d t2 = _mm_mul_pd(_mm_shuffle_pd(x, x, 1), _mm_unpackhi_pd(y, y));
                    const __m128d v = _mm_addsub_pd(t1, t2);
                    const __m128d u = _mm_loadu_pd((const double *) &lower[k]);

                    _mm_storeu_pd((double *) &lower[k], _mm_add_pd(u, v));
                    _mm_storeu_pd((double *) &upper[k], _mm_sub_pd(u, v));
#else
                    const Cd w = twiddle[k * step];
                    const Cd u = lower[k];
                    const Cd v = upper[k] * w;

                    lower[k] = u + v;
                    upper[k] = u - v;
#endif
                }
            }
        }
    }

    //! The smallest power of two at least as large as n
    static int nextPowerOfTwo(int n)
    {
        int size = 1;

        while (size < n) {
            size <<= 1;
        }

        return size;
    }

private:

    int m_size = 0;
    std::vector<Cd> m_twiddle;
    std::vector<Cd> m_twiddleInverse;
};

#endif // INCLUDE_ACARSIRIDIUMFFT_H
