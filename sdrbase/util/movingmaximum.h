///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                            //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_UTIL_MOVINGMAXIMUM_H
#define INCLUDE_UTIL_MOVINGMAXIMUM_H

#include <algorithm>
#include <QDebug>

// Calculates moving maximum over a number of samples
template <typename T>
class MovingMaximum
{
public:

    MovingMaximum() :
        m_samples(nullptr),
        m_size(0)
    {
        reset();
    }

    ~MovingMaximum()
    {
        delete[] m_samples;
    }

    void reset()
    {
        m_count = 0;
        m_index = 0;
        m_max = NAN;
    }

    void setSize(int size)
    {
        delete[] m_samples;
        m_samples = new T[size]();
        m_size = size;
        reset();
    }

    void operator()(T sample)
    {
        if (m_count < m_size)
        {
            m_samples[m_count++] = sample;
            if (m_count == 1) {
                m_max = sample;
            } else {
                m_max = std::max(m_max, sample);
            }
        }
        else
        {
            T oldest = m_samples[m_index];
            m_samples[m_index] = sample;
            m_index = (m_index + 1) % m_size;
            m_max = std::max(m_max, sample);
            if (oldest >= m_max)
            {
                // Find new maximum, that will be lower than the oldest sample
                m_max = m_samples[0];
                for (unsigned int i = 1; i < m_size; i++) {
                    m_max = std::max(m_max, m_samples[i]);
                }
            }
        }
    }

    T getMaximum() const {
        return m_max;
    }

private:
    T *m_samples;
    unsigned int m_size;    // Max number of samples
    unsigned int m_count;   // Number of samples used
    unsigned int m_index;   // Current index
    T m_max;
};

#endif /* INCLUDE_UTIL_MOVINGMAXIMUM_H */

