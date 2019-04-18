///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB.                             //
//                                                                               //
// Remote sink channel (Rx) UDP sender thread                                    //
//                                                                               //
// SDRangel can work as a detached SDR front end. With this plugin it can        //
// sends the I/Q samples stream to another SDRangel instance via UDP.            //
// It is controlled via a Web REST API.                                          //
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

#ifndef PLUGINS_CHANNELRX_REMOTESINK_REMOTESINKTHREAD_H_
#define PLUGINS_CHANNELRX_REMOTESINK_REMOTESINKTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QHostAddress>

#include "cm256cc/cm256.h"

#include "util/message.h"
#include "util/messagequeue.h"

class RemoteDataBlock;
class CM256;
class QUdpSocket;

class RemoteSinkThread : public QThread {
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

    RemoteSinkThread(QObject* parent = 0);
    ~RemoteSinkThread();

    void startStop(bool start);

public slots:
    void processDataBlock(RemoteDataBlock *dataBlock);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	volatile bool m_running;

    CM256 m_cm256;
    CM256 *m_cm256p;

    QHostAddress m_address;
    QUdpSocket *m_socket;

    MessageQueue m_inputMessageQueue;

    void startWork();
    void stopWork();

    void run();
    void handleDataBlock(RemoteDataBlock& dataBlock);

private slots:
    void handleInputMessages();
};

#endif // PLUGINS_CHANNELRX_REMOTESINK_REMOTESINKTHREAD_H_

