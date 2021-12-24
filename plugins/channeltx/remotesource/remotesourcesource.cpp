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

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include "remotesourceworker.h"
#include "remotesourcesource.h"

RemoteSourceSource::RemoteSourceSource() :
    m_running(false),
    m_sourceWorker(nullptr),
    m_nbCorrectableErrors(0),
    m_nbUncorrectableErrors(0),
    m_channelSampleRate(48000)
{
    connect(&m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()), Qt::QueuedConnection);
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
    m_currentMeta.init();
    m_dataReadQueue.setSize(20);
    applyChannelSettings(m_channelSampleRate, true);
}

RemoteSourceSource::~RemoteSourceSource()
{
    stop();
}

void RemoteSourceSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void RemoteSourceSource::pullOne(Sample& sample)
{
    m_dataReadQueue.readSample(sample, true);
}

void RemoteSourceSource::start()
{
    qDebug("RemoteSourceSource::start");

    if (m_running) {
        stop();
    }

    m_sourceWorker = new RemoteSourceWorker(&m_dataQueue);
    m_sourceWorker->moveToThread(&m_sourceWorkerThread);
    startWorker();
    m_sourceWorker->dataBind(m_settings.m_dataAddress, m_settings.m_dataPort);
    m_running = true;
}

void RemoteSourceSource::stop()
{
    qDebug("RemoteSourceSource::stop");

    if (m_sourceWorker)
    {
        stopWorker();
        m_sourceWorker->deleteLater();
        m_sourceWorker = nullptr;
    }

    m_running = false;
}

void RemoteSourceSource::startWorker()
{
    m_sourceWorker->startWork();
    m_sourceWorkerThread.start();
}

void RemoteSourceSource::stopWorker()
{
	m_sourceWorker->stopWork();
	m_sourceWorkerThread.quit();
	m_sourceWorkerThread.wait();
}

void RemoteSourceSource::handleData()
{
    RemoteDataFrame* dataFrame;

    while (m_running && ((dataFrame = m_dataQueue.pop()) != nullptr)) {
        handleDataFrame(dataFrame);
    }
}

