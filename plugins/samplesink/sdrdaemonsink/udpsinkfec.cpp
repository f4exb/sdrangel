///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <sys/time.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "udpsinkfec.h"

MESSAGE_CLASS_DEFINITION(UDPSinkFECWorker::MsgUDPFECEncodeAndSend, Message)
MESSAGE_CLASS_DEFINITION(UDPSinkFECWorker::MsgConfigureRemoteAddress, Message)


UDPSinkFEC::UDPSinkFEC() :
    m_sampleRate(48000),
    m_nbSamples(0),
    m_nbBlocksFEC(0),
    m_txDelayRatio(0.0),
    m_txDelay(0),
    m_txBlockIndex(0),
    m_txBlocksIndex(0),
    m_frameCount(0),
    m_sampleIndex(0)
{
    memset((char *) m_txBlocks, 0, 4*256*sizeof(SDRDaemonSuperBlock));
    memset((char *) &m_superBlock, 0, sizeof(SDRDaemonSuperBlock));
    m_currentMetaFEC.init();
    m_bufMeta = new uint8_t[m_udpSize];
    m_buf = new uint8_t[m_udpSize];
    m_udpThread = new QThread();
    m_udpWorker = new UDPSinkFECWorker();

    m_udpWorker->moveToThread(m_udpThread);

    connect(m_udpThread, SIGNAL(started()), m_udpWorker, SLOT(process()));
    connect(m_udpWorker, SIGNAL(finished()), m_udpThread, SLOT(quit()));

    m_udpThread->start();
}

UDPSinkFEC::~UDPSinkFEC()
{
    m_udpWorker->stop();
    m_udpThread->wait();

    delete[] m_buf;
    delete[] m_bufMeta;
    delete m_udpWorker;
    delete m_udpThread;
}

void UDPSinkFEC::setTxDelay(float txDelayRatio)
{
    // delay is calculated from the fraction of the nominal UDP block process time
    // frame size: 127 * (126 or 63 samples depending on I or Q sample bytes of 2 or 4 bytes respectively)
    // divided by sample rate gives the frame process time
    // divided by the number of actual blocks including FEC blocks gives the block (i.e. UDP block) process time
    m_txDelayRatio = txDelayRatio;
    int samplesPerBlock = SDRDaemonNbBytesPerBlock / (SDR_RX_SAMP_SZ <= 16 ? 4 : 8);
    double delay = ((127*samplesPerBlock*txDelayRatio) / m_sampleRate)/(128 + m_nbBlocksFEC);
    m_txDelay = delay * 1e6;
    qDebug() << "UDPSinkFEC::setTxDelay: txDelay: " << txDelayRatio << " m_txDelay: " << m_txDelay << " us";
}

void UDPSinkFEC::setNbBlocksFEC(uint32_t nbBlocksFEC)
{
    qDebug() << "UDPSinkFEC::setNbBlocksFEC: nbBlocksFEC: " << nbBlocksFEC;
    m_nbBlocksFEC = nbBlocksFEC;
    setTxDelay(m_txDelayRatio);
}

void UDPSinkFEC::setSampleRate(uint32_t sampleRate)
{
    qDebug() << "UDPSinkFEC::setSampleRate: sampleRate: " << sampleRate;
    m_sampleRate = sampleRate;
    setTxDelay(m_txDelayRatio);
}

void UDPSinkFEC::setRemoteAddress(const QString& address, uint16_t port)
{
    qDebug() << "UDPSinkFEC::setRemoteAddress: address: " << address << " port: " << port;
    m_udpWorker->setRemoteAddress(address, port);
}

