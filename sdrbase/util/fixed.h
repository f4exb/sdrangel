///////////////////////////////////////////////////////////////////////////////////
// (C) Copyright 2007 Anthony Williams                                           //
//                                                                               //
// Distributed under the Boost Software License, Version 1.0.                    //
// See: http://www.boost.org/LICENSE_1_0.txt)                                    //
//                                                                               //
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Modified as fully templatized class with variable size and type internal      //
// representation                                                                //
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

#ifndef SDRBASE_UTIL_FIXED_H_
#define SDRBASE_UTIL_FIXED_H_

#include <ostream>
#include <complex>
#include <stdint.h>

#include "fixedtraits.h"

// Internally 1 is 2^28. 28 is the highest power of two that can represent 9.99999... safely on 64 bits internally

unsigned const fixed_resolution_shift=28;
int64_t const fixed_resolution=1LL<<fixed_resolution_shift;

template<typename IntType, uint32_t IntBits>
class Fixed
{
private:
    IntType m_nVal;

public:

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
        return m_nVal?true:false;
    }

    inline operator double() const
    {
        return as_double();
    }

    int64_t as_internal() const
    {
        return m_nVal;
    }

    float as_float() const
    {
        return m_nVal/(float)fixed_resolution;
    }

    double as_double() const
    {
        return m_nVal/(double)fixed_resolution;
    }

    int64_t as_long() const
    {
        return (int64_t)(m_nVal/fixed_resolution);
    }
    int64_t as_int64() const
    {
        return m_nVal/fixed_resolution;
    }

    int as_int() const
    {
        return (int)(m_nVal/fixed_resolution);
    }

    uint64_t as_unsigned_long() const
    {
        return (uint64_t)(m_nVal/fixed_resolution);
    }
    uint64_t as_unsigned_int64() const
    {
        return (uint64_t)m_nVal/fixed_resolution;
    }

    unsigned int as_unsigned_int() const
    {
        return (unsigned int)(m_nVal/fixed_resolution);
    }

    short as_short() const
    {
        return (short)(m_nVal/fixed_resolution);
    }

    unsigned short as_unsigned_short() const
    {
        return (unsigned short)(m_nVal/fixed_resolution);
    }

    Fixed operator++()
    {
        m_nVal += fixed_resolution;
        return *this;
    }

    Fixed operator--()
    {
        m_nVal -= fixed_resolution;
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
        return (*this)*=Fixed(val);
    }

    Fixed& operator*=(float val)
    {
        return (*this)*=Fixed(val);
    }

    Fixed& operator*=(int64_t val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(int val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(short val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(char val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(uint64_t val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(unsigned int val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(unsigned short val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator*=(unsigned char val)
    {
        m_nVal*=val;
        return *this;
    }

    Fixed& operator/=(double val)
    {
        return (*this)/=Fixed(val);
    }

    Fixed& operator/=(float val)
    {
        return (*this)/=Fixed(val);
    }

    Fixed& operator/=(int64_t val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(int val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(short val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(char val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(uint64_t val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(unsigned int val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(unsigned short val)
    {
        m_nVal/=val;
        return *this;
    }

    Fixed& operator/=(unsigned char val)
    {
        m_nVal/=val;
        return *this;
    }

    bool operator!() const
    {
        return m_nVal==0;
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
inline Fixed<IntType, IntBits> operator-(Fixed const& a, unsigned b)
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
    Fixed temp(a);
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
inline Fixed<IntType, IntBits> operator+(Fixed const& a, double b)
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
inline bool operator<(short a, Fixed const& b)
{
    return Fixed(a)<b;
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
    if(m_nVal%fixed_resolution)
    {
        return floor()+1;
    }
    else
    {
        return *this;
    }
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::floor() const
{
    Fixed res(*this);
    int64_t const remainder=m_nVal%fixed_resolution;
    if(remainder)
    {
        res.m_nVal-=remainder;
        if(m_nVal<0)
        {
            res-=1;
        }
    }
    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::sin() const
{
    Fixed res;
    sin_cos(*this,&res,0);
    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::cos() const
{
    Fixed res;
    sin_cos(*this,0,&res);
    return res;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::tan() const
{
    Fixed s,c;
    sin_cos(*this,&s,&c);
    return s/c;
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::operator-() const
{
    return Fixed(internal(),-m_nVal);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::abs() const
{
    return Fixed(internal(),m_nVal<0?-m_nVal:m_nVal);
}

template<typename IntType, uint32_t IntBits>
inline Fixed<IntType, IntBits> Fixed<IntType, IntBits>::modf(Fixed*integral_part) const
{
    int64_t fractional_part=m_nVal%fixed_resolution;
    if(m_nVal<0 && fractional_part>0)
    {
        fractional_part-=fixed_resolution;
    }
    integral_part->m_nVal=m_nVal-fractional_part;
    return Fixed(internal(),fractional_part);
}

template<typename IntType, uint32_t IntBits>
inline Fixed arg(const std::complex<Fixed<IntType, IntBits>>& val)
{
    Fixed<IntType, IntBits> r,theta;
    Fixed<IntType, IntBits>::to_polar(val.real(),val.imag(),&r,&theta);
    return theta;
}

template<typename IntType, uint32_t IntBits>
inline std::complex<Fixed<IntType, IntBits>> polar(Fixed<IntType, IntBits> const& rho, Fixed<IntType, IntBits> const& theta)
{
    Fixed<IntType, IntBits> s,c;
    Fixed<IntType, IntBits>::sin_cos(theta,&s,&c);
    return std::complex<Fixed<IntType, IntBits>>(rho * c, rho * s);
}

Fixed const fixed_max(Fixed::internal(),0x7fffffffffffffffLL);
Fixed const fixed_one(Fixed::internal(),1LL<<(fixed_resolution_shift));
Fixed const fixed_zero(Fixed::internal(),0);
Fixed const fixed_half(Fixed::internal(),1LL<<(fixed_resolution_shift-1));
extern Fixed const fixed_pi;
extern Fixed const fixed_two_pi;
extern Fixed const fixed_half_pi;
extern Fixed const fixed_quarter_pi;

#endif /* SDRBASE_UTIL_FIXED_H_ */
