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

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

namespace leansdr
{

template <typename T>
struct complex
{
    T re, im;
    complex() {}
    complex(T x) : re(x), im(0) {}
    complex(T x, T y) : re(x), im(y) {}
    inline void operator+=(const complex<T> &x)
    {
        re += x.re;
        im += x.im;
    }
    inline void operator*=(const complex<T> &c)
    {
        T tre = re * c.re - im * c.im;
        im = re * c.im + im * c.re;
        re = tre;
    }
    inline void operator*=(const T &k)
    {
        re *= k;
        im *= k;
    }
};

template <typename T>
complex<T> operator+(const complex<T> &a, const complex<T> &b)
{
    return complex<T>(a.re + b.re, a.im + b.im);
}

template <typename T>
complex<T> operator*(const complex<T> &a, const complex<T> &b)
{
    return complex<T>(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

template <typename T>
complex<T> operator*(const complex<T> &a, const T &k)
{
    return complex<T>(a.re * k, a.im * k);
}

template <typename T>
complex<T> operator*(const T &k, const complex<T> &a)
{
    return complex<T>(k * a.re, k * a.im);
}

template <typename T>
T dotprod(const T *u, const T *v, int n)
{
    T acc = 0;
    while (n--)
        acc += (*u++) * (*v++);
    return acc;
}

template <typename T>
inline T cnorm2(const complex<T> &u)
{
    return u.re * u.re + u.im * u.im;
}

template <typename T>
T cnorm2(const complex<T> *p, int n)
{
    T res = 0;
    for (; n--; ++p)
        res += cnorm2(*p);
    return res;
}

// Return conj(u)*v
template <typename T>
inline complex<T> conjprod(const complex<T> &u, const complex<T> &v)
{
    return complex<T>(u.re * v.re + u.im * v.im,
                      u.re * v.im - u.im * v.re);
}

// Return sum(conj(u[i])*v[i])
template <typename T>
complex<T> conjprod(const complex<T> *u, const complex<T> *v, int n)
{
    complex<T> acc = 0;
    while (n--)
        acc += conjprod(*u++, *v++);
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
    complex<float> lut[65536]; // TBD static and shared
    trig16()
    {
        for (int a = 0; a < 65536; ++a)
        {
            float af = a * 2 * M_PI / 65536;
            lut[a].re = cosf(af);
            lut[a].im = sinf(af);
        }
    }
    inline const complex<float> &expi(uint16_t a) const
    {
        return lut[a];
    }
    // a must fit in a int32_t, otherwise behaviour is undefined
    inline const complex<float> &expi(float a) const
    {
        return expi((uint16_t)(int16_t)(int32_t)a);
    }
};

} // namespace leansdr

#endif // LEANSDR_MATH_H
