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

#include <channel/remotedatablock.h>
#include <channel/remotedataqueue.h>
#include <algorithm>

#include <QUdpSocket>
#include "cm256cc/cm256.h"

#include "remotesourceworker.h"

MESSAGE_CLASS_DEFINITION(RemoteSourceWorker::MsgDataBind, Message)

RemoteSourceWorker::RemoteSourceWorker(RemoteDataQueue *dataQueue, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_address(QHostAddress::LocalHost),
    m_socket(nullptr)
{
    std::fill(m_dataBlocks, m_dataBlocks+4, (RemoteDataBlock *) 0);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

RemoteSourceWorker::~RemoteSourceWorker()
{
    qDebug("RemoteSourceWorker::~RemoteSourceWorker");
}

void RemoteSourceWorker::dataBind(const QString& address, uint16_t port)
{
    MsgDataBind *msg = MsgDataBind::create(address, port);
    m_inputMessageQueue.push(msg);
}

void RemoteSourceWorker::startWork()
{
    qDebug("RemoteSourceWorker::startWork");
    m_socket = new QUdpSocket(this);
    m_running = false;
}

void RemoteSourceWorker::stopWork()
{
    qDebug("RemoteSourceWorker::stopWork");
    delete m_socket;
    m_socket = nullptr;
    m_running = false;
}

void RemoteSourceWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgDataBind::match(*message))
        {
            MsgDataBind* notif = (MsgDataBind*) message;
            qDebug("RemoteSourceWorker::handleInputMessages: MsgDataBind: %s:%d", qPrintable(notif->getAddress().toString()), notif->getPort());

            if (m_socket)
            {
                disconnect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
                m_socket->bind(notif->getAddress(), notif->getPort());
                connect(m_socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
            }
        }
    }
}

void RemoteSourceWorker::readPendingDatagrams()
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
                    //qDebug("RemoteSourceWorker::readPendingDatagrams: push frame %u", frameIndex);
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
            qWarning("RemoteSourceWorker::readPendingDatagrams: wrong super block size not processing");
        }
    }
}

