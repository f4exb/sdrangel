///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "udpsinkfecworker.h"

#include <QUdpSocket>


MESSAGE_CLASS_DEFINITION(UDPSinkFECWorker::MsgUDPFECEncodeAndSend, Message)
MESSAGE_CLASS_DEFINITION(UDPSinkFECWorker::MsgConfigureRemoteAddress, Message)
MESSAGE_CLASS_DEFINITION(UDPSinkFECWorker::MsgStartStop, Message)

UDPSinkFECWorker::UDPSinkFECWorker() :
        m_running(false),
        m_udpSocket(0),
        m_remotePort(9090)
{
    m_cm256Valid = m_cm256.isInitialized();
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

UDPSinkFECWorker::~UDPSinkFECWorker()
{
}

void UDPSinkFECWorker::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void UDPSinkFECWorker::startWork()
{
    qDebug("UDPSinkFECWorker::startWork");
    m_startWaitMutex.lock();
    m_udpSocket = new QUdpSocket(this);

    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void UDPSinkFECWorker::stopWork()
{
    qDebug("UDPSinkFECWorker::stopWork");
    delete m_udpSocket;
    m_udpSocket = 0;
    m_running = false;
    wait();
}

void UDPSinkFECWorker::run()
{
    m_running  = true;
    m_startWaiter.wakeAll();

    qDebug("UDPSinkFECWorker::process: started");

    while (m_running)
    {
        sleep(1);
    }
    m_running = false;

    qDebug("UDPSinkFECWorker::process: stopped");
}

void UDPSinkFECWorker::pushTxFrame(RemoteSuperBlock *txBlocks,
    uint32_t nbBlocksFEC,
    uint32_t txDelay,
    uint16_t frameIndex)
{
    //qDebug("UDPSinkFECWorker::pushTxFrame. %d", m_inputMessageQueue.size());
    m_inputMessageQueue.push(MsgUDPFECEncodeAndSend::create(txBlocks, nbBlocksFEC, txDelay, frameIndex));
}

void UDPSinkFECWorker::setRemoteAddress(const QString& address, uint16_t port)
{
    m_inputMessageQueue.push(MsgConfigureRemoteAddress::create(address, port));
}

void UDPSinkFECWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgUDPFECEncodeAndSend::match(*message))
        {
            MsgUDPFECEncodeAndSend *sendMsg = (MsgUDPFECEncodeAndSend *) message;
            encodeAndTransmit(sendMsg->getTxBlocks(), sendMsg->getFrameIndex(), sendMsg->getNbBlocsFEC(), sendMsg->getTxDelay());
        }
        else if (MsgConfigureRemoteAddress::match(*message))
        {
            qDebug("UDPSinkFECWorker::handleInputMessages: %s", message->getIdentifier());
            MsgConfigureRemoteAddress *addressMsg = (MsgConfigureRemoteAddress *) message;
            m_remoteAddress = addressMsg->getAddress();
            m_remotePort = addressMsg->getPort();
            m_remoteHostAddress.setAddress(addressMsg->getAddress());
        }
        else if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("UDPSinkFECWorker::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }
        }

        delete message;
    }
}

void UDPSinkFECWorker::encodeAndTransmit(RemoteSuperBlock *txBlockx, uint16_t frameIndex, uint32_t nbBlocksFEC, uint32_t txDelay)
{
    CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
    CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
    RemoteProtectedBlock fecBlocks[256];   //!< FEC data

    if ((nbBlocksFEC == 0) || !m_cm256Valid)
    {
        if (m_udpSocket)
        {
            for (unsigned int i = 0; i < RemoteNbOrginalBlocks; i++)
            {
                m_udpSocket->writeDatagram((const char *) &txBlockx[i], RemoteUdpSize, m_remoteHostAddress, m_remotePort);
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
                memset((char *) &txBlockx[i].m_protectedBlock, 0, sizeof(RemoteProtectedBlock));
            }

            txBlockx[i].m_header.m_frameIndex = frameIndex;
            txBlockx[i].m_header.m_blockIndex = i;
            txBlockx[i].m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            txBlockx[i].m_header.m_sampleBits = SDR_RX_SAMP_SZ;
            descriptorBlocks[i].Block = (void *) &(txBlockx[i].m_protectedBlock);
            descriptorBlocks[i].Index = txBlockx[i].m_header.m_blockIndex;
        }

        // Encode FEC blocks
        if (m_cm256.cm256_encode(cm256Params, descriptorBlocks, fecBlocks))
        {
            qDebug("UDPSinkFECWorker::encodeAndTransmit: CM256 encode failed. No transmission.");
            return;
        }

        // Merge FEC with data to transmit
        for (int i = 0; i < cm256Params.RecoveryCount; i++)
        {
            txBlockx[i + cm256Params.OriginalCount].m_protectedBlock = fecBlocks[i];
        }

        // Transmit all blocks
        if (m_udpSocket)
        {
            for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++)
            {
    #ifdef REMOTE_PUNCTURE
                if (i == REMOTE_PUNCTURE) {
                    continue;
                }
    #endif

                m_udpSocket->writeDatagram((const char *) &txBlockx[i], RemoteUdpSize, m_remoteHostAddress, m_remotePort);
                usleep(txDelay);
            }
        }
    }
}




