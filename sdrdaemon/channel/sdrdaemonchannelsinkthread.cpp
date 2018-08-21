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

#include <QUdpSocket>

#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"
#include "channel/sdrdaemonchannelsinkthread.h"

#include "cm256.h"

MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSinkThread::MsgStartStop, Message)

SDRDaemonChannelSinkThread::SDRDaemonChannelSinkThread(SDRDaemonDataQueue *dataQueue, CM256 *cm256, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_cm256(cm256),
    m_address(QHostAddress::LocalHost),
    m_port(9090)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    connect(m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()), Qt::QueuedConnection);
}

SDRDaemonChannelSinkThread::~SDRDaemonChannelSinkThread()
{
    qDebug("SDRDaemonChannelSinkThread::~SDRDaemonChannelSinkThread");
}

void SDRDaemonChannelSinkThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void SDRDaemonChannelSinkThread::startWork()
{
    qDebug("SDRDaemonChannelSinkThread::startWork");
	m_startWaitMutex.lock();
	m_socket = new QUdpSocket(this);
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void SDRDaemonChannelSinkThread::stopWork()
{
	qDebug("SDRDaemonChannelSinkThread::stopWork");
    delete m_socket;
	m_running = false;
	wait();
}

void SDRDaemonChannelSinkThread::run()
{
    qDebug("SDRDaemonChannelSinkThread::run: begin");
	m_running = true;
	m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("SDRDaemonChannelSinkThread::run: end");
}

bool SDRDaemonChannelSinkThread::handleDataBlock(SDRDaemonDataBlock& dataBlock)
{
	CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
	CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
	SDRDaemonProtectedBlock fecBlocks[256];   //!< FEC data

    uint16_t frameIndex = dataBlock.m_controlBlock.m_frameIndex;
    int nbBlocksFEC = dataBlock.m_controlBlock.m_nbBlocksFEC;
    int txDelay = dataBlock.m_controlBlock.m_txDelay;
    SDRDaemonSuperBlock *txBlockx = dataBlock.m_superBlocks;

    if ((nbBlocksFEC == 0) || !m_cm256) // Do not FEC encode
    {
        for (int i = 0; i < SDRDaemonNbOrginalBlocks; i++)
        {
            // send block via UDP
            m_socket->writeDatagram((const char*)&txBlockx[i], (qint64 ) SDRDaemonUdpSize, m_address, m_port);
            usleep(txDelay);
        }
    }
    else
    {
        cm256Params.BlockBytes = sizeof(SDRDaemonProtectedBlock);
        cm256Params.OriginalCount = SDRDaemonNbOrginalBlocks;
        cm256Params.RecoveryCount = nbBlocksFEC;

        // Fill pointers to data
        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; ++i)
        {
            if (i >= cm256Params.OriginalCount) {
                memset((void *) &txBlockx[i].m_protectedBlock, 0, sizeof(SDRDaemonProtectedBlock));
            }

            txBlockx[i].m_header.m_frameIndex = frameIndex;
            txBlockx[i].m_header.m_blockIndex = i;
            descriptorBlocks[i].Block = (void *) &(txBlockx[i].m_protectedBlock);
            descriptorBlocks[i].Index = txBlockx[i].m_header.m_blockIndex;
        }

        // Encode FEC blocks
        if (m_cm256->cm256_encode(cm256Params, descriptorBlocks, fecBlocks))
        {
            qWarning("SDRDaemonChannelSinkThread::handleDataBlock: CM256 encode failed. No transmission.");
            // TODO: send without FEC changing meta data to set indication of no FEC
            return true;
        }

        // Merge FEC with data to transmit
        for (int i = 0; i < cm256Params.RecoveryCount; i++)
        {
            txBlockx[i + cm256Params.OriginalCount].m_protectedBlock = fecBlocks[i];
        }

        // Transmit all blocks
        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++)
        {
            // send block via UDP
            m_socket->writeDatagram((const char*)&txBlockx[i], (qint64 ) SDRDaemonUdpSize, m_address, m_port);
            usleep(txDelay);
        }
    }

    dataBlock.m_controlBlock.m_processed = true;
    return true;
}

void SDRDaemonChannelSinkThread::handleData()
{
    SDRDaemonDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue->pop()) != 0))
    {
        if (handleDataBlock(*dataBlock))
        {
            delete dataBlock;
        }
    }
}

void SDRDaemonChannelSinkThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("SDRDaemonChannelSinkThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}