void RemoteSourceSource::handleDataFrame(RemoteDataFrame* dataFrame)
{
    if (dataFrame->m_rxControlBlock.m_blockCount < RemoteNbOrginalBlocks)
    {
        qWarning("RemoteSourceSource::handleDataFrame: incomplete data frame (%d): not processing", dataFrame->m_rxControlBlock.m_blockCount);
    }
    else
    {
        int blockCount = 0;

        for (int blockIndex = 0; blockIndex < 256; blockIndex++)
        {
            if ((blockIndex == 0) && (dataFrame->m_rxControlBlock.m_metaRetrieved))
            {
                m_cm256DescriptorBlocks[blockCount].Index = 0;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataFrame->m_superBlocks[0].m_protectedBlock);
                blockCount++;
            }
            else if (dataFrame->m_superBlocks[blockIndex].m_header.m_blockIndex != 0)
            {
                m_cm256DescriptorBlocks[blockCount].Index = dataFrame->m_superBlocks[blockIndex].m_header.m_blockIndex;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataFrame->m_superBlocks[blockIndex].m_protectedBlock);
                blockCount++;
            }
        }

        //qDebug("RemoteSourceSource::handleDataFrame: frame: %u blocks: %d", dataFrame.m_rxControlBlock.m_frameIndex, blockCount);

        // Need to use the CM256 recovery
        if (m_cm256p &&(dataFrame->m_rxControlBlock.m_originalCount < RemoteNbOrginalBlocks))
        {
            qDebug("RemoteSourceSource::handleDataFrame: %d recovery blocks", dataFrame->m_rxControlBlock.m_recoveryCount);
            CM256::cm256_encoder_params paramsCM256;
            paramsCM256.BlockBytes = sizeof(RemoteProtectedBlock); // never changes
            paramsCM256.OriginalCount = RemoteNbOrginalBlocks;  // never changes

            if (m_currentMeta.m_tv_sec == 0) {
                paramsCM256.RecoveryCount = dataFrame->m_rxControlBlock.m_recoveryCount;
            } else {
                paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            }

            // update counters
            if (dataFrame->m_rxControlBlock.m_originalCount < RemoteNbOrginalBlocks - paramsCM256.RecoveryCount) {
                m_nbUncorrectableErrors += RemoteNbOrginalBlocks - paramsCM256.RecoveryCount - dataFrame->m_rxControlBlock.m_originalCount;
            } else {
                m_nbCorrectableErrors += dataFrame->m_rxControlBlock.m_recoveryCount;
            }

            if (m_cm256.cm256_decode(paramsCM256, m_cm256DescriptorBlocks)) // CM256 decode
            {
                qWarning() << "RemoteSourceSource::handleDataFrame: decode CM256 error:"
                        << " m_originalCount: " << dataFrame->m_rxControlBlock.m_originalCount
                        << " m_recoveryCount: " << dataFrame->m_rxControlBlock.m_recoveryCount;
            }
            else
            {
                for (int ir = 0; ir < dataFrame->m_rxControlBlock.m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = RemoteNbOrginalBlocks - dataFrame->m_rxControlBlock.m_recoveryCount + ir;
                    int blockIndex = m_cm256DescriptorBlocks[recoveryIndex].Index;
                    RemoteProtectedBlock *recoveredBlock =
                            (RemoteProtectedBlock *) m_cm256DescriptorBlocks[recoveryIndex].Block;
                    memcpy((void *) &(dataFrame->m_superBlocks[blockIndex].m_protectedBlock), recoveredBlock, sizeof(RemoteProtectedBlock));
                    if ((blockIndex == 0) && !dataFrame->m_rxControlBlock.m_metaRetrieved) {
                        dataFrame->m_rxControlBlock.m_metaRetrieved = true;
                    }
                }
            }
        }

        // Validate block zero and retrieve its data
        if (dataFrame->m_rxControlBlock.m_metaRetrieved)
        {
            RemoteMetaDataFEC *metaData = (RemoteMetaDataFEC *) &(dataFrame->m_superBlocks[0].m_protectedBlock);
            boost::crc_32_type crc32;
            crc32.process_bytes(metaData, sizeof(RemoteMetaDataFEC)-4);

            if (crc32.checksum() == metaData->m_crc32)
            {
                if (!(m_currentMeta == *metaData))
                {
                    printMeta("RemoteSourceSource::handleDataFrame", metaData);

                    if (m_currentMeta.m_sampleRate != metaData->m_sampleRate) {
                        emit newRemoteSampleRate(metaData->m_sampleRate);
                        // returns via applyChannelSettings to set interpolator
                    }
                }

                m_currentMeta = *metaData;
            }
            else
            {
                qWarning() << "RemoteSource::handleDataFrame: recovered meta: invalid CRC32";
            }
        }

        m_dataReadQueue.push(dataFrame); // Push into R/W buffer
    }
}

void RemoteSourceSource::printMeta(const QString& header, RemoteMetaDataFEC *metaData)
{
    qDebug().noquote() << header << ": "
            << "|" << metaData->m_centerFrequency
            << ":" << metaData->m_sampleRate
            << ":" << (int) (metaData->m_sampleBytes & 0xF)
            << ":" << (int) metaData->m_sampleBits
            << ":" << (int) metaData->m_nbOriginalBlocks
            << ":" << (int) metaData->m_nbFECBlocks
            << "|" << metaData->m_deviceIndex
            << ":" << metaData->m_channelIndex
            << "|" << metaData->m_tv_sec
            << ":" << metaData->m_tv_usec
            << "|";
}

void RemoteSourceSource::dataBind(const QString& dataAddress, uint16_t dataPort)
{
    if (m_sourceWorker) {
        m_sourceWorker->dataBind(dataAddress, dataPort);
    }

    m_settings.m_dataAddress = dataAddress;
    m_settings.m_dataPort = dataPort;
}

void RemoteSourceSource::applyChannelSettings(int channelSampleRate, bool force)
{
    qDebug() << "RemoteSourceSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " force: " << force;
    m_channelSampleRate = channelSampleRate;
}