void UDPSinkFEC::write(const SampleVector::iterator& begin, uint32_t sampleChunkSize)
{
    //qDebug("UDPSinkFEC::write(: %u samples", sampleChunkSize);
    const SampleVector::iterator end = begin + sampleChunkSize;
    SampleVector::iterator it = begin;

    while (it != end)
    {
        int inRemainingSamples = end - it;

        if (m_txBlockIndex == 0) // Tx block index 0 is a block with only meta data
        {
            struct timeval tv;
            SDRDaemonMetaDataFEC metaData;

            gettimeofday(&tv, 0);

            metaData.m_centerFrequency = 0; // frequency not set by stream
            metaData.m_sampleRate = m_sampleRate;
            metaData.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            metaData.m_sampleBits = SDR_RX_SAMP_SZ;
            metaData.m_nbOriginalBlocks = m_nbOriginalBlocks;
            metaData.m_nbFECBlocks = m_nbBlocksFEC;
            metaData.m_tv_sec = tv.tv_sec;
            metaData.m_tv_usec = tv.tv_usec;

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, 20);

            metaData.m_crc32 = crc32.checksum();

            memset((char *) &m_superBlock, 0, sizeof(m_superBlock));

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_superBlock.m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            m_superBlock.m_header.m_sampleBits = SDR_RX_SAMP_SZ;
            memcpy((char *) &m_superBlock.m_protectedBlock, (const char *) &metaData, sizeof(SDRDaemonMetaDataFEC));

            if (!(metaData == m_currentMetaFEC))
            {
                qDebug() << "UDPSinkFEC::write: meta: "
                        << "|" << metaData.m_centerFrequency
                        << ":" << metaData.m_sampleRate
                        << ":" << (int) (metaData.m_sampleBytes & 0xF)
                        << ":" << (int) metaData.m_sampleBits
                        << "|" << (int) metaData.m_nbOriginalBlocks
                        << ":" << (int) metaData.m_nbFECBlocks
                        << "|" << metaData.m_tv_sec
                        << ":" << metaData.m_tv_usec
                        << "|";

                m_currentMetaFEC = metaData;
            }

            m_txBlocks[m_txBlocksIndex][0] = m_superBlock;
            m_txBlockIndex = 1; // next Tx block with data
        }

        int samplesPerBlock = SDRDaemonNbBytesPerBlock / (SDR_RX_SAMP_SZ <= 16 ? 4 : 8); // two I or Q samples

        if (m_sampleIndex + inRemainingSamples < samplesPerBlock) // there is still room in the current super block
        {
            memcpy((char *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const char *) &(*it),
                    inRemainingSamples * sizeof(Sample));
            m_sampleIndex += inRemainingSamples;
            it = end; // all input samples are consumed
        }
        else // complete super block and initiate the next if not end of frame
        {
            memcpy((char *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const char *) &(*it),
                    (samplesPerBlock - m_sampleIndex) * sizeof(Sample));
            it += samplesPerBlock - m_sampleIndex;
            m_sampleIndex = 0;

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_superBlock.m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            m_superBlock.m_header.m_sampleBits = SDR_RX_SAMP_SZ;
            m_txBlocks[m_txBlocksIndex][m_txBlockIndex] =  m_superBlock;

            if (m_txBlockIndex == m_nbOriginalBlocks - 1) // frame complete
            {
                int nbBlocksFEC = m_nbBlocksFEC;
                int txDelay = m_txDelay;

                //qDebug("UDPSinkFEC::write: push frame to worker: %u", m_frameCount);
                m_udpWorker->pushTxFrame(m_txBlocks[m_txBlocksIndex], nbBlocksFEC, txDelay, m_frameCount);
                //m_txThread = new std::thread(transmitUDP, this, m_txBlocks[m_txBlocksIndex], m_frameCount, nbBlocksFEC, txDelay, m_cm256Valid);
                //transmitUDP(this, m_txBlocks[m_txBlocksIndex], m_frameCount, m_nbBlocksFEC, m_txDelay, m_cm256Valid);

                m_txBlocksIndex = (m_txBlocksIndex + 1) % 4;
                m_txBlockIndex = 0;
                m_frameCount++;
            }
            else
            {
                m_txBlockIndex++;
            }
        }
    }
}

