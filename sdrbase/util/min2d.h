///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRBASE_UTIL_MIN2D_H_
#define SDRBASE_UTIL_MIN2D_H_

#include <algorithm>

template<typename T>
class Min2D
{
public:
    Min2D() : m_min(0), m_maxSize(0), m_width(0), m_size(0), m_index(0) {}

    ~Min2D()
    {
        if (m_min) {
            delete[] m_min;
        }
    }

    void resize(unsigned int width, unsigned int size)
    {
        if (width > m_maxSize)
        {
            m_maxSize = width;
            if (m_min) {
                delete[] m_min;
            }
            m_min = new T[m_maxSize];
        }

        m_width = width;
        m_size = size;

        std::fill(m_min, m_min+m_width, 0);
        m_index = 0;
    }

    bool storeAndGetMin(T& min, T v, unsigned int index)
    {
        if (m_size <= 1)
        {
            min = v;
            return true;
        }

        if (m_index == 0)
        {
            m_min[index] = v;
            return false;
        }
        else if (m_index == m_size - 1)
        {
            m_min[index] = std::min(m_min[index], v);
            min = m_min[index];
            return true;
        }
        else
        {
            m_min[index] = std::min(m_min[index], v);
            return false;
        }
    }

    bool nextMin()
    {
        if (m_size <= 1) {
            return true;
        }

        if (m_index == m_size - 1)
        {
            m_index = 0;
            std::fill(m_min, m_min+m_width, 0);
            return true;
        }
        else
        {
            m_index++;
            return false;
        }
    }

private:
    T *m_min;
    unsigned int m_maxSize;
    unsigned int m_width;
    unsigned int m_size;
    unsigned int m_index;
};

#endif /* SDRBASE_UTIL_MIN2D_H_ */
