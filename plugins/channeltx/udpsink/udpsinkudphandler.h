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
#include <stdint.h>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"

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
    void resizeBuffer(float sampleRate);

    void readSample(FixReal &t);
    void readSample(Sample &s);

    void setAutoRWBalance(bool autoRWBalance) { m_autoRWBalance = autoRWBalance; }
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
    static const int m_minNbUDPFrames = 256;  // number of frames of block size in the UDP buffer

public slots:
    void dataReadyRead();

private:
    class MsgUDPAddressAndPort : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getAddress() const { return m_address; }
        quint16 getPort() const { return m_port; }

        static MsgUDPAddressAndPort* create(QString address, quint16 port)
        {
            return new MsgUDPAddressAndPort(address, port);
        }

    private:
        QString m_address;
        quint16 m_port;

        MsgUDPAddressAndPort(QString address, quint16 port) :
            Message(),
            m_address(address),
            m_port(port)
        { }
    };

    typedef char (udpBlk_t)[m_udpBlockSize];

    void moveData(char *blk);
    void advanceReadPointer(int nbBytes);
    void applyUDPLink(const QString& address, quint16 port);
    bool handleMessage(const Message& message);

    QUdpSocket *m_dataSocket;
    QHostAddress m_dataAddress;
    QHostAddress m_remoteAddress;
    quint16 m_dataPort;
    quint16 m_remotePort;
    bool m_dataConnected;
    udpBlk_t *m_udpBuf;
    char m_udpDump[m_udpBlockSize + 8192]; // UDP block size + largest possible block
    int m_udpDumpIndex;
    int m_nbUDPFrames;
    int m_nbAllocatedUDPFrames;
    int m_writeIndex;
    int m_readFrameIndex;
    int m_readIndex;
    int m_rwDelta;
    float m_d;
    bool m_autoRWBalance;
    MessageQueue *m_feedbackMessageQueue;
    MessageQueue m_inputMessageQueue;

private slots:
    void handleMessages();
};



#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKUDPHANDLER_H_ */
