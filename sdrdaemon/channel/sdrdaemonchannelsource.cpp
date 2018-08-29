///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx)                                                 //
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

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/upchannelizer.h"
#include "dsp/devicesamplesink.h"
#include "device/devicesinkapi.h"
#include "sdrdaemonchannelsource.h"
#include "channel/sdrdaemonchannelsourcethread.h"
#include "channel/sdrdaemondatablock.h"

MESSAGE_CLASS_DEFINITION(SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource, Message)

const QString SDRDaemonChannelSource::m_channelIdURI = "sdrangel.channel.sdrdaemonsource";
const QString SDRDaemonChannelSource::m_channelId = "SDRDaemonChannelSource";

SDRDaemonChannelSource::SDRDaemonChannelSource(DeviceSinkAPI *deviceAPI) :
        ChannelSourceAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_sourceThread(0),
        m_running(false),
        m_samplesCount(0)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    connect(&m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()), Qt::QueuedConnection);
    m_cm256p = m_cm256.isInitialized() ? &m_cm256 : 0;
    m_currentMeta.init();
}

SDRDaemonChannelSource::~SDRDaemonChannelSource()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void SDRDaemonChannelSource::pull(Sample& sample)
{
    m_dataReadQueue.readSample(sample);
    m_samplesCount++;
}

void SDRDaemonChannelSource::start()
{
    qDebug("SDRDaemonChannelSink::start");

    if (m_running) {
        stop();
    }

    m_sourceThread = new SDRDaemonChannelSourceThread(&m_dataQueue);
    m_sourceThread->startStop(true);
    m_sourceThread->dataBind(m_settings.m_dataAddress, m_settings.m_dataPort);
    m_running = true;
}

void SDRDaemonChannelSource::stop()
{
    qDebug("SDRDaemonChannelSink::stop");

    if (m_sourceThread != 0)
    {
        m_sourceThread->startStop(false);
        m_sourceThread->deleteLater();
        m_sourceThread = 0;
    }

    m_running = false;
}

void SDRDaemonChannelSource::setDataLink(const QString& dataAddress, uint16_t dataPort)
{
    SDRDaemonChannelSourceSettings settings = m_settings;
    settings.m_dataAddress = dataAddress;
    settings.m_dataPort = dataPort;

    MsgConfigureSDRDaemonChannelSource *msg = MsgConfigureSDRDaemonChannelSource::create(settings, false);
    m_inputMessageQueue.push(msg);
}

bool SDRDaemonChannelSource::handleMessage(const Message& cmd __attribute__((unused)))
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "SDRDaemonChannelSource::handleMessage: UpChannelizer::MsgChannelizerNotification:"
                << " basebandSampleRate: " << notif.getBasebandSampleRate()
                << " outputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        //applyChannelSettings(notif.getBasebandSampleRate(), notif.getSampleRate(), notif.getFrequencyOffset());

        return true;
    }
    else if (MsgConfigureSDRDaemonChannelSource::match(cmd))
    {
        MsgConfigureSDRDaemonChannelSource& cfg = (MsgConfigureSDRDaemonChannelSource&) cmd;
        qDebug() << "SDRDaemonChannelSource::handleMessage: MsgConfigureSDRDaemonChannelSource";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SDRDaemonChannelSource::serialize() const
{
    return m_settings.serialize();
}

bool SDRDaemonChannelSource::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSDRDaemonChannelSource *msg = MsgConfigureSDRDaemonChannelSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSDRDaemonChannelSource *msg = MsgConfigureSDRDaemonChannelSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SDRDaemonChannelSource::applySettings(const SDRDaemonChannelSourceSettings& settings, bool force)
{
    qDebug() << "SDRDaemonChannelSource::applySettings:"
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " force: " << force;

    bool change = false;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        change = true;
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        change = true;
    }

    if (change && m_sourceThread) {
        m_sourceThread->dataBind(settings.m_dataAddress, settings.m_dataPort);
    }

    m_settings = settings;
}

