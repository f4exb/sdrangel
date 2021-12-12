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
#include <QUdpSocket>

#include "util/message.h"
#include "util/messagequeue.h"

class RemoteDataQueue;
class RemoteDataFrame;

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

    bool startWork();
    void stopWork();
    void dataBind(const QString& address, uint16_t port);

private:
    volatile bool m_running;

    MessageQueue m_inputMessageQueue;
    RemoteDataQueue *m_dataQueue;

    QHostAddress m_address;
    QUdpSocket m_socket;
    QMutex m_mutex;

    static const uint32_t m_nbDataFrames = 4;       //!< number of data frames in the ring buffer
    RemoteDataFrame *m_dataFrames[m_nbDataFrames];  //!< ring buffer of data frames indexed by frame affinity
    uint32_t m_sampleRate;                          //!< current sample rate from meta data

    qint64 m_udpReadBytes;
    char *m_udpBuf;

    static int getDataSocketBufferSize(uint32_t inSampleRate);
    void processData();

private slots:
    void started();
    void finished();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void handleInputMessages();
    void dataReadyRead();
};



#endif /* PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCWORKER_H_ */
