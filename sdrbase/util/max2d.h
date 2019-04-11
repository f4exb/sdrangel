///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_UTIL_MAX2D_H_
#define SDRBASE_UTIL_MAX2D_H_

#include <algorithm>

template<typename T>
class Max2D
{
public:
    Max2D() : m_max(0), m_maxSize(0), m_width(0), m_size(0), m_maxIndex(0) {}

    ~Max2D()
    {
        if (m_max) {
            delete[] m_max;
        }
    }

    void resize(unsigned int width, unsigned int size)
    {
        if (width > m_maxSize)
        {
            m_maxSize = width;
            if (m_max) {
                delete[] m_max;
            }
            m_max = new T[m_maxSize];
        }

        m_width = width;
        m_size = size;

        std::fill(m_max, m_max+m_width, 0);
        m_maxIndex = 0;
    }

    bool storeAndGetMax(T& max, T v, unsigned int index)
    {
        if (m_size <= 1)
        {
            max = v;
            return true;
        }

        if (m_maxIndex == 0)
        {
            m_max[index] = v;
            return false;
        }
        else if (m_maxIndex == m_size - 1)
        {
            m_max[index] = std::max(m_max[index], v);
            max = m_max[index];
            return true;
        }
        else
        {
            m_max[index] = std::max(m_max[index], v);
            return false;
        }
    }

    bool nextMax()
    {
        if (m_size <= 1) {
            return true;
        }

        if (m_maxIndex == m_size - 1)
        {
            m_maxIndex = 0;
            std::fill(m_max, m_max+m_width, 0);
            return true;
        }
        else
        {
            m_maxIndex++;
            return false;
        }
    }

private:
    T *m_max;
    unsigned int m_maxSize;
    unsigned int m_width;
    unsigned int m_size;
    unsigned int m_maxIndex;
};

#endif /* SDRBASE_UTIL_MAX2D_H_ */
