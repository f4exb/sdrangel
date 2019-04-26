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

#include "udpsinkfec.h"

#include <QDebug>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "util/timeutil.h"

#include "udpsinkfecworker.h"


UDPSinkFEC::UDPSinkFEC() :
    m_sampleRate(48000),
    m_nbSamples(0),
    m_nbBlocksFEC(0),
    m_txDelayRatio(0.0),
    m_txDelay(0),
    m_txBlockIndex(0),
    m_txBlocksIndex(0),
    m_frameCount(0),
    m_sampleIndex(0),
    m_udpWorker(0),
    m_remoteAddress("127.0.0.1"),
    m_remotePort(9090)
{
    memset((char *) m_txBlocks, 0, 4*256*sizeof(RemoteSuperBlock));
    memset((char *) &m_superBlock, 0, sizeof(RemoteSuperBlock));
    m_currentMetaFEC.init();
    m_bufMeta = new uint8_t[m_udpSize];
    m_buf = new uint8_t[m_udpSize];
}

UDPSinkFEC::~UDPSinkFEC()
{
    delete[] m_buf;
    delete[] m_bufMeta;
}

void UDPSinkFEC::start()
{
    m_udpWorker = new UDPSinkFECWorker();
    m_udpWorker->setRemoteAddress(m_remoteAddress, m_remotePort);
    m_udpWorker->startStop(true);
}

void UDPSinkFEC::stop()
{
    if (m_udpWorker)
    {
        m_udpWorker->startStop(false);
        m_udpWorker->deleteLater();
        m_udpWorker = 0;
    }
}

void UDPSinkFEC::setTxDelay(float txDelayRatio)
{
    // delay is calculated from the fraction of the nominal UDP block process time
    // frame size: 127 * (126 or 63 samples depending on I or Q sample bytes of 2 or 4 bytes respectively)
    // divided by sample rate gives the frame process time
    // divided by the number of actual blocks including FEC blocks gives the block (i.e. UDP block) process time
    m_txDelayRatio = txDelayRatio;
    int samplesPerBlock = RemoteNbBytesPerBlock / (SDR_RX_SAMP_SZ <= 16 ? 4 : 8);
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
    m_remoteAddress = address;
    m_remotePort = port;

    if (m_udpWorker) {
        m_udpWorker->setRemoteAddress(m_remoteAddress, m_remotePort);
    }
}

void UDPSinkFEC::write(const SampleVector::iterator& begin, uint32_t sampleChunkSize)
{
    const SampleVector::iterator end = begin + sampleChunkSize;
    SampleVector::iterator it = begin;

    while (it != end)
    {
        int inRemainingSamples = end - it;

        if (m_txBlockIndex == 0) // Tx block index 0 is a block with only meta data
        {
            RemoteMetaDataFEC metaData;

            uint64_t ts_usecs = TimeUtil::nowus();

            metaData.m_centerFrequency = 0; // frequency not set by stream
            metaData.m_sampleRate = m_sampleRate;
            metaData.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            metaData.m_sampleBits = SDR_RX_SAMP_SZ;
            metaData.m_nbOriginalBlocks = m_nbOriginalBlocks;
            metaData.m_nbFECBlocks = m_nbBlocksFEC;
            metaData.m_tv_sec = ts_usecs / 1000000UL;
            metaData.m_tv_usec = ts_usecs % 1000000UL;

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, sizeof(RemoteMetaDataFEC)-4);

            metaData.m_crc32 = crc32.checksum();

            memset((char *) &m_superBlock, 0, sizeof(m_superBlock));

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_superBlock.m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            m_superBlock.m_header.m_sampleBits = SDR_RX_SAMP_SZ;

            RemoteMetaDataFEC *destMeta = (RemoteMetaDataFEC *) &m_superBlock.m_protectedBlock;
            *destMeta = metaData;

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

        int samplesPerBlock = RemoteNbBytesPerBlock / (SDR_RX_SAMP_SZ <= 16 ? 4 : 8); // two I or Q samples

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

                if (m_udpWorker) {
                    m_udpWorker->pushTxFrame(m_txBlocks[m_txBlocksIndex], nbBlocksFEC, txDelay, m_frameCount);
                }

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

