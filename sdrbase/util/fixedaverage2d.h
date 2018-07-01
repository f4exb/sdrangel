///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
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

#ifndef SDRBASE_UTIL_FIXEDAVERAGE2D_H_
#define SDRBASE_UTIL_FIXEDAVERAGE2D_H_

#include <algorithm>

template<typename T>
class FixedAverage2D
{
public:
    FixedAverage2D() : m_sum(0), m_sumSize(0), m_width(0), m_size(0), m_avgIndex(0) {}

    ~FixedAverage2D()
    {
        if (m_sum) {
            delete[] m_sum;
        }
    }

    void resize(unsigned int width, unsigned int size)
    {
        if (width > m_sumSize)
        {
            m_sumSize = width;
            if (m_sum) {
                delete[] m_sum;
            }
            m_sum = new T[m_sumSize];
        }

        m_width = width;
        m_size = size;

        std::fill(m_sum, m_sum+m_width, 0);
        m_avgIndex = 0;
    }

    bool storeAndGetAvg(T& avg, T v, unsigned int index)
    {
        if (m_size <= 1)
        {
            avg = v;
            return true;
        }

        m_sum[index] += v;

        if (m_avgIndex == m_size - 1)
        {
            avg = m_sum[index]/m_size;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool storeAndGetSum(T& sum, T v, unsigned int index)
    {
        if (m_size <= 1)
        {
            sum = v;
            return true;
        }

        m_sum[index] += v;

        if (m_avgIndex < m_size - 1)
        {
            sum = m_sum[index];
            return true;
        }
        else
        {
            return false;
        }
    }

    bool nextAverage()
    {
        if (m_size <= 1) {
            return true;
        }

        if (m_avgIndex == m_size - 1)
        {
            m_avgIndex = 0;
            std::fill(m_sum, m_sum+m_width, 0);
            return true;
        }
        else
        {
            m_avgIndex++;
            return false;
        }
    }

private:
    T *m_sum;
    unsigned int m_sumSize;
    unsigned int m_width;
    unsigned int m_size;
    unsigned int m_avgIndex;
};



#endif /* SDRBASE_UTIL_FIXEDAVERAGE2D_H_ */
