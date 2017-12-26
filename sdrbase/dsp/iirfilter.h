///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

/**
 * IIR filter
 * See: https://cdn.mikroe.com/ebooks/img/8/2016/02/digital-filter-design-chapter-03-image-2-9.gif
 */

#ifndef SDRBASE_DSP_IIRFILTER_H_
#define SDRBASE_DSP_IIRFILTER_H_

#include <stdint.h>
#include <string.h>
#include <assert.h>

template <typename Type, uint32_t Order> class IIRFilter
{
public:
    IIRFilter(const Type *a, const Type *b);
    ~IIRFilter();
    void setCoeffs(const Type *a, const Type *b);
    Type run(const Type& sample);

private:
    Type *m_a;
    Type *m_b;
    Type *m_x;
    Type *m_y;
};

template <typename Type> class IIRFilter<Type, 2>
{
public:
    IIRFilter(const Type *a, const Type *b);
    ~IIRFilter();
    void setCoeffs(const Type *a, const Type *b);
    Type run(const Type& sample);

private:
    Type m_a[3];
    Type m_b[3];
    Type m_x[2];
    Type m_y[2];
};

template <typename Type, uint32_t Order>
IIRFilter<Type, Order>::IIRFilter(const Type *a, const Type *b)
{
    assert(Order > 1);

    m_a = new Type[Order+1];
    m_b = new Type[Order+1];
    m_x = new Type[Order];
    m_y = new Type[Order];

    setCoeffs(a, b);
}

template <typename Type, uint32_t Order>
IIRFilter<Type, Order>::~IIRFilter()
{
    delete[] m_y;
    delete[] m_x;
    delete[] m_b;
    delete[] m_a;
}

template <typename Type, uint32_t Order>
void IIRFilter<Type, Order>::setCoeffs(const Type *a, const Type *b)
{
    memcpy(m_a, b, (Order+1)*sizeof(Type));
    memcpy(m_b, a, (Order+1)*sizeof(Type));

    for (uint32_t i = 0; i < Order; i++)
    {
        m_x[i] = 0;
        m_y[i] = 0;
    }
}


template <typename Type, uint32_t Order>
Type IIRFilter<Type, Order>::run(const Type& sample)
{
    Type y = m_b[0] * sample;

    for (uint32_t i = Order; i > 0; i--)
    {
        y += m_b[i] * m_x[i-1] + m_a[i] * m_y[i-1];

        if (i > 1) // shift
        {
            m_x[i-1] = m_x[i-2];
            m_y[i-1] = m_y[i-2];
        }
    }

    // last shift
    m_x[0] = sample;
    m_y[0] = y;

    return y;
}


template <typename Type>
IIRFilter<Type, 2>::IIRFilter(const Type *a, const Type *b)
{
    setCoeffs(a, b);
}

template <typename Type>
IIRFilter<Type, 2>::~IIRFilter()
{
}

template <typename Type>
void IIRFilter<Type, 2>::setCoeffs(const Type *a, const Type *b)
{
    m_a[0] = a[0];
    m_a[1] = a[1];
    m_a[2] = a[2];
    m_b[0] = b[0];
    m_b[1] = b[1];
    m_b[2] = b[2];
    m_x[0] = 0;
    m_x[1] = 0;
    m_y[0] = 0;
    m_y[1] = 0;
}

template <typename Type>
Type IIRFilter<Type, 2>::run(const Type& sample)
{
    Type y = m_b[0]*sample + m_b[1]*m_x[0] + m_b[2]*m_x[1] + m_a[1]*m_y[0] + m_a[2]*m_y[1]; // this is y[n]

    m_x[1] = m_x[0];
    m_x[0] = sample;

    m_y[1] = m_y[0];
    m_y[0] = y;

    return y;
}

#endif /* SDRBASE_DSP_IIRFILTER_H_ */
