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

#ifndef SDRBASE_DSP_IIRFILTER_H_
#define SDRBASE_DSP_IIRFILTER_H_

#include <stdint.h>
#include <assert.h>

template <typename Type, uint32_t Order> class IIRFilter
{
public:
    IIRFilter(const Type *a, const Type *b);
    ~IIRFilter();
    Type run(Type sample);

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
    Type run(Type sample);

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

    memcpy(m_a, a, (Order+1)*sizeof(Type));
    memcpy(m_b, b, (Order+1)*sizeof(Type));

    for (int i = 0; i < Order; i++)
    {
        m_x[i] = 0;
        m_y[i] = 0;
    }
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
Type IIRFilter<Type, Order>::run(Type sample)
{
    Type y = m_a[0] * sample;

    for (int i = 0; i < Order; i++)
    {
        y += m_a[i+1] * m_x[i] + m_b[i+1] * m_y[i];
    }

    m_x[0] = sample;
    m_y[0] = y;

    memcpy(&m_x[1], &m_x[0], (Order-1)*sizeof(Type));
}

template <typename Type>
IIRFilter<Type, 2>::IIRFilter(const Type *a, const Type *b)
{
    m_a[0] = a[0];
    m_a[1] = a[1];
    m_a[2] = a[2];
    m_b[0] = b[0];
    m_b[1] = b[1];
    m_b[2] = b[2];
}

template <typename Type>
IIRFilter<Type, 2>::~IIRFilter()
{
}

template <typename Type>
Type IIRFilter<Type, 2>::run(Type sample)
{
    Type y = m_a[0]*sample + m_a[1]*m_x[0] + m_a[2]*m_x[1] + m_b[1]*m_y[0] + m_b[2]*m_y[1]; // this is y[n]

    m_x[1] = m_x[0];
    m_x[0] = sample;

    m_y[1] = m_y[0];
    m_y[0] = y;

    return y;
}

#endif /* SDRBASE_DSP_IIRFILTER_H_ */
