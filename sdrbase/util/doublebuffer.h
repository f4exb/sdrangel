///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_UTIL_DOUBLEBUFFER_H_
#define SDRBASE_UTIL_DOUBLEBUFFER_H_

#include <vector>
#include <algorithm>

#include <QByteArray>

#include "simpleserializer.h"

template<typename T>
class DoubleBufferSimple
{
public:
	DoubleBufferSimple()
    {
	    m_size = 0;
        m_currentPosition = 0;
	}

	~DoubleBufferSimple() {}

	DoubleBufferSimple(const DoubleBufferSimple& other)
	{
	    m_size = other.m_size;
	    m_data = other.m_data;
        m_currentPosition = 0;
	}

    DoubleBufferSimple& operator=(const DoubleBufferSimple& other)
    {
        if (&other == this) {
            return *this;
        }

        m_size = other.m_size;
        m_data = other.m_data;
        m_currentPosition = 0;
        return *this;
    }

	void resize(int size)
	{
	    m_size = size;
        m_data.resize(2*size);
        m_currentPosition = 0;
	}

    void write(const typename std::vector<T>::const_iterator& begin, int length)
    {
        int insize = length > m_size ? m_size : length;

        std::copy(begin, begin + insize, m_data.begin() + m_currentPosition);

        if ((m_currentPosition + insize) > m_size)
        {
            int sizeLeft = m_size - m_currentPosition;
            std::copy(begin, begin + sizeLeft, m_data.begin() + m_currentPosition + m_size);
            std::copy(begin + sizeLeft, begin + insize, m_data.begin());
            m_currentPosition = insize - sizeLeft;
        }
        else
        {
            std::copy(begin, begin + insize, m_data.begin() + m_currentPosition + m_size);
            m_currentPosition += insize;
        }
    }

	typename std::vector<T>::iterator getCurrent() { return m_data.begin() + m_currentPosition + m_size; }
    void getCurrent(typename std::vector<T>::iterator& it) { it = m_data.begin() + m_currentPosition + m_size; }
	typename std::vector<T>::const_iterator begin() const { return m_data.begin(); }
    typename std::vector<T>::iterator begin() { return m_data.begin(); }
	unsigned int absoluteFill() const { return m_currentPosition; }
	void reset() { m_currentPosition = 0; }

    QByteArray serialize() const
    {
        SimpleSerializer s(1);

        QByteArray buf(reinterpret_cast<const char*>(m_data.data()), m_data.size()*sizeof(T));
        s.writeS32(1, m_size);
        s.writeU32(2, m_currentPosition);
        s.writeBlob(3, buf);

        return s.final();
    }

    bool deserialize(const QByteArray& data)
    {
        SimpleDeserializer d(data);

        if(!d.isValid()) {
            return false;
        }

        if (d.getVersion() == 1)
        {
            unsigned int tmpUInt;
            QByteArray buf;

            d.readS32(1, &m_size, m_data.size()/2);
            m_data.resize(2*m_size);
            d.readU32(2, &tmpUInt, 0);
            m_currentPosition = tmpUInt;
            d.readBlob(3, &buf);
            //qDebug("DoubleBufferSimple::deserialize: m_data.size(): %u buf.size(): %d", m_data.size(), buf.size());
            //std::copy(reinterpret_cast<char *>(m_data.data()), buf.data(), buf.data() + buf.size()); // bug
            memcpy(reinterpret_cast<char *>(m_data.data()), buf.data(), buf.size());

            return true;
        }
        else
        {
            return false;
        }
    }

private:
	int m_size;
	std::vector<T> m_data;
    int m_currentPosition;
};

#endif /* SDRBASE_UTIL_DOUBLEBUFFER_H_ */
