///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "m17modfifo.h"

M17ModFIFO::M17ModFIFO() :
	m_fifo(nullptr)
{
	m_size = 0;
	m_writeIndex = 0;
	m_readIndex = 0;
}

M17ModFIFO::M17ModFIFO(uint32_t numSamples) :
	m_fifo(nullptr)
{
	QMutexLocker mutexLocker(&m_mutex);
	create(numSamples);
}

M17ModFIFO::~M17ModFIFO()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fifo)
	{
		delete[] m_fifo;
		m_fifo = nullptr;
	}

	m_size = 0;
}

void M17ModFIFO::setSize(uint32_t numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);
	create(numSamples);
}

void M17ModFIFO::create(uint32_t numSamples)
{
	if (m_fifo)
	{
		delete[] m_fifo;
		m_fifo = nullptr;
	}

	m_writeIndex = 0;
	m_readIndex = 0;

	m_fifo = new int16_t[numSamples];
	m_size = numSamples;
}

uint32_t M17ModFIFO::write(const int16_t* data, uint32_t numSamples)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_readIndex > m_writeIndex)
    {
        int rem = m_readIndex - m_writeIndex;
        int written = std::min(rem, (int) numSamples);
        std::copy(data, data + written, &m_fifo[m_writeIndex]);
        m_writeIndex += written;
        return written;
    }

    int remSize = m_size - m_writeIndex;
    int written = std::min(remSize, (int) numSamples);
    std::copy(data, data + written, &m_fifo[m_writeIndex]);
    m_writeIndex += written;
    m_writeIndex = m_writeIndex == (int) m_size ? 0 : m_writeIndex;

    if (written == (int) numSamples) {
        return written;
    }

    // wrap
    int remWrite = (int) numSamples - written;
    int remWritten = std::min(remWrite, m_readIndex);
    std::copy(data + written, data + written + remWritten, m_fifo);
    m_writeIndex = remWritten;
    return written + remWritten;
}

uint32_t M17ModFIFO::readOne(int16_t* data)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_readIndex == m_writeIndex)
    {
        *data = 0;
        return 0;
    }

    *data = m_fifo[m_readIndex++];
    m_readIndex = m_readIndex == (int) m_size ? 0 : m_readIndex;
    return 1;
}

