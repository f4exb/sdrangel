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

#include <QThread>
#include <QVariant>

#include "cm256cc/cm256.h"
#include "remotesourceworker.h"

MESSAGE_CLASS_DEFINITION(RemoteSourceWorker::MsgDataBind, Message)

RemoteSourceWorker::RemoteSourceWorker(RemoteDataQueue *dataQueue, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_dataQueue(dataQueue),
    m_address(QHostAddress::LocalHost),
    m_socket(this),
    m_sampleRate(0),
    m_udpReadBytes(0)
{
    m_udpBuf = new char[RemoteUdpSize];

    std::fill(m_dataFrames, m_dataFrames+4, (RemoteDataFrame *) 0);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    connect(&m_socket, SIGNAL(readyRead()),this, SLOT(recv()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteSourceWorker::errorOccurred);
#else
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, &RemoteSourceWorker::errorOccurred);
#endif
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

bool RemoteSourceWorker::startWork()
{
    qDebug("RemoteSourceWorker::startWork");
    QMutexLocker mutexLocker(&m_mutex);
    m_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, getDataSocketBufferSize(m_sampleRate));
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_running = true;
    return m_running;
}

void RemoteSourceWorker::started()
{
    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void RemoteSourceWorker::stopWork()
{
    qDebug("RemoteSourceWorker::stopWork");
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void RemoteSourceWorker::finished()
{
    // Close any existing connection
    if (m_socket.isOpen()) {
        m_socket.close();
    }
    m_running = false;
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
}

void RemoteSourceWorker::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qWarning() << "RemoteSourceWorker::errorOccurred: " << socketError;
}

void RemoteSourceWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (MsgDataBind::match(*message))
        {
            QMutexLocker mutexLocker(&m_mutex);
            MsgDataBind* notif = (MsgDataBind*) message;
            qDebug("RemoteSourceWorker::handleInputMessages: MsgDataBind: %s:%d", qPrintable(notif->getAddress().toString()), notif->getPort());
            disconnect(&m_socket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
            m_socket.abort();
            m_socket.bind(notif->getAddress(), notif->getPort());
            connect(&m_socket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
        }
    }
}

void RemoteSourceWorker::dataReadyRead()
{
    m_udpReadBytes = 0;

	while (m_socket.hasPendingDatagrams())
	{
		qint64 pendingDataSize = m_socket.pendingDatagramSize();
        QHostAddress sender;
		m_udpReadBytes += m_socket.readDatagram(&m_udpBuf[m_udpReadBytes], pendingDataSize, &sender, nullptr);

		if (m_udpReadBytes == RemoteUdpSize)
        {
		    processData();
		    m_udpReadBytes = 0;
		}
	}
}

void RemoteSourceWorker::processData()
{
    RemoteSuperBlock *superBlock = (RemoteSuperBlock *) m_udpBuf;
    unsigned int dataBlockIndex = superBlock->m_header.m_frameIndex % m_nbDataFrames;
    int blockIndex = superBlock->m_header.m_blockIndex;

    if (blockIndex == 0) // first block with meta data
    {
        const RemoteMetaDataFEC *metaData = (const RemoteMetaDataFEC *) &superBlock->m_protectedBlock;
        uint32_t sampleRate = metaData->m_sampleRate;

        if (m_sampleRate != sampleRate)
        {
            qDebug("RemoteSourceWorker::processData: sampleRate: %u", sampleRate);
            m_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, getDataSocketBufferSize(sampleRate));
            m_sampleRate = sampleRate;
        }
    }

    // create the first block for this index
    if (m_dataFrames[dataBlockIndex] == nullptr) {
        m_dataFrames[dataBlockIndex] = new RemoteDataFrame();
    }

    if (m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_frameIndex < 0)
    {
        // initialize virgin block with the frame index
        m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_frameIndex = superBlock->m_header.m_frameIndex;
    }
    else
    {
        // if the frame index is not the same for the same slot it means we are starting a new frame
        uint32_t frameIndex = m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_frameIndex;

        if (superBlock->m_header.m_frameIndex != frameIndex)
        {
            m_dataQueue->push(m_dataFrames[dataBlockIndex]);
            m_dataFrames[dataBlockIndex] = new RemoteDataFrame();
            m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_frameIndex = superBlock->m_header.m_frameIndex;
        }
    }

    m_dataFrames[dataBlockIndex]->m_superBlocks[superBlock->m_header.m_blockIndex] = *superBlock;

    if (superBlock->m_header.m_blockIndex == 0) {
        m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_metaRetrieved = true;
    }

    if (superBlock->m_header.m_blockIndex < RemoteNbOrginalBlocks) {
        m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_originalCount++;
    } else {
        m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_recoveryCount++;
    }

    m_dataFrames[dataBlockIndex]->m_rxControlBlock.m_blockCount++;
}

int RemoteSourceWorker::getDataSocketBufferSize(uint32_t inSampleRate)
{
    // set a floor value at 96 kS/s
    uint32_t samplerate = inSampleRate < 96000 ? 96000 : inSampleRate;
    // 250 ms (1/4s) at current sample rate
    int bufferSize = (samplerate * 2 * (SDR_RX_SAMP_SZ == 16 ? 2 : 4)) / 4;
    qDebug("RemoteSourceWorker::getDataSocketBufferSize: %d bytes", bufferSize);
    return bufferSize;
}
