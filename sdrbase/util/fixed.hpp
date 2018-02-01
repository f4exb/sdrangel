#ifndef FIXED_HPP
#define FIXED_HPP
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007 Anthony Williams

#include <ostream>
#include <complex>
#include <stdint.h>

// Internally 1 is 2^28. 28 is the highest power of two that can represent 9.99999... safely on 64 bits internally

unsigned const fixed_resolution_shift=28;
int64_t const fixed_resolution=1LL<<fixed_resolution_shift;

class fixed
{
private:
    int64_t m_nVal;

public:

    struct internal
    {};

    fixed():
        m_nVal(0)
    {}
    
    fixed(internal, int64_t nVal):
        m_nVal(nVal)
    {}
    fixed(int64_t nVal):
        m_nVal(nVal<<fixed_resolution_shift)
    {}
    
    fixed(int nVal):
        m_nVal(int64_t(nVal)<<fixed_resolution_shift)
    {}
    
    fixed(short nVal):
        m_nVal(int64_t(nVal)<<fixed_resolution_shift)
    {}
    
    fixed(uint64_t nVal):
        m_nVal(nVal<<fixed_resolution_shift)
    {}
    
    fixed(unsigned int nVal):
        m_nVal(int64_t(nVal)<<fixed_resolution_shift)
    {}
    fixed(unsigned short nVal):
        m_nVal(int64_t(nVal)<<fixed_resolution_shift)
    {}
    fixed(double nVal):
        m_nVal(static_cast<int64_t>(nVal*static_cast<double>(fixed_resolution)))
    {}
    fixed(float nVal):
        m_nVal(static_cast<int64_t>(nVal*static_cast<float>(fixed_resolution)))
    {}

