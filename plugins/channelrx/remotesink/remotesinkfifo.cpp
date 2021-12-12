///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "remotesinkfifo.h"

RemoteSinkFifo::RemoteSinkFifo(QObject *parent) :
    QObject(parent)
{}

RemoteSinkFifo::RemoteSinkFifo(unsigned int size, QObject *parent) :
    QObject(parent)
{
    resize(size);
}

RemoteSinkFifo::~RemoteSinkFifo()
{}

void RemoteSinkFifo::resize(unsigned int size)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_size = size;
    m_data.resize(m_size);
    m_readHead = 0;
    m_servedHead = 0;
    m_writeHead = 0;
}

void RemoteSinkFifo::reset()
{
    m_readHead = 0;
    m_servedHead = 0;
    m_writeHead = 0;
}

RemoteDataFrame *RemoteSinkFifo::getDataFrame()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_servedHead = m_writeHead;

    if (m_writeHead < m_size - 1) {
        m_writeHead++;
    } else {
        m_writeHead = 0;
    }

    emit dataBlockServed();
    return &m_data[m_servedHead];
}

unsigned int RemoteSinkFifo::readDataFrame(RemoteDataFrame **dataFrame)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (calculateRemainder() == 0)
    {
        *dataFrame = nullptr;
        return 0;
    }
    else
    {
        *dataFrame = &m_data[m_readHead];
        m_readHead = m_readHead < m_size - 1 ? m_readHead + 1 : 0;
        return calculateRemainder();
    }
}

unsigned int RemoteSinkFifo::getRemainder()
{
    QMutexLocker mutexLocker(&m_mutex);
    return calculateRemainder();
}

unsigned int RemoteSinkFifo::calculateRemainder()
{
    if (m_readHead <= m_servedHead) {
        return m_servedHead - m_readHead;
    } else {
        return m_size - (m_readHead - m_servedHead);
    }
}
