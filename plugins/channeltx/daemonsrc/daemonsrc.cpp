///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <sys/time.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QDebug>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGDaemonSourceReport.h"

#include "dsp/devicesamplesink.h"
#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"

#include "daemonsrcthread.h"
#include "daemonsrc.h"

MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgConfigureDaemonSrc, Message)
MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgQueryStreamData, Message)
MESSAGE_CLASS_DEFINITION(DaemonSrc::MsgReportStreamData, Message)

const QString DaemonSrc::m_channelIdURI = "sdrangel.channeltx.daemonsrc";
const QString DaemonSrc::m_channelId ="DaemonSrc";

DaemonSrc::DaemonSrc(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI),
    m_sourceThread(0),
    m_running(false),
    m_nbCorrectableErrors(0),
    m_nbUncorrectableErrors(0)
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

DaemonSrc::~DaemonSrc()
{
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void DaemonSrc::pull(Sample& sample)
{
    m_dataReadQueue.readSample(sample);
}

void DaemonSrc::pullAudio(int nbSamples __attribute__((unused)))
{
}

void DaemonSrc::start()
{
    qDebug("DaemonSrc::start");

    if (m_running) {
        stop();
    }

    m_sourceThread = new DaemonSrcThread(&m_dataQueue);
    m_sourceThread->startStop(true);
    m_sourceThread->dataBind(m_settings.m_dataAddress, m_settings.m_dataPort);
    m_running = true;
}

void DaemonSrc::stop()
{
    qDebug("DaemonSrc::stop");

    if (m_sourceThread != 0)
    {
        m_sourceThread->startStop(false);
        m_sourceThread->deleteLater();
        m_sourceThread = 0;
    }

    m_running = false;
}

void DaemonSrc::setDataLink(const QString& dataAddress, uint16_t dataPort)
{
    DaemonSrcSettings settings = m_settings;
    settings.m_dataAddress = dataAddress;
    settings.m_dataPort = dataPort;

    MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(settings, false);
    m_inputMessageQueue.push(msg);
}

bool DaemonSrc::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "DaemonSrc::handleMessage: MsgChannelizerNotification:"
                << " basebandSampleRate: " << notif.getBasebandSampleRate()
                << " outputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        if (m_guiMessageQueue)
        {
            MsgSampleRateNotification *msg = MsgSampleRateNotification::create(notif.getBasebandSampleRate());
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    else if (MsgConfigureDaemonSrc::match(cmd))
    {
        MsgConfigureDaemonSrc& cfg = (MsgConfigureDaemonSrc&) cmd;
        qDebug() << "MsgConfigureDaemonSrc::handleMessage: MsgConfigureDaemonSrc";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgQueryStreamData::match(cmd))
    {
        if (m_guiMessageQueue)
        {
            struct timeval tv;
            gettimeofday(&tv, 0);

            MsgReportStreamData *msg = MsgReportStreamData::create(
                    tv.tv_sec,
                    tv.tv_usec,
                    m_dataReadQueue.size(),
                    m_dataReadQueue.length(),
                    m_dataReadQueue.readSampleCount(),
                    m_nbCorrectableErrors,
                    m_nbUncorrectableErrors,
                    m_currentMeta.m_nbOriginalBlocks,
                    m_currentMeta.m_nbFECBlocks,
                    m_currentMeta.m_centerFrequency,
                    m_currentMeta.m_sampleRate);
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    return false;
}

QByteArray DaemonSrc::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSrc::deserialize(const QByteArray& data __attribute__((unused)))
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DaemonSrc::applySettings(const DaemonSrcSettings& settings, bool force)
{
    qDebug() << "DaemonSrc::applySettings:"
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

void DaemonSrc::handleDataBlock(SDRDaemonDataBlock* dataBlock __attribute__((unused)))
{
    if (dataBlock->m_rxControlBlock.m_blockCount < SDRDaemonNbOrginalBlocks)
    {
        qWarning("DaemonSrc::handleDataBlock: incomplete data block: not processing");
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

        //qDebug("DaemonSrc::handleDataBlock: frame: %u blocks: %d", dataBlock.m_rxControlBlock.m_frameIndex, blockCount);

        // Need to use the CM256 recovery
        if (m_cm256p &&(dataBlock->m_rxControlBlock.m_originalCount < SDRDaemonNbOrginalBlocks))
        {
            qDebug("DaemonSrc::handleDataBlock: %d recovery blocks", dataBlock->m_rxControlBlock.m_recoveryCount);
            CM256::cm256_encoder_params paramsCM256;
            paramsCM256.BlockBytes = sizeof(SDRDaemonProtectedBlock); // never changes
            paramsCM256.OriginalCount = SDRDaemonNbOrginalBlocks;  // never changes

            if (m_currentMeta.m_tv_sec == 0) {
                paramsCM256.RecoveryCount = dataBlock->m_rxControlBlock.m_recoveryCount;
            } else {
                paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            }

            // update counters
            if (dataBlock->m_rxControlBlock.m_originalCount < SDRDaemonNbOrginalBlocks - paramsCM256.RecoveryCount) {
                m_nbUncorrectableErrors += SDRDaemonNbOrginalBlocks - paramsCM256.RecoveryCount - dataBlock->m_rxControlBlock.m_originalCount;
            } else {
                m_nbCorrectableErrors += dataBlock->m_rxControlBlock.m_recoveryCount;
            }

            if (m_cm256.cm256_decode(paramsCM256, m_cm256DescriptorBlocks)) // CM256 decode
            {
                qWarning() << "DaemonSrc::handleDataBlock: decode CM256 error:"
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
                    printMeta("DaemonSrc::handleDataBlock", metaData);

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
                qWarning() << "DaemonSrc::handleDataBlock: recovered meta: invalid CRC32";
            }
        }

        m_dataReadQueue.push(dataBlock); // Push into R/W buffer
    }
}

void DaemonSrc::handleData()
{
    SDRDaemonDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue.pop()) != 0)) {
        handleDataBlock(dataBlock);
    }
}

void DaemonSrc::printMeta(const QString& header, SDRDaemonMetaDataFEC *metaData)
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

uint32_t DaemonSrc::calculateDataReadQueueSize(int sampleRate)
{
    // scale for 20 blocks at 48 kS/s. Take next even number.
    uint32_t maxSize = sampleRate / 2400;
    maxSize = (maxSize % 2 == 0) ? maxSize : maxSize + 1;
    qDebug("DaemonSrc::calculateDataReadQueueSize: set max queue size to %u blocks", maxSize);
    return maxSize;
}

int DaemonSrc::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setDaemonSourceSettings(new SWGSDRangel::SWGDaemonSourceSettings());
    response.getDaemonSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DaemonSrc::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    DaemonSrcSettings settings = m_settings;

    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getDaemonSourceSettings()->getDataAddress();
    }
    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getDaemonSourceSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDaemonSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDaemonSourceSettings()->getTitle();
    }

    MsgConfigureDaemonSrc *msg = MsgConfigureDaemonSrc::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DaemonSrc::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDaemonSrc *msgToGUI = MsgConfigureDaemonSrc::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int DaemonSrc::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setDaemonSourceReport(new SWGSDRangel::SWGDaemonSourceReport());
    response.getDaemonSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DaemonSrc::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DaemonSrcSettings& settings)
{
    if (response.getDaemonSourceSettings()->getDataAddress()) {
        *response.getDaemonSourceSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getDaemonSourceSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getDaemonSourceSettings()->setDataPort(settings.m_dataPort);
    response.getDaemonSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getDaemonSourceSettings()->getTitle()) {
        *response.getDaemonSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getDaemonSourceSettings()->setTitle(new QString(settings.m_title));
    }
}

