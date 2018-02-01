// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007 Anthony Williams
#include "fixed.hpp"

// all constants are scaled to 2^28 (as 1)
int64_t const internal_pi=0x3243f6a8;
int64_t const internal_two_pi=0x6487ed51;
int64_t const internal_half_pi=0x1921fb54;
int64_t const internal_quarter_pi=0xc90fdaa;

extern fixed const fixed_pi(fixed::internal(),internal_pi);
extern fixed const fixed_two_pi(fixed::internal(),internal_two_pi);
extern fixed const fixed_half_pi(fixed::internal(),internal_half_pi);
extern fixed const fixed_quarter_pi(fixed::internal(),internal_quarter_pi);

fixed& fixed::operator%=(fixed const& other)
{
    m_nVal = m_nVal%other.m_nVal;
    return *this;
}

fixed& fixed::operator*=(fixed const& val)
{
    bool const val_negative=val.m_nVal<0;
    bool const this_negative=m_nVal<0;
    bool const negate=val_negative ^ this_negative;
    uint64_t const other=val_negative?-val.m_nVal:val.m_nVal;
    uint64_t const self=this_negative?-m_nVal:m_nVal;
    
    if(uint64_t const self_upper=(self>>32))
    {
        m_nVal=(self_upper*other)<<(32-fixed_resolution_shift);
    }
    else
    {
        m_nVal=0;
    }
    if(uint64_t const self_lower=(self&0xffffffff))
    {
        uint64_t const other_upper=static_cast<uint64_t>(other>>32);
        uint64_t const other_lower=static_cast<uint64_t>(other&0xffffffff);
        uint64_t const lower_self_upper_other_res=self_lower*other_upper;
        uint64_t const lower_self_lower_other_res=self_lower*other_lower;
        m_nVal+=(lower_self_upper_other_res<<(32-fixed_resolution_shift))
            + (lower_self_lower_other_res>>fixed_resolution_shift);
    }
    
    if(negate)
    {
        m_nVal=-m_nVal;
    }
    return *this;
}


fixed& fixed::operator/=(fixed const& divisor)
{
    if( !divisor.m_nVal)
    {
        m_nVal=fixed_max.m_nVal;
    }
    else
    {
        bool const negate_this=(m_nVal<0);
        bool const negate_divisor=(divisor.m_nVal<0);
        bool const negate=negate_this ^ negate_divisor;
        uint64_t a=negate_this?-m_nVal:m_nVal;
        uint64_t b=negate_divisor?-divisor.m_nVal:divisor.m_nVal;

        uint64_t res=0;
    
        uint64_t temp=b;
        bool const a_large=a>b;
        unsigned shift=fixed_resolution_shift;

        if(a_large)
        {
            uint64_t const half_a=a>>1;
            while(temp<half_a)
            {
                temp<<=1;
                ++shift;
            }
        }
        uint64_t d=1LL<<shift;
        if(a_large)
        {
            a-=temp;
            res+=d;
        }

        while(a && temp && shift)
        {
            unsigned right_shift=0;
            while(right_shift<shift && (temp>a))
            {
                temp>>=1;
                ++right_shift;
            }
            d>>=right_shift;
            shift-=right_shift;
            a-=temp;
            res+=d;
        }
        m_nVal=(negate?-(int64_t)res:res);
    }
    
    return *this;
}


fixed fixed::sqrt() const
{
    unsigned const max_shift=62;
    uint64_t a_squared=1LL<<max_shift;
    unsigned b_shift=(max_shift+fixed_resolution_shift)/2;
    uint64_t a=1LL<<b_shift;
    
    uint64_t x=m_nVal;
    
    while(b_shift && a_squared>x)
    {
        a>>=1;
        a_squared>>=2;
        --b_shift;
    }

    uint64_t remainder=x-a_squared;
    --b_shift;
    
    while(remainder && b_shift)
    {
        uint64_t b_squared=1LL<<(2*b_shift-fixed_resolution_shift);
        int const two_a_b_shift=b_shift+1-fixed_resolution_shift;
        uint64_t two_a_b=(two_a_b_shift>0)?(a<<two_a_b_shift):(a>>-two_a_b_shift);
        
        while(b_shift && remainder<(b_squared+two_a_b))
        {
            b_squared>>=2;
            two_a_b>>=1;
            --b_shift;
        }
        uint64_t const delta=b_squared+two_a_b;
        if((2*remainder)>delta)
        {
            a+=(1LL<<b_shift);
            remainder-=delta;
            if(b_shift)
            {
                --b_shift;
            }
        }
    }
    return fixed(internal(),a);
}

