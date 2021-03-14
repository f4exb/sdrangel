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
    m_objMutex(QMutex::NonRecursive)
{
    cleanUp();
    m_intTotalReceived = 0;
    m_intPacketReceived = 0;
    m_intMemoryLimit = DefaultMemoryLimit;
    MultiThreaded = false;
    ThreadTimeOut = -1;

    m_objeventLoop.connect(this,SIGNAL(onDataAvailable()), &m_objeventLoop, SLOT(quit()),Qt::QueuedConnection);
}

DATVideostream::~DATVideostream()
{
    m_objeventLoop.disconnect(this,SIGNAL(onDataAvailable()), &m_objeventLoop, SLOT(quit()));
    cleanUp();
}

void DATVideostream::cleanUp()
{
    if (m_objFIFO.size() > 0) {
        m_objFIFO.clear();
    }

    if (m_objeventLoop.isRunning()) {
        m_objeventLoop.exit();
    }

    m_intBytesAvailable = 0;
    m_intBytesWaiting = 0;
    m_intQueueWaiting = 0;
    m_intPercentBuffer = 0;
}

bool DATVideostream::setMemoryLimit(int intMemoryLimit)
{
    if (intMemoryLimit <= 0) {
        return false;
    }

    m_intMemoryLimit = intMemoryLimit;

    return true;
}

void DATVideostream::resetTotalReceived()
{
    m_intTotalReceived = 0;
    emit onDataPackets(&m_intQueueWaiting, &m_intBytesWaiting, &m_intPercentBuffer, &m_intTotalReceived);
}

int DATVideostream::pushData(const char * chrData, int intSize)
{
    if (intSize <= 0) {
        return 0;
    }

    m_objMutex.lock();

    m_intPacketReceived++;
    m_intBytesWaiting += intSize;

    if (m_intBytesWaiting > m_intMemoryLimit) {
        m_intBytesWaiting -= m_objFIFO.dequeue().size();
    }

    m_objFIFO.enqueue(QByteArray(chrData,intSize));
    m_intBytesAvailable = m_objFIFO.head().size();
    m_intTotalReceived += intSize;
    m_intQueueWaiting=m_objFIFO.count();

    m_objMutex.unlock();

    if ((m_objeventLoop.isRunning())
        && (m_intQueueWaiting >= MinStackSize))
    {
        emit onDataAvailable();
    }

    if (m_intPacketReceived % MinStackSize == 1)
    {
        m_intPercentBuffer = (100*m_intBytesWaiting)/m_intMemoryLimit;

        if (m_intPercentBuffer > 100) {
            m_intPercentBuffer = 100;
        }

        emit onDataPackets(&m_intQueueWaiting, &m_intBytesWaiting, &m_intPercentBuffer, &m_intTotalReceived);
    }

    return intSize;
}

bool DATVideostream::isSequential() const
{
    return true;
}

qint64  DATVideostream::bytesAvailable() const
{
    return m_intBytesAvailable;
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
    QByteArray objCurrentArray;
    int intEffectiveLen = 0;
    int intExpectedLen = 0;
    int intThreadLoop = 0;

    intExpectedLen = (int) len;

    if (intExpectedLen <= 0) {
        return 0;
    }

    if (m_objeventLoop.isRunning()) {
        return 0;
    }

    m_objMutex.lock();

    //DATA in FIFO ? -> Waiting for DATA
    if ((m_objFIFO.isEmpty()) || (m_objFIFO.count()<MinStackSize))
    {
        m_objMutex.unlock();

        if (MultiThreaded == true)
        {
            intThreadLoop=0;

            while ((m_objFIFO.isEmpty()) || (m_objFIFO.count() < MinStackSize))
            {
                QThread::msleep(5);
                intThreadLoop++;

                if (ThreadTimeOut >= 0)
                {
                    if (intThreadLoop*5 > ThreadTimeOut) {
                        return -1;
                    }
                }
            }
        }
        else
        {
            m_objeventLoop.exec();
        }

        m_objMutex.lock();
    }

    //Read DATA
    intEffectiveLen=m_objFIFO.head().size();

    if (intExpectedLen < intEffectiveLen)
    {
        //Partial Read
        objCurrentArray = m_objFIFO.head();
        memcpy((void *)data,objCurrentArray.constData(),intExpectedLen);
        m_objFIFO.head().remove(0,intExpectedLen);
        intEffectiveLen = intExpectedLen;
        m_intBytesWaiting -= intExpectedLen;
    }
    else
    {
        //Complete Read
        objCurrentArray = m_objFIFO.dequeue();
        memcpy((void *)data,objCurrentArray.constData(),intEffectiveLen);
        m_intBytesWaiting -= intEffectiveLen;
    }

    m_intQueueWaiting = m_objFIFO.count();
    m_intPercentBuffer = (100*m_intBytesWaiting) / m_intMemoryLimit;

    emit onDataPackets(&m_intQueueWaiting, &m_intBytesWaiting, &m_intPercentBuffer, &m_intTotalReceived);

    //Next available DATA
    m_intBytesAvailable = m_objFIFO.head().size();

    m_objMutex.unlock();

    return (qint64)intEffectiveLen;
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
