///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#include <math.h>
#include <complex>
#include <string>
#include <algorithm>

#include "util/timeutil.h"
#include "util.h"

namespace FT8 {

double now()
{
    return TimeUtil::nowus() / 1000000.0;
}

//
// Goertzel Algorithm for a Non-integer Frequency Index, Rick Lyons
// https://www.dsprelated.com/showarticle/495.php
//
std::complex<float> goertzel(std::vector<float> v, int rate, int i0, int n, float hz)
{
    // float radians_per_sample = (hz * 2 * M_PI) / rate;
    // float k = radians_per_sample * n;
    float bin_hz = rate / (float)n;
    float k = hz / bin_hz;

    float alpha = 2 * M_PI * k / n;
    float beta = 2 * M_PI * k * (n - 1.0) / n;

    float two_cos_alpha = 2 * cos(alpha);
    float a = cos(beta);
    float b = -sin(beta);
    float c = sin(alpha) * sin(beta) - cos(alpha) * cos(beta);
    float d = sin(2 * M_PI * k);

    float w1 = 0;
    float w2 = 0;

    for (int i = 0; i < n; i++)
    {
        float w0 = v[i0 + i] + two_cos_alpha * w1 - w2;
        w2 = w1;
        w1 = w0;
    }

    float re = w1 * a + w2 * c;
    float im = w1 * b + w2 * d;

    return std::complex<float>(re, im);
}

float vmax(const std::vector<float> &v)
{
    float mx = 0;
    int got = 0;
    for (int i = 0; i < (int)v.size(); i++)
    {
        if (got == 0 || v[i] > mx)
        {
            got = 1;
            mx = v[i];
        }
    }
    return mx;
}

std::vector<float> vreal(const std::vector<std::complex<float>> &a)
{
    std::vector<float> b(a.size());
    for (int i = 0; i < (int)a.size(); i++)
    {
        b[i] = a[i].real();
    }
    return b;
}

std::vector<float> vimag(const std::vector<std::complex<float>> &a)
{
    std::vector<float> b(a.size());
    for (int i = 0; i < (int)a.size(); i++)
    {
        b[i] = a[i].imag();
    }
    return b;
}

// generate 8-FSK, at 25 hz, bin size 6.25 hz,
// 200 samples/second, 32 samples/symbol.
// used as reference to detect pairs of symbols.
// superseded by gfsk().
std::vector<std::complex<float>> fsk_c(const std::vector<int> &syms)
{
    int n = syms.size();
    std::vector<std::complex<float>> v(n * 32);
    float theta = 0;
    for (int si = 0; si < n; si++)
    {
        float hz = 25 + syms[si] * 6.25;
        for (int i = 0; i < 32; i++)
        {
            v[si * 32 + i] = std::complex<float>(cos(theta), sin(theta));
            theta += 2 * M_PI / (200 / hz);
        }
    }
    return v;
}

// copied from wsjt-x ft2/gfsk_pulse.f90.
// b is 1.0 for FT4; 2.0 for FT8.
float gfsk_point(float b, float t)
{
    float c = M_PI * sqrt(2.0 / log(2.0));
    float x = 0.5 * (erf(c * b * (t + 0.5)) - erf(c * b * (t - 0.5)));
    return x;
}

// the smoothing window for gfsk.
// run the window over impulses of symbol frequencies,
// each impulse at the center of its symbol time.
// three symbols wide.
// most of the pulse is in the center symbol.
// b is 1.0 for FT4; 2.0 for FT8.
std::vector<float> gfsk_window(int samples_per_symbol, float b)
{
    std::vector<float> v(3 * samples_per_symbol);
    float sum = 0;
    for (int i = 0; i < (int)v.size(); i++)
    {
        float x = i / (float)samples_per_symbol;
        x -= 1.5;
        float y = gfsk_point(b, x);
        v[i] = y;
        sum += y;
    }

    for (int i = 0; i < (int)v.size(); i++)
    {
        v[i] /= sum;
    }

    return v;
}

// gaussian-smoothed fsk.
// the gaussian smooths the instantaneous frequencies,
// so that the transitions between symbols don't
// cause clicks.
// gwin is gfsk_window(32, 2.0)
std::vector<std::complex<float>> gfsk_c(
    const std::vector<int> &symbols,
    float hz0, float hz1,
    float spacing, int rate, int symsamples,
    float phase0,
    const std::vector<float> &gwin
)
{
    if (!((gwin.size() % 2) == 0))
    {
        std::vector<std::complex<float>> v(symsamples * symbols.size());
        return v;
    }

    // compute frequency for each symbol.
    // generate a spike in the middle of each symbol time;
    // the gaussian filter will turn it into a waveform.
    std::vector<float> hzv(symsamples * (symbols.size() + 2), 0.0);
    for (int bi = 0; bi < (int)symbols.size(); bi++)
    {
        float base_hz = hz0 + (hz1 - hz0) * (bi / (float)symbols.size());
        float fr = base_hz + (symbols[bi] * spacing);
        int mid = symsamples * (bi + 1) + symsamples / 2;
        // the window has even size, so split the impulse over
        // the two middle samples to be symmetric.
        hzv[mid] = fr * symsamples / 2.0;
        hzv[mid - 1] = fr * symsamples / 2.0;
    }

    // repeat first and last symbols
    for (int i = 0; i < symsamples; i++)
    {
        hzv[i] = hzv[i + symsamples];
        hzv[symsamples * (symbols.size() + 1) + i] = hzv[symsamples * symbols.size() + i];
    }

    // run the per-sample frequency vector through
    // the gaussian filter.
    int half = gwin.size() / 2;
    std::vector<float> o(hzv.size());
    for (int i = 0; i < (int)o.size(); i++)
    {
        float sum = 0;
        for (int j = 0; j < (int)gwin.size(); j++)
        {
            int k = i - half + j;
            if (k >= 0 && k < (int)hzv.size())
            {
                sum += hzv[k] * gwin[j];
            }
        }
        o[i] = sum;
    }

    // drop repeated first and last symbols
    std::vector<float> oo(symsamples * symbols.size());
    for (int i = 0; i < (int)oo.size(); i++)
    {
        oo[i] = o[i + symsamples];
    }

    // now oo[i] contains the frequency for the i'th sample.

    std::vector<std::complex<float>> v(symsamples * symbols.size());
    float theta = phase0;
    for (int i = 0; i < (int)v.size(); i++)
    {
        v[i] = std::complex<float>(cos(theta), sin(theta));
        float hz = oo[i];
        theta += 2 * M_PI / (rate / hz);
    }

    return v;
}

// gaussian-smoothed fsk.
// the gaussian smooths the instantaneous frequencies,
// so that the transitions between symbols don't
// cause clicks.
// gwin is gfsk_window(32, 2.0)
std::vector<float> gfsk_r(
    const std::vector<int> &symbols,
    float hz0, float hz1,
    float spacing, int rate, int symsamples,
    float phase0,
    const std::vector<float> &gwin
)
{
    if (!((gwin.size() % 2) == 0))
    {
        std::vector<float> v(symsamples * symbols.size());
        return v;
    }

    // compute frequency for each symbol.
    // generate a spike in the middle of each symbol time;
    // the gaussian filter will turn it into a waveform.
    std::vector<float> hzv(symsamples * (symbols.size() + 2), 0.0);
    for (int bi = 0; bi < (int)symbols.size(); bi++)
    {
        float base_hz = hz0 + (hz1 - hz0) * (bi / (float)symbols.size());
        float fr = base_hz + (symbols[bi] * spacing);
        int mid = symsamples * (bi + 1) + symsamples / 2;
        // the window has even size, so split the impulse over
        // the two middle samples to be symmetric.
        hzv[mid] = fr * symsamples / 2.0;
        hzv[mid - 1] = fr * symsamples / 2.0;
    }

    // repeat first and last symbols
    for (int i = 0; i < symsamples; i++)
    {
        hzv[i] = hzv[i + symsamples];
        hzv[symsamples * (symbols.size() + 1) + i] = hzv[symsamples * symbols.size() + i];
    }

    // run the per-sample frequency vector through
    // the gaussian filter.
    int half = gwin.size() / 2;
    std::vector<float> o(hzv.size());
    for (int i = 0; i < (int)o.size(); i++)
    {
        float sum = 0;
        for (int j = 0; j < (int)gwin.size(); j++)
        {
            int k = i - half + j;
            if (k >= 0 && k < (int)hzv.size())
            {
                sum += hzv[k] * gwin[j];
            }
        }
        o[i] = sum;
    }

    // drop repeated first and last symbols
    std::vector<float> oo(symsamples * symbols.size());
    for (int i = 0; i < (int)oo.size(); i++)
    {
        oo[i] = o[i + symsamples];
    }

    // now oo[i] contains the frequency for the i'th sample.

    std::vector<float> v(symsamples * symbols.size());
    float theta = phase0;
    for (int i = 0; i < (int)v.size(); i++)
    {
        v[i] = cos(theta);
        float hz = oo[i];
        theta += 2 * M_PI / (rate / hz);
    }

    return v;
}

const std::string WHITESPACE = " \n\r\t\f\v";

std::string ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string &s) {
    return rtrim(ltrim(s));
}

} // namespace FT8
