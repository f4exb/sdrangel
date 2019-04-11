///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCTHREAD_H_
#define PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QHostAddress>

#include "util/message.h"
#include "util/messagequeue.h"

class RemoteDataQueue;
class RemoteDataBlock;
class QUdpSocket;

class RemoteSourceThread : public QThread {
    Q_OBJECT
public:
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

    class MsgDataBind : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QHostAddress getAddress() const { return m_address; }
        uint16_t getPort() const { return m_port; }

        static MsgDataBind* create(const QString& address, uint16_t port) {
            return new MsgDataBind(address, port);
        }

    protected:
        QHostAddress m_address;
        uint16_t m_port;

        MsgDataBind(const QString& address, uint16_t port) :
            Message(),
            m_port(port)
        {
            m_address.setAddress(address);
        }
    };

    RemoteSourceThread(RemoteDataQueue *dataQueue, QObject* parent = 0);
    ~RemoteSourceThread();

    void startStop(bool start);
    void dataBind(const QString& address, uint16_t port);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    volatile bool m_running;

    MessageQueue m_inputMessageQueue;
    RemoteDataQueue *m_dataQueue;

    QHostAddress m_address;
    QUdpSocket *m_socket;

    static const uint32_t m_nbDataBlocks = 4;          //!< number of data blocks in the ring buffer
    RemoteDataBlock *m_dataBlocks[m_nbDataBlocks];  //!< ring buffer of data blocks indexed by frame affinity

    void startWork();
    void stopWork();

    void run();

private slots:
    void handleInputMessages();
    void readPendingDatagrams();
};



#endif /* PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCTHREAD_H_ */
