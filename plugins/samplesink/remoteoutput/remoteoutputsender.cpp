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


#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QUdpSocket>

#include "cm256cc/cm256.h"

#include "channel/remotedatablock.h"
#include "remoteoutputsender.h"

RemoteOutputSender::RemoteOutputSender() :
    m_fifo(20, this),
    m_udpSocket(nullptr),
    m_remotePort(9090)
{
    qDebug("RemoteOutputSender::RemoteOutputSender");
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : nullptr;
    m_udpSocket = new QUdpSocket(this);

    QObject::connect(
        &m_fifo,
        &RemoteOutputFifo::dataBlockServed,
        this,
        &RemoteOutputSender::handleData,
        Qt::QueuedConnection
    );
}

RemoteOutputSender::~RemoteOutputSender()
{
    qDebug("RemoteOutputSender::~RemoteOutputSender");
    delete m_udpSocket;
}

void RemoteOutputSender::setDestination(const QString& address, uint16_t port)
{
    m_remoteAddress = address;
    m_remotePort = port;
    m_remoteHostAddress.setAddress(address);
}

RemoteDataFrame *RemoteOutputSender::getDataFrame()
{
    return m_fifo.getDataFrame();
}

void RemoteOutputSender::handleData()
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

void RemoteOutputSender::sendDataFrame(RemoteDataFrame *dataFrame)
{
	CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
	CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
	RemoteProtectedBlock fecBlocks[256];   //!< FEC data

    uint16_t frameIndex = dataFrame->m_txControlBlock.m_frameIndex;
    int nbBlocksFEC = dataFrame->m_txControlBlock.m_nbBlocksFEC;
    m_remoteHostAddress.setAddress(dataFrame->m_txControlBlock.m_dataAddress);
    uint16_t dataPort = dataFrame->m_txControlBlock.m_dataPort;
    RemoteSuperBlock *txBlockx = dataFrame->m_superBlocks;

    if ((nbBlocksFEC == 0) || !m_cm256p) // Do not FEC encode
    {
        if (m_udpSocket)
        {
            for (int i = 0; i < RemoteNbOrginalBlocks; i++) { // send blocks via UDP
                m_udpSocket->writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_remoteHostAddress, dataPort);
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
            qWarning("RemoteSinkSender::handleDataBlock: CM256 encode failed. Transmit without FEC.");
            cm256Params.RecoveryCount = 0;
            RemoteSuperBlock& superBlock = dataFrame->m_superBlocks[0]; // first block
            RemoteMetaDataFEC *destMeta = (RemoteMetaDataFEC *) &superBlock.m_protectedBlock;
            destMeta->m_nbFECBlocks = 0;
            boost::crc_32_type crc32;
            crc32.process_bytes(destMeta, sizeof(RemoteMetaDataFEC)-4);
            destMeta->m_crc32 = crc32.checksum();
        }

        // Merge FEC with data to transmit
        for (int i = 0; i < cm256Params.RecoveryCount; i++) {
            txBlockx[i + cm256Params.OriginalCount].m_protectedBlock = fecBlocks[i];
        }

        // Transmit all blocks
        if (m_udpSocket)
        {
            for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++) { // send blocks via UDP
                m_udpSocket->writeDatagram((const char*)&txBlockx[i], (qint64 ) RemoteUdpSize, m_remoteHostAddress, dataPort);
            }
        }
    }

    dataFrame->m_txControlBlock.m_processed = true;
}
