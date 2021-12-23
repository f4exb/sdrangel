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

#include <QThread>

#include "cm256cc/cm256.h"

#include "channel/remotedatablock.h"
#include "remotesinksender.h"

RemoteSinkSender::RemoteSinkSender() :
    m_running(false),
    m_fifo(20, this),
    m_address(QHostAddress::LocalHost),
    m_socket(this)
{
    qDebug("RemoteSinkSender::RemoteSinkSender");
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : nullptr;
}

RemoteSinkSender::~RemoteSinkSender()
{
    qDebug("RemoteSinkSender::~RemoteSinkSender");
    m_socket.close();
}

bool RemoteSinkSender::startWork()
{
    qDebug("RemoteSinkSender::startWork");
    QObject::connect(
        &m_fifo,
        &RemoteSinkFifo::dataBlockServed,
        this,
        &RemoteSinkSender::handleData,
        Qt::QueuedConnection
    );
    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_running = true;
    return m_running;
}

void RemoteSinkSender::started()
{
    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void RemoteSinkSender::stopWork()
{
    qDebug("RemoteSinkSender::stopWork");
    QObject::disconnect(
        &m_fifo,
        &RemoteSinkFifo::dataBlockServed,
        this,
        &RemoteSinkSender::handleData
    );
}

void RemoteSinkSender::finished()
{
    // Close any existing connection
    if (m_socket.isOpen()) {
        m_socket.close();
    }

    m_running = false;
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
}

RemoteDataFrame *RemoteSinkSender::getDataFrame()
{
    return m_fifo.getDataFrame();
}

void RemoteSinkSender::handleData()
{
    RemoteDataFrame *dataFrame;
    unsigned int remainder = m_fifo.getRemainder();

    while (remainder != 0)
    {
        remainder = m_fifo.readDataFrame(&dataFrame);

        if (dataFrame) {
            sendDataFrame(dataFrame);
        }
    }
}

void RemoteSinkSender::sendDataFrame(RemoteDataFrame *dataFrame)
{
	CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
	CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
	RemoteProtectedBlock fecBlocks[256];   //!< FEC data

    uint16_t frameIndex = dataFrame->m_txControlBlock.m_frameIndex;
    int nbBlocksFEC = dataFrame->m_txControlBlock.m_nbBlocksFEC;
    m_address.setAddress(dataFrame->m_txControlBlock.m_dataAddress);
    uint16_t dataPort = dataFrame->m_txControlBlock.m_dataPort;
    RemoteSuperBlock *txBlockx = dataFrame->m_superBlocks;

    if ((nbBlocksFEC == 0) || !m_cm256p) // Do not FEC encode
    {
        for (int i = 0; i < RemoteNbOrginalBlocks; i++) { // send block via UDP
            m_socket.writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_address, dataPort);
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
            qWarning("RemoteSinkSender::handleDataBlock: CM256 encode failed. No transmission.");
            // TODO: send without FEC changing meta data to set indication of no FEC
        }

        // Merge FEC with data to transmit
        for (int i = 0; i < cm256Params.RecoveryCount; i++)
        {
            txBlockx[i + cm256Params.OriginalCount].m_protectedBlock = fecBlocks[i];
        }

        // Transmit all blocks
        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++) { // send block via UDP
            m_socket.writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_address, dataPort);
        }
    }

    dataFrame->m_txControlBlock.m_processed = true;
}
