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

#ifndef SDRBASE_UTIL_DOUBLEBUFFERMULTIPLE_H_
#define SDRBASE_UTIL_DOUBLEBUFFERMULTIPLE_H_

#include <vector>
#include <algorithm>

#include <QByteArray>

#include "simpleserializer.h"

template<typename T>
class DoubleBufferMultiple
{
public:
	DoubleBufferMultiple()
    {
	    m_size = 0;
        m_currentPosition = 0;
        m_current = m_data.end();
	}

	~DoubleBufferMultiple() {}

	DoubleBufferMultiple(const DoubleBufferMultiple& other)
	{
	    m_size = other.m_size;
        m_data.resize(other.m_data.size());
        m_current.resize(other.m_data.size());
        m_currentPosition = 0;

        for (unsigned int i = 0; i < other.m_data.size(); i++)
        {
            m_data[i] = other.m_data[i];
            m_current[i] = m_data[i].begin();
        }
	}

    DoubleBufferMultiple& operator=(const DoubleBufferMultiple& other)
    {
        if (&other == this) {
            return *this;
        }

        m_size = other.m_size;
        m_data.resize(other.m_data.size());
        m_current.resize(other.m_data.size());
        m_currentPosition = 0;

        for (unsigned int i = 0; i < other.m_data.size(); i++)
        {
            m_data[i] = other.m_data[i];
            m_current[i] = m_data[i].begin();
        }

        return *this;
    }

	void resize(int size)
	{
	    m_size = size;
        m_currentPosition = 0;

        for (unsigned int i = 0; i < m_data.size(); i++)
        {
            m_data[i].resize(2*size);
            m_current[i] = m_data[i].begin();
        }
	}

    void addStream()
    {
        m_data.push_back(std::vector<T>(2*m_size));
        m_current.push_back(std::vector<T>::iterator());
        m_current.back() = m_data.back().begin();
    }

    void write(const std::vector<typename std::vector<T>::const_iterator>& vbegin, int length)
    {
        int insize = length > m_size ? m_size : length;

        if ((m_currentPosition + insize) > m_size)
        {
            int sizeLeft = m_size - m_currentPosition;
            m_currentPosition = insize - sizeLeft;
        }
        else
        {
            m_currentPosition += insize;
        }

        for (unsigned int i = 0; i < vbegin.size(); i++)
        {
            if (i >= m_data.size()) {
                break;
            }

            std::copy(vbegin[i], vbegin[i] + insize, m_current[i]);

            if (((m_current[i] - m_data[i].begin()) + insize) > m_size)
            {
                int sizeLeft = m_size - (m_current[i] - m_data[i].begin());
                std::copy(vbegin[i], vbegin[i] + sizeLeft, m_current[i] + m_size);
                std::copy(vbegin[i] + sizeLeft, vbegin[i] + insize, m_data[i].begin());
                m_current[i] = m_data[i].begin() + (insize - sizeLeft);
            }
            else
            {
                std::copy(vbegin[i], vbegin[i] + insize, m_current[i] + m_size);
                m_current[i] += insize;
            }
        }
    }

	// void write(const typename std::vector<T>::const_iterator& begin, const typename std::vector<T>::const_iterator& cend)
	// {
	// 	typename std::vector<T>::const_iterator end = cend;

	// 	if ((end - begin) > m_size) {
	// 		end = begin + m_size;
	// 	}

	// 	int insize = end - begin;

	// 	std::copy(begin, end, m_current);

	// 	if (((m_current - m_data.begin()) + insize) > m_size)
	// 	{
	// 		int sizeLeft = m_size - (m_current - m_data.begin());
	// 		std::copy(begin, begin + sizeLeft, m_current + m_size);
	// 		std::copy(begin + sizeLeft, end, m_data.begin());
	// 		m_current = m_data.begin() + (insize - sizeLeft);
	// 	}
	// 	else
	// 	{
	// 		std::copy(begin, end, m_current + m_size);
	// 		m_current += insize;
	// 	}
	// }

	typename std::vector<T>::iterator getCurrent(unsigned int i) const { return m_current[i] + m_size; }
	typename std::vector<T>::const_iterator begin(unsigned int i) const { return m_data[i].begin(); }
    typename std::vector<T>::iterator begin(unsigned int i) { return m_data[i].begin(); }
	unsigned int absoluteFill(unsigned int i) const { return m_current[i] - m_data[i].begin(); }
	void reset(unsigned int i) { m_current[i] = m_data[i].begin(); }

    void getCurrent(std::vector<typename std::vector<T>::iterator>& vcurrent) const
    {
        vcurrent.clear();

        for (unsigned int i = 0; i < m_data.size(); i++) {
            vcurrent.push_back(m_data[i].begin() + m_currentPosition + m_size);
        }
    }

    int getCurrentPosition() const {
        return m_currentPosition + m_size;
    }

    void begin(typename std::vector<typename std::vector<T>::const_iterator>& vbegin) const
    {
        vbegin.clear();

        for (unsigned int i = 0; i < m_data.size(); i++) {
            vbegin.push_back(m_data[i].begin());
        }
    }

    void begin(typename std::vector<typename std::vector<T>::iterator>& vbegin)
    {
        vbegin.clear();

        for (unsigned int i = 0; i < m_data.size(); i++) {
            vbegin.push_back(m_data[i].begin());
        }
    }

    unsigned int absoluteFill() const { return m_currentPosition; }

    void reset()
    {
        m_currentPosition = 0;

        for (unsigned int i = 0; i < m_current.size(); i++) {
            m_current[i] = m_data[i].begin();
        }
    }

    QByteArray serialize() const
    {
        SimpleSerializer s(1);

        s.writeU32(1, std::min(m_data.size(), 10U));
        s.writeS32(2, m_size);

        for (unsigned int i = 0; i < std::min(m_data.size(), 10U); i++)
        {
            QByteArray buf(reinterpret_cast<const char*>(m_data[i].data()), m_data[i].size()*sizeof(T));
            s.writeU32(10*i + 11, m_current[i] - m_data[i].begin());
            s.writeBlob(10*i + 12, buf);
        }

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
            unsigned int nbStreams;
            QByteArray buf;

            d.readU32(1, &nbStreams, 0);
            nbStreams = nbStreams > 10 ? 10 : nbStreams;
            m_data.resize(nbStreams);
            m_current.resize(nbStreams);
            d.readS32(2, &m_size, 0);

            for (unsigned int i = 0; i < nbStreams; i++)
            {
                m_data.resize(2*m_size);
                d.readU32(10*i + 11, &tmpUInt, 0);
                d.readBlob(10*i + 12, &buf);
                m_current[i] = m_data[i].begin() + tmpUInt;
                memcpy(reinterpret_cast<char *>(m_data[i].data()), buf.data(), buf.size());
                //qDebug("DoubleBufferMutiple::deserialize: i: %u m_data.size(): %u buf.size(): %d", i, m_data[i].size(), buf.size());
                //std::copy(reinterpret_cast<char *>(m_data.data()), buf.data(), buf.data() + buf.size()); // bug
            }

            return true;
        }
        else
        {
            return false;
        }
    }

private:
	int m_size;
	std::vector<std::vector<T>> m_data;
	std::vector<typename std::vector<T>::iterator> m_current;
    int m_currentPosition;
};

#endif /* SDRBASE_UTIL_DOUBLEBUFFERMULTIPLE_H_ */