    template<typename T>
    fixed& operator=(T other)
    {
        m_nVal=fixed(other).m_nVal;
        return *this;
    }
    fixed& operator=(fixed const& other)
    {
        m_nVal=other.m_nVal;
        return *this;
    }
    friend bool operator==(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal==rhs.m_nVal;
    }
    friend bool operator!=(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal!=rhs.m_nVal;
    }
    friend bool operator<(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal<rhs.m_nVal;
    }
    friend bool operator>(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal>rhs.m_nVal;
    }
    friend bool operator<=(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal<=rhs.m_nVal;
    }
    friend bool operator>=(fixed const& lhs,fixed const& rhs)
    {
        return lhs.m_nVal>=rhs.m_nVal;
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

    fixed operator++()
    {
        m_nVal += fixed_resolution;
        return *this;
    }

    fixed operator--()
    {
        m_nVal -= fixed_resolution;
        return *this;
    }

    fixed floor() const;
    fixed ceil() const;
    fixed sqrt() const;
    fixed exp() const;
    fixed log() const;
    fixed& operator%=(fixed const& other);
    fixed& operator*=(fixed const& val);
    fixed& operator/=(fixed const& val);
    fixed& operator-=(fixed const& val)
    {
        m_nVal -= val.m_nVal;
        return *this;
    }

    fixed& operator+=(fixed const& val)
    {
        m_nVal += val.m_nVal;
        return *this;
    }
    fixed& operator*=(double val)
    {
        return (*this)*=fixed(val);
    }
    fixed& operator*=(float val)
    {
        return (*this)*=fixed(val);
    }
    fixed& operator*=(int64_t val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(int val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(short val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(char val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(uint64_t val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(unsigned int val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(unsigned short val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator*=(unsigned char val)
    {
        m_nVal*=val;
        return *this;
    }
    fixed& operator/=(double val)
    {
        return (*this)/=fixed(val);
    }
    fixed& operator/=(float val)
    {
        return (*this)/=fixed(val);
    }
    fixed& operator/=(int64_t val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(int val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(short val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(char val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(uint64_t val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(unsigned int val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(unsigned short val)
    {
        m_nVal/=val;
        return *this;
    }
    fixed& operator/=(unsigned char val)
    {
        m_nVal/=val;
        return *this;
    }
    

    bool operator!() const
    {
        return m_nVal==0;
    }
    
    fixed modf(fixed* integral_part) const;
    fixed atan() const;

    static void sin_cos(fixed const& theta,fixed* s,fixed*c);
    static void to_polar(fixed const& x,fixed const& y,fixed* r,fixed*theta);

    fixed sin() const;
    fixed cos() const;
    fixed tan() const;
    fixed operator-() const;
    fixed abs() const;
};

/* why always as double ?
inline std::ostream& operator<<(std::ostream& os,fixed const& value)
{
    return os<<value.as_double();
}
*/

inline fixed operator-(double a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}


inline fixed operator-(float a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(uint64_t a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(int64_t a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(unsigned a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(int a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(unsigned short a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(short a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(unsigned char a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(char a, fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,double b)
{
    fixed temp(a);
    return temp-=b;
}


inline fixed operator-(fixed const& a,float b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,uint64_t b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,int64_t b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,unsigned b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,int b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,unsigned short b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,short b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,unsigned char b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,char b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator-(fixed const& a,fixed const& b)
{
    fixed temp(a);
    return temp-=b;
}

inline fixed operator%(double a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}


inline fixed operator%(float a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(uint64_t a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(int64_t a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(unsigned a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(int a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(unsigned short a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(short a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(unsigned char a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(char a, fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,double b)
{
    fixed temp(a);
    return temp%=b;
}


inline fixed operator%(fixed const& a,float b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,uint64_t b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,int64_t b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,unsigned b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,int b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,unsigned short b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,short b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,unsigned char b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,char b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator%(fixed const& a,fixed const& b)
{
    fixed temp(a);
    return temp%=b;
}

inline fixed operator+(double a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}


inline fixed operator+(float a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(uint64_t a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(int64_t a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(unsigned a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(int a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(unsigned short a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(short a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(unsigned char a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(char a, fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,double b)
{
    fixed temp(a);
    return temp+=b;
}


inline fixed operator+(fixed const& a,float b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,uint64_t b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,int64_t b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,unsigned b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,int b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,unsigned short b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,short b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,unsigned char b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,char b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator+(fixed const& a,fixed const& b)
{
    fixed temp(a);
    return temp+=b;
}

inline fixed operator*(double a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}


inline fixed operator*(float a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(uint64_t a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(int64_t a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(unsigned a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(int a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(unsigned short a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(short a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(unsigned char a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(char a, fixed const& b)
{
    fixed temp(b);
    return temp*=a;
}

inline fixed operator*(fixed const& a,double b)
{
    fixed temp(a);
    return temp*=b;
}


inline fixed operator*(fixed const& a,float b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,uint64_t b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,int64_t b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,unsigned b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,int b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,unsigned short b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,short b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,unsigned char b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,char b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator*(fixed const& a,fixed const& b)
{
    fixed temp(a);
    return temp*=b;
}

inline fixed operator/(double a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}


inline fixed operator/(float a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(uint64_t a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(int64_t a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(unsigned a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(int a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(unsigned short a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(short a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(unsigned char a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(char a, fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,double b)
{
    fixed temp(a);
    return temp/=b;
}


inline fixed operator/(fixed const& a,float b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,uint64_t b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,int64_t b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,unsigned b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,int b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,unsigned short b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,short b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,unsigned char b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,char b)
{
    fixed temp(a);
    return temp/=b;
}

inline fixed operator/(fixed const& a,fixed const& b)
{
    fixed temp(a);
    return temp/=b;
}

inline bool operator==(double a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(float a, fixed const& b)
{
    return fixed(a)==b;
}

inline bool operator==(uint64_t a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(int64_t a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(unsigned a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(int a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(unsigned short a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(short a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(unsigned char a, fixed const& b)
{
    return fixed(a)==b;
}
inline bool operator==(char a, fixed const& b)
{
    return fixed(a)==b;
}

inline bool operator==(fixed const& a,double b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,float b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,uint64_t b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,int64_t b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,unsigned b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,int b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,unsigned short b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,short b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,unsigned char b)
{
    return a==fixed(b);
}
inline bool operator==(fixed const& a,char b)
{
    return a==fixed(b);
}

inline bool operator!=(double a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(float a, fixed const& b)
{
    return fixed(a)!=b;
}

inline bool operator!=(uint64_t a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(int64_t a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(unsigned a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(int a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(unsigned short a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(short a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(unsigned char a, fixed const& b)
{
    return fixed(a)!=b;
}
inline bool operator!=(char a, fixed const& b)
{
    return fixed(a)!=b;
}

inline bool operator!=(fixed const& a,double b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,float b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,uint64_t b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,int64_t b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,unsigned b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,int b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,unsigned short b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,short b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,unsigned char b)
{
    return a!=fixed(b);
}
inline bool operator!=(fixed const& a,char b)
{
    return a!=fixed(b);
}

inline bool operator<(double a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(float a, fixed const& b)
{
    return fixed(a)<b;
}

inline bool operator<(uint64_t a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(int64_t a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(unsigned a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(int a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(unsigned short a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(short a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(unsigned char a, fixed const& b)
{
    return fixed(a)<b;
}
inline bool operator<(char a, fixed const& b)
{
    return fixed(a)<b;
}

inline bool operator<(fixed const& a,double b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,float b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,uint64_t b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,int64_t b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,unsigned b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,int b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,unsigned short b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,short b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,unsigned char b)
{
    return a<fixed(b);
}
inline bool operator<(fixed const& a,char b)
{
    return a<fixed(b);
}

inline bool operator>(double a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(float a, fixed const& b)
{
    return fixed(a)>b;
}

inline bool operator>(uint64_t a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(int64_t a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(unsigned a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(int a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(unsigned short a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(short a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(unsigned char a, fixed const& b)
{
    return fixed(a)>b;
}
inline bool operator>(char a, fixed const& b)
{
    return fixed(a)>b;
}

inline bool operator>(fixed const& a,double b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,float b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,uint64_t b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,int64_t b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,unsigned b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,int b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,unsigned short b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,short b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,unsigned char b)
{
    return a>fixed(b);
}
inline bool operator>(fixed const& a,char b)
{
    return a>fixed(b);
}

inline bool operator<=(double a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(float a, fixed const& b)
{
    return fixed(a)<=b;
}

inline bool operator<=(uint64_t a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(int64_t a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(unsigned a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(int a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(unsigned short a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(short a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(unsigned char a, fixed const& b)
{
    return fixed(a)<=b;
}
inline bool operator<=(char a, fixed const& b)
{
    return fixed(a)<=b;
}

inline bool operator<=(fixed const& a,double b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,float b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,uint64_t b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,int64_t b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,unsigned b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,int b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,unsigned short b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,short b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,unsigned char b)
{
    return a<=fixed(b);
}
inline bool operator<=(fixed const& a,char b)
{
    return a<=fixed(b);
}

inline bool operator>=(double a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(float a, fixed const& b)
{
    return fixed(a)>=b;
}

inline bool operator>=(uint64_t a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(int64_t a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(unsigned a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(int a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(unsigned short a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(short a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(unsigned char a, fixed const& b)
{
    return fixed(a)>=b;
}
inline bool operator>=(char a, fixed const& b)
{
    return fixed(a)>=b;
}

inline bool operator>=(fixed const& a,double b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,float b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a, uint64_t b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a, int64_t b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,unsigned b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,int b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,unsigned short b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,short b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,unsigned char b)
{
    return a>=fixed(b);
}
inline bool operator>=(fixed const& a,char b)
{
    return a>=fixed(b);
}

inline fixed sin(fixed const& x)
{
    return x.sin();
}
inline fixed cos(fixed const& x)
{
    return x.cos();
}
inline fixed tan(fixed const& x)
{
    return x.tan();
}

inline fixed sqrt(fixed const& x)
{
    return x.sqrt();
}

inline fixed exp(fixed const& x)
{
    return x.exp();
}

inline fixed log(fixed const& x)
{
    return x.log();
}

inline fixed floor(fixed const& x)
{
    return x.floor();
}

inline fixed ceil(fixed const& x)
{
    return x.ceil();
}

inline fixed abs(fixed const& x)
{
    return x.abs();
}

inline fixed modf(fixed const& x,fixed*integral_part)
{
    return x.modf(integral_part);
}

inline fixed fixed::ceil() const
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

inline fixed fixed::floor() const
{
    fixed res(*this);
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


inline fixed fixed::sin() const
{
    fixed res;
    sin_cos(*this,&res,0);
    return res;
}

inline fixed fixed::cos() const
{
    fixed res;
    sin_cos(*this,0,&res);
    return res;
}

inline fixed fixed::tan() const
{
    fixed s,c;
    sin_cos(*this,&s,&c);
    return s/c;
}

inline fixed fixed::operator-() const
{
    return fixed(internal(),-m_nVal);
}

inline fixed fixed::abs() const
{
    return fixed(internal(),m_nVal<0?-m_nVal:m_nVal);
}

inline fixed fixed::modf(fixed*integral_part) const
{
    int64_t fractional_part=m_nVal%fixed_resolution;
    if(m_nVal<0 && fractional_part>0)
    {
        fractional_part-=fixed_resolution;
    }
    integral_part->m_nVal=m_nVal-fractional_part;
    return fixed(internal(),fractional_part);
}

inline fixed arg(const std::complex<fixed>& val)
{
	fixed r,theta;
	fixed::to_polar(val.real(),val.imag(),&r,&theta);
	return theta;
}

inline std::complex<fixed> polar(fixed const& rho, fixed const& theta)
{
	fixed s,c;
	fixed::sin_cos(theta,&s,&c);
	return std::complex<fixed>(rho * c, rho * s);
}

fixed const fixed_max(fixed::internal(),0x7fffffffffffffffLL);
fixed const fixed_one(fixed::internal(),1LL<<(fixed_resolution_shift));
fixed const fixed_zero(fixed::internal(),0);
fixed const fixed_half(fixed::internal(),1LL<<(fixed_resolution_shift-1));
extern fixed const fixed_pi;
extern fixed const fixed_two_pi;
extern fixed const fixed_half_pi;
extern fixed const fixed_quarter_pi;

#endif
