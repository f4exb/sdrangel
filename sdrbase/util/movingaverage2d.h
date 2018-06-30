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

#ifndef _UTIL_MOVINGAVERAGE2D_H_
#define _UTIL_MOVINGAVERAGE2D_H_

#include <algorithm>

template<typename T>
class MovingAverage2D
{
public:
    MovingAverage2D() : m_data(0), m_sum(0), m_dataSize(0), m_sumSize(0), m_width(0), m_depth(0), m_avgIndex(0) {}

    ~MovingAverage2D()
    {
        if (m_data) {
            delete[] m_data;
        }

        if (m_sum) {
            delete[] m_sum;
        }
    }

    void resize(unsigned int width, unsigned int depth)
    {
        if (width*depth > m_dataSize)
        {
            m_dataSize = width*depth;
            if (m_data) {
                delete[] m_data;
            }
            m_data = new T[m_dataSize];
        }

        if (width > m_sumSize)
        {
            m_sumSize = width;
            if (m_sum) {
                delete[] m_sum;
            }
            m_sum = new T[m_sumSize];
        }

        m_width = width;
        m_depth = depth;

        std::fill(m_data, m_data+(m_width*m_depth), 0.0);
        std::fill(m_sum, m_sum+m_width, 0.0);

        m_avgIndex = 0;
    }

    T storeAndGetAvg(T v, unsigned int index)
    {
        if (m_depth <= 1)
        {
            return v;
        }
        else if (index < m_width)
        {
            T first = m_data[m_avgIndex*m_width+index];
            m_sum[index] += (v - first);
            m_data[m_avgIndex*m_width+index] = v;
            return m_sum[index] / m_depth;
        }
        else
        {
            return 0;
        }
    }

    T storeAndGetSum(T v, unsigned int index)
    {
        if (m_depth == 1)
        {
            return v;
        }
        else if (index < m_width)
        {
            T first = m_data[m_avgIndex*m_width+index];
            m_sum[index] += (v - first);
            m_data[m_avgIndex*m_width+index] = v;
            return m_sum[index];
        }
        else
        {
            return 0;
        }
    }

    void nextAverage() {
        m_avgIndex = m_avgIndex == m_depth-1 ? 0 : m_avgIndex+1;
    }

private:
    T *m_data;
    T *m_sum;
    unsigned int m_dataSize;
    unsigned int m_sumSize;
    unsigned int m_width;
    unsigned int m_depth;
    unsigned int m_avgIndex;
};


#endif
