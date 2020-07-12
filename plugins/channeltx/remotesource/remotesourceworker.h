///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCWORKER_H_
#define PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCWORKER_H_

#include <QObject>
#include <QHostAddress>

#include "util/message.h"
#include "util/messagequeue.h"

class RemoteDataQueue;
class RemoteDataBlock;
class QUdpSocket;

class RemoteSourceWorker : public QObject {
    Q_OBJECT
public:
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

    RemoteSourceWorker(RemoteDataQueue *dataQueue, QObject* parent = 0);
    ~RemoteSourceWorker();

    void startWork();
    void stopWork();
    void dataBind(const QString& address, uint16_t port);

private:
    volatile bool m_running;

    MessageQueue m_inputMessageQueue;
    RemoteDataQueue *m_dataQueue;

    QHostAddress m_address;
    QUdpSocket *m_socket;

    static const uint32_t m_nbDataBlocks = 4;          //!< number of data blocks in the ring buffer
    RemoteDataBlock *m_dataBlocks[m_nbDataBlocks];  //!< ring buffer of data blocks indexed by frame affinity

private slots:
    void handleInputMessages();
    void readPendingDatagrams();
};



#endif /* PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCWORKER_H_ */