void DaemonSrc::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    response.getDaemonSourceReport()->setTvSec(tv.tv_sec);
    response.getDaemonSourceReport()->setTvUSec(tv.tv_usec);
    response.getDaemonSourceReport()->setQueueSize(m_dataReadQueue.size());
    response.getDaemonSourceReport()->setQueueLength(m_dataReadQueue.length());
    response.getDaemonSourceReport()->setSamplesCount(m_dataReadQueue.readSampleCount());
    response.getDaemonSourceReport()->setCorrectableErrorsCount(m_nbCorrectableErrors);
    response.getDaemonSourceReport()->setUncorrectableErrorsCount(m_nbUncorrectableErrors);
    response.getDaemonSourceReport()->setNbOriginalBlocks(m_currentMeta.m_nbOriginalBlocks);
    response.getDaemonSourceReport()->setNbFecBlocks(m_currentMeta.m_nbFECBlocks);
    response.getDaemonSourceReport()->setCenterFreq(m_currentMeta.m_centerFrequency);
    response.getDaemonSourceReport()->setSampleRate(m_currentMeta.m_sampleRate);
    response.getDaemonSourceReport()->setDeviceCenterFreq(m_deviceAPI->getSampleSink()->getCenterFrequency()/1000);
    response.getDaemonSourceReport()->setDeviceSampleRate(m_deviceAPI->getSampleSink()->getSampleRate());
}