void SDRDaemonChannelSource::handleDataBlock(SDRDaemonDataBlock* dataBlock)
{
    if (dataBlock->m_rxControlBlock.m_blockCount < SDRDaemonNbOrginalBlocks)
    {
        qWarning("SDRDaemonChannelSource::handleDataBlock: incomplete data block: not processing");
    }
    else
    {
        int blockCount = 0;

        for (int blockIndex = 0; blockIndex < 256; blockIndex++)
        {
            if ((blockIndex == 0) && (dataBlock->m_rxControlBlock.m_metaRetrieved))
            {
                m_cm256DescriptorBlocks[blockCount].Index = 0;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataBlock->m_superBlocks[0].m_protectedBlock);
                blockCount++;
            }
            else if (dataBlock->m_superBlocks[blockIndex].m_header.m_blockIndex != 0)
            {
                m_cm256DescriptorBlocks[blockCount].Index = dataBlock->m_superBlocks[blockIndex].m_header.m_blockIndex;
                m_cm256DescriptorBlocks[blockCount].Block = (void *) &(dataBlock->m_superBlocks[blockIndex].m_protectedBlock);
                blockCount++;
            }
        }

        //qDebug("SDRDaemonChannelSource::handleDataBlock: frame: %u blocks: %d", dataBlock.m_rxControlBlock.m_frameIndex, blockCount);

        // Need to use the CM256 recovery
        if (m_cm256p &&(dataBlock->m_rxControlBlock.m_originalCount < SDRDaemonNbOrginalBlocks))
        {
            qDebug("SDRDaemonChannelSource::handleDataBlock: %d recovery blocks", dataBlock->m_rxControlBlock.m_recoveryCount);
            CM256::cm256_encoder_params paramsCM256;
            paramsCM256.BlockBytes = sizeof(SDRDaemonProtectedBlock); // never changes
            paramsCM256.OriginalCount = SDRDaemonNbOrginalBlocks;  // never changes

            if (m_currentMeta.m_tv_sec == 0) {
                paramsCM256.RecoveryCount = dataBlock->m_rxControlBlock.m_recoveryCount;
            } else {
                paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            }

            if (m_cm256.cm256_decode(paramsCM256, m_cm256DescriptorBlocks)) // CM256 decode
            {
                qWarning() << "SDRDaemonChannelSource::handleDataBlock: decode CM256 error:"
                        << " m_originalCount: " << dataBlock->m_rxControlBlock.m_originalCount
                        << " m_recoveryCount: " << dataBlock->m_rxControlBlock.m_recoveryCount;
            }
            else
            {
                for (int ir = 0; ir < dataBlock->m_rxControlBlock.m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = SDRDaemonNbOrginalBlocks - dataBlock->m_rxControlBlock.m_recoveryCount + ir;
                    int blockIndex = m_cm256DescriptorBlocks[recoveryIndex].Index;
                    SDRDaemonProtectedBlock *recoveredBlock =
                            (SDRDaemonProtectedBlock *) m_cm256DescriptorBlocks[recoveryIndex].Block;
                    memcpy((void *) &(dataBlock->m_superBlocks[blockIndex].m_protectedBlock), recoveredBlock, sizeof(SDRDaemonProtectedBlock));
                    if ((blockIndex == 0) && !dataBlock->m_rxControlBlock.m_metaRetrieved) {
                        dataBlock->m_rxControlBlock.m_metaRetrieved = true;
                    }
                }
            }
        }

        // Validate block zero and retrieve its data
        if (dataBlock->m_rxControlBlock.m_metaRetrieved)
        {
            SDRDaemonMetaDataFEC *metaData = (SDRDaemonMetaDataFEC *) &(dataBlock->m_superBlocks[0].m_protectedBlock);
            boost::crc_32_type crc32;
            crc32.process_bytes(metaData, 20);

            if (crc32.checksum() == metaData->m_crc32)
            {
                if (!(m_currentMeta == *metaData))
                {
                    printMeta("SDRDaemonChannelSource::handleDataBlock", metaData);

                    if (m_currentMeta.m_centerFrequency != metaData->m_centerFrequency) {
                        m_deviceAPI->getSampleSink()->setCenterFrequency(metaData->m_centerFrequency);
                    }

                    if (m_currentMeta.m_sampleRate != metaData->m_sampleRate)
                    {
                        m_channelizer->configure(m_channelizer->getInputMessageQueue(), metaData->m_sampleRate, 0);
                        m_dataReadQueue.setSize(calculateDataReadQueueSize(metaData->m_sampleRate));
                    }
                }

                m_currentMeta = *metaData;
            }
            else
            {
                qWarning() << "SDRDaemonChannelSource::handleDataBlock: recovered meta: invalid CRC32";
            }
        }

        m_dataReadQueue.push(dataBlock); // Push into R/W buffer
    }
}

void SDRDaemonChannelSource::handleData()
{
    SDRDaemonDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue.pop()) != 0)) {
        handleDataBlock(dataBlock);
    }
}

void SDRDaemonChannelSource::printMeta(const QString& header, SDRDaemonMetaDataFEC *metaData)
{
    qDebug().noquote() << header << ": "
            << "|" << metaData->m_centerFrequency
            << ":" << metaData->m_sampleRate
            << ":" << (int) (metaData->m_sampleBytes & 0xF)
            << ":" << (int) metaData->m_sampleBits
            << ":" << (int) metaData->m_nbOriginalBlocks
            << ":" << (int) metaData->m_nbFECBlocks
            << "|" << metaData->m_tv_sec
            << ":" << metaData->m_tv_usec
            << "|";
}

uint32_t SDRDaemonChannelSource::calculateDataReadQueueSize(int sampleRate)
{
    // scale for 20 blocks at 48 kS/s. Take next even number.
    uint32_t maxSize = sampleRate / 2400;
    maxSize = (maxSize % 2 == 0) ? maxSize : maxSize + 1;
    qDebug("SDRDaemonChannelSource::calculateDataReadQueueSize: set max queue size to %u blocks", maxSize);
    return maxSize;
}
