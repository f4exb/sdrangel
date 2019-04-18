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

#include "remotesinkthread.h"

#include <channel/remotedatablock.h>
#include <QUdpSocket>

#include "cm256cc/cm256.h"

MESSAGE_CLASS_DEFINITION(RemoteSinkThread::MsgStartStop, Message)

RemoteSinkThread::RemoteSinkThread(QObject* parent) :
    QThread(parent),
    m_running(false),
    m_address(QHostAddress::LocalHost),
    m_socket(0)
{

    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

RemoteSinkThread::~RemoteSinkThread()
{
    qDebug("RemoteSinkThread::~RemoteSinkThread");
}

void RemoteSinkThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void RemoteSinkThread::startWork()
{
    qDebug("RemoteSinkThread::startWork");
	m_startWaitMutex.lock();
	m_socket = new QUdpSocket(this);
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void RemoteSinkThread::stopWork()
{
	qDebug("RemoteSinkThread::stopWork");
    delete m_socket;
    m_socket = 0;
	m_running = false;
	wait();
}

void RemoteSinkThread::run()
{
    qDebug("RemoteSinkThread::run: begin");
	m_running = true;
	m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("RemoteSinkThread::run: end");
}

void RemoteSinkThread::processDataBlock(RemoteDataBlock *dataBlock)
{
    handleDataBlock(*dataBlock);
    delete dataBlock;
}

void RemoteSinkThread::handleDataBlock(RemoteDataBlock& dataBlock)
{
	CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
	CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
	RemoteProtectedBlock fecBlocks[256];   //!< FEC data

    uint16_t frameIndex = dataBlock.m_txControlBlock.m_frameIndex;
    int nbBlocksFEC = dataBlock.m_txControlBlock.m_nbBlocksFEC;
    int txDelay = dataBlock.m_txControlBlock.m_txDelay;
    m_address.setAddress(dataBlock.m_txControlBlock.m_dataAddress);
    uint16_t dataPort = dataBlock.m_txControlBlock.m_dataPort;
    RemoteSuperBlock *txBlockx = dataBlock.m_superBlocks;

    if ((nbBlocksFEC == 0) || !m_cm256p) // Do not FEC encode
    {
        if (m_socket)
        {
            for (int i = 0; i < RemoteNbOrginalBlocks; i++)
            {
                // send block via UDP
                m_socket->writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_address, dataPort);
                usleep(txDelay);
            }
        }
    }
    else
    {
        cm256Params.BlockBytes = sizeof(RemoteProtectedBlock);
        cm256Params.OriginalCount = RemoteNbOrginalBlocks;
        cm256Params.RecoveryCount = nbBlocksFEC;

        // Fill pointers to data
        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; ++i)
        {
            if (i >= cm256Params.OriginalCount) {
                memset((void *) &txBlockx[i].m_protectedBlock, 0, sizeof(RemoteProtectedBlock));
            }

            txBlockx[i].m_header.m_frameIndex = frameIndex;
            txBlockx[i].m_header.m_blockIndex = i;
            txBlockx[i].m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            txBlockx[i].m_header.m_sampleBits = SDR_RX_SAMP_SZ;
            descriptorBlocks[i].Block = (void *) &(txBlockx[i].m_protectedBlock);
            descriptorBlocks[i].Index = txBlockx[i].m_header.m_blockIndex;
        }

        // Encode FEC blocks
        if (m_cm256p->cm256_encode(cm256Params, descriptorBlocks, fecBlocks))
        {
            qWarning("RemoteSinkThread::handleDataBlock: CM256 encode failed. No transmission.");
            // TODO: send without FEC changing meta data to set indication of no FEC
        }

        // Merge FEC with data to transmit
        for (int i = 0; i < cm256Params.RecoveryCount; i++)
        {
            txBlockx[i + cm256Params.OriginalCount].m_protectedBlock = fecBlocks[i];
        }

        // Transmit all blocks
        if (m_socket)
        {
            for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++)
            {
                // send block via UDP
                m_socket->writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_address, dataPort);
                usleep(txDelay);
            }
        }
    }

    dataBlock.m_txControlBlock.m_processed = true;
}

void RemoteSinkThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("RemoteSinkThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}
