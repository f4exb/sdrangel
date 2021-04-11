///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#include "datvideostream.h"
#include <stdio.h>

DATVideostream::DATVideostream():
    m_mutex(QMutex::NonRecursive)
{
    cleanUp();
    m_totalReceived = 0;
    m_packetReceived = 0;
    m_memoryLimit = m_defaultMemoryLimit;
    m_multiThreaded = false;
    m_threadTimeout = -1;

    m_eventLoop.connect(this, SIGNAL(dataAvailable()), &m_eventLoop, SLOT(quit()), Qt::QueuedConnection);
}

DATVideostream::~DATVideostream()
{
    m_eventLoop.disconnect(this, SIGNAL(dataAvailable()), &m_eventLoop, SLOT(quit()));
    cleanUp();
}

void DATVideostream::cleanUp()
{
    if (m_fifo.size() > 0) {
        m_fifo.clear();
    }

    if (m_eventLoop.isRunning()) {
        m_eventLoop.exit();
    }

    m_bytesAvailable = 0;
    m_bytesWaiting = 0;
    m_percentBuffer = 0;
}

void DATVideostream::resetTotalReceived()
{
    m_totalReceived = 0;
    emit fifoData(m_bytesWaiting, m_percentBuffer, m_totalReceived);
}

void DATVideostream::setMultiThreaded(bool multiThreaded)
{
    if (multiThreaded)
    {
        if (m_eventLoop.isRunning()) {
            m_eventLoop.exit();
        }
    }

    m_multiThreaded = multiThreaded;
}

int DATVideostream::pushData(const char * chrData, int intSize)
{
    if (intSize <= 0) {
        return 0;
    }

    m_mutex.lock();

    m_packetReceived++;
    m_bytesWaiting += intSize;

    if (m_bytesWaiting > m_memoryLimit) {
        m_bytesWaiting -= m_fifo.dequeue().size();
    }

    m_fifo.enqueue(QByteArray(chrData,intSize));
    m_bytesAvailable = m_fifo.head().size();
    m_totalReceived += intSize;

    m_mutex.unlock();

    if (m_eventLoop.isRunning()) {
        emit dataAvailable();
    }

    m_percentBuffer = (100*m_bytesWaiting) / m_memoryLimit;
    m_percentBuffer = m_percentBuffer > 100 ? 100 : m_percentBuffer;

    if (m_packetReceived % 10 == 1) {
        emit fifoData(m_bytesWaiting, m_percentBuffer, m_totalReceived);
    }

    return intSize;
}

bool DATVideostream::isSequential() const
{
    return true;
}

qint64  DATVideostream::bytesAvailable() const
{
    return m_bytesAvailable;
}

void  DATVideostream::close()
{
    QIODevice::close();
    cleanUp();
}

bool  DATVideostream::open(OpenMode mode)
{
    //cleanUp();
    return QIODevice::open(mode);
}

//PROTECTED

qint64 DATVideostream::readData(char *data, qint64 len)
{
    QByteArray currentArray;
    int effectiveLen = 0;
    int expectedLen = (int) len;
    int threadLoop = 0;

    if (expectedLen <= 0) {
        return 0;
    }

    if (m_eventLoop.isRunning()) {
        return 0;
    }

    m_mutex.lock();

    //DATA in FIFO ? -> Waiting for DATA
    if ((m_fifo.isEmpty()) || (m_fifo.count() < m_minStackSize))
    {
        m_mutex.unlock();

        if (m_multiThreaded == true)
        {
            threadLoop = 0;

            while ((m_fifo.isEmpty()) || (m_fifo.count() < m_minStackSize))
            {
                QThread::msleep(5);
                threadLoop++;

                if (m_threadTimeout >= 0)
                {
                    if (threadLoop*5 > m_threadTimeout) {
                        return -1;
                    }
                }
            }
        }
        else
        {
            m_eventLoop.exec();
        }

        m_mutex.lock();
    }

    //Read DATA
    effectiveLen = m_fifo.head().size();

    if (expectedLen < effectiveLen)
    {
        //Partial Read
        currentArray = m_fifo.head();
        std::copy(
            currentArray.constData(),
            currentArray.constData() + expectedLen,
            data
        );
        m_fifo.head().remove(0, expectedLen);
        effectiveLen = expectedLen;
        m_bytesWaiting -= expectedLen;
    }
    else
    {
        //Complete Read
        currentArray = m_fifo.dequeue();
        std::copy(
            currentArray.constData(),
            currentArray.constData() + effectiveLen,
            data
        );
        m_bytesWaiting -= effectiveLen;
    }

    m_percentBuffer = (100*m_bytesWaiting) / m_memoryLimit;
    m_percentBuffer = m_percentBuffer > 100 ? 100 : m_percentBuffer;

    if (m_packetReceived % 10 == 0) {
        emit fifoData(m_bytesWaiting, m_percentBuffer, m_totalReceived);
    }

    //Next available DATA
    m_bytesAvailable = m_fifo.head().size();

    m_mutex.unlock();

    return (qint64) effectiveLen;
}

qint64 DATVideostream::writeData(const char *data, qint64 len)
{
    (void) data;
    (void) len;
    return 0;
}

qint64 	DATVideostream::readLineData(char *data, qint64 maxSize)
{
    (void) data;
    (void) maxSize;
    return 0;
}
