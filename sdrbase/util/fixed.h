///////////////////////////////////////////////////////////////////////////////////
// (C) Copyright 2007 Anthony Williams                                           //
//                                                                               //
// Distributed under the Boost Software License, Version 1.0.                    //
// See: http://www.boost.org/LICENSE_1_0.txt)                                    //
//                                                                               //
// Original article:                                                             //
// http://www.drdobbs.com/cpp/optimizing-math-intensive-applications-w/207000448 //
//                                                                               //
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Modified as fully templatized class with variable size and type internal      //
// representation                                                                //
//                                                                               //
// sqrt requires even IntBits                                                    //
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

#ifndef SDRBASE_UTIL_FIXED_H_
#define SDRBASE_UTIL_FIXED_H_

#include <ostream>
#include <complex>
#include <limits>
#include <stdint.h>

#include "fixedtraits.h"

template<typename IntType, uint32_t IntBits>
class Fixed
{
private:
    IntType m_nVal;

    static int64_t scale_cordic_result(int64_t a);
    static int64_t right_shift(int64_t val,int shift);
    static void perform_cordic_rotation(int64_t&px, int64_t&py, int64_t theta);
    static void perform_cordic_polarization(int64_t& argx, int64_t&argy);

public:
    static const Fixed fixed_max;
    static const Fixed fixed_one;
    static const Fixed fixed_zero;
    static const Fixed fixed_half;
    static const Fixed fixed_pi;
    static const Fixed fixed_two_pi;
    static const Fixed fixed_half_pi;
    static const Fixed fixed_quarter_pi;

    struct internal
    {};

    Fixed():
        m_nVal(0)
    {}

    Fixed(internal, IntType nVal):
        m_nVal(nVal)
    {}
    Fixed(int64_t nVal):
        m_nVal(nVal << FixedTraits<IntBits>::fixed_resolution_shift)
    {}

    Fixed(int nVal):
        m_nVal(int64_t(nVal) << FixedTraits<IntBits>::fixed_resolution_shift)
    {}

    Fixed(short nVal):
        m_nVal(int64_t(nVal) << FixedTraits<IntBits>::fixed_resolution_shift)
    {}

    Fixed(uint64_t nVal):
        m_nVal(nVal << FixedTraits<IntBits>::fixed_resolution_shift)
    {}

    Fixed(unsigned int nVal):
        m_nVal(int64_t(nVal) << FixedTraits<IntBits>::fixed_resolution_shift)
    {}
    Fixed(unsigned short nVal):
        m_nVal(int64_t(nVal) << FixedTraits<IntBits>::fixed_resolution_shift)
    {}
    Fixed(double nVal):
        m_nVal(static_cast<int64_t>(nVal*static_cast<double>(FixedTraits<IntBits>::fixed_resolution)))
    {}
    Fixed(float nVal):
        m_nVal(static_cast<int64_t>(nVal*static_cast<float>(FixedTraits<IntBits>::fixed_resolution)))
    {}

    template<typename T>
    Fixed& operator=(T other)
    {
        m_nVal = Fixed(other).m_nVal;
        return *this;
    }

    Fixed& operator=(Fixed const& other)
    {
        m_nVal = other.m_nVal;
        return *this;
    }

    friend bool operator==(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal == rhs.m_nVal;
    }

    friend bool operator!=(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal != rhs.m_nVal;
    }

    friend bool operator<(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal < rhs.m_nVal;
    }

    friend bool operator>(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal > rhs.m_nVal;
    }

    friend bool operator<=(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal <= rhs.m_nVal;
    }

    friend bool operator>=(Fixed const& lhs,Fixed const& rhs)
    {
        return lhs.m_nVal >= rhs.m_nVal;
    }

    operator bool() const
    {
        return m_nVal ? true : false;
    }

    inline operator double() const
    {
        return as_double();
    }

    IntType as_internal() const
    {
        return m_nVal;
    }

    float as_float() const
    {
        return m_nVal / (float) FixedTraits<IntBits>::fixed_resolution;
    }

    double as_double() const
    {
        return m_nVal / (double) FixedTraits<IntBits>::fixed_resolution;
    }

    int64_t as_long() const
    {
        return (int64_t) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }
    int64_t as_int64() const
    {
        return m_nVal / FixedTraits<IntBits>::fixed_resolution;
    }

    int as_int() const
    {
        return (int) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }

    uint64_t as_unsigned_long() const
    {
        return (uint64_t) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }
    uint64_t as_unsigned_int64() const
    {
        return (uint64_t) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }

