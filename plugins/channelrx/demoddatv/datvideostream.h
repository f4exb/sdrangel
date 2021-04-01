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

#define MinStackSize    4
#define DefaultMemoryLimit  2048000

class DATVideostream : public QIODevice
{
    Q_OBJECT

public:
    DATVideostream();
    ~DATVideostream();

    bool MultiThreaded;
    int ThreadTimeOut;

    int pushData(const char * chrData, int intSize);
    bool setMemoryLimit(int intMemoryLimit);
    void resetTotalReceived();
    void cleanUp();

    virtual bool isSequential() const;
    virtual qint64 bytesAvailable() const;
    virtual void close();
    virtual bool open(OpenMode mode);

    QQueue<QByteArray> m_objFIFO;

signals:

    void onDataAvailable();
    void onDataPackets(int *intDataPackets, int *intDataBytes, int *intPercentBuffer,qint64 *intTotalReceived);

protected:

    virtual qint64 readData(char *data, qint64 len);
    virtual qint64 writeData(const char *data, qint64 len);
    virtual qint64 readLineData(char *data, qint64 maxSize);

private:

    QEventLoop m_objeventLoop;
    QMutex m_objMutex;
    int m_intMemoryLimit;
    int m_intBytesAvailable;
    int m_intBytesWaiting;
    int m_intQueueWaiting;
    int m_intPercentBuffer;
    qint64 m_intTotalReceived;
    qint64 m_intPacketReceived;

};

#endif // DATVIDEOSTREAM_H