UDPSinkFECWorker::UDPSinkFECWorker() :
        m_running(false),
        m_remotePort(9090)
{
    m_cm256Valid = m_cm256.isInitialized();
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::DirectConnection);
}

UDPSinkFECWorker::~UDPSinkFECWorker()
{
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_inputMessageQueue.clear();
}

void UDPSinkFECWorker::pushTxFrame(SDRDaemonSuperBlock *txBlocks,
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

void UDPSinkFECWorker::process()
{
    m_running  = true;

    qDebug("UDPSinkFECWorker::process: started");

    while (m_running)
    {
        usleep(250000);
    }

    qDebug("UDPSinkFECWorker::process: stopped");
    emit finished();
}

void UDPSinkFECWorker::stop()
{
    m_running = false;
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
        }

        delete message;
    }
}

void UDPSinkFECWorker::encodeAndTransmit(SDRDaemonSuperBlock *txBlockx, uint16_t frameIndex, uint32_t nbBlocksFEC, uint32_t txDelay)
{
    CM256::cm256_encoder_params cm256Params;  //!< Main interface with CM256 encoder
    CM256::cm256_block descriptorBlocks[256]; //!< Pointers to data for CM256 encoder
    SDRDaemonProtectedBlock fecBlocks[256];   //!< FEC data

    if ((nbBlocksFEC == 0) || !m_cm256Valid)
    {
//        qDebug("UDPSinkFECWorker::encodeAndTransmit: transmit frame without FEC to %s:%d", m_remoteAddress.toStdString().c_str(), m_remotePort);

        for (unsigned int i = 0; i < UDPSinkFEC::m_nbOriginalBlocks; i++)
        {
            m_socket.SendDataGram((const void *) &txBlockx[i], (int) UDPSinkFEC::m_udpSize, m_remoteAddress.toStdString(), (uint32_t) m_remotePort);
            //m_udpSocket->writeDatagram((const char *) &txBlockx[i], (int) UDPSinkFEC::m_udpSize, m_remoteAddress, m_remotePort);
            usleep(txDelay);
        }
    }
    else
    {
        cm256Params.BlockBytes = sizeof(SDRDaemonProtectedBlock);
        cm256Params.OriginalCount = UDPSinkFEC::m_nbOriginalBlocks;
        cm256Params.RecoveryCount = nbBlocksFEC;


        // Fill pointers to data
        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; ++i)
        {
            if (i >= cm256Params.OriginalCount) {
                memset((char *) &txBlockx[i].m_protectedBlock, 0, sizeof(SDRDaemonProtectedBlock));
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

//        qDebug("UDPSinkFECWorker::encodeAndTransmit: transmit frame with FEC to %s:%d", m_remoteAddress.toStdString().c_str(), m_remotePort);

        for (int i = 0; i < cm256Params.OriginalCount + cm256Params.RecoveryCount; i++)
        {
#ifdef SDRDAEMON_PUNCTURE
            if (i == SDRDAEMON_PUNCTURE) {
                continue;
            }
#endif
//            std::cerr << "UDPSinkFEC::transmitUDP:"
//                  << " i: " << i
//                  << " frameIndex: " << (int) m_txBlocks[i].header.frameIndex
//                  << " blockIndex: " << (int) m_txBlocks[i].header.blockIndex
//                  << " i.q:";
//
//            for (int j = 0; j < 10; j++)
//            {
//                std::cerr << " " << (int) m_txBlocks[i].protectedBlock.m_samples[j].m_real
//                        << "." << (int) m_txBlocks[i].protectedBlock.m_samples[j].m_imag;
//            }
//
//            std::cerr << std::endl;

            m_socket.SendDataGram((const void *) &txBlockx[i], (int) UDPSinkFEC::m_udpSize, m_remoteAddress.toStdString(), (uint32_t) m_remotePort);
            //m_udpSocket->writeDatagram((const char *) &txBlockx[i], (int) UDPSinkFEC::m_udpSize, m_remoteAddress, m_remotePort);
            usleep(txDelay);
        }
    }
}