    unsigned int as_unsigned_int() const
    {
        return (unsigned int) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }

    short as_short() const
    {
        return (short) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }

    unsigned short as_unsigned_short() const
    {
        return (unsigned short) (m_nVal / FixedTraits<IntBits>::fixed_resolution);
    }

    Fixed operator++()
    {
        m_nVal += FixedTraits<IntBits>::fixed_resolution;
        return *this;
    }

    Fixed operator--()
    {
        m_nVal -= FixedTraits<IntBits>::fixed_resolution;
        return *this;
    }

    Fixed floor() const;
    Fixed ceil() const;
    Fixed sqrt() const;
    Fixed exp() const;
    Fixed log() const;
    Fixed& operator%=(Fixed const& other);
    Fixed& operator*=(Fixed const& val);
    Fixed& operator/=(Fixed const& val);

    Fixed& operator-=(Fixed const& val)
    {
        m_nVal -= val.m_nVal;
        return *this;
    }

    Fixed& operator+=(Fixed const& val)
    {
        m_nVal += val.m_nVal;
        return *this;
    }

    Fixed& operator*=(double val)
    {
        return (*this) *= Fixed(val);
    }

    Fixed& operator*=(float val)
    {
        return (*this) *= Fixed(val);
    }

    Fixed& operator*=(int64_t val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(int val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(short val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(char val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(uint64_t val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(unsigned int val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(unsigned short val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator*=(unsigned char val)
    {
        m_nVal *= val;
        return *this;
    }

    Fixed& operator/=(double val)
    {
        return (*this) /= Fixed(val);
    }

    Fixed& operator/=(float val)
    {
        return (*this) /= Fixed(val);
    }

    Fixed& operator/=(int64_t val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(int val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(short val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(char val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(uint64_t val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(unsigned int val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(unsigned short val)
    {
        m_nVal /= val;
        return *this;
    }

    Fixed& operator/=(unsigned char val)
    {
        m_nVal /= val;
        return *this;
    }

    bool operator!() const
    {
        return m_nVal == 0;
    }

    Fixed modf(Fixed* integral_part) const;
    Fixed atan() const;

    static void sin_cos(Fixed const& theta,Fixed* s,Fixed*c);
    static void to_polar(Fixed const& x,Fixed const& y,Fixed* r,Fixed*theta);

    Fixed sin() const;
    Fixed cos() const;
    Fixed tan() const;
    Fixed operator-() const;
    Fixed abs() const;
};

/* why always as double ?
inline std::ostream& operator<<(std::ostream& os,fixed const& value)
{
    return os<<value.as_double();
}
*/

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits>& Fixed<IntType, IntBits>::operator%=(Fixed<IntType, IntBits> const& other)
{
    m_nVal = m_nVal%other.m_nVal;
    return *this;
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits>& Fixed<IntType, IntBits>::operator*=(Fixed<IntType, IntBits> const& val)
{
    bool const val_negative = val.m_nVal < 0;
    bool const this_negative = m_nVal < 0;
    bool const negate = val_negative ^ this_negative;
    uint64_t const other = val_negative ? -val.m_nVal : val.m_nVal;
    uint64_t const self = this_negative ? -m_nVal : m_nVal;

    if (uint64_t const self_upper = (self >> 32)) {
        m_nVal = (self_upper*other) << (32 - FixedTraits<IntBits>::fixed_resolution_shift);
    } else {
        m_nVal = 0;
    }

    if (uint64_t const self_lower = (self&0xffffffff))
    {
        uint64_t const other_upper = static_cast<uint64_t>(other >> 32);
        uint64_t const other_lower = static_cast<uint64_t>(other & 0xffffffff);
        uint64_t const lower_self_upper_other_res = self_lower*other_upper;
        uint64_t const lower_self_lower_other_res = self_lower*other_lower;
        m_nVal += (lower_self_upper_other_res << (32 - FixedTraits<IntBits>::fixed_resolution_shift))
            + (lower_self_lower_other_res >> FixedTraits<IntBits>::fixed_resolution_shift);
    }

    if (negate) {
        m_nVal=-m_nVal;
    }

    return *this;
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits>& Fixed<IntType, IntBits>::operator/=(Fixed<IntType, IntBits> const& divisor)
{
    if( !divisor.m_nVal)
    {
        m_nVal = fixed_max.m_nVal;
    }
    else
    {
        bool const negate_this = (m_nVal < 0);
        bool const negate_divisor = (divisor.m_nVal < 0);
        bool const negate = negate_this ^ negate_divisor;
        uint64_t a = negate_this ? -m_nVal : m_nVal;
        uint64_t b = negate_divisor ? -divisor.m_nVal : divisor.m_nVal;

        uint64_t res = 0;

        uint64_t temp = b;
        bool const a_large = a > b;
        unsigned shift = FixedTraits<IntBits>::fixed_resolution_shift;

        if(a_large)
        {
            uint64_t const half_a = a>>1;

            while (temp < half_a)
            {
                temp <<= 1;
                ++shift;
            }
        }

        uint64_t d = 1LL << shift;

        if (a_large)
        {
            a -= temp;
            res += d;
        }

        while (a && temp && shift)
        {
            unsigned right_shift = 0;

            while(right_shift < shift && (temp > a))
            {
                temp >>= 1;
                ++right_shift;
            }

            d >>= right_shift;
            shift -= right_shift;
            a -= temp;
            res += d;
        }

        m_nVal = (negate ? -(int64_t) res : res);
    }

    return *this;
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits> Fixed<IntType, IntBits>::sqrt() const
{
    unsigned int const max_shift = 62;
    uint64_t a_squared = 1LL << max_shift;
    unsigned int b_shift = (max_shift + FixedTraits<IntBits>::fixed_resolution_shift) / 2;
    uint64_t a = 1LL << b_shift;

    uint64_t x = m_nVal;

    while (b_shift && (a_squared > x))
    {
        a >>= 1;
        a_squared >>= 2;
        --b_shift;
    }

    uint64_t remainder = x - a_squared;
    --b_shift;

    while (remainder && b_shift)
    {
        uint64_t b_squared = 1LL << (2*b_shift - FixedTraits<IntBits>::fixed_resolution_shift);
        int const two_a_b_shift = b_shift + 1 - FixedTraits<IntBits>::fixed_resolution_shift;
        uint64_t two_a_b = (two_a_b_shift > 0) ? (a << two_a_b_shift) : (a >> -two_a_b_shift);

        while (b_shift && (remainder < (b_squared+two_a_b)))
        {
            b_squared >>= 2;
            two_a_b >>= 1;
            --b_shift;
        }

        uint64_t const delta = b_squared + two_a_b;

        if ((2*remainder) > delta)
        {
            a += (1LL << b_shift);
            remainder -= delta;

            if (b_shift) {
                --b_shift;
            }
        }
    }

    return Fixed<IntType, IntBits>(Fixed<IntType, IntBits>::internal(), a);
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits> Fixed<IntType, IntBits>::exp() const
{
    if (m_nVal >= FixedTraits<IntBits>::log_two_power_n_reversed[0])
    {
        return fixed_max;
    }

    if (m_nVal < -FixedTraits<IntBits>::log_two_power_n_reversed[63 - 2*FixedTraits<IntBits>::fixed_resolution_shift])
    {
        return Fixed<IntType, IntBits>(internal(), 0);
    }

    if (!m_nVal)
    {
        return Fixed<IntType, IntBits>(internal(), FixedTraits<IntBits>::fixed_resolution);
    }

    int64_t res = FixedTraits<IntBits>::fixed_resolution;

    if (m_nVal > 0)
    {
        int power = FixedTraits<IntBits>::max_power;
        int64_t const* log_entry = FixedTraits<IntBits>::log_two_power_n_reversed;
        int64_t temp=m_nVal;

        while (temp && power>(-(int)FixedTraits<IntBits>::fixed_resolution_shift))
        {
            while (!power || (temp<*log_entry))
            {
                if (!power) {
                    log_entry = FixedTraits<IntBits>::log_one_plus_two_power_minus_n;
                } else {
                    ++log_entry;
                }

                --power;
            }

            temp -= *log_entry;

            if (power < 0) {
                res += (res >> (-power));
            } else {
                res <<= power;
            }
        }
    }
    else
    {
        int power = FixedTraits<IntBits>::fixed_resolution_shift;
        int64_t const* log_entry = FixedTraits<IntBits>::log_two_power_n_reversed + (FixedTraits<IntBits>::max_power-power);
        int64_t temp=m_nVal;

        while (temp && (power > (-(int) FixedTraits<IntBits>::fixed_resolution_shift)))
        {
            while (!power || (temp > (-*log_entry)))
            {
                if(!power) {
                    log_entry = FixedTraits<IntBits>::log_one_over_one_minus_two_power_minus_n;
                } else {
                    ++log_entry;
                }

                --power;
            }

            temp += *log_entry;

            if (power <0 ) {
                res -= (res >> (-power));
            } else {
                res >>= power;
            }
        }
    }

    return Fixed<IntType, IntBits>(Fixed<IntType, IntBits>::internal(), res);
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits> Fixed<IntType, IntBits>::log() const
{
    if ( m_nVal <= 0) {
        return -fixed_max;
    }

    if (m_nVal == FixedTraits<IntBits>::fixed_resolution) {
        return fixed_zero;
    }

    uint64_t temp = m_nVal;
    int left_shift = 0;
    uint64_t const scale_position = 0x8000000000000000ULL;

    while (temp < scale_position)
    {
        ++left_shift;
        temp <<= 1;
    }

    int64_t res = (left_shift < FixedTraits<IntBits>::max_power) ?
            FixedTraits<IntBits>::log_two_power_n_reversed[left_shift] :
            -FixedTraits<IntBits>::log_two_power_n_reversed[2*FixedTraits<IntBits>::max_power - left_shift];
    unsigned int right_shift = 1;
    uint64_t shifted_temp = temp >> 1;

    while (temp && (right_shift < FixedTraits<IntBits>::fixed_resolution_shift))
    {
        while ((right_shift < FixedTraits<IntBits>::fixed_resolution_shift) && (temp < (shifted_temp + scale_position)))
        {
            shifted_temp >>= 1;
            ++right_shift;
        }

        temp -= shifted_temp;
        shifted_temp = temp >> right_shift;
        res += FixedTraits<IntBits>::log_one_over_one_minus_two_power_minus_n[right_shift-1];
    }

    return Fixed<IntType, IntBits>(Fixed<IntType, IntBits>::internal(), res);
}

template<typename IntType, uint32_t IntBits>
int64_t Fixed<IntType, IntBits>::scale_cordic_result(int64_t a)
{
    int64_t const cordic_scale_factor = 0x22C2DD1C; /* 0.271572 * 2^31*/
    return (int64_t)((((int64_t)a)*cordic_scale_factor) >> 31);
}

template<typename IntType, uint32_t IntBits>
int64_t Fixed<IntType, IntBits>::right_shift(int64_t val, int shift)
{
    return (shift < 0) ? (val << -shift) : (val >> shift);
}

template<typename IntType, uint32_t IntBits>
void Fixed<IntType, IntBits>::perform_cordic_rotation(int64_t&px, int64_t&py, int64_t theta)
{
    int64_t x = px, y = py;
    int64_t const *arctanptr = FixedTraits<IntBits>::arctantab;
    for (int i = -1; i <= (int) FixedTraits<IntBits>::fixed_resolution_shift; ++i)
    {
        int64_t const yshift = right_shift(y,i);
        int64_t const xshift = right_shift(x,i);

        if (theta < 0)
        {
            x += yshift;
            y -= xshift;
            theta += *arctanptr++;
        }
        else
        {
            x -= yshift;
            y += xshift;
            theta -= *arctanptr++;
        }
    }

    px = scale_cordic_result(x);
    py = scale_cordic_result(y);
}

template<typename IntType, uint32_t IntBits>
void Fixed<IntType, IntBits>::perform_cordic_polarization(int64_t& argx, int64_t&argy)
{
    int64_t theta = 0;
    int64_t x = argx, y = argy;
    int64_t const *arctanptr = FixedTraits<IntBits>::arctantab;

    for (int i = -1; i <= (int) FixedTraits<IntBits>::fixed_resolution_shift; ++i)
    {
        int64_t const yshift = right_shift(y,i);
        int64_t const xshift = right_shift(x,i);

        if(y < 0)
        {
            y += xshift;
            x -= yshift;
            theta -= *arctanptr++;
        }
        else
        {
            y -= xshift;
            x += yshift;
            theta += *arctanptr++;
        }
    }

    argx = scale_cordic_result(x);
    argy = theta;
}

template<typename IntType, uint32_t IntBits>
void Fixed<IntType, IntBits>::sin_cos(Fixed<IntType, IntBits> const& theta, Fixed<IntType, IntBits>* s, Fixed<IntType, IntBits>*c)
{
    int64_t x = theta.m_nVal % FixedTraits<IntBits>::internal_two_pi;

    if (x < 0) {
        x += FixedTraits<IntBits>::internal_two_pi;
    }

    bool negate_cos = false;
    bool negate_sin = false;

    if (x > FixedTraits<IntBits>::internal_pi)
    {
        x = FixedTraits<IntBits>::internal_two_pi - x;
        negate_sin=true;
    }

    if (x > FixedTraits<IntBits>::internal_half_pi)
    {
        x = FixedTraits<IntBits>::internal_pi - x;
        negate_cos=true;
    }

    int64_t x_cos = 1<<FixedTraits<IntBits>::fixed_resolution_shift, x_sin = 0;

    perform_cordic_rotation(x_cos, x_sin, (int64_t) x);

    if (s) {
        s->m_nVal = negate_sin ? -x_sin : x_sin;
    }

    if(c) {
        c->m_nVal = negate_cos ? -x_cos : x_cos;
    }
}

template<typename IntType, uint32_t IntBits>
Fixed<IntType, IntBits> Fixed<IntType, IntBits>::atan() const
{
    Fixed<IntType, IntBits> r, theta;
    to_polar(1, *this, &r, &theta);
    return theta;
}

template<typename IntType, uint32_t IntBits>
void Fixed<IntType, IntBits>::to_polar(Fixed<IntType, IntBits> const& x, Fixed<IntType, IntBits> const& y, Fixed<IntType, IntBits>* r, Fixed<IntType, IntBits> *theta)
{
    bool const negative_x = x.m_nVal < 0;
    bool const negative_y = y.m_nVal < 0;

    uint64_t a = negative_x ? -x.m_nVal : x.m_nVal;
    uint64_t b = negative_y ? -y.m_nVal : y.m_nVal;

    unsigned int right_shift = 0;
    unsigned const max_value = 1U << FixedTraits<IntBits>::fixed_resolution_shift;

    while ((a >= max_value) || (b >= max_value))
    {
        ++right_shift;
        a >>= 1;
        b >>= 1;
    }

    int64_t xtemp = (int64_t) a;
    int64_t ytemp = (int64_t) b;

    perform_cordic_polarization(xtemp, ytemp);

    r->m_nVal = int64_t(xtemp) << right_shift;
    theta->m_nVal = ytemp;

    if (negative_x && negative_y) {
        theta->m_nVal -= FixedTraits<IntBits>::internal_pi;
    } else if (negative_x) {
        theta->m_nVal = FixedTraits<IntBits>::internal_pi - theta->m_nVal;
    }

    else if(negative_y) {
        theta->m_nVal = -theta->m_nVal;
    }
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(double a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}


template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(float a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(int64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(unsigned a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(int a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, double b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, float b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a,uint64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, int64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, unsigned b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, int b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator-(Fixed<IntType, IntBits> const& a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp -= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(double a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(float a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(int64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(unsigned a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(int a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a,double b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, float b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, int64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, unsigned b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, int b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator%(Fixed<IntType, IntBits> const& a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp %= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(double a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(float a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(int64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(unsigned a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(int a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, double b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, float b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, int64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, unsigned b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, int b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator+(Fixed<IntType, IntBits> const& a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp += b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(double a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(float a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(int64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(unsigned a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(int a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(b);
    return temp *= a;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, double b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, float b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, int64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, unsigned b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, int b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator*(Fixed<IntType, IntBits> const& a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp *= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(double a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(float a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(int64_t a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(unsigned a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(int a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(short a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(char a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, double b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, float b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, int64_t b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, unsigned b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, int b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, short b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, char b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> operator/(Fixed<IntType, IntBits> const& a, Fixed<IntType, IntBits> const& b)
{
    Fixed<IntType, IntBits> temp(a);
    return temp /= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) == b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, double b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, float b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, int b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, short b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator==(Fixed<IntType, IntBits> const& a, char b)
{
    return a == Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) != b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, double b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, float b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, int b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, short b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator!=(Fixed<IntType, IntBits> const& a, char b)
{
    return a != Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) < b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, double b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, float b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, int b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, short b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<(Fixed<IntType, IntBits> const& a, char b)
{
    return a < Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) > b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, double b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, float b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, int b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, short b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>(Fixed<IntType, IntBits> const& a, char b)
{
    return a > Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) <= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, double b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, float b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, int b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a, short b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a,unsigned char b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator<=(Fixed<IntType, IntBits> const& a,char b)
{
    return a <= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(double a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(float a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(uint64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(int64_t a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(unsigned a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(int a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(unsigned short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(short a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a)>=b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(unsigned char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(char a, Fixed<IntType, IntBits> const& b)
{
    return Fixed<IntType, IntBits>(a) >= b;
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, double b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a,float b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, uint64_t b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, int64_t b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, unsigned b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, int b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, unsigned short b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, short b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, unsigned char b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline bool operator>=(Fixed<IntType, IntBits> const& a, char b)
{
    return a >= Fixed<IntType, IntBits>(b);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> sin(Fixed<IntType, IntBits> const& x)
{
    return x.sin();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> cos(Fixed<IntType, IntBits> const& x)
{
    return x.cos();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> tan(Fixed<IntType, IntBits> const& x)
{
    return x.tan();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> sqrt(Fixed<IntType, IntBits> const& x)
{
    return x.sqrt();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> exp(Fixed<IntType, IntBits> const& x)
{
    return x.exp();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> log(Fixed<IntType, IntBits> const& x)
{
    return x.log();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> floor(Fixed<IntType, IntBits> const& x)
{
    return x.floor();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> ceil(Fixed<IntType, IntBits> const& x)
{
    return x.ceil();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> abs(Fixed<IntType, IntBits> const& x)
{
    return x.abs();
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> modf(Fixed<IntType, IntBits> const& x, Fixed<IntType, IntBits>*integral_part)
{
    return x.modf(integral_part);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::ceil() const
{
    if (m_nVal % FixedTraits<IntBits>::fixed_resolution) {
        return floor()+1;
    } else {
        return *this;
    }
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::floor() const
{
    Fixed res(*this);
    int64_t const remainder = m_nVal % FixedTraits<IntBits>::fixed_resolution;

    if (remainder)
    {
        res.m_nVal -= remainder;

        if(m_nVal<0) {
            res-=1;
        }
    }

    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::sin() const
{
    Fixed res;
    sin_cos(*this, &res, 0);
    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::cos() const
{
    Fixed res;
    sin_cos(*this, 0, &res);
    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::tan() const
{
    Fixed s, c;
    sin_cos(*this, &s, &c);
    return s/c;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::operator-() const
{
    return Fixed(internal(), -m_nVal);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::abs() const
{
    return Fixed<IntType, IntBits>(Fixed<IntType, IntBits>::internal(), m_nVal < 0 ? -m_nVal : m_nVal);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::modf(Fixed<IntType, IntBits> *integral_part) const
{
    int64_t fractional_part = m_nVal % FixedTraits<IntBits>::fixed_resolution;
    if ((m_nVal < 0) && (fractional_part > 0)) {
        fractional_part -= FixedTraits<IntBits>::fixed_resolution;
    }

    integral_part->m_nVal = m_nVal - fractional_part;
    return Fixed<IntType, IntBits>(Fixed<IntType, IntBits>::internal(), fractional_part);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> arg(const std::complex<Fixed<IntType, IntBits> >& val)
{
    Fixed<IntType, IntBits> r,theta;
    Fixed<IntType, IntBits>::to_polar(val.real(),val.imag(),&r,&theta);
    return theta;
}

template<typename IntType, uint32_t IntBits>
inline std::complex<Fixed<IntType, IntBits> > polar(const Fixed<IntType, IntBits>& rho, const Fixed<IntType, IntBits>& theta)
{
    Fixed<IntType, IntBits> s,c;
    Fixed<IntType, IntBits>::sin_cos(theta,&s,&c);
    return std::complex<Fixed<IntType, IntBits> >(rho * c, rho * s);
}

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_max(Fixed<IntType, IntBits>::internal(), std::numeric_limits<IntType>::max());

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_one(Fixed<IntType, IntBits>::internal(), 1LL << (FixedTraits<IntBits>::fixed_resolution_shift));

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_zero(Fixed<IntType, IntBits>::internal(), 0);

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_half(Fixed<IntType, IntBits>::internal(), 1LL << (FixedTraits<IntBits>::fixed_resolution_shift-1));

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_pi(Fixed<IntType, IntBits>::internal(), FixedTraits<IntBits>::internal_pi);

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_two_pi(Fixed<IntType, IntBits>::internal(), FixedTraits<IntBits>::internal_two_pi);

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_half_pi(Fixed<IntType, IntBits>::internal(), FixedTraits<IntBits>::internal_half_pi);

template<typename IntType, uint32_t IntBits>
const Fixed<IntType, IntBits> Fixed<IntType, IntBits>::fixed_quarter_pi(Fixed<IntType, IntBits>::internal(), FixedTraits<IntBits>::internal_quarter_pi);

#endif /* SDRBASE_UTIL_FIXED_H_ */
