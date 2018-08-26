///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx) UDP receiver thread                             //
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

#include <QUdpSocket>

#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"
#include "channel/sdrdaemonchannelsourcethread.h"

#include "cm256.h"

MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSourceThread::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSourceThread::MsgDataBind, Message)

SDRDaemonChannelSourceThread::SDRDaemonChannelSourceThread(SDRDaemonDataQueue *dataQueue, CM256 *cm256, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_cm256(cm256),
    m_address(QHostAddress::LocalHost),
    m_socket(0)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

SDRDaemonChannelSourceThread::~SDRDaemonChannelSourceThread()
{
    qDebug("SDRDaemonChannelSourceThread::~SDRDaemonChannelSourceThread");
}

void SDRDaemonChannelSourceThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void SDRDaemonChannelSourceThread::dataBind(const QString& address, uint16_t port)
{
    MsgDataBind *msg = MsgDataBind::create(address, port);
    m_inputMessageQueue.push(msg);
}

void SDRDaemonChannelSourceThread::startWork()
{
    qDebug("SDRDaemonChannelSourceThread::startWork");
    m_startWaitMutex.lock();
    m_socket = new QUdpSocket(this);
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void SDRDaemonChannelSourceThread::stopWork()
{
    qDebug("SDRDaemonChannelSourceThread::stopWork");
    delete m_socket;
    m_socket = 0;
    m_running = false;
    wait();
}

void SDRDaemonChannelSourceThread::run()
{
    qDebug("SDRDaemonChannelSourceThread::run: begin");
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("SDRDaemonChannelSourceThread::run: end");
}


void SDRDaemonChannelSourceThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("SDRDaemonChannelSourceThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
        else if (MsgDataBind::match(*message))
        {
            MsgDataBind* notif = (MsgDataBind*) message;
            qDebug("SDRDaemonChannelSourceThread::handleInputMessages: MsgDataBind: %s:%d", qPrintable(notif->getAddress().toString()), notif->getPort());

            if (m_socket)
            {
                disconnect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
                m_socket->bind(notif->getAddress(), notif->getPort());
                connect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
            }
        }
    }
}

void SDRDaemonChannelSourceThread::readPendingDatagrams()
{
    char data[1024];
    while (m_socket->hasPendingDatagrams())
    {
        QHostAddress sender;
        quint16 senderPort = 0;
        qint64 pendingDataSize = m_socket->pendingDatagramSize();
        m_socket->readDatagram(data, pendingDataSize, &sender, &senderPort);
        qDebug("SDRDaemonChannelSourceThread::readPendingDatagrams: %lld bytes received from %s:%d",
                pendingDataSize,
                qPrintable(sender.toString()),
                senderPort);

    }
}
