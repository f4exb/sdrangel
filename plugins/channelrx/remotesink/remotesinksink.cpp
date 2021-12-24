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
#include <QThread>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "dsp/hbfilterchainconverter.h"
#include "util/timeutil.h"

#include "remotesinksender.h"
#include "remotesinksink.h"

RemoteSinkSink::RemoteSinkSink() :
        m_running(false),
        m_remoteSinkSender(nullptr),
        m_txBlockIndex(0),
        m_frameCount(0),
        m_sampleIndex(0),
        m_dataFrame(nullptr),
        m_deviceCenterFrequency(0),
        m_frequencyOffset(0),
        m_basebandSampleRate(48000),
        m_nbBlocksFEC(0),
        m_nbTxBytes(SDR_RX_SAMP_SZ <= 16 ? 2 : 4),
        m_dataAddress("127.0.0.1"),
        m_dataPort(9090)
{
    qDebug("RemoteSinkSink::RemoteSinkSink");
    applySettings(m_settings, true);
}

RemoteSinkSink::~RemoteSinkSink()
{
    qDebug("RemoteSinkSink::~RemoteSinkSink");
    stop();
}

void RemoteSinkSink::start()
{
    qDebug("RemoteSinkSink::start");

    if (m_running) {
        stop();
    }

    m_remoteSinkSender = new RemoteSinkSender();
    m_remoteSinkSender->moveToThread(&m_senderThread);
    startSender();
    m_running = true;
}

void RemoteSinkSink::stop()
{
    qDebug("RemoteSinkSink::stop");

    if (m_remoteSinkSender)
    {
        stopSender();
        m_remoteSinkSender->deleteLater();
        m_remoteSinkSender = nullptr;
    }

    m_running = false;
}

void RemoteSinkSink::startSender()
{
    qDebug("RemoteSinkSink::startSender");
    m_remoteSinkSender->startWork();
    m_senderThread.start();
}

void RemoteSinkSink::stopSender()
{
    qDebug("RemoteSinkSink::stopSender");
	m_remoteSinkSender->stopWork();
	m_senderThread.quit();
	m_senderThread.wait();
}

void RemoteSinkSink::init()
{
    m_dataFrame = nullptr;
    m_txBlockIndex = 0;
    m_frameCount = 0;
    m_sampleIndex = 0;
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

            metaData.m_centerFrequency = m_deviceCenterFrequency + m_frequencyOffset;
            metaData.m_sampleRate = m_basebandSampleRate / (1<<m_settings.m_log2Decim);
            metaData.m_sampleBytes = m_nbTxBytes;
            metaData.m_sampleBits = getNbSampleBits();
            metaData.m_nbOriginalBlocks = RemoteNbOrginalBlocks;
            metaData.m_nbFECBlocks = m_nbBlocksFEC;
            metaData.m_deviceIndex = m_deviceIndex % 256;
            metaData.m_channelIndex = m_channelIndex % 256;
            metaData.m_tv_sec = nowus / 1000000UL;  // tv.tv_sec;
            metaData.m_tv_usec = nowus % 1000000UL; // tv.tv_usec;

            if (!m_dataFrame) { // on the very first cycle there is no data block allocated
                m_dataFrame = m_remoteSinkSender->getDataFrame(); // ask a new block to sender
            }

            boost::crc_32_type crc32;
            crc32.process_bytes(&metaData, sizeof(RemoteMetaDataFEC)-4);
            metaData.m_crc32 = crc32.checksum();
            RemoteSuperBlock& superBlock = m_dataFrame->m_superBlocks[0]; // first block
            superBlock.init();
            superBlock.m_header.m_frameIndex = m_frameCount;
            superBlock.m_header.m_blockIndex = m_txBlockIndex;
            superBlock.m_header.m_sampleBytes = m_nbTxBytes;
            superBlock.m_header.m_sampleBits = getNbSampleBits();

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
                        << "|" << metaData.m_deviceIndex
                        << ":" << metaData.m_channelIndex
                        << "|" << metaData.m_tv_sec
                        << ":" << metaData.m_tv_usec;

                m_currentMetaFEC = metaData;
            }

            m_txBlockIndex = 1; // next Tx block with data
        } // block zero

        // handle different sample sizes...
        int samplesPerBlock = RemoteNbBytesPerBlock / (2 * m_nbTxBytes); // two I or Q samples
        if (m_sampleIndex + inRemainingSamples < samplesPerBlock) // there is still room in the current super block
        {
            convertSampleToData(begin + inSamplesIndex, inRemainingSamples, false);
            // memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
            //         (const void *) &(*(begin+inSamplesIndex)),
            //         inRemainingSamples * sizeof(Sample));
            m_sampleIndex += inRemainingSamples;
            it = end; // all input samples are consumed
        }
        else // complete super block and initiate the next if not end of frame
        {
            convertSampleToData(begin + inSamplesIndex, samplesPerBlock - m_sampleIndex, false);
            // memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*sizeof(Sample)],
            //         (const void *) &(*(begin+inSamplesIndex)),
            //         (samplesPerBlock - m_sampleIndex) * sizeof(Sample));
            it += samplesPerBlock - m_sampleIndex;
            m_sampleIndex = 0;

            m_superBlock.m_header.m_frameIndex = m_frameCount;
            m_superBlock.m_header.m_blockIndex = m_txBlockIndex;
            m_superBlock.m_header.m_sampleBytes = m_nbTxBytes;
            m_superBlock.m_header.m_sampleBits = getNbSampleBits();
            m_dataFrame->m_superBlocks[m_txBlockIndex] = m_superBlock;

            if (m_txBlockIndex == RemoteNbOrginalBlocks - 1) // frame complete
            {
                m_dataFrame->m_txControlBlock.m_frameIndex = m_frameCount;
                m_dataFrame->m_txControlBlock.m_processed = false;
                m_dataFrame->m_txControlBlock.m_complete = true;
                m_dataFrame->m_txControlBlock.m_nbBlocksFEC = m_nbBlocksFEC;
                m_dataFrame->m_txControlBlock.m_dataAddress = m_dataAddress;
                m_dataFrame->m_txControlBlock.m_dataPort = m_dataPort;

                m_dataFrame = m_remoteSinkSender->getDataFrame(); // ask a new block to sender

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

void RemoteSinkSink::applySettings(const RemoteSinkSettings& settings, bool force)
{
    qDebug() << "RemoteSinkSink::applySettings:"
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        m_dataAddress = settings.m_dataAddress;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        m_dataPort = settings.m_dataPort;
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim)
     || (m_settings.m_filterChainHash != settings.m_filterChainHash)
     || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks) || force)
    {
        double shiftFactor = HBFilterChainConverter::getShiftFactor(settings.m_log2Decim, settings.m_filterChainHash);
        m_frequencyOffset = round(shiftFactor*m_basebandSampleRate);
        setNbBlocksFEC(settings.m_nbFECBlocks);
    }

    m_settings = settings;
}

void RemoteSinkSink::applyBasebandSampleRate(uint32_t sampleRate)
{
    m_basebandSampleRate = sampleRate;
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_frequencyOffset = round(shiftFactor*m_basebandSampleRate);
}

uint32_t RemoteSinkSink::getNbSampleBits()
{
    if (m_nbTxBytes == 1) {
        return 8;
    } else if (m_nbTxBytes == 2) {
        return 16;
    } else if (m_nbTxBytes == 4) {
        return 24;
    } else {
        return 16;
    }
}
