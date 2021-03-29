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

#ifndef LEANSDR_MATH_H
#define LEANSDR_MATH_H

#include <cmath>
#include <complex>
#include <stdint.h>

namespace leansdr
{

template <typename T>
T dotprod(const T *u, const T *v, int n)
{
    T acc = 0;

    while (n--) {
        acc += (*u++) * (*v++);
    }

    return acc;
}

template <typename T>
inline T cnorm2(const std::complex<T> &u)
{
    return u.real() * u.real() + u.imag() * u.imag();
}

template <typename T>
T cnorm2(const std::complex<T> *p, int n)
{
    T res = 0;

    for (; n--; ++p) {
        res += cnorm2(*p);
    }

    return res;
}

// Return conj(u)*v
template <typename T>
inline std::complex<T> conjprod(const std::complex<T> &u, const std::complex<T> &v)
{
    return std::complex<T>(
        u.real() * v.real() + u.imag() * v.imag(),
        u.real() * v.imag() - u.imag() * v.real()
    );
}

// Return sum(conj(u[i])*v[i])
template <typename T>
std::complex<T> conjprod(const std::complex<T> *u, const std::complex<T> *v, int n)
{
    std::complex<T> acc = 0;

    while (n--) {
        acc += conjprod(*u++, *v++);
    }

    return acc;
}

// TBD Optimize with dedicated instructions
int hamming_weight(uint8_t x);
int hamming_weight(uint16_t x);
int hamming_weight(uint32_t x);
int hamming_weight(uint64_t x);
unsigned char parity(uint8_t x);
unsigned char parity(uint16_t x);
unsigned char parity(uint32_t x);
unsigned char parity(uint64_t x);
int log2i(uint64_t x);

// Pre-computed sin/cos for 16-bit angles

struct trig16
{
    std::complex<float> lut[65536]; // TBD static and shared

    trig16()
    {
        for (int a = 0; a < 65536; ++a)
        {
            float af = a * 2 * M_PI / 65536;
            lut[a] = {cosf(af), sinf(af)};
        }
    }

    inline const std::complex<float> &expi(uint16_t a) const
    {
        return lut[a];
    }

    // a must fit in a int32_t, otherwise behaviour is undefined
    inline const std::complex<float> &expi(float a) const
    {
        return expi((uint16_t)(int16_t)(int32_t)a);
    }
};

// Modulo with signed result in [-m/2..m/2[

inline float fmodfs(float v, float m)
{
    v = fmodf(v, m);
    return (v>=m/2) ? v-m : (v<-m/2) ? v+m : v;
}

inline double rand_compat()
{
#ifdef WIN32
    return double(rand())/RAND_MAX;
#else
    return drand48();
#endif
}

// Simple statistics

template<typename T>
struct statistics
{
    statistics() {
        reset();
    }

    void reset()
    {
        vm1 = vm2 = 0;
        count = 0;
        vmin = vmax = 99;/*comp warning*/
    }

    void add(const T &v)
    {
        vm1 += v;
        vm2 += v*v;

        if ( count == 0 ) {
            vmin = vmax = v;
        } else if (
            v < vmin ) { vmin = v;
        } else if ( v > vmax ) {
            vmax = v;
        }

        ++count;
    }

    T average() { return vm1 / count; }
    T variance() { return vm2/count - (vm1/count)*(vm1/count); }
    T stddev() { return gen_sqrt(variance()); }
    T min() { return vmin; }
    T max() { return vmax; }

private:
    T vm1, vm2;    // Moments
    T vmin, vmax;  // Range
    int count;     // Number of samples in vm1, vm2
};  // statistics

} // namespace leansdr

#endif // LEANSDR_MATH_H
