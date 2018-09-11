///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <algorithm>

#include <QUdpSocket>
#include "cm256.h"

#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"

#include "daemonsrcthread.h"

MESSAGE_CLASS_DEFINITION(DaemonSourceThread::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(DaemonSourceThread::MsgDataBind, Message)

DaemonSourceThread::DaemonSourceThread(SDRDaemonDataQueue *dataQueue, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_address(QHostAddress::LocalHost),
    m_socket(0)
{
    std::fill(m_dataBlocks, m_dataBlocks+4, (SDRDaemonDataBlock *) 0);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

DaemonSourceThread::~DaemonSourceThread()
{
    qDebug("DaemonSourceThread::~DaemonSourceThread");
}

void DaemonSourceThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void DaemonSourceThread::dataBind(const QString& address, uint16_t port)
{
    MsgDataBind *msg = MsgDataBind::create(address, port);
    m_inputMessageQueue.push(msg);
}

void DaemonSourceThread::startWork()
{
    qDebug("DaemonSourceThread::startWork");
    m_startWaitMutex.lock();
    m_socket = new QUdpSocket(this);
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void DaemonSourceThread::stopWork()
{
    qDebug("DaemonSourceThread::stopWork");
    delete m_socket;
    m_socket = 0;
    m_running = false;
    wait();
}

void DaemonSourceThread::run()
{
    qDebug("DaemonSourceThread::run: begin");
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("DaemonSourceThread::run: end");
}


void DaemonSourceThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("DaemonSourceThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

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
            qDebug("DaemonSourceThread::handleInputMessages: MsgDataBind: %s:%d", qPrintable(notif->getAddress().toString()), notif->getPort());

            if (m_socket)
            {
                disconnect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
                m_socket->bind(notif->getAddress(), notif->getPort());
                connect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
            }
        }
    }
}

void DaemonSourceThread::readPendingDatagrams()
{
    SDRDaemonSuperBlock superBlock;
    qint64 size;

    while (m_socket->hasPendingDatagrams())
    {
        QHostAddress sender;
        quint16 senderPort = 0;
        //qint64 pendingDataSize = m_socket->pendingDatagramSize();
        size = m_socket->readDatagram((char *) &superBlock, (long long int) sizeof(SDRDaemonSuperBlock), &sender, &senderPort);

        if (size == sizeof(SDRDaemonSuperBlock))
        {
            unsigned int dataBlockIndex = superBlock.m_header.m_frameIndex % m_nbDataBlocks;

            // create the first block for this index
            if (m_dataBlocks[dataBlockIndex] == 0) {
                m_dataBlocks[dataBlockIndex] = new SDRDaemonDataBlock();
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
                    //qDebug("DaemonSourceThread::readPendingDatagrams: push frame %u", frameIndex);
                    m_dataQueue->push(m_dataBlocks[dataBlockIndex]);
                    m_dataBlocks[dataBlockIndex] = new SDRDaemonDataBlock();
                    m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_frameIndex = superBlock.m_header.m_frameIndex;
                }
            }

            m_dataBlocks[dataBlockIndex]->m_superBlocks[superBlock.m_header.m_blockIndex] = superBlock;

            if (superBlock.m_header.m_blockIndex == 0) {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_metaRetrieved = true;
            }

            if (superBlock.m_header.m_blockIndex < SDRDaemonNbOrginalBlocks) {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_originalCount++;
            } else {
                m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_recoveryCount++;
            }

            m_dataBlocks[dataBlockIndex]->m_rxControlBlock.m_blockCount++;
        }
        else
        {
            qWarning("DaemonSourceThread::readPendingDatagrams: wrong super block size not processing");
        }
    }
}

