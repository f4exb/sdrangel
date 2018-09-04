///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) UDP sender thread                                 //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QHostAddress>

#include "util/message.h"
#include "util/messagequeue.h"

class SDRDaemonDataQueue;
class SDRDaemonDataBlock;
class CM256;
class QUdpSocket;

class DaemonSinkThread : public QThread {
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

    DaemonSinkThread(SDRDaemonDataQueue *dataQueue, CM256 *cm256, QObject* parent = 0);
    ~DaemonSinkThread();

    void startStop(bool start);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

    SDRDaemonDataQueue *m_dataQueue;
    CM256 *m_cm256;                       //!< CM256 library object

    QHostAddress m_address;
    QUdpSocket *m_socket;

    MessageQueue m_inputMessageQueue;

    void startWork();
    void stopWork();

    void run();
    bool handleDataBlock(SDRDaemonDataBlock& dataBlock);

private slots:
    void handleData();
    void handleInputMessages();
};
