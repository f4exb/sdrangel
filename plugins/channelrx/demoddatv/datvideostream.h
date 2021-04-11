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

#ifndef DATVIDEOSTREAM_H
#define DATVIDEOSTREAM_H

#include <QIODevice>
#include <QQueue>
#include <QByteArray>
#include <QEventLoop>
#include <QMutex>
#include <QThread>

class DATVideostream : public QIODevice
{
    Q_OBJECT

public:
    DATVideostream();
    virtual ~DATVideostream();

    int pushData(const char * chrData, int intSize);
    void resetTotalReceived();
    void cleanUp();
    void setMultiThreaded(bool multiThreaded);
    void setThreadTimeout(int timeOut) { m_threadTimeout = timeOut; }

    virtual bool isSequential() const;
    virtual qint64 bytesAvailable() const;
    virtual void close();
    virtual bool open(OpenMode mode);

    static const int m_defaultMemoryLimit = 2820000;
    static const int m_minStackSize = 4;

signals:
    void dataAvailable();
    void fifoData(int intDataBytes, int intPercentBuffer, qint64 intTotalReceived);

protected:
    virtual qint64 readData(char *data, qint64 len);
    virtual qint64 writeData(const char *data, qint64 len);
    virtual qint64 readLineData(char *data, qint64 maxSize);

private:
    QQueue<QByteArray> m_fifo;
    bool m_multiThreaded;
    int m_threadTimeout;

    QEventLoop m_eventLoop;
    QMutex m_mutex;
    int m_memoryLimit;
    int m_bytesAvailable;
    int m_bytesWaiting;
    int m_percentBuffer;
    qint64 m_totalReceived;
    qint64 m_packetReceived;

};

#endif // DATVIDEOSTREAM_H