namespace
{
    int const max_power=63-fixed_resolution_shift;
    // 35 values (63-28)
    int64_t const log_two_power_n_reversed[]={
        0x18429946ELL,0x1791272EFLL,0x16DFB516FLL,0x162E42FF0LL,0x157CD0E70LL,0x14CB5ECF1LL,0x1419ECB71LL,0x13687A9F2LL,
        0x12B708872LL,0x1205966F3LL,0x115424573LL,0x10A2B23F4LL,0xFF140274LL,0xF3FCE0F5LL,0xE8E5BF75LL,0xDDCE9DF6LL,
        0xD2B77C76LL,0xC7A05AF7LL,0xBC893977LL,0xB17217F8LL,0xA65AF679LL,0x9B43D4F9LL,0x902CB379LL,0x851591FaLL,
        0x79FE707bLL,0x6EE74EFbLL,0x63D02D7BLL,0x58B90BFcLL,0x4DA1EA7CLL,0x428AC8FdLL,0x3773A77DLL,0x2C5C85FeLL,
        0x2145647ELL,0x162E42FfLL,0xB17217FLL
    };
    
    // 28 values
    int64_t const log_one_plus_two_power_minus_n[]={
        0x67CC8FBLL,0x391FEF9LL,0x1E27077LL,0xF85186LL,
        0x7E0A6CLL,0x3F8151LL,0x1FE02ALL,0xFF805LL,0x7FE01LL,0x3FF80LL,0x1FFE0LL,0xFFF8LL,
        0x7FFELL,0x4000LL,0x2000LL,0x1000LL,0x800LL,0x400LL,0x200LL,0x100LL,
        0x80LL,0x40LL,0x20LL,0x10LL,0x8LL,0x4LL,0x2LL,0x1LL
    };

    // 28 values
    int64_t const log_one_over_one_minus_two_power_minus_n[]={
        0xB172180LL,0x49A5884LL,0x222F1D0LL,0x108598BLL,
        0x820AECLL,0x408159LL,0x20202BLL,0x100805LL,0x80201LL,0x40080LL,0x20020LL,0x10008LL,
        0x8002LL,0x4001LL,0x2000LL,0x1000LL,0x800LL,0x400LL,0x200LL,0x100LL,
        0x80LL,0x40LL,0x20LL,0x10LL,0x8LL,0x4LL,0x2LL,0x1LL
    };
}


fixed fixed::exp() const
{
    if(m_nVal>=log_two_power_n_reversed[0])
    {
        return fixed_max;
    }
    if(m_nVal<-log_two_power_n_reversed[63-2*fixed_resolution_shift])
    {
        return fixed(internal(),0);
    }
    if(!m_nVal)
    {
        return fixed(internal(),fixed_resolution);
    }

    int64_t res=fixed_resolution;

    if(m_nVal>0)
    {
        int power=max_power;
        int64_t const* log_entry=log_two_power_n_reversed;
        int64_t temp=m_nVal;
        while(temp && power>(-(int)fixed_resolution_shift))
        {
            while(!power || (temp<*log_entry))
            {
                if(!power)
                {
                    log_entry=log_one_plus_two_power_minus_n;
                }
                else
                {
                    ++log_entry;
                }
                --power;
            }
            temp-=*log_entry;
            if(power<0)
            {
                res+=(res>>(-power));
            }
            else
            {
                res<<=power;
            }
        }
    }
    else
    {
        int power=fixed_resolution_shift;
        int64_t const* log_entry=log_two_power_n_reversed+(max_power-power);
        int64_t temp=m_nVal;

        while(temp && power>(-(int)fixed_resolution_shift))
        {
            while(!power || (temp>(-*log_entry)))
            {
                if(!power)
                {
                    log_entry=log_one_over_one_minus_two_power_minus_n;
                }
                else
                {
                    ++log_entry;
                }
                --power;
            }
            temp+=*log_entry;
            if(power<0)
            {
                res-=(res>>(-power));
            }
            else
            {
                res>>=power;
            }
        }
    }
    
    return fixed(internal(),res);
}

