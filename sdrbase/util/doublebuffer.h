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

#ifndef SDRBASE_UTIL_DOUBLEBUFFER_H_
#define SDRBASE_UTIL_DOUBLEBUFFER_H_

#include <vector>
#include <algorithm>

template<typename T>
class DoubleBufferSimple
{
public:
	DoubleBufferSimple()
    {
	    m_size = 0;
        m_current = m_data.end();
	}

	~DoubleBufferSimple() {}

	DoubleBufferSimple(const DoubleBufferSimple& other)
	{
	    m_size = other.m_size;
	    m_data = other.m_data;
	    m_current = m_data.begin();
	}

    DoubleBufferSimple& operator=(const DoubleBufferSimple& other)
    {
        if (&other == this) {
            return *this;
        }

        m_size = other.m_size;
        m_data = other.m_data;
        m_current = m_data.begin();
        return *this;
    }

	void resize(int size)
	{
	    m_size = size;
        m_data.resize(2*size);
        m_current = m_data.begin();
	}

	void write(const typename std::vector<T>::const_iterator& begin, const typename std::vector<T>::const_iterator& cend)
	{
		typename std::vector<T>::const_iterator end = cend;

		if ((end - begin) > m_size)
		{
			end = begin + m_size;
		}

		int insize = end - begin;

		std::copy(begin, end, m_current);

		if (((m_current - m_data.begin()) + insize) > m_size)
		{
			int sizeLeft = m_size - (m_current - m_data.begin());
			std::copy(begin, begin + sizeLeft, m_current + m_size);
			std::copy(begin + sizeLeft, end, m_data.begin());
			m_current = m_data.begin() + (insize - sizeLeft);
		}
		else
		{
			std::copy(begin, end, m_current + m_size);
			m_current += insize;
		}
	}

	typename std::vector<T>::iterator getCurrent() const { return m_current + m_size; }
	unsigned int absoluteFill() const { return m_current - m_data.begin(); }
	void reset() { m_current = m_data.begin(); }

private:
	int m_size;
	std::vector<T> m_data;
	typename std::vector<T>::iterator m_current;
};

#endif /* SDRBASE_UTIL_DOUBLEBUFFER_H_ */
