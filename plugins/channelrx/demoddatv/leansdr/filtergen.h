// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_FILTERGEN_H
#define LEANSDR_FILTERGEN_H

#include <math.h>

#include "framework.h"

namespace leansdr
{
namespace filtergen
{

template <typename T>
void normalize_power(int n, T *coeffs, float gain = 1)
{
    float s2 = 0;

    for (int i = 0; i < n; ++i) {
        s2 = s2 + coeffs[i] * coeffs[i]; // TBD std::complex
    }

    if (s2) {
        gain /= gen_sqrt(s2);
    }

    for (int i = 0; i < n; ++i) {
        coeffs[i] = coeffs[i] * gain;
    }
}

template <typename T>
void normalize_dcgain(int n, T *coeffs, float gain = 1)
{
    float s = 0;

    for (int i = 0; i < n; ++i) {
        s = s + coeffs[i];
    }

    if (s) {
        gain /= s;
    }

    for (int i = 0; i < n; ++i) {
        coeffs[i] = coeffs[i] * gain;
    }
}

template <typename T>
void cancel_dcgain(int n, T *coeffs)
{
    float s = 0;

    for (int i = 0; i < n; ++i) {
        s = s + coeffs[i];
    }

    for (int i = 0; i < n; ++i) {
        coeffs[i] -= s / n;
    }
}

// Generate coefficients for a sinc filter.
// https://en.wikipedia.org/wiki/Sinc_filter

template <typename T>
int lowpass(int order, float Fcut, T **coeffs, float gain = 1)
{
    int ncoeffs = order + 1;
    *coeffs = new T[ncoeffs];

    for (int i = 0; i < ncoeffs; ++i)
    {
        float t = i - (ncoeffs - 1) * 0.5;
        float sinc = 2 * Fcut * (t ? sin(2 * M_PI * Fcut * t) / (2 * M_PI * Fcut * t) : 1);
#if 0 // Hamming
	float alpha = 25.0/46, beta = 21.0/46;
	float window = alpha - beta*cos(2*M_PI*i/order);
#else
        float window = 1;
#endif
        (*coeffs)[i] = sinc * window;
    }

    normalize_dcgain(ncoeffs, *coeffs, gain);
    return ncoeffs;
}

// Generate coefficients for a RRC filter.
// https://en.wikipedia.org/wiki/Root-raised-cosine_filter

template <typename T>
int root_raised_cosine(int order, float Fs, float rolloff, T **coeffs, float gain=1)
{
    float B = rolloff, pi = M_PI;
    int ncoeffs = (order + 1) | 1;
    *coeffs = new T[ncoeffs];

    for (int i = 0; i < ncoeffs; ++i)
    {
        int t = i - ncoeffs / 2;
        float c;

        if (t == 0)
        {
            c = (1 - B + 4*B/pi);
        }
        else
        {
            float tT = t * Fs;
            float den = pi * tT * (1 - (4 * B * tT) * (4 * B * tT));

            if (!den) {
                c = B/sqrtf(2) * ( (1+2/pi)*sinf(pi/(4*B)) + (1-2/pi)*cosf(pi/(4*B)) );
            } else {
                c = ( sinf(pi*tT*(1-B)) + 4*B*tT*cosf(pi*tT*(1+B)) ) / den;
            }
        }

        (*coeffs)[i] = Fs * c * gain;
    }

    return ncoeffs;
}

// Dump filter coefficients for matlab/octave
void dump_filter(const char *name, int ncoeffs, float *coeffs);

} // namespace filtergen
} // namespace leansdr

#endif // LEANSDR_FILTERGEN_H
