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

#include "remotesourcethread.h"

#include <channel/remotedatablock.h>
#include <channel/remotedataqueue.h>
#include <algorithm>

#include <QUdpSocket>
#include "cm256.h"



MESSAGE_CLASS_DEFINITION(RemoteSourceThread::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RemoteSourceThread::MsgDataBind, Message)

RemoteSourceThread::RemoteSourceThread(RemoteDataQueue *dataQueue, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_address(QHostAddress::LocalHost),
    m_socket(0)
{
    std::fill(m_dataBlocks, m_dataBlocks+4, (RemoteDataBlock *) 0);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

RemoteSourceThread::~RemoteSourceThread()
{
    qDebug("RemoteSourceThread::~RemoteSourceThread");
}

void RemoteSourceThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void RemoteSourceThread::dataBind(const QString& address, uint16_t port)
{
    MsgDataBind *msg = MsgDataBind::create(address, port);
    m_inputMessageQueue.push(msg);
}

void RemoteSourceThread::startWork()
{
    qDebug("RemoteSourceThread::startWork");
    m_startWaitMutex.lock();
    m_socket = new QUdpSocket(this);
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void RemoteSourceThread::stopWork()
{
    qDebug("RemoteSourceThread::stopWork");
    delete m_socket;
    m_socket = 0;
    m_running = false;
    wait();
}

void RemoteSourceThread::run()
{
    qDebug("RemoteSourceThread::run: begin");
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("RemoteSourceThread::run: end");
}


void RemoteSourceThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("RemoteSourceThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

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
            qDebug("RemoteSourceThread::handleInputMessages: MsgDataBind: %s:%d", qPrintable(notif->getAddress().toString()), notif->getPort());

            if (m_socket)
            {
                disconnect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
                m_socket->bind(notif->getAddress(), notif->getPort());
                connect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
            }
        }
    }
}

void RemoteSourceThread::readPendingDatagrams()
{
    RemoteSuperBlock superBlock;
    qint64 size;

    while (m_socket->hasPendingDatagrams())
    {
        QHostAddress sender;
        quint16 senderPort = 0;
        //qint64 pendingDataSize = m_socket->pendingDatagramSize();
        size = m_socket->readDatagram((char *) &superBlock, (long long int) sizeof(RemoteSuperBlock), &sender, &senderPort);

        if (size == sizeof(RemoteSuperBlock))
        {
            unsigned int dataBlockIndex = superBlock.m_header.m_frameIndex % m_nbDataBlocks;

            // create the first block for this index
            if (m_dataBlocks[dataBlockIndex] == 0) {
                m_dataBlocks[dataBlockIndex] = new RemoteDataBlock();
            }

            if (m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_frameIndex < 0)
            {
                // initialize virgin block with the frame index
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_frameIndex = superBlock.m_header.m_frameIndex;
            }
            else
            {
                // if the frame index is not the same for the same slot it means we are starting a new frame
                uint32_t frameIndex = m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_frameIndex;

                if (superBlock.m_header.m_frameIndex != frameIndex)
                {
                    //qDebug("RemoteSourceThread::readPendingDatagrams: push frame %u", frameIndex);
                    m_dataQueue->push(m_dataBlocks[dataBlockIndex]);
                    m_dataBlocks[dataBlockIndex] = new RemoteDataBlock();
                    m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_frameIndex = superBlock.m_header.m_frameIndex;
                }
            }

            m_dataBlocks[dataBlockIndex]->m_superBlocks[superBlock.m_header.m_blockIndex] = superBlock;

            if (superBlock.m_header.m_blockIndex == 0) {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_metaRetrieved = true;
            }

            if (superBlock.m_header.m_blockIndex < RemoteNbOrginalBlocks) {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_originalCount++;
            } else {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_recoveryCount++;
            }

            m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_blockCount++;
        }
        else
        {
            qWarning("RemoteSourceThread::readPendingDatagrams: wrong super block size not processing");
        }
    }
}

