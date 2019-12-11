///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QMutexLocker>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "dsp/hbfilterchainconverter.h"
#include "util/timeutil.h"

#include "remotesinkthread.h"
#include "remotesinksink.h"

RemoteSinkSink::RemoteSinkSink() :
        m_running(false),
        m_remoteSinkThread(nullptr),
        m_txBlockIndex(0),
        m_frameCount(0),
        m_sampleIndex(0),
        m_dataBlock(nullptr),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_sampleRate(48000),
        m_nbBlocksFEC(0),
        m_txDelay(35),
        m_dataAddress("127.0.0.1"),
        m_dataPort(9090)
{
    applySettings(m_settings, true);
}

RemoteSinkSink::~RemoteSinkSink()
{
    QMutexLocker mutexLocker(&m_dataBlockMutex);

    if (m_dataBlock && !m_dataBlock->m_txControlBlock.m_complete) {
        delete m_dataBlock;
    }
}

void RemoteSinkSink::setTxDelay(int txDelay, int nbBlocksFEC)
{
    double txDelayRatio = txDelay / 100.0;
    int samplesPerBlock = RemoteNbBytesPerBlock / sizeof(Sample);
    double delay = m_sampleRate == 0 ? 1.0 : (127*samplesPerBlock*txDelayRatio) / m_sampleRate;
    delay /= 128 + nbBlocksFEC;
    m_txDelay = roundf(delay*1e6); // microseconds
    qDebug() << "RemoteSinkSink::setTxDelay:"
        << "txDelay:" << txDelay << "%"
        << "m_txDelay:" << m_txDelay << "us"
        << "m_sampleRate: " << m_sampleRate << "S/s";
}

void RemoteSinkSink::setNbBlocksFEC(int nbBlocksFEC)
{
    qDebug() << "RemoteSinkSink::setNbBlocksFEC: nbBlocksFEC: " << nbBlocksFEC;
    m_nbBlocksFEC = nbBlocksFEC;
}

void RemoteSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    SampleVector::const_iterator it = begin;

    while (it != end)
    {
        int inSamplesIndex = it - begin;
        int inRemainingSamples = end - it;

        if (m_txBlockIndex == 0)
        {
            // struct timeval tv;
            RemoteMetaDataFEC metaData;
            uint64_t nowus = TimeUtil::nowus();
            // gettimeofday(&tv, 0);

            metaData.m_centerFrequency = m_centerFrequency + m_frequencyOffset;
            metaData.m_sampleRate = m_sampleRate;
            metaData.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            metaData.m_sampleBits = SDR_RX_SAMP_SZ;
            metaData.m_nbOriginalBlocks = RemoteNbOrginalBlocks;
            metaData.m_nbFECBlocks = m_nbBlocksFEC;
            metaData.m_tv_sec = nowus / 1000000UL;  // tv.tv_sec;
            metaData.m_tv_usec = nowus % 1000000UL; // tv.tv_usec;

            if (!m_dataBlock) { // on the very first cycle there is no data block allocated
                m_dataBlock = new RemoteDataBlock();
            }

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, sizeof(RemoteMetaDataFEC)-4);
            metaData.m_crc32 = crc32.checksum();
            RemoteSuperBlock& superBlock = m_dataBlock->m_superBlocks[0]; // first block
            superBlock.init();
            superBlock.m_header.m_frameIndex = m_frameCount;
            superBlock.m_header.m_blockIndex = m_txBlockIndex;
            superBlock.m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            superBlock.m_header.m_sampleBits = SDR_RX_SAMP_SZ;

            RemoteMetaDataFEC *destMeta = (RemoteMetaDataFEC *) &superBlock.m_protectedBlock;
            *destMeta = metaData;

            if (!(metaData == m_currentMetaFEC))
            {
                qDebug() << "RemoteSinkSink::feed: meta: "
                        << "|" << metaData.m_centerFrequency
                        << ":" << metaData.m_sampleRate
                        << ":" << (int) (metaData.m_sampleBytes & 0xF)
                        << ":" << (int) metaData.m_sampleBits
                        << "|" << (int) metaData.m_nbOriginalBlocks
                        << ":" << (int) metaData.m_nbFECBlocks
                        << "|" << metaData.m_tv_sec
                        << ":" << metaData.m_tv_usec;

                m_currentMetaFEC = metaData;
            }

            m_txBlockIndex = 1; // next Tx block with data
        } // block zero

        // handle different sample sizes...
        int samplesPerBlock = RemoteNbBytesPerBlock / (SDR_RX_SAMP_SZ <= 16 ? 4 : 8); // two I or Q samples
        if (m_sampleIndex + inRemainingSamples < samplesPerBlock) // there is still room in the current super block
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const void *) &(*(begin+inSamplesIndex)),
                    inRemainingSamples * sizeof(Sample));
            m_sampleIndex += inRemainingSamples;
            it = end; // all input samples are consumed
        }
        else // complete super block and initiate the next if not end of frame
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
                    (const void *) &(*(begin+inSamplesIndex)),
                    (samplesPerBlock - m_sampleIndex) * sizeof(Sample));
            it += samplesPerBlock - m_sampleIndex;
            m_sampleIndex = 0;

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_superBlock.m_header.m_sampleBytes = (SDR_RX_SAMP_SZ <= 16 ? 2 : 4);
            m_superBlock.m_header.m_sampleBits = SDR_RX_SAMP_SZ;
            m_dataBlock->m_superBlocks[m_txBlockIndex] = m_superBlock;

            if (m_txBlockIndex == RemoteNbOrginalBlocks - 1) // frame complete
            {
                m_dataBlockMutex.lock();
                m_dataBlock->m_txControlBlock.m_frameIndex = m_frameCount;
                m_dataBlock->m_txControlBlock.m_processed = false;
                m_dataBlock->m_txControlBlock.m_complete = true;
                m_dataBlock->m_txControlBlock.m_nbBlocksFEC = m_nbBlocksFEC;
                m_dataBlock->m_txControlBlock.m_txDelay = m_txDelay;
                m_dataBlock->m_txControlBlock.m_dataAddress = m_dataAddress;
                m_dataBlock->m_txControlBlock.m_dataPort = m_dataPort;

                emit dataBlockAvailable(m_dataBlock);
                m_dataBlock = new RemoteDataBlock(); // create a new one immediately
                m_dataBlockMutex.unlock();

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

void RemoteSinkSink::start()
{
    qDebug("RemoteSinkSink::start");

    memset((void *) &m_currentMetaFEC, 0, sizeof(RemoteMetaDataFEC));

    if (m_running) {
        stop();
    }

    m_remoteSinkThread = new RemoteSinkThread();
    connect(this,
            SIGNAL(dataBlockAvailable(RemoteDataBlock *)),
            m_remoteSinkThread,
            SLOT(processDataBlock(RemoteDataBlock *)),
            Qt::QueuedConnection);
    m_remoteSinkThread->startStop(true);
    m_running = true;
}

void RemoteSinkSink::stop()
{
    qDebug("RemoteSinkSink::stop");

    if (m_remoteSinkThread)
    {
        m_remoteSinkThread->startStop(false);
        m_remoteSinkThread->deleteLater();
    }

    m_running = false;
}

void RemoteSinkSink::applySettings(const RemoteSinkSettings& settings, bool force)
{
    qDebug() << "RemoteSinkSink::applySettings:"
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_txDelay: " << settings.m_txDelay
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    if ((m_settings.m_nbFECBlocks != settings.m_nbFECBlocks) || force)
    {
        setNbBlocksFEC(settings.m_nbFECBlocks);
        setTxDelay(settings.m_txDelay, settings.m_nbFECBlocks);
    }

    if ((m_settings.m_txDelay != settings.m_txDelay) || force) {
        setTxDelay(settings.m_txDelay, settings.m_nbFECBlocks);
    }

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        m_dataAddress = settings.m_dataAddress;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        m_dataPort = settings.m_dataPort;
    }

    m_settings = settings;
}

void RemoteSinkSink::applySampleRate(uint32_t sampleRate)
{
    m_sampleRate = sampleRate;
    setTxDelay(m_settings.m_txDelay, m_settings.m_nbFECBlocks);
}