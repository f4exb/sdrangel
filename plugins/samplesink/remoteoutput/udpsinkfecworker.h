///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFECWORKER_H_
#define PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFECWORKER_H_

#include <channel/remotedatablock.h>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QHostAddress>

#include "cm256cc/cm256.h"

#include "util/messagequeue.h"
#include "util/message.h"

class QUdpSocket;

class UDPSinkFECWorker : public QThread
{
    Q_OBJECT
public:
    class MsgUDPFECEncodeAndSend : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        RemoteSuperBlock *getTxBlocks() const { return m_txBlockx; }
        uint32_t getNbBlocsFEC() const { return m_nbBlocksFEC; }
        uint32_t getTxDelay() const { return m_txDelay; }
        uint16_t getFrameIndex() const { return m_frameIndex; }

        static MsgUDPFECEncodeAndSend* create(
                RemoteSuperBlock *txBlocks,
                uint32_t nbBlocksFEC,
                uint32_t txDelay,
                uint16_t frameIndex)
        {
            return new MsgUDPFECEncodeAndSend(txBlocks, nbBlocksFEC, txDelay, frameIndex);
        }

    private:
        RemoteSuperBlock *m_txBlockx;
        uint32_t m_nbBlocksFEC;
        uint32_t m_txDelay;
        uint16_t m_frameIndex;

        MsgUDPFECEncodeAndSend(
                RemoteSuperBlock *txBlocks,
                uint32_t nbBlocksFEC,
                uint32_t txDelay,
                uint16_t frameIndex) :
            m_txBlockx(txBlocks),
            m_nbBlocksFEC(nbBlocksFEC),
            m_txDelay(txDelay),
            m_frameIndex(frameIndex)
        {}
    };

    class MsgConfigureRemoteAddress : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const QString& getAddress() const { return m_address; }
        uint16_t getPort() const { return m_port; }

        static MsgConfigureRemoteAddress* create(const QString& address, uint16_t port)
        {
            return new MsgConfigureRemoteAddress(address, port);
        }

    private:
        QString m_address;
        uint16_t m_port;

        MsgConfigureRemoteAddress(const QString& address, uint16_t port) :
            m_address(address),
            m_port(port)
        {}
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    UDPSinkFECWorker();
    ~UDPSinkFECWorker();

    void startStop(bool start);

    void pushTxFrame(RemoteSuperBlock *txBlocks,
        uint32_t nbBlocksFEC,
        uint32_t txDelay,
        uint16_t frameIndex);
    void setRemoteAddress(const QString& address, uint16_t port);

    MessageQueue m_inputMessageQueue;    //!< Queue for asynchronous inbound communication

private slots:
    void handleInputMessages();

private:
    void startWork();
    void stopWork();
    void run();
    void encodeAndTransmit(RemoteSuperBlock *txBlockx, uint16_t frameIndex, uint32_t nbBlocksFEC, uint32_t txDelay);

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    volatile bool m_running;
    CM256 m_cm256;                       //!< CM256 library object
    bool m_cm256Valid;                   //!< true if CM256 library is initialized correctly
    QUdpSocket   *m_udpSocket;
    QString      m_remoteAddress;
    uint16_t     m_remotePort;
    QHostAddress m_remoteHostAddress;
};

#endif /* PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFECWORKER_H_ */
