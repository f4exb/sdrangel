///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_UTIL_DOUBLEBUFFERFIFO_H_
#define SDRBASE_UTIL_DOUBLEBUFFERFIFO_H_


template<typename T>
class DoubleBufferFIFO
{
public:
    DoubleBufferFIFO(int size) : m_size(size), m_writeIndex(0), m_currentIndex(0)
    {
        m_data = new T[2*m_size];
    }

    ~DoubleBufferFIFO()
    {
        delete[] m_data;
    }

    void resize(int size)
    {
        delete[] m_data;
        m_size = size;
        m_data = new T[2*m_size];
        m_writeIndex = 0;
        m_currentIndex = 0;
    }

    void write(const T& element)
    {
        m_data[m_writeIndex] = element;
        m_data[m_writeIndex+m_size] = element;
        m_currentIndex = m_writeIndex;

        if (m_writeIndex < m_size - 1) {
            m_writeIndex++;
        } else {
            m_writeIndex = 0;
        }
    }

    T& readBack(int delay)
    {
        if (delay > m_size) {
            delay = m_size;
        }

        return m_data[m_currentIndex + m_size - delay];
    }

    void zeroBack(int delay)
    {
        if (delay > m_size) {
            delay = m_size;
        }

        for (int i = 0; i < delay; i++) {
            m_data[m_currentIndex + m_size - i] = 0;
        }
    }

private:
    int m_size;
    T *m_data;
    int m_writeIndex;
    int m_currentIndex;
};

#endif /* SDRBASE_UTIL_DOUBLEBUFFERFIFO_H_ */