fixed fixed::log() const
{
    if(m_nVal<=0)
    {
        return -fixed_max;
    }
    if(m_nVal==fixed_resolution)
    {
        return fixed_zero;
    }
    uint64_t temp=m_nVal;
    int left_shift=0;
    uint64_t const scale_position=0x8000000000000000;
    while(temp<scale_position)
    {
        ++left_shift;
        temp<<=1;
    }
    
    int64_t res=(left_shift<max_power)?
        log_two_power_n_reversed[left_shift]:
        -log_two_power_n_reversed[2*max_power-left_shift];
    unsigned right_shift=1;
    uint64_t shifted_temp=temp>>1;
    while(temp && (right_shift<fixed_resolution_shift))
    {
        while((right_shift<fixed_resolution_shift) && (temp<(shifted_temp+scale_position)))
        {
            shifted_temp>>=1;
            ++right_shift;
        }
        
        temp-=shifted_temp;
        shifted_temp=temp>>right_shift;
        res+=log_one_over_one_minus_two_power_minus_n[right_shift-1];
    }
    return fixed(fixed::internal(),res);
}


namespace
{
    const int64_t arctantab[32] = {
        297197971, 210828714, 124459457, 65760959, 33381290, 16755422, 8385879,
        4193963, 2097109, 1048571, 524287, 262144, 131072, 65536, 32768, 16384,
        8192, 4096, 2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0, 0,
    };


    int64_t scale_cordic_result(int64_t a)
    {
        int64_t const cordic_scale_factor=0x22C2DD1C; /* 0.271572 * 2^31*/
        return (int64_t)((((int64_t)a)*cordic_scale_factor)>>31);
    }
    
    int64_t right_shift(int64_t val,int shift)
    {
        return (shift<0)?(val<<-shift):(val>>shift);
    }
    
    void perform_cordic_rotation(int64_t&px, int64_t&py, int64_t theta)
    {
        int64_t x = px, y = py;
        int64_t const *arctanptr = arctantab;
        for (int i = -1; i <= (int)fixed_resolution_shift; ++i)
        {
            int64_t const yshift=right_shift(y,i);
            int64_t const xshift=right_shift(x,i);

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


    void perform_cordic_polarization(int64_t& argx, int64_t&argy)
    {
        int64_t theta=0;
        int64_t x = argx, y = argy;
        int64_t const *arctanptr = arctantab;
        for(int i = -1; i <= (int)fixed_resolution_shift; ++i)
        {
            int64_t const yshift=right_shift(y,i);
            int64_t const xshift=right_shift(x,i);
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
}

void fixed::sin_cos(fixed const& theta,fixed* s,fixed*c)
{
    int64_t x=theta.m_nVal%internal_two_pi;
    if( x < 0 )
        x += internal_two_pi;

    bool negate_cos=false;
    bool negate_sin=false;

    if( x > internal_pi )
    {
        x =internal_two_pi-x;
        negate_sin=true;
    }
    if(x>internal_half_pi)
    {
        x=internal_pi-x;
        negate_cos=true;
    }
    int64_t x_cos=1<<28,x_sin=0;

    perform_cordic_rotation(x_cos,x_sin,(int64_t)x);

    if(s)
    {
        s->m_nVal=negate_sin?-x_sin:x_sin;
    }
    if(c)
    {
        c->m_nVal=negate_cos?-x_cos:x_cos;
    }
}

fixed fixed::atan() const
{
    fixed r,theta;
    to_polar(1,*this,&r,&theta);
    return theta;
}

void fixed::to_polar(fixed const& x,fixed const& y,fixed* r,fixed*theta)
{
    bool const negative_x=x.m_nVal<0;
    bool const negative_y=y.m_nVal<0;
    
    uint64_t a=negative_x?-x.m_nVal:x.m_nVal;
    uint64_t b=negative_y?-y.m_nVal:y.m_nVal;

    unsigned right_shift=0;
    unsigned const max_value=1U<<fixed_resolution_shift;

    while((a>=max_value) || (b>=max_value))
    {
        ++right_shift;
        a>>=1;
        b>>=1;
    }
    int64_t xtemp=(int64_t)a;
    int64_t ytemp=(int64_t)b;
    perform_cordic_polarization(xtemp,ytemp);
    r->m_nVal=int64_t(xtemp)<<right_shift;
    theta->m_nVal=ytemp;

    if(negative_x && negative_y)
    {
        theta->m_nVal-=internal_pi;
    }
    else if(negative_x)
    {
        theta->m_nVal=internal_pi-theta->m_nVal;
    }
    else if(negative_y)
    {
        theta->m_nVal=-theta->m_nVal;
    }
}

