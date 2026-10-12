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
#include <cstring>

#include "dvbs.h"

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define DVBS_SSE2
#endif

namespace dvbs {

double rollOffValue(RollOff rollOff)
{
    switch (rollOff)
    {
    case RollOff::R0_25: return 0.25;
    case RollOff::R0_20: return 0.20;
    default: return 0.35;
    }
}

const char *toString(RollOff rollOff)
{
    switch (rollOff)
    {
    case RollOff::R0_25: return "0.25";
    case RollOff::R0_20: return "0.20";
    default: return "0.35";
    }
}

void puncturePattern(CodeRate rate, std::vector<PunctureEntry>& pattern, int& period)
{
    switch (rate)
    {
    case CodeRate::R1_2:
        pattern = {{0, 0}, {0, 1}};
        period = 1;
        break;
    case CodeRate::R2_3: // X: 10, Y: 11
        pattern = {{0, 0}, {0, 1}, {1, 1}};
        period = 2;
        break;
    case CodeRate::R3_4: // X: 101, Y: 110
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 0}};
        period = 3;
        break;
    case CodeRate::R5_6: // X: 10101, Y: 11010
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 0}, {3, 1}, {4, 0}};
        period = 5;
        break;
    case CodeRate::R7_8: // X: 1000101, Y: 1111010
    default:
        pattern = {{0, 0}, {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 0}, {5, 1}, {6, 0}};
        period = 7;
        break;
    }
}

int punctureLength(CodeRate rate)
{
    std::vector<PunctureEntry> pattern;
    int period;
    puncturePattern(rate, pattern, period);
    return (int) pattern.size();
}

double usefulBitRate(double symbolRate, CodeRate rate)
{
    int num, den;
    dvbt::codeRateFraction(rate, num, den);
    return 2.0 * symbolRate * num / den * 188.0 / 204.0;
}

double rootRaisedCosine(double t, double a)
{
    static const double pi = 3.141592653589793;

    if (std::fabs(t) < 1e-9) {
        return 1.0 - a + 4.0 * a / pi;
    }

    if ((a > 0.0) && (std::fabs(std::fabs(t) - 1.0 / (4.0 * a)) < 1e-9)) {
        return a / std::sqrt(2.0) * ((1.0 + 2.0 / pi) * std::sin(pi / (4.0 * a)) + (1.0 - 2.0 / pi) * std::cos(pi / (4.0 * a)));
    }

    const double x = 4.0 * a * t;
    return (std::sin(pi * t * (1.0 - a)) + 4.0 * a * t * std::cos(pi * t * (1.0 + a))) / (pi * t * (1.0 - x * x));
}

void softBits(const Cf *symbols, int nbSymbols, float scale, int8_t *out)
{
    const float *in = reinterpret_cast<const float *>(symbols);
    const int n = 2 * nbSymbols;
    int i = 0;
#if defined(DVBS_SSE2)
    const __m128 vScale = _mm_set1_ps(scale);
    const __m128 vMax = _mm_set1_ps(127.0f);
    const __m128 vMin = _mm_set1_ps(-127.0f);

    for (; i + 16 <= n; i += 16)
    {
        __m128i q[4];

        for (int j = 0; j < 4; j++)
        {
            __m128 v = _mm_mul_ps(_mm_loadu_ps(in + i + 4 * j), vScale);
            q[j] = _mm_cvtps_epi32(_mm_max_ps(vMin, _mm_min_ps(vMax, v)));
        }

        _mm_storeu_si128(reinterpret_cast<__m128i *>(out + i), _mm_packs_epi16(_mm_packs_epi32(q[0], q[1]), _mm_packs_epi32(q[2], q[3])));
    }
#endif
    for (; i < n; i++) {
        out[i] = (int8_t) std::lrint(std::max(-127.0f, std::min(127.0f, in[i] * scale)));
    }
}

void fft(std::vector<std::complex<double>>& x, bool inverse)
{
    static const double pi = 3.141592653589793;
    const int n = (int) x.size();

    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;

        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }

        j ^= bit;

        if (i < j) {
            std::swap(x[i], x[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double angle = (inverse ? 2.0 : -2.0) * pi / len;
        const std::complex<double> step(std::cos(angle), std::sin(angle));

        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);

            for (int j = 0; j < len / 2; j++)
            {
                const std::complex<double> t = x[i + j + len / 2] * w;
                x[i + j + len / 2] = x[i + j] - t;
                x[i + j] += t;
                w *= step;
            }
        }
    }
}

void FFT::transform(std::vector<std::complex<double>>& x, bool inverse)
{
    static const double pi = 3.141592653589793;
    const int n = (int) x.size();

    if ((int) m_twiddles.size() < n / 2)
    {
        m_twiddles.resize(n / 2);

        for (int k = 0; k < n / 2; k++) {
            m_twiddles[k] = std::polar(1.0, -2.0 * pi * k / n);
        }
    }

    const int size = 2 * (int) m_twiddles.size();
    const double sign = inverse ? -1.0 : 1.0;   // Of the twiddles' imaginary parts

    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;

        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }

        j ^= bit;

        if (i < j) {
            std::swap(x[i], x[j]);
        }
    }

    // The products written out (as std::complex's may check for infinities at each)
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int stride = size / len;

        for (int i = 0; i < n; i += len)
        {
            for (int j = 0; j < half; j++)
            {
                const std::complex<double>& w = m_twiddles[(size_t) j * stride];
                const double wr = w.real(), wi = sign * w.imag();
                const double xr = x[i + j + half].real(), xi = x[i + j + half].imag();
                const std::complex<double> t(xr * wr - xi * wi, xr * wi + xi * wr);
                x[i + j + half] = x[i + j] - t;
                x[i + j] += t;
            }
        }
    }
}

double peakFrequency(const std::vector<std::complex<double>>& x)
{
    const int n = (int) x.size();
    int best = 0;

    for (int i = 1; i < n; i++)
    {
        if (std::norm(x[i]) > std::norm(x[best])) {
            best = i;
        }
    }

    const double a = std::abs(x[(best + n - 1) % n]), b = std::abs(x[best]), c = std::abs(x[(best + 1) % n]);
    const double denom = a - 2.0 * b + c;
    double offset = denom < 0.0 ? std::max(-0.5, std::min(0.5, 0.5 * (a - c) / denom)) : 0.0;
    double bin = best + offset;

    if (bin >= n / 2.0) {
        bin -= n;
    }

    return bin / n;
}

} // namespace dvbs
