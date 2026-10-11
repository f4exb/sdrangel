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

#ifndef MODEMDVB_DVBS_H
#define MODEMDVB_DVBS_H

#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

#include "export.h"
#include "dvbt.h"

// DVB-S (EN 300 421): what the DVB-S and DVB-S2 receivers share

namespace dvbs {

typedef std::complex<float> Cf;

//! DVB-S's code rates are DVB-T's: 1/2, 2/3, 3/4, 5/6 and 7/8, punctured from the same K = 7 code the same way
typedef dvbt::CodeRate CodeRate;

//! Roll-off of the root raised cosine pulse: DVB-S's is 0.35, DVB-S2 also has 0.25 and 0.20
enum class RollOff { R0_35 = 0, R0_25 = 1, R0_20 = 2 };

MODEMDVB_API double rollOffValue(RollOff rollOff);
MODEMDVB_API const char *toString(RollOff rollOff);

//! One transmitted bit of a puncturing period: the information bit within the period, and X (0) or Y (1)
struct PunctureEntry
{
    int m_bit;
    int m_y;
};

//! EN 300 421 table 2 (the same as EN 300 744's): the transmitted bits of a period of period information bits, in
//! order. Consecutive pairs are a symbol's I and Q
MODEMDVB_API void puncturePattern(CodeRate rate, std::vector<PunctureEntry>& pattern, int& period);
//! Bits transmitted in a puncturing period
MODEMDVB_API int punctureLength(CodeRate rate);
//! Useful (transport stream) bit rate at a symbol rate: 2 Rs x rate x 188 / 204
MODEMDVB_API double usefulBitRate(double symbolRate, CodeRate rate);

//! Root raised cosine impulse response at t symbols from its centre, peak 1 - a + 4a / pi
MODEMDVB_API double rootRaisedCosine(double t, double rollOff);

//! magnitude with the sign of x, without a branch (decisions on noisy symbols would mispredict half the time)
inline float withSignOf(float magnitude, float x)
{
    uint32_t m, s;
    std::memcpy(&m, &magnitude, 4);
    std::memcpy(&s, &x, 4);
    m = (m & 0x7fffffffu) | (s & 0x80000000u);
    float r;
    std::memcpy(&r, &m, 4);
    return r;
}

//! Soft bits of symbols: I and Q of each times scale, rounded and limited to +/-127 (SSE2 where there is it)
MODEMDVB_API void softBits(const Cf *symbols, int nbSymbols, float scale, int8_t *out);

//! In place radix-2 FFT, of a power of 2 points (forward: e^-j). For occasional use: twiddles computed each time
MODEMDVB_API void fft(std::vector<std::complex<double>>& x, bool inverse = false);

//! fft() for repeated use: its twiddles computed once (exactly, rather than by a recurrence), for the largest size it's
//! been used at, smaller sizes taking every so many
class MODEMDVB_API FFT
{
public:
    void transform(std::vector<std::complex<double>>& x, bool inverse = false);

private:
    std::vector<std::complex<double>> m_twiddles;   //!< e^-j2 pi k / N, for k < N / 2
};

//! The peak of |x|^2 (from fft()), interpolated between bins, as a frequency in cycles a sample, -0.5 to 0.5
MODEMDVB_API double peakFrequency(const std::vector<std::complex<double>>& x);

} // namespace dvbs

#endif // MODEMDVB_DVBS_H
