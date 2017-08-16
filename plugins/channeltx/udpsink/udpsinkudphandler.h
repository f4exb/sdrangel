///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINKUDPHANDLER_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINKUDPHANDLER_H_

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QMutex>

#include "dsp/dsptypes.h"

class MessageQueue;

class UDPSinkUDPHandler : public QObject
{
    Q_OBJECT
public:
    UDPSinkUDPHandler();
    virtual ~UDPSinkUDPHandler();

    void start();
    void stop();
    void configureUDPLink(const QString& address, quint16 port);
    void resetReadIndex();

    void readSample(Real &t);
    void readSample(Sample &s);

    void setFeedbackMessageQueue(MessageQueue *messageQueue) { m_feedbackMessageQueue = messageQueue; }

    /** Get buffer gauge value in % of buffer size ([-50:50])
     *  [-50:0] : write leads or read lags
     *  [0:50]  : read leads or write lags
     */
    inline int32_t getBufferGauge() const
    {
        int32_t val = m_rwDelta - (m_nbUDPFrames/2);
        return (100*val) / m_nbUDPFrames;
    }

    static const int m_udpBlockSize = 512; // UDP block size in number of bytes
    static const int m_nbUDPFrames = 256;  // number of frames of block size in the UDP buffer

public slots:
    void dataReadyRead();

private:
    void moveData();
    void advanceReadPointer(int nbBytes);

    QUdpSocket *m_dataSocket;
    QHostAddress m_dataAddress;
    QHostAddress m_remoteAddress;
    quint16 m_dataPort;
    bool m_dataConnected;
    char m_udpTmpBuf[m_udpBlockSize];
    qint64 m_udpReadBytes;
    char m_udpBuf[m_nbUDPFrames][m_udpBlockSize];
    int m_writeIndex;
    int m_readFrameIndex;
    int m_readIndex;
    int m_rwDelta;
    float m_d;
    MessageQueue *m_feedbackMessageQueue;
};



#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKUDPHANDLER_H_ */
